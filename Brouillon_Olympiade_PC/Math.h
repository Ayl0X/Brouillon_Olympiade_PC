#pragma once

#include <numeric>
#include <iostream>
#include <vector>

// Fonction de soustraction de deux vecteurs
std::vector<double> soustractionScalaire(const std::vector<double>& v1, const std::vector<double>& v2) {
    if (v1.size() != v2.size()) {
        std::cerr << "Les vecteurs doivent avoir la même taille !" << std::endl;
        return {};
    }
    std::vector<double> resultat(v1.size());
    for (size_t i = 0; i < v1.size(); ++i) {
        resultat[i] = v1[i] - v2[i];
    }
    return resultat;
}

// Ajout d'une fonction pour multiplier un vecteur par un scalaire
std::vector<double> multiplicationScalaire(const std::vector<double>& v, double scalaire) {
    std::vector<double> resultat(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        resultat[i] = v[i] * scalaire;
    }
    return resultat;
}
