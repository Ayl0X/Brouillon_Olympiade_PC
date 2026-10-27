#pragma once

#include <vector>

// --- Mecanique des fluides : chute et rebond d'un corps ---
// Simulation d'une balle dans le champ de pesanteur, avec rebond sur le sol (y = 0).
// Axes : x horizontal, y vertical vers le haut.

// Etat global de la simulation (defini dans MecaniqueDesFluides.cpp)
extern float gravity;                // pesanteur (m/s^2)
extern float coefficientRestitution; // coefficient de restitution du sol (0 = inelastique, 1 = elastique)
extern float seuilRepos;             // vitesse verticale (m/s) en dessous de laquelle la balle s'immobilise
extern std::vector<std::vector<float>> positions;  // positions de toutes les particules (m), utilisees par calculDensite()
extern std::vector<std::vector<float>> velocites;  // vitesses de toutes les particules (m/s)

struct particule {
	std::vector<float> position;
	std::vector<float> velocite;
};

// Integre un pas de temps : applique la gravite puis deplace la balle
void Update(float deltaT, particule& p);

// Collision avec le sol (y = 0). A appeler apres Update(deltaT) :
// - detection : la balle a-t-elle traverse le sol ?
// - reponse   : on la replace au niveau du sol et on fait rebondir sa vitesse
//   avec la formule de Rebond.cpp : v' = v - (1 + e) * (v.n) * n
void collision(particule& p);

// Noyau de lissage (SPH) : poids d'une particule en fonction de sa distance
float noyauDeLissage(float rayon, float distance);

// Densite SPH au point donne, somme des contributions des particules de "positions"
float calculDensite(std::vector<float> point);
