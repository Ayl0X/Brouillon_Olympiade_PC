#include <numeric>
#include <iostream>
#include <vector>
#include "Math.h"

std::vector<double> static rebond(std::vector<double> v) {
    std::vector<double> vPrime = {0.0, 0.0};
    std::vector<double> n = {0.0, 0.0};
    double e = 0.75; // Entre 0 et 1, coefficient de restitution

    // Calcul du produit scalaire
    double produitScalaire = std::inner_product(v.begin(), v.end(), n.begin(), 0.0);

    // Calcul du vecteur à soustraire : (1 + e) * produitScalaire * n
    std::vector<double> vecteurASoustraire = multiplicationScalaire(n, (1 + e) * produitScalaire);

    // Exemple d'utilisation de la soustraction
    vPrime = soustractionScalaire(v, vecteurASoustraire);

	return vPrime;
}