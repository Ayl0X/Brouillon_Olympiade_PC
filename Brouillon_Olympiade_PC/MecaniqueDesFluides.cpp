#include <numeric>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include "Math.h"
#include "RebondEtGravite.h"
#include "MecaniqueDesFluides.h"

// --- Etat global de la simulation ---
// Axes : x horizontal, y vertical vers le haut. Le sol est le plan y = 0.

float gravity = 9.81f;                 // pesanteur (m/s^2)
float coefficientRestitution = 0.75f; // coefficient de restitution du sol (0 = inelastique, 1 = elastique)
float seuilRepos = 0.1f;              // vitesse verticale (m/s) en dessous de laquelle la balle s'immobilise
float pi = 3.14159265358979323846f; // valeur de pi
float rayonDeLissage = 1.0f; // rayon de lissage pour le calcul de densité (m)

std::vector<std::vector<float>> positions;  // position (m) : position[0] = x, position[1] = y (hauteur)
std::vector<std::vector<float>> velocites;  // vitesse (m/s) : velocity[1] = vitesse verticale

void Update(float deltaT, particule& p) {
	p.velocite[1] -= gravity * deltaT;
	p.position = additionVecteur(p.position, multiplicationScalaire(p.velocite, deltaT)); // Ca revient à faire:  position += velocity * deltaT
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
		p.velocite = rebond(p.velocite, normale, coefficientRestitution, 0.0f);
		// Anti micro-rebonds : si la vitesse de rebond est trop faible, la balle s'immobilise sur le sol
		if (std::abs(p.velocite[1]) < seuilRepos) {
			p.velocite[1] = 0.0f;
		}
	}
}

float noyauDeLissage(float rayon, float distance) {
	float volume = pi * pow(rayon, 8) / 4;
	float valeur = std::max(0.0f, (rayon * rayon) - (distance * distance));
	return valeur * valeur * valeur / volume;
}

float calculDensite(std::vector<float> point) {
	float densite = 0;
	const float mass = 1;

	for (const std::vector<float> p : positions)
	{
		float distance = norme(soustractionVecteur(p, point)); // distance entre le point et la particule
		float influence = noyauDeLissage(rayonDeLissage, distance); // influence de la particule sur le point
		densite += mass * influence; // densite = somme des masses * influence
	}
	return densite;
}