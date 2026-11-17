#include <numeric>
#include <iostream>
#include <vector>
#include "..\..\Math.h"
#include "..\..\Constantes.h"
#include "rebondCoef.h"
#include "..\..\MecaniqueDesFluides\realiste\MecaniqueDesFluides.h"

// Calcule le rebond d'un vecteur vitesse sur une surface
// v      : vitesse incidente (m/s)
// n      : vecteur normal unitaire de la surface, pointant vers l'exterieur
// e      : coefficient de restitution (0 = parfaitement inelastique, 1 = parfaitement elastique)
// deltaT : pas de temps (s), non utilise : le rebond est instantane
Vec3 rebondCoef(Vec3& v, Vec3& n, float deltaT) {
    (void)deltaT; // le rebond est instantane

    // Calcul du produit scalaire v.n
	float produit = produitScalaire(v, n);

    // Calcul du vecteur a soustraire : (1 + e) * (v.n) * n
    Vec3 vecteurASoustraire = multiplicationScalaire(n, (1.0f + g_coefficientRestitution) * produit);

    // v' = v - (1 + e) * (v.n) * n
    Vec3 vPrime = soustractionScalaire(v, vecteurASoustraire);

    return vPrime;
}

// Pour les fichiers test de MecaniqueDesFluides, on utilise des std::vector<float> pour les positions et vitesses
std::vector<float> testRebond(std::vector<float>& v, std::vector<float>& n, float deltaT) {
    (void)deltaT; // le rebond est instantane

    // Calcul du produit scalaire v.n
    float produit = produitScalaire(v, n);

    // Calcul du vecteur a soustraire : (1 + e) * (v.n) * n
    std::vector<float> vecteurASoustraire = multiplicationScalaire(n, (1.0f + g_coefficientRestitution) * produit);

    // v' = v - (1 + e) * (v.n) * n
    std::vector<float> vPrime = soustractionScalaire(v, vecteurASoustraire);

    return vPrime;
}

// Collision d'une sphere (centre = p.position) avec un plan passant par l'origine
// normale : normale unitaire du plan, pointant vers l'exterieur
// rayon   : rayon de la balle
void collisionCoef(particule& p, Vec3 normale, float rayon)
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

        p.velocite = rebondCoef(p.velocite, normale, 0.0f);

        // Anti micro-rebonds : si la vitesse normale restante est negligeable, on l'annule
        // (la composante tangentielle est conservee : la balle glisse sur la pente)
        vn = produitScalaire(p.velocite, normale);
        if (std::abs(vn) < seuilRepos) {
            p.velocite = soustractionScalaire(p.velocite, multiplicationScalaire(normale, vn));
        }
    }
}


Vec3 graviteCoef(const Vec3& v, float deltaT)
{
    Vec3 resultat = v;

    // Nouvelle position
    resultat.x = v.x + v.y * deltaT - 0.5f * g_gravite * deltaT * deltaT;

    // Nouvelle vitesse
    resultat.y = v.y - g_gravite * deltaT;

    return resultat;
}
