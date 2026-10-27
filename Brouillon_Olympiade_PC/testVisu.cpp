// testVisu.cpp : visualisation 2D des particules avec SFML 3.x
//
// Controles :
//   Espace : pause / reprise
//   R      : reinitialiser
//   D      : couleur selon la vitesse <-> couleur selon la densite
//   Echap  : quitter
//
// IMPORTANT : ce fichier contient son propre main(). Il ne faut pas le compiler
// avec main.cpp dans le meme projet (voir les instructions de mise en place).

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>
#include "MecaniqueDesFluides.h"

// Variable globale definie dans MecaniqueDesFluides.cpp, utilisee par calculDensite().
// (Re-declarer un extern est sans danger si le .h la declare deja.)
extern std::vector<std::vector<float>> positions;

// --- Parametres de la vue ---
static const unsigned int LARGEUR = 800;
static const unsigned int HAUTEUR = 900;
static const float ECHELLE = 8.0f;      // 1 m = 8 pixels
static const float MARGE_BAS = 40.0f;   // espace sous le sol (pixels)
static const float RAYON_M = 0.8f;      // rayon d'affichage d'une particule (m)

// Monde (m, y vers le haut) -> ecran (pixels, y vers le bas)
static sf::Vector2f mondeVersEcran(float x, float y) {
    return { x * ECHELLE, static_cast<float>(HAUTEUR) - MARGE_BAS - y * ECHELLE };
}

// Cree une grille 10 x 10 de particules lachees sans vitesse initiale
static std::vector<particule> creerParticules() {
    std::vector<particule> liste;
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            particule p;
            p.position = { 5.0f + i * 10.0f, 20.0f + j * 7.0f };
            p.velocite = { 0.0f, 0.0f };
            liste.push_back(p);
        }
    }
    return liste;
}

// Degrade bleu -> rouge pour t dans [0, 1]
static sf::Color degrade(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return sf::Color(static_cast<std::uint8_t>(255 * t), 60,
                     static_cast<std::uint8_t>(255 * (1.0f - t)));
}

int main() {
    sf::RenderWindow fenetre(sf::VideoMode({ LARGEUR, HAUTEUR }), "Visualisation des particules");
    fenetre.setFramerateLimit(60);

    std::vector<particule> particules = creerParticules();

    const float deltaT = 0.01f;     // pas de temps de la simulation (s)
    float accumulateur = 0.0f;
    bool pause = false;
    bool modeDensite = false;

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
                case sf::Keyboard::Key::D:      modeDensite = !modeDensite; break;
                case sf::Keyboard::Key::R:
                    particules = creerParticules();
                    accumulateur = 0.0f;
                    break;
                default: break;
                }
            }
        }

        // --- Simulation a pas fixe ---
        float dtFrame = std::min(horloge.restart().asSeconds(), 0.1f); // evite les gros sauts
        if (!pause) {
            accumulateur += dtFrame;
            while (accumulateur >= deltaT) {
                for (particule& p : particules) {
                    Update(deltaT, p);
                    collision(p);
                }
                accumulateur -= deltaT;
            }
        }

        // --- Calcul des valeurs de couleur ---
        std::vector<float> valeurs(particules.size(), 0.0f);
        if (modeDensite) {
            // calculDensite() lit le tableau global "positions" : on le met a jour
            positions.clear();
            for (const particule& p : particules) positions.push_back(p.position);
            for (size_t i = 0; i < particules.size(); ++i) {
                valeurs[i] = calculDensite(particules[i].position);
            }
        }
        else {
            for (size_t i = 0; i < particules.size(); ++i) {
                valeurs[i] = std::abs(particules[i].velocite[1]);
            }
        }
        float maxValeur = 0.0f;
        for (float v : valeurs) maxValeur = std::max(maxValeur, v);
        if (maxValeur <= 0.0f) maxValeur = 1.0f;

        // --- Dessin ---
        fenetre.clear(sf::Color(25, 25, 35));

        // Sol (y = 0)
        sf::RectangleShape sol({ static_cast<float>(LARGEUR), MARGE_BAS });
        sol.setPosition({ 0.0f, static_cast<float>(HAUTEUR) - MARGE_BAS });
        sol.setFillColor(sf::Color(70, 70, 80));
        fenetre.draw(sol);

        // Particules
        const float rayonPx = RAYON_M * ECHELLE;
        sf::CircleShape cercle(rayonPx);
        cercle.setOrigin({ rayonPx, rayonPx });
        for (size_t i = 0; i < particules.size(); ++i) {
            const particule& p = particules[i];
            // le bas du cercle touche le sol quand y = 0 : on decale de un rayon
            cercle.setPosition(mondeVersEcran(p.position[0], p.position[1] + RAYON_M));
            cercle.setFillColor(degrade(valeurs[i] / maxValeur));
            fenetre.draw(cercle);
        }

        fenetre.display();

        fenetre.setTitle(std::string("Particules : ") + std::to_string(particules.size())
            + (modeDensite ? " | couleur = densite" : " | couleur = vitesse")
            + (pause ? " | PAUSE" : ""));
    }

    return 0;
}
