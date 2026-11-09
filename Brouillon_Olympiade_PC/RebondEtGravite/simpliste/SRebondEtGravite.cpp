#include <numeric>
#include <iostream>
#include <vector>
#include "..\..\Math.h"
#include "..\..\Constantes.h"
#include "..\..\MecaniqueDesFluides\realiste\RMecaniqueDesFluides.h"
#include "SRebondEtGravite.h"
#include <cmath>
#include <cstdint>
#include <cstring>

// Fast Inverse Square Root
float fastInverseSqrt(float x)
{
    if (x <= 0.0f)
        return 0.0f;

    float x2 = x * 0.5f;
    float y = x;

    std::uint32_t i;
    std::memcpy(&i, &y, sizeof(i));

    i = 0x5f3759df - (i >> 1);

    std::memcpy(&y, &i, sizeof(y));

    // Une itération de Newton-Raphson
    y = y * (1.5f - (x2 * y * y));

    return y;
}

// Calcule le rebond d'un vecteur vitesse sur une surface
Vec3 Srebond( const Vec3& v, const Vec3& n, float deltaT) {
    (void)deltaT;

    // Calcul de la norme au carré de la normale
    float normeCarree = produitScalaire(n, n);

    if (normeCarree < 1e-12f)
        return v;

    // Normalisation rapide de la normale
    float invNorme = fastInverseSqrt(normeCarree);

    Vec3 normale = multiplicationScalaire(n, invNorme);

    // Produit scalaire v . n
    float produit = produitScalaire(v, normale);

    // Pas de rebond si la particule s'éloigne de la surface
    if (produit >= 0.0f)
        return v;

    // v' = v - (1 + e) * (v . n) * n
    float facteur =
        (1.0f + g_coefficientRestitution) * produit;

    Vec3 vPrime = soustractionVecteur(v, multiplicationScalaire(normale, facteur));

    return vPrime;
}

void Scollision(particule& p)
{
    // Détection de la collision avec le sol (y = 0)
    if (p.position.y >= 0.0f)
        return;

    // Correction de la position
    p.position.y = 0.0f;
    // Rebond uniquement si la particule descend
    if (p.velocite.y < 0.0f) {
        const Vec3 normale = {
            0.0f, 1.0f, 0.0f
        };

        p.velocite = Srebond(p.velocite, normale, 0.0f);

        // Anti-micro-rebonds
        if (std::abs(p.velocite.y) < seuilRepos)
            p.velocite.y = 0.0f;
    }
}

Vec3 Sgravite(
    const Vec3& v,
    float deltaT)
{
    Vec3 resultat = v;

    resultat.x = v.x + v.x * deltaT - 0.5f * g_gravite * deltaT * deltaT;

    resultat.y = v.y - g_gravite * deltaT;

    return resultat;
}