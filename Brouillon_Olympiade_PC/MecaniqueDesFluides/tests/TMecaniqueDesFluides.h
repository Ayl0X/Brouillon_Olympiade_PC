#pragma once

#include <vector>

struct testParticule {
	std::vector<float> position;  // [x, y] (m)
	std::vector<float> velocite;  // [vx, vy] (m/s)
};

// --- Mecanique des fluides : chute et rebond d'un corps ---
// Simulation d'une balle dans le champ de pesanteur, avec rebond sur le sol (y = 0).
// Axes : x horizontal, y vertical vers le haut.

// Etat global de la simulation (defini dans MecaniqueDesFluides.cpp)
extern float testSeuilRepos;             // vitesse verticale (m/s) en dessous de laquelle la balle s'immobilise
extern std::vector<std::vector<float>> testPositions;  // positions de toutes les particules (m), utilisees par calculDensite()
extern std::vector<std::vector<float>> testVelocites;  // vitesses de toutes les particules (m/s)
extern std::vector<float> testDensites;  // densites de toutes les particules (kg/m^3)
extern std::vector<float> testProprietesParticules;  // proprietes de toutes les particules (N/m^2)
extern float testViscosite;

void testPreparerNoyaux();

// Mise a jour de la position et de la vitesse d'une particule pendant un pas de temps deltaT (s)
void testPasDeSimulation(float deltaT);

// Collision avec le sol (y = 0). A appeler apres Update(deltaT) :
// - detection : la balle a-t-elle traverse le sol ?
// - reponse   : on la replace au niveau du sol et on fait rebondir sa vitesse
//   avec la formule de Rebond.cpp : v' = v - (1 + e) * (v.n) * n
void testCollision(testParticule& p);

// Noyau de lissage (SPH) : poids d'une particule en fonction de sa distance
float testNoyauDeLissage(float rayon, float distance);

// Densite SPH au point donne, somme des contributions des particules de "positions"
float testCalculDensite(std::vector<float> point);

// Met a jour le tableau global "densites" pour toutes les particules, utilise seulement pour le test de champ et gradient (testChamp.cpp)
void testUpdateDensites();

// Propriete SPH au point donne, somme des contributions des particules de "positions"
std::vector<float> testCalculForceDePression(int IndexParticule);

// Propriete SPH au point donne, somme des contributions des particules de "positions"
std::vector<float> testCalculForceViscosite(int i);

// Conversion d'une densite en pression (N/m^2)
float testCalculPressionPartage(float densiteA, float densiteB);

///////////////////////////// Optimisation ////////////////////////////

// Recherche spatiale (table de hachage de cellules)
void testUpdateSpatialLookup();

// Applique l'action sur chaque particule voisine du point donne, en utilisant la table de hachage de cellules
template <typename Fonction>
static void testPourChaqueVoisin(const std::vector<float>& point, Fonction action);