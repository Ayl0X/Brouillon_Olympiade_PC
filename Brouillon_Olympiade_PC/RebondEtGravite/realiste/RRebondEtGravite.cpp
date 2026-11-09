#include <numeric>
#include <iostream>
#include <vector>
#include "..\..\Math.h"
#include "..\..\Constantes.h"
#include "..\..\MecaniqueDesFluides\realiste\RMecaniqueDesFluides.h"
#include "RRebondEtGravite.h"

// Calcule le rebond d'un vecteur vitesse sur une surface
// v      : vitesse incidente (m/s)
// n      : vecteur normal unitaire de la surface, pointant vers l'exterieur
// e      : coefficient de restitution (0 = parfaitement inelastique, 1 = parfaitement elastique)
// deltaT : pas de temps (s), non utilise : le rebond est instantane
Vec3 Rrebond(Vec3& v, Vec3& n, float deltaT) {
    (void)deltaT; // le rebond est instantane

    // Calcul du produit scalaire v.n
	float produit = produitScalaire(v, n);

    // Calcul du vecteur a soustraire : (1 + e) * (v.n) * n
    Vec3 vecteurASoustraire = multiplicationScalaire(n, (1.0f + g_coefficientRestitution) * produit);

    // v' = v - (1 + e) * (v.n) * n
    Vec3 vPrime = soustractionScalaire(v, vecteurASoustraire);

    return vPrime;
}

void Rcollision(particule& p)
{
    // 1) Detection de la collision avec le sol (y = 0)
    if (p.position.y >= 0.0f) {
        return;
    }

    // 2) Correction de la position
    p.position.y = 0.0f;

    // 3) Rebond uniquement si la particule descend
    if (p.velocite.y < 0.0f) {

        // Normale du sol en 3D : direction Y positive
        Vec3 normale = {0.0f, 1.0f, 0.0f};

        p.velocite = Rrebond(p.velocite,normale,0.0f);

        // Anti micro-rebonds
        if (std::abs(p.velocite.y) < seuilRepos) {
            p.velocite.y = 0.0f;
        }
    }
}


Vec3 Rgravite(const Vec3& v, float deltaT)
{
    Vec3 resultat = v;

    resultat.x = v.x + v.x * deltaT - 0.5f * g_gravite * deltaT * deltaT;

    resultat.y = v.y - g_gravite * deltaT;
    return resultat;
}
