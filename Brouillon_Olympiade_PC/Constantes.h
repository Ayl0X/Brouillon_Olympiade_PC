#pragma once

const float g_gravite = 9.81f;                 // pesanteur (m/s^2)
const float g_coefficientRestitution = 0.75f; // coefficient de restitution du sol (0 = inelastique, 1 = elastique)
const float pi = 3.14159265358979323846f; // valeur de pi

struct Vec3 {
	float x, y, z;
};

struct particule {
	Vec3 position;
	Vec3 velocite;
};
