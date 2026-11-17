#include <numeric>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include "Math.h"
#include "..\..\RebondEtGravite\realiste\RebondCoef.h"
#include "TMecaniqueDesFluides.h"

// --- Etat global de la simulation ---
// Axes : x horizontal, y vertical vers le haut. Le sol est le plan y = 0.

float testSeuilRepos = 0.1f;              
float testRayonDeLissage = 0.35f; // rayon de lissage pour le calcul de densité
float testMasse = 1.0f; 
float testDensiteCible = 35.0f;
float testMultiplicateurPression = 100.0f;

std::vector<std::vector<float>> testPositions;  // position (m) : position[0] = x, position[1] = y (hauteur)
std::vector<std::vector<float>> testVelocites;  // vitesse (m/s) : velocity[1] = vitesse verticale

std::vector<float> testProprietesParticules;
std::vector<float> testDensites;

float testViscosite = 0.5f; // coefficient de viscosite dynamique (mu)
std::vector<std::array<float, 2>> testAccelerations; // buffer d'accelerations

std::vector<std::vector<float>> testPositionsPredites;

static float volumeNoyau = 0.0f;       // pi * r^8 / 4
static float volumeDerivee = 0.0f;     // -24 / (pi * r^8)

void testPreparerNoyaux() {                // a rappeler si rayonDeLissage change
	float r8 = std::pow(testRayonDeLissage, 8.0f);
	volumeNoyau = pi * r8 / 4.0f;
	volumeDerivee = -24.0f / (pi * r8);
}

void testPasDeSimulation(float deltaT) {
	const int n = (int)testPositions.size();
	testPositionsPredites.resize(n);
	testAccelerations.resize(n);

	// 1) Gravite + positions predites
#pragma omp parallel for
	for (int i = 0; i < n; i++)
	{
		testVelocites[i][1] -= g_gravite * deltaT;
		testPositionsPredites[i] = { testPositions[i][0] + testVelocites[i][0] / 120.0f,
								 testPositions[i][1] + testVelocites[i][1] / 120.0f };
	}

	// 2) Voisins et densites calcules sur les positions predites :
	//    apres le swap, "positions" contient les predictions
	testPositions.swap(testPositionsPredites);

	testUpdateSpatialLookup();
	testUpdateDensites();

	// 3) Pression + viscosite : on stocke les accelerations (calculees a partir
	//    des vitesses du debut du pas) avant de modifier les vitesses
#pragma omp parallel for
	for (int i = 0; i < n; i++)
	{
		std::vector<float> fPression = testCalculForceDePression(i);
		std::vector<float> fViscosite = testCalculForceViscosite(i);
		const float densite = testDensites[i];

		if (densite > 1e-6f) {
			testAccelerations[i] = { (fPression[0] + fViscosite[0]) / densite,
								 (fPression[1] + fViscosite[1]) / densite };
		}
		else {
			testAccelerations[i] = { 0.0f, 0.0f };
		}
	}

	testPositions.swap(testPositionsPredites);   // on remet les vraies positions
	// 4) Application des accelerations aux vitesses
#pragma omp parallel for
	for (int i = 0; i < n; i++)
	{
		testVelocites[i][0] += testAccelerations[i][0] * deltaT;
		testVelocites[i][1] += testAccelerations[i][1] * deltaT;
	}

	// 5) Deplacement + collisions
#pragma omp parallel for
	for (int i = 0; i < n; i++)
	{
		testPositions[i][0] += testVelocites[i][0] * deltaT;
		testPositions[i][1] += testVelocites[i][1] * deltaT;

		testParticule pTemp{ testPositions[i], testVelocites[i] };
		testCollision(pTemp);
		testPositions[i] = pTemp.position;
		testVelocites[i] = pTemp.velocite;
	}
}

void testUpdateDensites() {
	testDensites.resize(testPositions.size());
#pragma omp parallel for
	for (int i = 0; i < testPositions.size(); i++)
	{
		testDensites[i] = testCalculDensite(testPositions[i]);
	}
}


void testCollision(testParticule& p) {
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
		p.velocite = testRebond(p.velocite, normale, 0.0f);
		// Anti micro-rebonds : si la vitesse de rebond est trop faible, la balle s'immobilise sur le sol
		if (std::abs(p.velocite[1]) < testSeuilRepos) {
			p.velocite[1] = 0.0f;
		}
	}
}

float testNoyauDeLissage(float rayon, float distance) {
	if (distance >= rayon) return 0.0f;
	float volume = (pi * pow(rayon, 4)) / 6;
	return (rayon - distance) * (rayon - distance) / volume;
}

static float testDeriveNoyauDeLissage(float rayon, float distance) {
	if (distance >= rayon) return 0.0f;
	float volume = 12.0f / (pi * pow(rayon, 4));
	return (distance - rayon) * volume;
}

float testCalculDensite(std::vector<float> point) {
	float densite = 0;
	testPourChaqueVoisin(point, [&](int, float distance) {
		densite += testMasse * testNoyauDeLissage(testRayonDeLissage, distance);
		});
	return densite;
}

float testCalculPropriete(std::vector<float> position) {
	float propriete = 0;
	testPourChaqueVoisin(position, [&](int i, float distance) {
		propriete += testProprietesParticules[i] * testNoyauDeLissage(testRayonDeLissage, distance) * testMasse / testDensites[i];
		});
	return propriete;
}

std::vector<float> testCalculGradient(std::vector<float> position) {
	float gx = 0.0f, gy = 0.0f;
	testPourChaqueVoisin(position, [&](int i, float distance) {
		if (distance < 1e-6f) return; // evite la division par zero
		float dirX = (testPositions[i][0] - position[0]) / distance;
		float dirY = (testPositions[i][1] - position[1]) / distance;
		float pente = testDeriveNoyauDeLissage(testRayonDeLissage, distance);
		float facteur = -testProprietesParticules[i] * pente * testMasse / testDensites[i];
		gx += dirX * facteur;
		gy += dirY * facteur;
		});
	return { gx, gy };
}

// Version optimisée de calculForceDePression en utilisant la recherche spatiale
std::vector<float> testCalculForceDePression(int IndexParticule) {
	float fx = 0.0f, fy = 0.0f;
	const float px = testPositions[IndexParticule][0];
	const float py = testPositions[IndexParticule][1];
	const float densiteI = testDensites[IndexParticule];
	testPourChaqueVoisin(testPositions[IndexParticule], [&](int j, float distance) {
		if (j == IndexParticule || distance < 1e-6f) return;

		const float dirX = (testPositions[j][0] - px) / distance;
		const float dirY = (testPositions[j][1] - py) / distance;
		const float pente = testDeriveNoyauDeLissage(testRayonDeLissage, distance);
		const float pression = testCalculPressionPartage(testDensites[j], densiteI);
		const float facteur = pression * pente * testMasse / testDensites[j];

		fx += dirX * facteur;
		fy += dirY * facteur;
		});

	return { fx, fy };
}

std::vector<float> testCalculForceViscosite(int i) {
	const float coef = 40.0f / (pi * std::pow(testRayonDeLissage, 5.0f)); // constante du Laplacien (2D)
	const float vix = testVelocites[i][0];
	const float viy = testVelocites[i][1];
	float fx = 0.0f, fy = 0.0f;

	testPourChaqueVoisin(testPositions[i], [&](int j, float distance) {
		if (j == i) return; // (v_j - v_i) = 0 de toute facon
		const float laplacien = coef * (testRayonDeLissage - distance);
		const float facteur = testViscosite * testMasse / testDensites[j] * laplacien;
		fx += (testVelocites[j][0] - vix) * facteur;
		fy += (testVelocites[j][1] - viy) * facteur;
		});

	return { fx, fy };
}

float testConvertirDensiteEnPression(float densite) {
	float erreurDeDensite = densite - testDensiteCible; // erreurDeDensite est la difference entre la densite calculee et la densite cible, si la densite est superieure a la densite cible, l'erreur est positive et la pression sera positive et inversement.
	float pression = testMultiplicateurPression * erreurDeDensite;
	return pression;
}

float testCalculPressionPartage(float densiteA, float densiteB) {
	float pressionA = testConvertirDensiteEnPression(densiteA);
	float pressionB = testConvertirDensiteEnPression(densiteB);
	return (pressionA + pressionB) / 2.0f;
}

/////////////////////// Optimisation //////////////////////////

// ---------- Recherche spatiale (table de hachage de cellules) ----------
struct Entree {
	int indexParticule;       // numero de la particule
	unsigned int cleCellule;  // cle de la cellule qui la contient
};

std::vector<Entree> testSpatialLookup;  // une entree par particule, triee par cle de cellule
std::vector<int> testStartIndices;      // startIndices[cle] = premiere position de cette cle dans spatialLookup

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
	return hash % (unsigned int)testSpatialLookup.size();
}

// A appeler apres chaque changement de positions (ou de rayonDeLissage), AVANT UpdateDensites()
void testUpdateSpatialLookup() {
	const int n = (int)testPositions.size();
	if (n == 0) return;

	testSpatialLookup.resize(n);
	testStartIndices.assign(n, std::numeric_limits<int>::max()); // "cellule vide"

	// 1) Table non triee : (particule, cle de sa cellule)
#pragma omp parallel for
	for (int i = 0; i < n; i++) {
		std::pair<int, int> cell = positionVersCellule(testPositions[i], testRayonDeLissage);
		testSpatialLookup[i] = { i, cleDepuisHash(hashCellule(cell.first, cell.second)) };
	}

	// 2) Tri par cle de cellule
	std::sort(testSpatialLookup.begin(), testSpatialLookup.end(),
		[](const Entree& a, const Entree& b) { return a.cleCellule < b.cleCellule; });

	// 3) Indice de depart de chaque cle dans la table triee
#pragma omp parallel for
	for (int i = 0; i < n; i++) {
		unsigned int key = testSpatialLookup[i].cleCellule;
		unsigned int keyPrev = (i == 0) ? std::numeric_limits<unsigned int>::max() : testSpatialLookup[i - 1].cleCellule;
		if (key != keyPrev) testStartIndices[key] = i;
	}
}

// Appelle action(indexParticule, distance) pour chaque particule a moins de rayonDeLissage de "point"
template <typename Fonction>
static void testPourChaqueVoisin(const std::vector<float>& point, Fonction action) {
	const int n = (int)testPositions.size();
	if (n == 0) return;
	if ((int)testSpatialLookup.size() != n) return; // Garde-fou

	const float r2 = testRayonDeLissage * testRayonDeLissage;
	std::pair<int, int> centre = positionVersCellule(point, testRayonDeLissage);

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
		const int debut = testStartIndices[cle];
		if (debut == std::numeric_limits<int>::max()) continue; // cellule vide

		for (int i = debut; i < n && testSpatialLookup[i].cleCellule == cle; i++) {
			const int index = testSpatialLookup[i].indexParticule;
			const float dx = testPositions[index][0] - point[0];
			const float dy = testPositions[index][1] - point[1];
			const float d2 = dx * dx + dy * dy;
			if (d2 <= r2) action(index, std::sqrt(d2));
		}
	}
}
