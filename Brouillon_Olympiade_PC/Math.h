#pragma once

#include <numeric>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include "Constantes.h"

// ============================================================
//  std::vector<float>
// ============================================================

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

// Multiplication d'un vecteur par un scalaire
inline std::vector<float> multiplicationScalaire(const std::vector<float>& v, float scalaire) {
    std::vector<float> resultat(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        resultat[i] = v[i] * scalaire;
    }
    return resultat;
}

// Division d'un vecteur par un scalaire
inline std::vector<float> divisionScalaire(const std::vector<float>& v, float scalaire) {
    if (scalaire == 0.0f) {
        std::cerr << "Division par zero !" << std::endl;
        return {};
    }
    std::vector<float> resultat(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        resultat[i] = v[i] / scalaire;
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

inline std::vector<float> GetRandomDir() {
    float angle = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159265358979323846f; // angle aleatoire entre 0 et 2π
    return { std::cos(angle), std::sin(angle) };
}

// ============================================================
//  Vec3
// ============================================================

// Addition de deux Vec3
inline Vec3 additionVecteur(const Vec3& v1, const Vec3& v2) {
    return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
}

// Soustraction de deux Vec3
inline Vec3 soustractionVecteur(const Vec3& v1, const Vec3& v2) {
    return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
}

// Meme fonction que soustractionVecteur (equivalent de soustractionScalaire pour Vec3)
inline Vec3 soustractionScalaire(const Vec3& v1, const Vec3& v2) {
    return soustractionVecteur(v1, v2);
}

// Multiplication d'un Vec3 par un scalaire
inline Vec3 multiplicationScalaire(const Vec3& v, float scalaire) {
    return { v.x * scalaire, v.y * scalaire, v.z * scalaire };
}

// Division d'un Vec3 par un scalaire
inline Vec3 divisionScalaire(const Vec3& v, float scalaire) {
    if (scalaire == 0.0f) {
        std::cerr << "Division par zero !" << std::endl;
        return { 0.0f, 0.0f, 0.0f };
    }
    return { v.x / scalaire, v.y / scalaire, v.z / scalaire };
}

// Norme d'un Vec3
inline float norme(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

// Normalisation d'un Vec3
inline Vec3 normaliser(const Vec3& v) {
    float n = norme(v);
    if (n == 0.0f) {
        return v;
    }
    return multiplicationScalaire(v, 1.0f / n);
}

// Produit scalaire de deux Vec3 (equivalent de std::inner_product)
inline float produitScalaire(const Vec3& v1, const Vec3& v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

// Racine carree composante par composante d'un Vec3
// (une composante negative donne NaN, comme std::sqrt)
inline Vec3 racineCarree(const Vec3& v) {
    return { std::sqrt(v.x), std::sqrt(v.y), std::sqrt(v.z) };
}