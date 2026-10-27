#pragma once

#include <numeric>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

// Fonction de soustraction de deux vecteurs
inline std::vector<float> soustractionScalaire(const std::vector<float>& v1, const std::vector<float>& v2) {
    if (v1.size() != v2.size()) {
        std::cerr << "Les vecteurs doivent avoir la meme taille !" << std::endl;
        return {};
    }
    std::vector<float> resultat(v1.size());
    for (size_t i = 0; i < v1.size(); ++i) {
        resultat[i] = v1[i] - v2[i];
    }
    return resultat;
}

// Ajout d'une fonction pour multiplier un vecteur par un scalaire
inline std::vector<float> multiplicationScalaire(const std::vector<float>& v, float scalaire) {
    std::vector<float> resultat(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        resultat[i] = v[i] * scalaire;
    }
    return resultat;
}

// Addition de deux vecteurs
inline std::vector<float> additionVecteur(const std::vector<float>& v1, const std::vector<float>& v2) {
    if (v1.size() != v2.size()) {
        std::cerr << "Les vecteurs doivent avoir la meme taille !" << std::endl;
        return {};
    }
    std::vector<float> resultat(v1.size());
    for (size_t i = 0; i < v1.size(); ++i) {
        resultat[i] = v1[i] + v2[i];
    }
    return resultat;
}

// Soustraction de deux vecteurs
inline std::vector<float> soustractionVecteur(const std::vector<float>& v1, const std::vector<float>& v2) {
    if (v1.size() != v2.size()) {
        std::cerr << "Les vecteurs doivent avoir la meme taille !" << std::endl;
        return {};
    }
    std::vector<float> resultat(v1.size());
    for (size_t i = 0; i < v1.size(); ++i) {
        resultat[i] = v1[i] - v2[i];
    }
    return resultat;
}

// Norme d'un vecteur
inline float norme(const std::vector<float>& v) {
    float somme = 0.0f;
    for (float val : v) {
        somme += val * val;
    }
    return std::sqrt(somme);
}

// Normalisation d'un vecteur
inline std::vector<float> normaliser(const std::vector<float>& v) {
    float n = norme(v);
    if (n == 0.0) {
        return v;
    }
    return multiplicationScalaire(v, 1.0 / n);
}