#include <iostream>
#include "MecaniqueDesFluides.h"

// Petit banc d'essai de la collision : on lache une balle de 100 m et on observe le rebond sur le sol.
// Verifications theoriques :
//   - premier impact a t = sqrt(2*h/g) = sqrt(2*100/9.81) = 4.52 s
//   - premiere vitesse d'impact : v = sqrt(2*g*h) = sqrt(2*9.81*100) = 44.3 m/s
//   - vitesse apres rebond      : e * v = 0.75 * 44.3 = 33.2 m/s
//   - hauteur du premier rebond  : e^2 * h = 56.25 m
//   (un seul rebond est visible en 10 s de simulation)
/*
int main() {
    // Conditions initiales : balle lachee de 100 m, sans vitesse initiale
    particule p = { { 0.0f, 100.0f }, { 0.0f, 0.0f } };

    const float deltaT = 0.01f;   // pas de temps (s)
    const int nbPas = 1000;       // 1000 pas de 0.01 s = 10 s de simulation
    const int pasAffichage = 25;  // affichage toutes les 0.25 s

    int nbRebonds = 0;
    float vyPrecedente = p.velocite[1];

    std::cout << "Chute d'une balle depuis 100 m (e = " << coefficientRestitution
              << ", g = " << gravity << " m/s^2)" << std::endl;
    std::cout << std::endl;
    std::cout << "t (s)\ty (m)\tvy (m/s)" << std::endl;

    for (int pas = 0; pas < nbPas; ++pas) {
        float t = pas * deltaT;

        if (pas % pasAffichage == 0) {
            std::cout << t << "\t" << p.position[1] << "\t" << p.velocite[1] << std::endl;
        }

        Update(deltaT, p); // gravite + deplacement
        collision(p);      // collision avec le sol (y = 0)

        // Un rebond vient d'avoir lieu si vy est passee de negative a positive
        if (vyPrecedente < 0.0f && p.velocite[1] > 0.0f) {
            ++nbRebonds;
            std::cout << "  -> rebond " << nbRebonds << " a t = " << t + deltaT
                      << " s : vy = " << p.velocite[1] << " m/s" << std::endl;
        }
        vyPrecedente = p.velocite[1];
    }

    // Etat final apres 10 s
    std::cout << nbPas * deltaT << "\t" << p.position[1] << "\t" << p.velocite[1] << std::endl;
    std::cout << std::endl;
    std::cout << "Resultat : " << nbRebonds << " rebond(s) en " << nbPas * deltaT << " s." << std::endl;
    std::cout << "Position finale : y = " << p.position[1] << " m, vy = " << p.velocite[1] << " m/s" << std::endl;

    return 0;
}*/
