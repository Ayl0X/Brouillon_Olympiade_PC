#include <numeric>
#include <iostream>
#include <vector>
#include "..\..\Math.h"
#include "..\..\Constantes.h"
#include "..\..\MecaniqueDesFluides\realiste\MecaniqueDesFluides.h"
#include "fastInverseSquareRoot.h"
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
Vec3 rebondFISR( const Vec3& v, const Vec3& n, float deltaT) {
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

// Collision d'une sphere (centre = p.position) avec un plan passant par l'origine
// normale : normale unitaire du plan, pointant vers l'exterieur
// rayon   : rayon de la balle
void collisionFISR(particule& p, Vec3 normale, float rayon)
{
    // 1) Distance signee entre la surface de la balle et le plan
    float dist = produitScalaire(p.position, normale) - rayon;
    if (dist >= 0.0f) {
        return;                                   // pas de contact
    }

    // 2) Correction de la position : on repousse la balle le long de la normale
    p.position = soustractionScalaire(p.position, multiplicationScalaire(normale, dist));

    // 3) Rebond uniquement si la balle va vers le plan
    float vn = produitScalaire(p.velocite, normale);
    if (vn < 0.0f) {

        p.velocite = rebondFISR(p.velocite, normale, 0.0f);

        // Anti micro-rebonds : si la vitesse normale restante est negligeable, on l'annule
        // (la composante tangentielle est conservee : la balle glisse sur la pente)
        vn = produitScalaire(p.velocite, normale);
        if (std::abs(vn) < seuilRepos) {
            p.velocite = soustractionScalaire(p.velocite, multiplicationScalaire(normale, vn));
        }
    }
}

Vec3 graviteFISR(const Vec3& v, float deltaT)
{
    Vec3 resultat = v;

    // Nouvelle position
    resultat.x = v.x + v.y * deltaT - 0.5f * g_gravite * deltaT * deltaT;

    // Nouvelle vitesse
    resultat.y = v.y - g_gravite * deltaT;

    return resultat;
}