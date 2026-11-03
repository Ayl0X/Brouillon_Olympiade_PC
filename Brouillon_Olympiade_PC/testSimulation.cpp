// testSimulation.cpp : visualisation de pasDeSimulation() (SFML 3.x)
//
// Touches :
//   Espace        : pause / lecture
//   Fleche droite : avancer d'UN pas (utile en pause)
//   R             : reinitialiser
//   Clic gauche   : attire les particules vers le curseur (maintenir)
//   Clic droit    : repousse les particules loin du curseur (maintenir)
//   G             : active / coupe la gravite (met la variable globale gravity a 0 ou a sa valeur initiale)
//   Haut / Bas    : change la vitesse qui correspond au rouge (echelle de couleur)
//   Echap         : quitter
//
// Couleur : bleu (lent) -> vert -> orange -> rouge (rapide)
//
// IMPORTANT : ce fichier a son propre main(). Ne le compilez pas avec testVisu.cpp ni testChamp.cpp.

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "MecaniqueDesFluides.h"

// Variables globales definies dans MecaniqueDesFluides.cpp
extern std::vector<std::vector<float>> positions;
extern std::vector<std::vector<float>> velocites;
extern float coefficientRestitution;
extern float gravity;
extern std::vector<float> densites;

// Fonction a tester (re-declarer est sans danger si le .h la declare deja)
void pasDeSimulation(float deltaT);

// --- Fenetre et vue ---
static const unsigned int LARGEUR = 1000;
static const unsigned int HAUTEUR = 700;
static const float ECHELLE = 60.0f;     // 1 m = 60 pixels
static const float MARGE_BAS = 20.0f;   // espace sous le sol (pixels)
static const float RAYON_M = 0.06f;     // rayon d'affichage d'une particule (m)

// Boite de simulation (m) : le sol est y = 0, gere par collision()
static const float BOITE_L = static_cast<float>(LARGEUR) / ECHELLE;
static const float BOITE_H = (static_cast<float>(HAUTEUR) - MARGE_BAS) / ECHELLE;

// --- Parametres de la simulation ---
static const float DELTA_T = 0.01f;     // pas de temps (s)
static const int PAS_MAX_PAR_IMAGE = 10;

// --- Interaction souris ---
static const float RAYON_SOURIS = 2.0f;         // rayon d'action autour du curseur (m)
static const float FORCE_SOURIS = 60.0f;        // acceleration au centre (m/s^2)
static const float AMORTISSEMENT_SOURIS = 4.0f; // freine les particules dans la zone (1/s), evite qu'elles oscillent

// Monde (m, y vers le haut) -> ecran (pixels, y vers le bas)
static sf::Vector2f mondeVersEcran(float x, float y) {
    return { x * ECHELLE, static_cast<float>(HAUTEUR) - MARGE_BAS - y * ECHELLE };
}

// Bloc de particules sur une grille legerement perturbee (pour casser la symetrie)
static void creerParticules() {
    positions.clear();
    velocites.clear();

    const int nx = 80, ny = 40;
    const float espacement = 0.035f;
    const float x0 = (BOITE_L - (nx - 1) * espacement) / 2.0f;
    const float y0 = 4.0f;

    std::mt19937 gen(7);
    std::uniform_real_distribution<float> bruit(-0.1f, 0.1f);

    for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < ny; ++j) {
            positions.push_back({ x0 + (i + bruit(gen)) * espacement, y0 + (j + bruit(gen)) * espacement });
            velocites.push_back({ 0.0f, 0.0f });
        }
    }
}

// collision() ne gere que le sol : on ajoute ici les murs gauche, droit et le plafond
static void appliquerMurs() {
    for (size_t i = 0; i < positions.size(); ++i) {
        float& x = positions[i][0];
        float& y = positions[i][1];
        float& vx = velocites[i][0];
        float& vy = velocites[i][1];

        if (x < 0.0f) { x = 0.0f;      if (vx < 0.0f) vx = -coefficientRestitution * vx; }
        if (x > BOITE_L) { x = BOITE_L;   if (vx > 0.0f) vx = -coefficientRestitution * vx; }
        if (y > BOITE_H) { y = BOITE_H;   if (vy > 0.0f) vy = -coefficientRestitution * vy; }
    }
}

// Interaction souris : sens = +1 attire vers (sx, sy), sens = -1 repousse.
// La force est maximale au centre et tombe a 0 au bord de la zone.
static void appliquerInteraction(float sx, float sy, int sens, float dt) {
    const float r2 = RAYON_SOURIS * RAYON_SOURIS;
    for (size_t i = 0; i < positions.size(); ++i) {
        const float dx = sx - positions[i][0];
        const float dy = sy - positions[i][1];
        const float d2 = dx * dx + dy * dy;
        if (d2 >= r2) continue;

        const float d = std::sqrt(d2);
        const float t = 1.0f - d / RAYON_SOURIS;   // 1 au centre, 0 au bord
        float dirX = 0.0f, dirY = 0.0f;
        if (d > 1e-4f) { dirX = dx / d; dirY = dy / d; }

        velocites[i][0] += (sens * dirX * FORCE_SOURIS - velocites[i][0] * AMORTISSEMENT_SOURIS) * t * dt;
        velocites[i][1] += (sens * dirY * FORCE_SOURIS - velocites[i][1] * AMORTISSEMENT_SOURIS) * t * dt;
    }
}

// Degrade bleu -> vert -> orange -> rouge pour t dans [0, 1]
static sf::Color couleurVitesse(float t) {
    static const sf::Color etapes[4] = {
        sf::Color(40, 110, 255),   // bleu (lent)
        sf::Color(60, 210, 90),    // vert
        sf::Color(255, 160, 30),   // orange
        sf::Color(240, 40, 40)     // rouge (rapide)
    };
    t = std::clamp(t, 0.0f, 1.0f) * 3.0f;
    const int segment = std::min(static_cast<int>(t), 2);
    const float u = t - segment;

    const sf::Color& a = etapes[segment];
    const sf::Color& b = etapes[segment + 1];
    return sf::Color(
        static_cast<std::uint8_t>(a.r + (b.r - a.r) * u),
        static_cast<std::uint8_t>(a.g + (b.g - a.g) * u),
        static_cast<std::uint8_t>(a.b + (b.b - a.b) * u));
}

// Affiche dans la console l'etat des densites et des vitesses (aide au diagnostic)
static void afficherDiagnostic(long nbPas) {
    float dMin = 1e30f, dMax = 0.0f;
    int densitesInvalides = 0;
    for (float d : densites) {
        if (!(d > 0.0f) || !std::isfinite(d)) ++densitesInvalides;
        else { dMin = std::min(dMin, d); dMax = std::max(dMax, d); }
    }

    float vMax = 0.0f;
    int particulesNaN = 0;
    for (size_t i = 0; i < positions.size(); ++i) {
        const float vx = velocites[i][0], vy = velocites[i][1];
        if (!std::isfinite(vx) || !std::isfinite(vy) ||
            !std::isfinite(positions[i][0]) || !std::isfinite(positions[i][1])) {
            ++particulesNaN;
        }
        else vMax = std::max(vMax, std::sqrt(vx * vx + vy * vy));
    }

    std::cout << "pas " << nbPas
        << " | densites : min = " << dMin << ", max = " << dMax
        << ", invalides = " << densitesInvalides << "/" << densites.size()
        << " | vitesse max = " << vMax << " m/s"
        << " | particules NaN = " << particulesNaN << std::endl;

    if (densites.size() != positions.size() || densitesInvalides == static_cast<int>(positions.size())) {
        std::cout << "  ATTENTION : aucune densite valide. UpdateSpatialLookup() est-elle appelee au debut de pasDeSimulation ?" << std::endl;
    }
}

int main() {
    sf::RenderWindow fenetre(sf::VideoMode({ LARGEUR, HAUTEUR }), "Simulation");
    fenetre.setFramerateLimit(60);
    fenetre.setKeyRepeatEnabled(false); // une pression = une action

    creerParticules();

    float accumulateur = 0.0f;
    bool pause = false;
    bool unPas = false;          // demande d'avancer d'un seul pas
    long nbPas = 0;
    float vitesseMax = 5.0f;     // vitesse (m/s) affichee en rouge
    const float graviteInitiale = gravity;   // valeur de depart de la gravite globale
    bool gravite = true;                     // la gravite est appliquee par pasDeSimulation (touche G pour la couper)

    sf::Clock horloge;

    while (fenetre.isOpen()) {
        // --- Evenements ---
        while (const std::optional<sf::Event> evenement = fenetre.pollEvent()) {
            if (evenement->is<sf::Event::Closed>()) {
                fenetre.close();
            }
            else if (const auto* touche = evenement->getIf<sf::Event::KeyPressed>()) {
                switch (touche->code) {
                case sf::Keyboard::Key::Escape: fenetre.close(); break;
                case sf::Keyboard::Key::Space:  pause = !pause; break;
                case sf::Keyboard::Key::Right:  unPas = true; break;
                case sf::Keyboard::Key::G:
                    gravite = !gravite;
                    gravity = gravite ? graviteInitiale : 0.0f;
                    break;
                case sf::Keyboard::Key::Up:     vitesseMax *= 1.25f; break;
                case sf::Keyboard::Key::Down:   vitesseMax = std::max(0.1f, vitesseMax / 1.25f); break;
                case sf::Keyboard::Key::R:
                    creerParticules();
                    accumulateur = 0.0f;
                    nbPas = 0;
                    break;
                default: break;
                }
            }
        }

        // --- Nombre de pas a executer pendant cette image ---
        const float dtImage = std::min(horloge.restart().asSeconds(), 0.1f);
        int pasAFaire = 0;
        if (!pause) {
            accumulateur += dtImage;
            while (accumulateur >= DELTA_T && pasAFaire < PAS_MAX_PAR_IMAGE) {
                ++pasAFaire;
                accumulateur -= DELTA_T;
            }
            if (pasAFaire == PAS_MAX_PAR_IMAGE) accumulateur = 0.0f; // on abandonne le retard
        }
        else if (unPas) {
            pasAFaire = 1;
        }
        unPas = false;

        // --- Etat de la souris (convertie en coordonnees du monde) ---
        const sf::Vector2i sourisPx = sf::Mouse::getPosition(fenetre);
        const float sourisX = sourisPx.x / ECHELLE;
        const float sourisY = (static_cast<float>(HAUTEUR) - MARGE_BAS - sourisPx.y) / ECHELLE;
        const bool gauche = fenetre.hasFocus() && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        const bool droit = fenetre.hasFocus() && sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
        const int interaction = (gauche == droit) ? 0 : (gauche ? +1 : -1);   // les deux ensemble : rien

        // --- Simulation ---
        for (int k = 0; k < pasAFaire; ++k) {
            pasDeSimulation(DELTA_T);
            if (interaction != 0) appliquerInteraction(sourisX, sourisY, interaction, DELTA_T);
            appliquerMurs();
            ++nbPas;
            if (nbPas == 1 || nbPas % 100 == 0) afficherDiagnostic(nbPas);
        }

        // --- Dessin ---
        fenetre.clear(sf::Color(25, 25, 35));

        // Sol
        sf::RectangleShape sol({ static_cast<float>(LARGEUR), MARGE_BAS });
        sol.setPosition({ 0.0f, static_cast<float>(HAUTEUR) - MARGE_BAS });
        sol.setFillColor(sf::Color(70, 70, 80));
        fenetre.draw(sol);

        // Particules
        const float rayonPx = RAYON_M * ECHELLE;
        sf::CircleShape cercle(rayonPx);
        cercle.setOrigin({ rayonPx, rayonPx });
        for (size_t i = 0; i < positions.size(); ++i) {
            const float vx = velocites[i][0];
            const float vy = velocites[i][1];
            const float vitesse = std::sqrt(vx * vx + vy * vy);

            cercle.setPosition(mondeVersEcran(positions[i][0], positions[i][1]));
            cercle.setFillColor(couleurVitesse(vitesse / vitesseMax));
            fenetre.draw(cercle);
        }

        // Zone d'action de la souris
        sf::CircleShape zone(RAYON_SOURIS * ECHELLE);
        zone.setOrigin({ RAYON_SOURIS * ECHELLE, RAYON_SOURIS * ECHELLE });
        zone.setPosition({ static_cast<float>(sourisPx.x), static_cast<float>(sourisPx.y) });
        zone.setFillColor(sf::Color::Transparent);
        zone.setOutlineThickness(2.0f);
        zone.setOutlineColor(interaction > 0 ? sf::Color(80, 200, 255)
            : interaction < 0 ? sf::Color(255, 90, 90)
            : sf::Color(255, 255, 255, 50));
        fenetre.draw(zone);

        fenetre.display();

        fenetre.setTitle("Particules : " + std::to_string(positions.size())
            + " | pas " + std::to_string(nbPas)
            + " | rouge = " + std::to_string(vitesseMax).substr(0, 4) + " m/s"
            + (gravite ? " | gravite ON" : " | gravite OFF")
            + (pause ? " | PAUSE (fleche droite = 1 pas)" : ""));
    }

    return 0;
}