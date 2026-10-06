#include <numeric>
#include <iostream>
#include <vector>
#include "Math.h"

std::vector<double> static gravite(std::vector<double> v) {

	std::vector<double> g = { 0.0, -9.81, 0.0 }; // Accélération due à la gravité (en m/s^2)
	std::vector<double> vPrime = { 0.0, 0.0, 0.0 }; // Vecteur vitesse après l'application de la gravité

	// Calcul de la vitesse après l'application de la gravité
	std::vector<double> vPrime = soustractionScalaire(v, g);

    return vPrime;
}
