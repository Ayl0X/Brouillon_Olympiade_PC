#pragma once

#include <vector>
#include "..\..\Constantes.h"

// --- Mecanique des fluides : chute et rebond d'un corps ---
// Simulation d'une balle dans le champ de pesanteur, avec rebond sur le sol (y = 0).
// Axes : x horizontal, y vertical vers le haut.

// Etat global de la simulation (defini dans MecaniqueDesFluides.cpp)
extern float seuilRepos;             // vitesse verticale (m/s) en dessous de laquelle la balle s'immobilise
extern std::vector<Vec3> positions;  // positions de toutes les particules (m), utilisees par calculDensite()
extern std::vector<Vec3> velocites;  // vitesses de toutes les particules (m/s)
extern std::vector<float> densites;  // densites de toutes les particules (kg/m^3)
extern std::vector<float> proprietesParticules;  // proprietes de toutes les particules (N/m^2)
extern float viscosite;

void preparerNoyaux();

// Mise a jour de la position et de la vitesse d'une particule pendant un pas de temps deltaT (s)
void pasDeSimulation(float deltaT);

// Noyau de lissage (SPH) : poids d'une particule en fonction de sa distance
float noyauDeLissage(float distance);

// Densite SPH au point donne, somme des contributions des particules de "positions"
float calculDensite(Vec3 point);

// Met a jour le tableau global "densites" pour toutes les particules, utilise seulement pour le test de champ et gradient (testChamp.cpp)
void UpdateDensites();

// Propriete SPH au point donne, somme des contributions des particules de "positions"
Vec3 calculForceDePression(int IndexParticule);

// Propriete SPH au point donne, somme des contributions des particules de "positions"
Vec3 calculForceViscosite(int i);

// Conversion d'une densite en pression (N/m^2)
float calculPressionPartage(float densiteA, float densiteB);

///////////////////////////// Optimisation ////////////////////////////

// Calcul de l'acceleration d'une particule a partir de la pression et de la viscosite
inline Vec3 calculAcceleration(int i);

// Construit la liste des voisins pour chaque particule, en utilisant la table de hachage de cellules
void construireVoisins();

// Recherche spatiale (table de hachage de cellules)
void UpdateSpatialLookup();

// Applique l'action sur chaque particule voisine du point donne, en utilisant la table de hachage de cellules
template <typename Fonction>
static void pourChaqueVoisin(const Vec3& point, Fonction action);