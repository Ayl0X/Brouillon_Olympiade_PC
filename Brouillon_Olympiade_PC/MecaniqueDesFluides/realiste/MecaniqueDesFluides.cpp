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
#include "..\..\RebondEtGravite\realiste\rebondCoef.h"
#include "MecaniqueDesFluides.h"
#ifdef _OPENMP
#include <omp.h>
#endif

// --- Etat global de la simulation ---
// Axes : x horizontal, y vertical vers le haut. Le sol est le plan y = 0.

float seuilRepos = 0.1f;
float rayonDeLissage = 0.35f;
float masse = 0.50f;                 // densite d'un reseau d'espacement 0.075 = ~1040
float densiteCible = 1.0f;
float multiplicateurPression = 70.0f;   // 70 a 150 testes stables

std::vector<Vec3> positions;  // position
std::vector<Vec3> velocites;  // vitesse

std::vector<float> proprietesParticules;
std::vector<float> densites;

float viscosite = 0.1f; // coefficient de viscosite dynamique (mu)
std::vector<Vec3> accelerations; // buffer d'accelerations

std::vector<Vec3> positionsPredites;

static float kSpiky = 0, kSpikyDeriv = 0, kVisc = 0;

// --- Tampons de travail en espace "trie" : l'indice k est la position de la particule dans l'ordre des
//     cellules de la grille (voisins contigus en memoire). sortedIdx[k] donne l'indice d'origine.
//     Les tableaux positions / velocites / densites... gardent l'ordre d'origine (indices stables). ---
static std::vector<float> rhoTrie, invRhoTrie, pressionTrie;
static std::vector<Vec3>  velTrie;
static std::vector<float> tamponThreads;   // nbThreads * 3 * n : accumulation par thread (chaque paire calculee 1 fois)
static void calculerAccelerations();

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

	// 3) Accélérations (pression + viscosité), chaque paire de voisins calculée une seule fois
	calculerAccelerations();

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
		//Rcollision(pTemp, { 0.0f, 1.0f, 0.0f }, 0.035f / 2.0f);
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

// construireVoisins(), UpdateDensites() et calculAcceleration() : voir la version optimisee en fin de fichier.

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

/////////////////////// Version optimisee (espace trie + paires uniques) //////////////////////////

static inline int nbThreadsMax() {
#ifdef _OPENMP
	return omp_get_max_threads();
#else
	return 1;
#endif
}
static inline int numeroThread() {
#ifdef _OPENMP
	return omp_get_thread_num();
#else
	return 0;
#endif
}
static inline int nbThreadsActifs() {
#ifdef _OPENMP
	return omp_get_num_threads();
#else
	return 1;
#endif
}

// Liste de voisins "demi" : pour la particule triee a, seulement les voisins k > a (indices tries).
// Chaque paire (a, k) n'apparait donc qu'une fois ; MAX_VOISINS est une limite vers le haut seulement.
void construireVoisins() {
	const int n = (int)positions.size();
	voisins.resize((size_t)n * MAX_VOISINS);
	distVoisins.resize((size_t)n * MAX_VOISINS);
	nbVoisins.resize(n);
	if (n == 0 || (int)sortedIdx.size() != n) return;

	const float r2 = rayonDeLissage * rayonDeLissage;
#pragma omp parallel for schedule(static)
	for (int a = 0; a < n; a++) {
		int c = 0;
		const size_t base = (size_t)a * MAX_VOISINS;
		const Vec3 point = sortedPos[a];

		int cx, cy, cz;
		cellulePoint(point, cx, cy, cz);
		const int x0 = std::max(cx - 1, 0), x1 = std::min(cx + 1, gNx - 1);
		const int y0 = std::max(cy - 1, 0), y1 = std::min(cy + 1, gNy - 1);
		const int z0 = std::max(cz - 1, 0), z1 = std::min(cz + 1, gNz - 1);

		for (int z = z0; z <= z1; z++) {
			for (int y = y0; y <= y1; y++) {
				const int ligne = gNx * (y + gNy * z);
				const int debut = std::max(cellStart[ligne + x0], a + 1);   // seulement k > a
				const int fin = cellStart[ligne + x1 + 1];
				for (int k = debut; k < fin; k++) {
					const float dx = sortedPos[k].x - point.x;
					const float dy = sortedPos[k].y - point.y;
					const float dz = sortedPos[k].z - point.z;
					const float d2 = dx * dx + dy * dy + dz * dz;
					if (d2 <= r2 && c < MAX_VOISINS) {
						voisins[base + c] = k;
						distVoisins[base + c] = std::sqrt(d2);
						c++;
					}
				}
			}
		}
		nbVoisins[a] = c;
	}
}

// Densites + pressions. Chaque paire ajoute son noyau aux deux particules ; accumulation par thread
// (pas de conflit d'ecriture), puis somme. Resultats copies dans l'ordre d'origine (densites, pressions).
void UpdateDensites() {
	const int n = (int)positions.size();
	densites.resize(n);
	pressions.resize(n);
	rhoTrie.resize(n);
	invRhoTrie.resize(n);
	pressionTrie.resize(n);
	if (n == 0 || (int)sortedIdx.size() != n) return;

	const size_t pas = (size_t)3 * n;
	tamponThreads.resize((size_t)nbThreadsMax() * pas);
	const float w0 = noyauDeLissage(0.0f);   // contribution de la particule elle-meme

#pragma omp parallel
	{
		const int nta = nbThreadsActifs();
		float* local = tamponThreads.data() + (size_t)numeroThread() * pas;
		std::fill(local, local + n, 0.0f);

#pragma omp for schedule(static)
		for (int a = 0; a < n; a++) {
			const size_t base = (size_t)a * MAX_VOISINS;
			float acc = 0.0f;
			for (int q = 0; q < nbVoisins[a]; q++) {
				const float w = noyauDeLissage(distVoisins[base + q]);
				acc += w;
				local[voisins[base + q]] += w;
			}
			local[a] += acc;
		}   // barriere implicite

#pragma omp for schedule(static)
		for (int a = 0; a < n; a++) {
			float rho = w0;
			for (int t = 0; t < nta; t++) rho += tamponThreads[(size_t)t * pas + a];
			rho *= masse;
			rhoTrie[a] = rho;
			invRhoTrie[a] = rho > 1e-6f ? 1.0f / rho : 0.0f;
			pressionTrie[a] = convertirDensiteEnPression(rho);
			const int o = sortedIdx[a];
			densites[o] = rho;
			pressions[o] = pressionTrie[a];
		}
	}
}

// Pression + viscosite : chaque paire est evaluee une seule fois, la force est appliquee aux deux
// particules (action / reaction). Positions = positions predites (sortedPos), vitesses = velocites courantes.
static void calculerAccelerations() {
	const int n = (int)positions.size();
	accelerations.resize(n);
	velTrie.resize(n);
	if (n == 0 || (int)sortedIdx.size() != n) return;

	const size_t pas = (size_t)3 * n;
	tamponThreads.resize((size_t)nbThreadsMax() * pas);
	const float h = rayonDeLissage;

#pragma omp parallel
	{
		const int nta = nbThreadsActifs();
		float* local = tamponThreads.data() + (size_t)numeroThread() * pas;
		std::fill(local, local + pas, 0.0f);

#pragma omp for schedule(static)
		for (int a = 0; a < n; a++) velTrie[a] = velocites[sortedIdx[a]];   // vitesses dans l'ordre trie

#pragma omp for schedule(static)
		for (int a = 0; a < n; a++) {
			const size_t base = (size_t)a * MAX_VOISINS;
			const Vec3 pa = sortedPos[a];
			const Vec3 va = velTrie[a];
			const float Pa = pressionTrie[a];
			const float invRa = invRhoTrie[a];
			float fx = 0.0f, fy = 0.0f, fz = 0.0f;

			for (int q = 0; q < nbVoisins[a]; q++) {
				const int k = voisins[base + q];
				const float d = distVoisins[base + q];
				if (d < 1e-6f) continue;
				const float invD = 1.0f / d;

				const float pPression = 0.5f * (Pa + pressionTrie[k]) * deriveNoyauDeLissage(d) * masse * invD;
				const float pVisc = viscosite * masse * kVisc * (h - d);

				// force "brute" sur a ; sur k c'est l'oppose
				const float tx = (sortedPos[k].x - pa.x) * pPression + (velTrie[k].x - va.x) * pVisc;
				const float ty = (sortedPos[k].y - pa.y) * pPression + (velTrie[k].y - va.y) * pVisc;
				const float tz = (sortedPos[k].z - pa.z) * pPression + (velTrie[k].z - va.z) * pVisc;

				const float invRk = invRhoTrie[k];
				fx += tx * invRk; fy += ty * invRk; fz += tz * invRk;
				local[3 * (size_t)k + 0] -= tx * invRa;
				local[3 * (size_t)k + 1] -= ty * invRa;
				local[3 * (size_t)k + 2] -= tz * invRa;
			}
			local[3 * (size_t)a + 0] += fx;
			local[3 * (size_t)a + 1] += fy;
			local[3 * (size_t)a + 2] += fz;
		}   // barriere implicite

#pragma omp for schedule(static)
		for (int a = 0; a < n; a++) {
			float fx = 0.0f, fy = 0.0f, fz = 0.0f;
			for (int t = 0; t < nta; t++) {
				const float* l = tamponThreads.data() + (size_t)t * pas + 3 * (size_t)a;
				fx += l[0]; fy += l[1]; fz += l[2];
			}
			const float invR = invRhoTrie[a];   // 0 si densite quasi nulle (comme avant : acceleration nulle)
			accelerations[sortedIdx[a]] = Vec3{ fx * invR, fy * invR, fz * invR };
		}
	}
}

// Conservee pour compatibilite (declaration dans le .h) : l'acceleration est calculee en bloc par calculerAccelerations()
inline Vec3 calculAcceleration(int i) {
	return accelerations[i];
}