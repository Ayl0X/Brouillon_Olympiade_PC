// testChamp.cpp : verification visuelle de calculPropriete() et calculGradient()
// (SFML 3.x)
//
// On place des particules sur une grille legerement perturbee, on leur donne la valeur
// d'un champ connu f(x, y) = cos(y - 3 + sin(x)), puis on reconstruit ce champ UNIQUEMENT
// a partir des particules avec vos fonctions :
//   - carte noir et blanc  : calculPropriete()  (doit ressembler a f)
//   - fleches violettes    : calculGradient()   (doivent pointer vers le blanc)
// Les valeurs exactes de f et de son gradient sont aussi comparees dans la console.
//
// Touches : H = carte, G = fleches, P = particules, Echap = quitter
//
// IMPORTANT : necessite  std::vector<float> proprietesParticules  (et pas vector<int>)
// dans MecaniqueDesFluides.cpp / .h. Ce fichier a son propre main().
/*
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
extern std::vector<float> proprietesParticules;
extern float rayonDeLissage;

// Fonctions a tester (re-declarer est sans danger si le .h les declare deja)
float calculPropriete(std::vector<float> position);
std::vector<float> calculGradient(std::vector<float> position);

// --- Zone affichee (m) et fenetre ---
static const float X_MIN = -6.0f, X_MAX = 6.0f;
static const float Y_MIN = -3.375f, Y_MAX = 3.375f;
static const float PX = 80.0f;   // pixels par metre
static const unsigned int LARGEUR = static_cast<unsigned int>((X_MAX - X_MIN) * PX); // 960
static const unsigned int HAUTEUR = static_cast<unsigned int>((Y_MAX - Y_MIN) * PX); // 540

// --- Parametres du test (a ajuster) ---
static const float RAYON_LISSAGE = 1.2f;       // rayon du noyau (m)
static const int NB_X = 16, NB_Y = 12;         // grille de particules (192 au total)
static const unsigned int CARTE_L = 48;        // resolution de la carte (cellules)
static const unsigned int CARTE_H = 27;        // (plus c'est grand, plus c'est lent : O(N^2) par cellule)
static const int FLECHES_X = 24, FLECHES_Y = 14;

// --- Champ de reference connu ---
static float champ(float x, float y) { return std::cos(y - 3.0f + std::sin(x)); }
static float champDx(float x, float y) { return -std::sin(y - 3.0f + std::sin(x)) * std::cos(x); }
static float champDy(float x, float y) { return -std::sin(y - 3.0f + std::sin(x)); }

// Monde (m, y vers le haut) -> ecran (pixels, y vers le bas)
static sf::Vector2f mondeVersEcran(float x, float y) {
    return { (x - X_MIN) * PX, (Y_MAX - y) * PX };
}

// Ajoute une fleche (3 segments) partant de a, dans la direction (gx, gy) du monde
static void ajouterFleche(std::vector<sf::Vertex>& v, sf::Vector2f a, float gx, float gy) {
    const float mag = std::sqrt(gx * gx + gy * gy);
    if (mag < 1e-4f) return;

    const float longueur = std::min(mag * 40.0f, 30.0f);   // longueur en pixels
    const sf::Vector2f dir(gx / mag, -gy / mag);            // y inverse a l'ecran
    const sf::Vector2f pointe = a + dir * longueur;
    const sf::Color violet(150, 80, 240);

    v.push_back({ a, violet });
    v.push_back({ pointe, violet });

    const float tete = std::min(7.0f, longueur * 0.5f);
    const float angle = 0.45f;
    for (int s = -1; s <= 1; s += 2) {
        const float c = std::cos(s * angle), si = std::sin(s * angle);
        const sf::Vector2f arriere(-dir.x, -dir.y);
        const sf::Vector2f bout(c * arriere.x - si * arriere.y, si * arriere.x + c * arriere.y);
        v.push_back({ pointe, violet });
        v.push_back({ pointe + bout * tete, violet });
    }
}

int main() {
    // ---------- 1) Creation des particules ----------
    rayonDeLissage = RAYON_LISSAGE;
    positions.clear();
    proprietesParticules.clear();

    // On etend la zone d'un rayon de lissage tout autour, sinon les bords de l'image
    // manquent de voisins et la valeur reconstruite s'effondre vers 0.
    const float marge = RAYON_LISSAGE;
    const float largeurTotale = (X_MAX - X_MIN) + 2.0f * marge;
    const float hauteurTotale = (Y_MAX - Y_MIN) + 2.0f * marge;
    const float dx = largeurTotale / NB_X;
    const float dy = hauteurTotale / NB_Y;

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> bruit(-0.4f, 0.4f);

    for (int i = 0; i < NB_X; ++i) {
        for (int j = 0; j < NB_Y; ++j) {
            const float x = X_MIN - marge + (i + 0.5f + bruit(gen)) * dx;
            const float y = Y_MIN - marge + (j + 0.5f + bruit(gen)) * dy;
            positions.push_back({ x, y });
            proprietesParticules.push_back(champ(x, y));
        }
    }
    int numParticles = static_cast<int>(positions.size());
    preparerNoyaux();
    UpdateSpatialLookup();
    UpdateDensites();

    // ---------- 2) Comparaison numerique dans la console ----------
    std::cout << "Particules : " << numParticles << ", rayon de lissage : " << rayonDeLissage << " m\n\n";
    std::cout << "point (x, y)     | f exact | f calcule | grad exact          | grad calcule\n";
    const float testes[][2] = { {0, 0}, {2, 1}, {-3, -1}, {4, -2}, {-1, 2} };
    for (const auto& pt : testes) {
        const float x = pt[0], y = pt[1];
        const float valeur = calculPropriete({ x, y });
        const std::vector<float> g = calculGradient({ x, y });
        std::cout << "(" << x << ", " << y << ")\t | "
                  << champ(x, y) << "\t | " << valeur << "\t | ("
                  << champDx(x, y) << ", " << champDy(x, y) << ")\t | ("
                  << g[0] << ", " << g[1] << ")\n";
    }
    std::cout << std::endl;

    // ---------- 3) Fleches du gradient (calculees une seule fois) ----------
    std::vector<sf::Vertex> fleches;
    for (int i = 0; i < FLECHES_X; ++i) {
        for (int j = 0; j < FLECHES_Y; ++j) {
            const float x = X_MIN + (i + 0.5f) * (X_MAX - X_MIN) / FLECHES_X;
            const float y = Y_MIN + (j + 0.5f) * (Y_MAX - Y_MIN) / FLECHES_Y;
            const std::vector<float> g = calculGradient({ x, y });
            ajouterFleche(fleches, mondeVersEcran(x, y), g[0], g[1]);
        }
    }

    // ---------- 4) Fenetre ----------
    sf::RenderWindow fenetre(sf::VideoMode({ LARGEUR, HAUTEUR }), "Test champ et gradient");
    fenetre.setFramerateLimit(60);

    // La carte est calculee progressivement (quelques cellules par image) pour ne pas figer la fenetre
    sf::Image image(sf::Vector2u{ CARTE_L, CARTE_H }, sf::Color(40, 0, 0));
    sf::Texture texture(image);
    texture.setSmooth(true);
    sf::Sprite sprite(texture);
    sprite.setScale({ (X_MAX - X_MIN) * PX / CARTE_L, (Y_MAX - Y_MIN) * PX / CARTE_H });

    unsigned int cellule = 0;
    const unsigned int totalCellules = CARTE_L * CARTE_H;

    bool afficherCarte = true, afficherFleches = true, afficherParticules = false;

    while (fenetre.isOpen()) {
        while (const std::optional<sf::Event> evenement = fenetre.pollEvent()) {
            if (evenement->is<sf::Event::Closed>()) {
                fenetre.close();
            }
            else if (const auto* touche = evenement->getIf<sf::Event::KeyPressed>()) {
                switch (touche->code) {
                case sf::Keyboard::Key::Escape: fenetre.close(); break;
                case sf::Keyboard::Key::H: afficherCarte = !afficherCarte; break;
                case sf::Keyboard::Key::G: afficherFleches = !afficherFleches; break;
                case sf::Keyboard::Key::P: afficherParticules = !afficherParticules; break;
                default: break;
                }
            }
        }

        // Calcul progressif de la carte (environ 20 ms par image)
        if (cellule < totalCellules) {
            sf::Clock budget;
            while (cellule < totalCellules && budget.getElapsedTime().asMilliseconds() < 20) {
                const unsigned int cx = cellule % CARTE_L;
                const unsigned int cy = cellule / CARTE_L;
                const float x = X_MIN + (cx + 0.5f) * (X_MAX - X_MIN) / CARTE_L;
                const float y = Y_MAX - (cy + 0.5f) * (Y_MAX - Y_MIN) / CARTE_H;

                const float valeur = calculPropriete({ x, y });
                const float t = std::clamp((valeur + 1.0f) * 0.5f, 0.0f, 1.0f); // [-1, 1] -> [0, 1]
                const std::uint8_t gris = static_cast<std::uint8_t>(255.0f * t);
                image.setPixel({ cx, cy }, sf::Color(gris, gris, gris));
                ++cellule;
            }
            texture.update(image);
            fenetre.setTitle("Calcul de la carte : " + std::to_string(100 * cellule / totalCellules) + " %");
        }
        else {
            fenetre.setTitle("Test champ | H carte, G fleches, P particules");
        }

        fenetre.clear(sf::Color(20, 20, 20));

        if (afficherCarte) fenetre.draw(sprite);
        if (afficherFleches && !fleches.empty()) {
            fenetre.draw(fleches.data(), fleches.size(), sf::PrimitiveType::Lines);
        }
        if (afficherParticules) {
            sf::CircleShape point(2.5f);
            point.setOrigin({ 2.5f, 2.5f });
            point.setFillColor(sf::Color(255, 90, 60));
            for (const std::vector<float>& p : positions) {
                point.setPosition(mondeVersEcran(p[0], p[1]));
                fenetre.draw(point);
            }
        }

        fenetre.display();
    }

    return 0;
}*/
