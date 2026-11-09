#include <numeric>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include "Math.h"
#include "..\..\RebondEtGravite\realiste\RebondEtGravite.h"
#include "TMecaniqueDesFluides.h"

// --- Etat global de la simulation ---
// Axes : x horizontal, y vertical vers le haut. Le sol est le plan y = 0.

float seuilRepos = 0.1f;              
float rayonDeLissage = 0.35f; // rayon de lissage pour le calcul de densité
float masse = 1.0f; 
float densiteCible = 35.0f;
float multiplicateurPression = 100.0f;

std::vector<std::vector<float>> positions;  // position (m) : position[0] = x, position[1] = y (hauteur)
std::vector<std::vector<float>> velocites;  // vitesse (m/s) : velocity[1] = vitesse verticale

std::vector<float> proprietesParticules;
std::vector<float> densites;

float viscosite = 0.5f; // coefficient de viscosite dynamique (mu)
std::vector<std::array<float, 2>> accelerations; // buffer d'accelerations

std::vector<std::vector<float>> positionsPredites;

static float volumeNoyau = 0.0f;       // pi * r^8 / 4
static float volumeDerivee = 0.0f;     // -24 / (pi * r^8)

void preparerNoyaux() {                // a rappeler si rayonDeLissage change
	float r8 = std::pow(rayonDeLissage, 8.0f);
	volumeNoyau = pi * r8 / 4.0f;
	volumeDerivee = -24.0f / (pi * r8);
}

void pasDeSimulation(float deltaT) {
	const int n = (int)positions.size();
	positionsPredites.resize(n);
	accelerations.resize(n);

	// 1) Gravite + positions predites
#pragma omp parallel for
	for (int i = 0; i < n; i++)
	{
		velocites[i][1] -= g_gravite * deltaT;
		positionsPredites[i] = { positions[i][0] + velocites[i][0] / 120.0f,
								 positions[i][1] + velocites[i][1] / 120.0f };
	}

	// 2) Voisins et densites calcules sur les positions predites :
	//    apres le swap, "positions" contient les predictions
	positions.swap(positionsPredites);

	UpdateSpatialLookup();
	UpdateDensites();

	// 3) Pression + viscosite : on stocke les accelerations (calculees a partir
	//    des vitesses du debut du pas) avant de modifier les vitesses
#pragma omp parallel for
	for (int i = 0; i < n; i++)
	{
		std::vector<float> fPression = calculForceDePression(i);
		std::vector<float> fViscosite = calculForceViscosite(i);
		const float densite = densites[i];

		if (densite > 1e-6f) {
			accelerations[i] = { (fPression[0] + fViscosite[0]) / densite,
								 (fPression[1] + fViscosite[1]) / densite };
		}
		else {
			accelerations[i] = { 0.0f, 0.0f };
		}
	}

	positions.swap(positionsPredites);   // on remet les vraies positions

	// 4) Application des accelerations aux vitesses
#pragma omp parallel for
	for (int i = 0; i < n; i++)
	{
		velocites[i][0] += accelerations[i][0] * deltaT;
		velocites[i][1] += accelerations[i][1] * deltaT;
	}

	// 5) Deplacement + collisions
#pragma omp parallel for
	for (int i = 0; i < n; i++)
	{
		positions[i][0] += velocites[i][0] * deltaT;
		positions[i][1] += velocites[i][1] * deltaT;

		particule pTemp{ positions[i], velocites[i] };
		collision(pTemp);
		positions[i] = pTemp.position;
		velocites[i] = pTemp.velocite;
	}
}

void UpdateDensites() {
	densites.resize(positions.size());
#pragma omp parallel for
	for (int i = 0; i < positions.size(); i++)
	{
		densites[i] = calculDensite(positions[i]);
	}
}


void collision(particule& p) {
	// 1) Detection : la balle a-t-elle traverse le sol (y = 0) ?
	if (p.position[1] >= 0.0f) {
		return; // pas de collision
	}

	// 2) Reponse en position : on replace la balle au contact du sol
	p.position[1] = 0.0f;

	// 3) Reponse en vitesse : rebond uniquement si la balle descend
	if (p.velocite[1] < 0.0f) {
		std::vector<float> normale = { 0.0f, 1.0f }; // normale du sol, pointe vers l'exterieur (vers le haut)

		// v' = v - (1 + e) * (v.n) * n  ->  ici, cela donne simplement vy' = -e * vy
		p.velocite = rebond(p.velocite, normale, 0.0f);
		// Anti micro-rebonds : si la vitesse de rebond est trop faible, la balle s'immobilise sur le sol
		if (std::abs(p.velocite[1]) < seuilRepos) {
			p.velocite[1] = 0.0f;
		}
	}
}

float noyauDeLissage(float rayon, float distance) {
	if (distance >= rayon) return 0.0f;
	float volume = (pi * pow(rayon, 4)) / 6;
	return (rayon - distance) * (rayon - distance) / volume;
}

static float deriveNoyauDeLissage(float rayon, float distance) {
	if (distance >= rayon) return 0.0f;
	float volume = 12.0f / (pi * pow(rayon, 4));
	return (distance - rayon) * volume;
}

float calculDensite(std::vector<float> point) {
	float densite = 0;
	pourChaqueVoisin(point, [&](int, float distance) {
		densite += masse * noyauDeLissage(rayonDeLissage, distance);
		});
	return densite;
}

float calculPropriete(std::vector<float> position) {
	float propriete = 0;
	pourChaqueVoisin(position, [&](int i, float distance) {
		propriete += proprietesParticules[i] * noyauDeLissage(rayonDeLissage, distance) * masse / densites[i];
		});
	return propriete;
}

std::vector<float> calculGradient(std::vector<float> position) {
	float gx = 0.0f, gy = 0.0f;
	pourChaqueVoisin(position, [&](int i, float distance) {
		if (distance < 1e-6f) return; // evite la division par zero
		float dirX = (positions[i][0] - position[0]) / distance;
		float dirY = (positions[i][1] - position[1]) / distance;
		float pente = deriveNoyauDeLissage(rayonDeLissage, distance);
		float facteur = -proprietesParticules[i] * pente * masse / densites[i];
		gx += dirX * facteur;
		gy += dirY * facteur;
		});
	return { gx, gy };
}
/*
std::vector<float> calculForceDePression(int IndexParticule) {
	std::vector<float> forceDePression = { 0.0f, 0.0f };
	for (int IndexAutreParticules = 0; IndexAutreParticules < positions.size(); IndexAutreParticules++) {

		if (IndexAutreParticules == IndexParticule) continue; // on ne calcule pas la force de pression sur soi-même

		std::vector<float> decalage = soustractionVecteur(positions[IndexAutreParticules], positions[IndexParticule]);
		float distance = norme(decalage); // distance entre le point et la particule
		if (distance < 1e-6f) continue;
		std::vector<float> direction = distance == 0.0f ? GetRandomDir() : divisionScalaire(decalage, distance);


		float pente = deriveNoyauDeLissage(rayonDeLissage, distance); // derivee du noyau de lissage
		float densite = densites[IndexAutreParticules]; // densite au point
		float pressionPartagee = calculPressionPartage(densite, densites[IndexParticule]);
		forceDePression = additionVecteur(forceDePression, multiplicationScalaire(direction, pressionPartagee * pente * masse / densite));
	}
	return forceDePression;
}*/

// Version optimisée de calculForceDePression en utilisant la recherche spatiale
std::vector<float> calculForceDePression(int IndexParticule) {
	float fx = 0.0f, fy = 0.0f;
	const float px = positions[IndexParticule][0];
	const float py = positions[IndexParticule][1];
	const float densiteI = densites[IndexParticule];

	pourChaqueVoisin(positions[IndexParticule], [&](int j, float distance) {
		if (j == IndexParticule || distance < 1e-6f) return;

		const float dirX = (positions[j][0] - px) / distance;
		const float dirY = (positions[j][1] - py) / distance;
		const float pente = deriveNoyauDeLissage(rayonDeLissage, distance);
		const float pression = calculPressionPartage(densites[j], densiteI);
		const float facteur = pression * pente * masse / densites[j];

		fx += dirX * facteur;
		fy += dirY * facteur;
		});

	return { fx, fy };
}

std::vector<float> calculForceViscosite(int i) {
	const float coef = 40.0f / (pi * std::pow(rayonDeLissage, 5.0f)); // constante du Laplacien (2D)
	const float vix = velocites[i][0];
	const float viy = velocites[i][1];
	float fx = 0.0f, fy = 0.0f;

	pourChaqueVoisin(positions[i], [&](int j, float distance) {
		if (j == i) return; // (v_j - v_i) = 0 de toute facon
		const float laplacien = coef * (rayonDeLissage - distance);
		const float facteur = viscosite * masse / densites[j] * laplacien;
		fx += (velocites[j][0] - vix) * facteur;
		fy += (velocites[j][1] - viy) * facteur;
		});

	return { fx, fy };
}

float convertirDensiteEnPression(float densite) {
	float erreurDeDesnite = densite - densiteCible; // erreurDeDensite est la difference entre la densite calculee et la densite cible, si la densite est superieure a la densite cible, l'erreur est positive et la pression sera positive et inversement.
	float pression = multiplicateurPression * erreurDeDesnite;
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

std::vector<Entree> spatialLookup;  // une entree par particule, triee par cle de cellule
std::vector<int> startIndices;      // startIndices[cle] = premiere position de cette cle dans spatialLookup

static std::pair<int, int> positionVersCellule(const std::vector<float>& point, float rayon) {
	// floor (et non troncature) pour que les coordonnees negatives soient correctes
	return { (int)std::floor(point[0] / rayon), (int)std::floor(point[1] / rayon) };
}

static unsigned int hashCellule(int cellX, int cellY) {
	unsigned int a = (unsigned int)cellX * 15823u;
	unsigned int b = (unsigned int)cellY * 9737333u;
	return a + b;
}

static unsigned int cleDepuisHash(unsigned int hash) {
	return hash % (unsigned int)spatialLookup.size();
}

// A appeler apres chaque changement de positions (ou de rayonDeLissage), AVANT UpdateDensites()
void UpdateSpatialLookup() {
	const int n = (int)positions.size();
	if (n == 0) return;

	spatialLookup.resize(n);
	startIndices.assign(n, std::numeric_limits<int>::max()); // "cellule vide"

	// 1) Table non triee : (particule, cle de sa cellule)
#pragma omp parallel for
	for (int i = 0; i < n; i++) {
		std::pair<int, int> cell = positionVersCellule(positions[i], rayonDeLissage);
		spatialLookup[i] = { i, cleDepuisHash(hashCellule(cell.first, cell.second)) };
	}

	// 2) Tri par cle de cellule
	std::sort(spatialLookup.begin(), spatialLookup.end(),
		[](const Entree& a, const Entree& b) { return a.cleCellule < b.cleCellule; });

	// 3) Indice de depart de chaque cle dans la table triee
#pragma omp parallel for
	for (int i = 0; i < n; i++) {
		unsigned int key = spatialLookup[i].cleCellule;
		unsigned int keyPrev = (i == 0) ? std::numeric_limits<unsigned int>::max() : spatialLookup[i - 1].cleCellule;
		if (key != keyPrev) startIndices[key] = i;
	}
}

// Appelle action(indexParticule, distance) pour chaque particule a moins de rayonDeLissage de "point"
template <typename Fonction>
static void pourChaqueVoisin(const std::vector<float>& point, Fonction action) {
	const int n = (int)positions.size();
	if (n == 0) return;
	if ((int)spatialLookup.size() != n) return; // Garde-fou

	const float r2 = rayonDeLissage * rayonDeLissage;
	std::pair<int, int> centre = positionVersCellule(point, rayonDeLissage);

	// Cles des 9 cellules voisines. Deux cellules differentes peuvent avoir la meme cle
	// (hash modulo n) : on ignore les doublons pour ne pas compter une particule deux fois.
	std::array<unsigned int, 9> cles;
	int nbCles = 0;
	for (int ox = -1; ox <= 1; ox++) {
		for (int oy = -1; oy <= 1; oy++) {
			unsigned int cle = cleDepuisHash(hashCellule(centre.first + ox, centre.second + oy));
			bool dejaVue = false;
			for (int k = 0; k < nbCles; k++) if (cles[k] == cle) dejaVue = true;
			if (!dejaVue) cles[nbCles++] = cle;
		}
	}

	for (int k = 0; k < nbCles; k++) {
		const unsigned int cle = cles[k];
		const int debut = startIndices[cle];
		if (debut == std::numeric_limits<int>::max()) continue; // cellule vide

		for (int i = debut; i < n && spatialLookup[i].cleCellule == cle; i++) {
			const int index = spatialLookup[i].indexParticule;
			const float dx = positions[index][0] - point[0];
			const float dy = positions[index][1] - point[1];
			const float d2 = dx * dx + dy * dy;
			if (d2 <= r2) action(index, std::sqrt(d2));
		}
	}
}
