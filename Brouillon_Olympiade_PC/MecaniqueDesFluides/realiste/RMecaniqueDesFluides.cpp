#include <numeric>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include "Math.h"
#include "..\..\Constantes.h"
#include "..\..\RebondEtGravite\realiste\RRebondEtGravite.h"
#include "RMecaniqueDesFluides.h"

// --- Etat global de la simulation ---
// Axes : x horizontal, y vertical vers le haut. Le sol est le plan y = 0.

/*
float seuilRepos = 0.1f;
float rayonDeLissage = 0.2f;
float masse = 1.0f;                 // densite d'un reseau d'espacement 0.075 = ~1040
float densiteCible = 210.0f;
float multiplicateurPression = 200.0f;   // 70 a 150 testes stables
*/

float seuilRepos = 0.1f;
float rayonDeLissage = 0.35f;
float masse = 1.0f;                 // densite d'un reseau d'espacement 0.075 = ~1040
float densiteCible = 50.0f;
float multiplicateurPression = 100.0f;   // 70 a 150 testes stables

std::vector<Vec3> positions;  // position
std::vector<Vec3> velocites;  // vitesse

std::vector<float> proprietesParticules;
std::vector<float> densites;

float viscosite = 0.1f; // coefficient de viscosite dynamique (mu)
std::vector<Vec3> accelerations; // buffer d'accelerations

std::vector<Vec3> positionsPredites;

static float kSpiky = 0, kSpikyDeriv = 0, kVisc = 0;

void preparerNoyaux() {            // à rappeler si rayonDeLissage change
	const float h = rayonDeLissage;
	const float h5 = h * h * h * h * h;
	kSpiky = 15.0f / (2.0f * pi * h5);
	kSpikyDeriv = 15.0f / (pi * h5);
	kVisc = 45.0f / (pi * h5 * h);
}

inline float noyauDeLissage(float d) {
	const float x = rayonDeLissage - d;
	return x > 0 ? kSpiky * x * x : 0.0f;
}
inline float deriveNoyauDeLissage(float d) {
	const float x = rayonDeLissage - d;
	return x > 0 ? -kSpikyDeriv * x : 0.0f;
}

void pasDeSimulation(float deltaT) {
	const int n = (int)positions.size();
	if (n == 0) return;

	positionsPredites.resize(n);
	accelerations.resize(n);

	// Facteur de prédiction (anciennement 1/120 en dur). Garde 1/120 pour
	// retrouver exactement ton comportement actuel, ou remplace par deltaT.
	const float dtPrediction = 1.0f / 240.0f;

	// 1) Gravité + positions prédites
#pragma omp parallel for
	for (int i = 0; i < n; i++)
	{
		velocites[i].y -= g_gravite * deltaT;
		positionsPredites[i] = { positions[i].x + velocites[i].x * dtPrediction,
								 positions[i].y + velocites[i].y * dtPrediction,
								 positions[i].z + velocites[i].z * dtPrediction };
	}

	// 2) Voisins, densités et pressions sur les positions prédites
	//    (après le swap, "positions" contient les prédictions)
	positions.swap(positionsPredites);

	UpdateSpatialLookup();   // grille / tri
	construireVoisins();     // UNE seule recherche de voisins par particule
	UpdateDensites();        // densités + pressions (1 calcul de pression par particule)

	// 3) Accélérations (pression + viscosité fusionnées), sans nouvelle recherche
#pragma omp parallel for schedule(static)
	for (int i = 0; i < n; i++)
	{
		accelerations[i] = calculAcceleration(i);
	}

	positions.swap(positionsPredites);   // on remet les vraies positions

	// 4) Vitesses + déplacement + collisions, fusionnés en une seule passe
#pragma omp parallel for schedule(static)
	for (int i = 0; i < n; i++)
	{
		velocites[i].x += accelerations[i].x * deltaT;
		velocites[i].y += accelerations[i].y * deltaT;
		velocites[i].z += accelerations[i].z * deltaT;

		positions[i].x += velocites[i].x * deltaT;
		positions[i].y += velocites[i].y * deltaT;
		positions[i].z += velocites[i].z * deltaT;

		particule pTemp{ positions[i], velocites[i] };
		Rcollision(pTemp);
		positions[i] = pTemp.position;
		velocites[i] = pTemp.velocite;
	}
}

float calculDensite(Vec3 point) {
	float densite = 0;
	pourChaqueVoisin(point, [&](int, float distance) {
		densite += masse * noyauDeLissage(distance);
		});
	return densite;
}

float calculPropriete(Vec3 position) {
	float propriete = 0;
	pourChaqueVoisin(position, [&](int i, float distance) {
		propriete += proprietesParticules[i] * noyauDeLissage(distance) * masse / densites[i];
		});
	return propriete;
}

Vec3 calculGradient(Vec3 position) {
	float gx = 0.0f, gy = 0.0f, gz = 0.0f;
	pourChaqueVoisin(position, [&](int i, float distance) {
		if (distance < 1e-6f) return; // evite la division par zero
		float dirX = (positions[i].x - position.x) / distance;
		float dirY = (positions[i].y - position.y) / distance;
		float dirZ = (positions[i].z - position.z) / distance;
		float pente = deriveNoyauDeLissage(distance);
		float facteur = -proprietesParticules[i] * pente * masse / densites[i];
		gx += dirX * facteur;
		gy += dirY * facteur;
		gz += dirZ * facteur;
		});
	return { gx, gy, gz };
}

// Version optimisée de calculForceDePression en utilisant la recherche spatiale
Vec3 calculForceDePression(int IndexParticule) {
	float fx = 0.0f, fy = 0.0f, fz = 0.0f;
	const float px = positions[IndexParticule].x;
	const float py = positions[IndexParticule].y;
	const float pz = positions[IndexParticule].z;
	const float densiteI = densites[IndexParticule];

	pourChaqueVoisin(positions[IndexParticule], [&](int j, float distance) {
		if (j == IndexParticule || distance < 1e-6f) return;

		const float dirX = (positions[j].x - px) / distance;
		const float dirY = (positions[j].y - py) / distance;
		const float dirZ = (positions[j].z - pz) / distance;
		const float pente = deriveNoyauDeLissage(distance);
		const float pression = calculPressionPartage(densites[j], densiteI);
		const float facteur = pression * pente * masse / densites[j];

		fx += dirX * facteur;
		fy += dirY * facteur;
		fz += dirZ * facteur;
		});

	return { fx, fy, fz };
}

Vec3 calculForceViscosite(int i) {
	const float coef = 45.0f / (pi * std::pow(rayonDeLissage, 6.0f)); // constante du Laplacien (3D)
	const float vix = velocites[i].x;
	const float viy = velocites[i].y;
	const float viz = velocites[i].z;
	float fx = 0.0f, fy = 0.0f, fz = 0.0f;

	pourChaqueVoisin(positions[i], [&](int j, float distance) {
		if (j == i) return; // (v_j - v_i) = 0 de toute facon
		const float laplacien = coef * (rayonDeLissage - distance);
		const float facteur = viscosite * masse / densites[j] * laplacien;
		fx += (velocites[j].x - vix) * facteur;
		fy += (velocites[j].y - viy) * facteur;
		fz += (velocites[j].z - viz) * facteur;
		});

	return { fx, fy, fz };
}

float convertirDensiteEnPression(float densite) {
	float erreurDeDesnite = densite - densiteCible; // erreurDeDensite est la difference entre la densite calculee et la densite cible, si la densite est superieure a la densite cible, l'erreur est positive et la pression sera positive et inversement.
	float pression = std::max(0.0f, multiplicateurPression * erreurDeDesnite);
	return pression;
}

float calculPressionPartage(float densiteA, float densiteB) {
	float pressionA = convertirDensiteEnPression(densiteA);
	float pressionB = convertirDensiteEnPression(densiteB);
	return (pressionA + pressionB) / 2.0f;
}

/////////////////////// Optimisation //////////////////////////

// ---------- Recherche spatiale (table de hachage de cellules) ----------
struct Entree {
	int indexParticule;       // numero de la particule
	unsigned int cleCellule;  // cle de la cellule qui la contient
};

constexpr int MAX_VOISINS = 128;
std::vector<int>   voisins;      // n * MAX_VOISINS
std::vector<float> distVoisins;  // idem
std::vector<int>   nbVoisins;    // n
std::vector<float> pressions;    // n

void construireVoisins() {
	const int n = (int)positions.size();
	voisins.resize((size_t)n * MAX_VOISINS);
	distVoisins.resize((size_t)n * MAX_VOISINS);
	nbVoisins.resize(n);
#pragma omp parallel for
	for (int i = 0; i < n; i++) {
		int c = 0;
		const size_t base = (size_t)i * MAX_VOISINS;
		pourChaqueVoisin(positions[i], [&](int j, float d) {
			if (j != i && c < MAX_VOISINS) {
				voisins[base + c] = j;
				distVoisins[base + c] = d;
				c++;
			}
			});
		nbVoisins[i] = c;
	}
}

void UpdateDensites() {
	const int n = (int)positions.size();
	densites.resize(n);
	pressions.resize(n);
#pragma omp parallel for
	for (int i = 0; i < n; i++) {
		float rho = noyauDeLissage(0.0f);            // contribution de i lui-même
		const size_t base = (size_t)i * MAX_VOISINS;
		for (int k = 0; k < nbVoisins[i]; k++)
			rho += noyauDeLissage(distVoisins[base + k]);
		densites[i] = masse * rho;
		pressions[i] = convertirDensiteEnPression(densites[i]);   // 1 fois par particule
	}
}

// Pression + viscosité dans la même boucle, sans nouvelle recherche
inline Vec3 calculAcceleration(int i) {
	float fx = 0, fy = 0, fz = 0;
	const size_t base = (size_t)i * MAX_VOISINS;
	for (int k = 0; k < nbVoisins[i]; k++) {
		const int j = voisins[base + k];
		const float d = distVoisins[base + k];
		if (d < 1e-6f) continue;
		const float invD = 1.0f / d;
		const float rhoJ = densites[j];

		const float pP = 0.5f * (pressions[i] + pressions[j]) * deriveNoyauDeLissage(d) * masse / rhoJ * invD;
		fx += (positions[j].x - positions[i].x) * pP;
		fy += (positions[j].y - positions[i].y) * pP;
		fz += (positions[j].z - positions[i].z) * pP;

		const float pV = viscosite * masse / rhoJ * kVisc * (rayonDeLissage - d);
		fx += (velocites[j].x - velocites[i].x) * pV;
		fy += (velocites[j].y - velocites[i].y) * pV;
		fz += (velocites[j].z - velocites[i].z) * pV;
	}
	const float rho = densites[i];
	return rho > 1e-6f ? Vec3{ fx / rho, fy / rho, fz / rho } : Vec3{ 0, 0, 0 };
}

std::vector<Entree> spatialLookup;  // une entree par particule, triee par cle de cellule
std::vector<int> startIndices;      // startIndices[cle] = premiere position de cette cle dans spatialLookup

static std::tuple<int, int, int> positionVersCellule(const Vec3& point, float rayon) {
	// floor (et non troncature) pour que les coordonnees negatives soient correctes
	return { (int)std::floor(point.x / rayon), (int)std::floor(point.y / rayon), (int)std::floor(point.z / rayon) };
}

static unsigned int hashCellule(int cellX, int cellY, int cellZ) {
	unsigned int a = (unsigned int)cellX * 15823u;
	unsigned int b = (unsigned int)cellY * 9737333u;
	unsigned int c = (unsigned int)cellZ * 1937333u;
	return a + b + c;
}

static unsigned int cleDepuisHash(unsigned int hash) {
	return hash % (unsigned int)spatialLookup.size();
}

static float gMinX = 0, gMinY = 0, gMinZ = 0;   // origine de la grille
static float gInvTaille = 1.0f;                  // 1 / taille d'une cellule
static int gNx = 1, gNy = 1, gNz = 1;            // dimensions de la grille

std::vector<int>  cellStart;    // taille nbCellules + 1
std::vector<int>  cellCurseur;  // curseurs temporaires pour la repartition
std::vector<int>  cellDeLaParticule;  // cellule de chaque particule
std::vector<int>  sortedIdx;    // indices de particules, ranges par cellule
std::vector<Vec3> sortedPos;    // copie des positions dans le meme ordre (acces contigus)

constexpr long long MAX_CELLULES = 400000;      // ~16 Mo pour cellStart

// Retourne les coordonnees de cellule d'un point, bornees a la grille.
// Le clamp est sans danger : il est monotone, donc deux points a moins d'une
// cellule l'un de l'autre restent dans des cellules adjacentes.
static inline void cellulePoint(const Vec3& p, int& cx, int& cy, int& cz) {
	float fx = (p.x - gMinX) * gInvTaille;
	float fy = (p.y - gMinY) * gInvTaille;
	float fz = (p.z - gMinZ) * gInvTaille;
	fx = std::max(0.0f, std::min(fx, (float)(gNx - 1)));
	fy = std::max(0.0f, std::min(fy, (float)(gNy - 1)));
	fz = std::max(0.0f, std::min(fz, (float)(gNz - 1)));
	cx = (int)fx; cy = (int)fy; cz = (int)fz;
}

// A appeler apres chaque changement de positions (ou de rayonDeLissage), AVANT construireVoisins()
void UpdateSpatialLookup() {
	const int n = (int)positions.size();
	if (n == 0) return;

	// 1) Boite englobante (min/max par thread, fusion a la fin ;
	//    MSVC OpenMP 2.0 ne supporte pas reduction(min/max))
	float mn[3] = { std::numeric_limits<float>::max(),  std::numeric_limits<float>::max(),  std::numeric_limits<float>::max() };
	float mx[3] = { -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };
#pragma omp parallel
	{
		float lmn[3] = { mn[0], mn[1], mn[2] };
		float lmx[3] = { mx[0], mx[1], mx[2] };
#pragma omp for nowait
		for (int i = 0; i < n; i++) {
			lmn[0] = std::min(lmn[0], positions[i].x); lmx[0] = std::max(lmx[0], positions[i].x);
			lmn[1] = std::min(lmn[1], positions[i].y); lmx[1] = std::max(lmx[1], positions[i].y);
			lmn[2] = std::min(lmn[2], positions[i].z); lmx[2] = std::max(lmx[2], positions[i].z);
		}
#pragma omp critical
		{
			for (int a = 0; a < 3; a++) {
				mn[a] = std::min(mn[a], lmn[a]);
				mx[a] = std::max(mx[a], lmx[a]);
			}
		}
	}

	// 2) Dimensions de la grille. Une cellule peut etre plus grande que
	//    rayonDeLissage sans fausser la recherche (les 27 cellules couvrent
	//    toujours le rayon) : si la grille est trop grosse, on grossit les cellules.
	float taille = rayonDeLissage;
	long long nbCellules;
	do {
		gInvTaille = 1.0f / taille;
		gMinX = mn[0]; gMinY = mn[1]; gMinZ = mn[2];
		gNx = (int)((mx[0] - mn[0]) * gInvTaille) + 1;
		gNy = (int)((mx[1] - mn[1]) * gInvTaille) + 1;
		gNz = (int)((mx[2] - mn[2]) * gInvTaille) + 1;
		nbCellules = (long long)gNx * gNy * gNz;
		if (nbCellules > MAX_CELLULES) taille *= 1.5f;
	} while (nbCellules > MAX_CELLULES);

	// 3) Cellule de chaque particule (parallele)
	cellDeLaParticule.resize(n);
#pragma omp parallel for
	for (int i = 0; i < n; i++) {
		int cx, cy, cz;
		cellulePoint(positions[i], cx, cy, cz);
		cellDeLaParticule[i] = cx + gNx * (cy + gNy * cz);
	}

	// 4) Tri par comptage : histogramme -> somme prefixe -> repartition
	//    O(n + nbCellules), deterministe (boucles sequentielles tres legeres)
	cellStart.assign((size_t)nbCellules + 1, 0);
	for (int i = 0; i < n; i++) cellStart[cellDeLaParticule[i] + 1]++;
	for (long long c = 0; c < nbCellules; c++) cellStart[c + 1] += cellStart[c];

	cellCurseur.assign(cellStart.begin(), cellStart.begin() + nbCellules);
	sortedIdx.resize(n);
	for (int i = 0; i < n; i++) sortedIdx[cellCurseur[cellDeLaParticule[i]]++] = i;

	// 5) Positions rangees dans l'ordre des cellules : la boucle interne de
	//    pourChaqueVoisin lit alors de la memoire contigue
	sortedPos.resize(n);
#pragma omp parallel for
	for (int k = 0; k < n; k++) sortedPos[k] = positions[sortedIdx[k]];
}

// Appelle action(indexParticule, distance) pour chaque particule a moins de rayonDeLissage de "point"
template <typename Fonction>
static void pourChaqueVoisin(const Vec3& point, Fonction action) {
	const int n = (int)positions.size();
	if (n == 0) return;
	if ((int)sortedIdx.size() != n) return; // Garde-fou

	const float r2 = rayonDeLissage * rayonDeLissage;

	int cx, cy, cz;
	cellulePoint(point, cx, cy, cz);

	const int x0 = std::max(cx - 1, 0), x1 = std::min(cx + 1, gNx - 1);
	const int y0 = std::max(cy - 1, 0), y1 = std::min(cy + 1, gNy - 1);
	const int z0 = std::max(cz - 1, 0), z1 = std::min(cz + 1, gNz - 1);

	// Les cellules x0..x1 d'une meme ligne sont consecutives : une seule plage
	// contigue par ligne (y, z), soit 9 plages au lieu de 27 recherches.
	for (int z = z0; z <= z1; z++) {
		for (int y = y0; y <= y1; y++) {
			const int ligne = gNx * (y + gNy * z);
			const int debut = cellStart[ligne + x0];
			const int fin = cellStart[ligne + x1 + 1];
			for (int k = debut; k < fin; k++) {
				const float dx = sortedPos[k].x - point.x;
				const float dy = sortedPos[k].y - point.y;
				const float dz = sortedPos[k].z - point.z;
				const float d2 = dx * dx + dy * dy + dz * dz;
				if (d2 <= r2) action(sortedIdx[k], std::sqrt(d2));
			}
		}
	}
}