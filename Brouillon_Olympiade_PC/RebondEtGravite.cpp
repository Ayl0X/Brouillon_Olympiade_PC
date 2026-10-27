#include <numeric>
#include <iostream>
#include <vector>
#include "Math.h"
#include "RebondEtGravite.h"

// Calcule le rebond d'un vecteur vitesse sur une surface
// v      : vitesse incidente (m/s)
// n      : vecteur normal unitaire de la surface, pointant vers l'exterieur
// e      : coefficient de restitution (0 = parfaitement inelastique, 1 = parfaitement elastique)
// deltaT : pas de temps (s), non utilise : le rebond est instantane
std::vector<float> rebond(const std::vector<float>& v, const std::vector<float>& n, float e, float deltaT) {
    (void)deltaT; // le rebond est instantane

    if (v.size() != n.size()) {
        std::cerr << "Les vecteurs doivent avoir la meme taille !" << std::endl;
        return v;
    }

    // Calcul du produit scalaire v.n
    float produitScalaire = std::inner_product(v.begin(), v.end(), n.begin(), 0.0f);

    // Calcul du vecteur a soustraire : (1 + e) * (v.n) * n
    std::vector<float> vecteurASoustraire = multiplicationScalaire(n, (1.0f + e) * produitScalaire);

    // v' = v - (1 + e) * (v.n) * n
    std::vector<float> vPrime = soustractionScalaire(v, vecteurASoustraire);

    return vPrime;
}

std::vector<double> gravite(const std::vector<double>& v, double deltaT)
{
    return std::vector<double>();
}
