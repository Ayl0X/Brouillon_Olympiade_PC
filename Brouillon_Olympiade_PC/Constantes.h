#pragma once

const float g_gravite = 9.81f;                 // pesanteur (m/s^2)
const float g_coefficientRestitution = 0.6f; // coefficient de restitution du sol (0 = inelastique, 1 = elastique)
const float pi = 3.14159265358979323846f; // valeur de pi
const float g_coefficientDeTrainee = 0.47f; // coefficient de traînée pour une sphère (sans unité)
const float g_rayonBalle = 0.035f / 2.0f; // rayon de la balle (m)
const float g_massBalle = 0.012f; // masse de la balle (kg)

struct Vec3 {
	float x, y, z;
};

struct Vec2 {
	float x, y;
};

struct particule {
	Vec3 position;
	Vec3 velocite;
};
