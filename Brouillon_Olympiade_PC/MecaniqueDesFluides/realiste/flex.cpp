// =====================================================================================
//  flex.cpp  --  Simulation SPH 3D : apercu temps reel (SFML 3 + OpenGL 3.3) + export hors ligne
//
//  - utilise MecaniqueDesFluides.cpp / .h tels quels (aucune modification necessaire)
//  - ajoute : boite (pave droit) avec collisions, bloc de fluide initial, camera orbitale,
//             pas de temps fixe, enregistrement des positions image par image
//
//  A placer dans le meme dossier que MecaniqueDesFluides.cpp et flex_gl.h.
//  Edition de liens : sfml-window, sfml-system et opengl32 (sfml-graphics n'est plus necessaire).
//  Commandes :
//     souris gauche + deplacement : tourner     molette : zoom
//     Espace : pause     N : une image (en pause)     R : recommencer
//     E : demarrer / arreter l'enregistrement (dossier cache/)     Echap : quitter
// =====================================================================================

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

#include <SFML/Window.hpp>

// Memes inclusions (et meme ordre) que MecaniqueDesFluides.cpp
#include "Math.h"
#include "..\..\Constantes.h"
#include "..\..\RebondEtGravite\realiste\rebondCoef.h"
#include "MecaniqueDesFluides.h"
#include "flex_gl.h"      // apres les en-tetes du projet : il inclut windows.h via SFML/OpenGL.hpp

// Redeclarations sans danger (si le .h les contient deja, c'est simplement ignore)
extern std::vector<Vec3> positions;
extern std::vector<Vec3> velocites;
extern std::vector<int>  nbVoisins;
extern float rayonDeLissage, masse, densiteCible, multiplicateurPression, viscosite;
void preparerNoyaux();
void pasDeSimulation(float deltaT);

namespace {

	constexpr float PI_F = 3.14159265358979f;

	// Le VBO est rempli directement depuis std::vector<Vec3> : il faut exactement 3 floats contigus.
	static_assert(sizeof(Vec3) == 3 * sizeof(float) && std::is_standard_layout_v<Vec3>,
		"Vec3 n'est pas 3 floats contigus : copier dans un tampon intermediaire avant l'envoi au GPU");

	// -------------------------------------------------------------------------------------
	//  Reglages (tout ce qu'on peut vouloir changer est ici)
	// -------------------------------------------------------------------------------------
	namespace Cfg {
		// Temps : une "image" de la video = 1/fps seconde simulee, decoupee en sousPas pas.
		constexpr float fps = 30.0f;
		constexpr int   sousPas = 6;                 // dt de simulation = 1 / (fps * sousPas)

		// Fluide
		constexpr float h = 0.20f;                   // rayon de lissage (remplace 0.35 de ton .cpp)
		constexpr float rapportHSurEspacement = 3.0f;
		constexpr float espacement = h / rapportHSurEspacement;   // ecart entre particules au depart
		constexpr float rho0 = 1000.0f;              // densite visee (eau)

		// Boite : pave droit, coin minimum a l'origine, le sol est le plan y = 0
		constexpr float boiteX = 3.0f, boiteY = 1.6f, boiteZ = 1.2f;

		// Bloc de fluide pose dans le coin x = 0 (type "rupture de barrage")
		constexpr float blocX = 1.2f, blocY = 1.3f, blocZ = 1.1f;
		constexpr float jitter = 0.02f;              // decalage aleatoire, en fraction de l'espacement

		// Parois
		constexpr float restitution = 0.3f;          // 0 = colle, 1 = rebond parfait
		constexpr float frottementParoi = 0.05f;     // part de vitesse tangentielle perdue a chaque contact

		// Rendu de l'apercu
		constexpr float rayonRendu = 0.55f * espacement;

		// Enregistrement
		constexpr int   framesMaxEnregistrement = 600;   // arret automatique (10 s a 60 fps)
		constexpr int   plafondVoisins = 192;            // = MAX_VOISINS de MecaniqueDesFluides.cpp
	}

	// -------------------------------------------------------------------------------------
	//  Petits outils vectoriels (independants de Math.h)
	// -------------------------------------------------------------------------------------
	struct V3 { float x, y, z; };
	inline V3    operator+(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	inline V3    operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
	inline V3    operator*(V3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
	inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	inline V3    cross(V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
	inline V3    normaliser(V3 a) { const float l = std::sqrt(dot(a, a)); return l > 1e-12f ? a * (1.0f / l) : V3{ 0, 0, 0 }; }

	// -------------------------------------------------------------------------------------
	//  Simulation : boite, initialisation, une image
	// -------------------------------------------------------------------------------------
	const V3 boiteMin{ 0.0f, 0.0f, 0.0f };
	const V3 boiteMax{ Cfg::boiteX, Cfg::boiteY, Cfg::boiteZ };

	// Densite (au sens de UpdateDensites) d'une particule au coeur d'un reseau cubique regulier.
	// Sert de densite cible : le bloc initial est alors exactement a l'equilibre, sans explosion.
	float densiteReseau(float h, float s, float m) {
		const float k = 15.0f / (2.0f * PI_F * std::pow(h, 5.0f));   // meme noyau que preparerNoyaux()
		const int R = (int)std::ceil(h / s);
		double somme = 0.0;
		for (int dz = -R; dz <= R; dz++)
			for (int dy = -R; dy <= R; dy++)
				for (int dx = -R; dx <= R; dx++) {
					const float d = s * std::sqrt((float)(dx * dx + dy * dy + dz * dz));
					if (d < h) { const float x = h - d; somme += k * x * x; }   // d = 0 : la particule elle-meme
				}
		return m * (float)somme;
	}

	inline void collisionAxe(float& p, float& vNormale, float& vT1, float& vT2, float bas, float haut) {
		if (p < bas) {
			p = bas;
			if (vNormale < 0.0f) {
				vNormale = -vNormale * Cfg::restitution;
				vT1 *= 1.0f - Cfg::frottementParoi;
				vT2 *= 1.0f - Cfg::frottementParoi;
			}
		}
		else if (p > haut) {
			p = haut;
			if (vNormale > 0.0f) {
				vNormale = -vNormale * Cfg::restitution;
				vT1 *= 1.0f - Cfg::frottementParoi;
				vT2 *= 1.0f - Cfg::frottementParoi;
			}
		}
	}

	// Collisions avec les 6 faces de la boite. Les particules s'arretent a une demi-espacement des parois.
	void appliquerBoite() {
		const int n = (int)positions.size();
		const float marge = 0.5f * Cfg::espacement;
		const V3 lo = boiteMin + V3{ marge, marge, marge };
		const V3 hi = boiteMax - V3{ marge, marge, marge };
		const Vec3 centre{ 0.5f * (lo.x + hi.x), 0.5f * (lo.y + hi.y), 0.5f * (lo.z + hi.z) };

#pragma omp parallel for schedule(static)
		for (int i = 0; i < n; i++) {
			Vec3& p = positions[i];
			Vec3& v = velocites[i];

			// Une seule valeur NaN fausserait toute la grille spatiale : on la neutralise.
			if (!(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
				std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z))) {
				p = centre;
				v = Vec3{ 0.0f, 0.0f, 0.0f };
				continue;
			}
			collisionAxe(p.x, v.x, v.y, v.z, lo.x, hi.x);
			collisionAxe(p.y, v.y, v.x, v.z, lo.y, hi.y);
			collisionAxe(p.z, v.z, v.x, v.y, lo.z, hi.z);
		}
	}

	void initialiserSimulation() {
		const float s = Cfg::espacement;

		rayonDeLissage = Cfg::h;
		preparerNoyaux();                                  // recalcule les constantes des noyaux
		masse = Cfg::rho0 * s * s * s;                     // masse d'une particule = densite * volume
		densiteCible = densiteReseau(Cfg::h, s, masse);    // (ton .cpp met 1.0 par defaut)
		// multiplicateurPression et viscosite : valeurs de MecaniqueDesFluides.cpp conservees

		const int nx = (int)(Cfg::blocX / s), ny = (int)(Cfg::blocY / s), nz = (int)(Cfg::blocZ / s);
		const float z0 = 0.5f * (Cfg::boiteZ - nz * s);    // bloc centre en profondeur

		positions.clear();
		positions.reserve((size_t)nx * ny * nz);

		std::mt19937 rng(12345);                           // graine fixe : simulation reproductible
		std::uniform_real_distribution<float> U(-1.0f, 1.0f);
		const float j = Cfg::jitter * s;
		for (int k = 0; k < nz; k++)
			for (int jy = 0; jy < ny; jy++)
				for (int i = 0; i < nx; i++)
					positions.push_back(Vec3{ (i + 0.5f) * s + j * U(rng),
											  (jy + 0.5f) * s + j * U(rng),
											  z0 + (k + 0.5f) * s + j * U(rng) });
		velocites.assign(positions.size(), Vec3{ 0.0f, 0.0f, 0.0f });
		appliquerBoite();

		std::cout << "Particules : " << positions.size() << "  (" << nx << " x " << ny << " x " << nz << ")\n"
			<< "h = " << Cfg::h << "  espacement = " << s << "  masse = " << masse
			<< "  densiteCible = " << densiteCible << "\n"
			<< "dt de simulation = 1/" << (Cfg::fps * Cfg::sousPas) << " s\n";
	}

	// Avance d'une image de video (1/fps seconde)
	void avancerUneImage() {
		const float dt = 1.0f / (Cfg::fps * Cfg::sousPas);
		for (int s = 0; s < Cfg::sousPas; s++) {
			pasDeSimulation(dt);
			appliquerBoite();
		}
	}

	// -------------------------------------------------------------------------------------
	//  Enregistrement : cache/meta.txt + cache/frame_00000.bin, ...
	//  Format d'une frame : uint32 n, float temps, puis n * (x, y, z) en float32, little endian.
	// -------------------------------------------------------------------------------------
	struct Enregistrement { bool actif = false; int prochaine = 0; } enreg;

	const std::filesystem::path dossierCache = "cache";

	void ecrireMeta() {
		std::ofstream f(dossierCache / "meta.txt");
		f << "fps " << Cfg::fps << "\n"
			<< "rayonDeLissage " << Cfg::h << "\n"
			<< "espacement " << Cfg::espacement << "\n"
			<< "rayonRendu " << Cfg::rayonRendu << "\n"
			<< "boiteMin " << boiteMin.x << " " << boiteMin.y << " " << boiteMin.z << "\n"
			<< "boiteMax " << boiteMax.x << " " << boiteMax.y << " " << boiteMax.z << "\n";
	}

	void ecrireFrame(int index) {
		static std::vector<float> tampon;
		const std::uint32_t n = (std::uint32_t)positions.size();
		tampon.resize((size_t)3 * n);
		for (std::uint32_t i = 0; i < n; i++) {
			tampon[3 * (size_t)i + 0] = positions[i].x;
			tampon[3 * (size_t)i + 1] = positions[i].y;
			tampon[3 * (size_t)i + 2] = positions[i].z;
		}
		char nom[48];
		std::snprintf(nom, sizeof nom, "frame_%05d.bin", index);
		std::ofstream f(dossierCache / nom, std::ios::binary);
		const float temps = index / Cfg::fps;
		f.write(reinterpret_cast<const char*>(&n), sizeof n);
		f.write(reinterpret_cast<const char*>(&temps), sizeof temps);
		f.write(reinterpret_cast<const char*>(tampon.data()), (std::streamsize)(tampon.size() * sizeof(float)));
	}

	void demarrerEnregistrement() {
		std::filesystem::create_directories(dossierCache);
		ecrireMeta();
		ecrireFrame(0);                 // etat courant = image 0
		enreg.actif = true;
		enreg.prochaine = 1;
		std::cout << "Enregistrement dans " << dossierCache.string() << "/ (arret auto apres "
			<< Cfg::framesMaxEnregistrement << " images)\n";
	}

	void arreterEnregistrement() {
		enreg.actif = false;
		std::cout << "Enregistrement arrete : " << enreg.prochaine << " images.\n";
	}

	// -------------------------------------------------------------------------------------
	//  Camera orbitale + projection 3D -> ecran (SFML ne fait que du 2D)
	// -------------------------------------------------------------------------------------
	struct Camera {
		float yaw = 0.7f, pitch = 0.35f, distance = 5.5f, fovDeg = 45.0f;
		V3 cible{ 0.5f * Cfg::boiteX, 0.4f * Cfg::boiteY, 0.5f * Cfg::boiteZ };
	};

	struct Projecteur {
		V3 pos{}, avant{}, droite{}, haut{};
		float f = 1.0f, cx = 0.0f, cy = 0.0f;

		Projecteur(const Camera& c, sf::Vector2u fenetre) {
			pos = c.cible + V3{ std::cos(c.pitch) * std::sin(c.yaw), std::sin(c.pitch), std::cos(c.pitch) * std::cos(c.yaw) } *c.distance;
			avant = normaliser(c.cible - pos);
			droite = normaliser(cross(avant, V3{ 0, 1, 0 }));
			haut = cross(droite, avant);
			cx = 0.5f * (float)fenetre.x;
			cy = 0.5f * (float)fenetre.y;
			f = cy / std::tan(0.5f * c.fovDeg * PI_F / 180.0f);   // focale en pixels
		}

		// false si le point est derriere la camera
		bool projeter(float x, float y, float z, float& sx, float& sy, float& profondeur) const {
			const V3 r{ x - pos.x, y - pos.y, z - pos.z };
			profondeur = dot(r, avant);
			if (profondeur < 0.05f) return false;
			sx = cx + dot(r, droite) / profondeur * f;
			sy = cy - dot(r, haut) / profondeur * f;
			return true;
		}
	};

	// Avertit si MAX_VOISINS est atteint (des paires de voisins seraient alors ignorees)
	void verifierVoisins() {
		static bool dejaAvertit = false;
		if (dejaAvertit) return;
		int maxi = 0;
		for (int v : nbVoisins) maxi = std::max(maxi, v);
		if (maxi >= Cfg::plafondVoisins) {
			dejaAvertit = true;
			std::cerr << "ATTENTION : une particule a atteint MAX_VOISINS (" << Cfg::plafondVoisins
				<< "). Augmente MAX_VOISINS dans MecaniqueDesFluides.cpp ou diminue rapportHSurEspacement.\n";
		}
	}

} // namespace

// =====================================================================================
int main() {
	initialiserSimulation();
	std::cout << "Espace : pause | N : une image | R : reset\n" << "D : carte de profondeur | T : carte d'epaisseur\n" << "E : enregistrer | souris : camera\n";

	// Resolution du rendu (FBO). Independante de la fenetre : plus tard, le rendu hors ligne
	// pourra etre plus grand que ce qu'on affiche.
	constexpr unsigned LARGEUR = 1280u, HAUTEUR = 720u;

	// ---- Fenetre + contexte OpenGL 3.3 (sf::Window : pas de dessin 2D SFML, OpenGL pur)
	sf::ContextSettings reglages;
	reglages.depthBits = 24;
	reglages.majorVersion = 3;
	reglages.minorVersion = 3;
	sf::Window fenetre(sf::VideoMode({ LARGEUR, HAUTEUR }), "flex",
		sf::Style::Titlebar | sf::Style::Close, sf::State::Windowed, reglages);
	fenetre.setFramerateLimit(60);

	const sf::ContextSettings obtenu = fenetre.getSettings();
	std::cout << "OpenGL demande 3.3, obtenu " << obtenu.majorVersion << "." << obtenu.minorVersion
		<< " | " << reinterpret_cast<const char*>(glGetString(GL_VERSION)) << "\n";

	if (!glx::charger()) { std::cerr << "Chargement des fonctions OpenGL impossible.\n"; return 1; }

	glx::CibleDeRendu cible;
	glx::CibleDeRendu carteProfondeur;
	glx::CibleDeRendu carteEpaisseur;
	glx::RenduParticules rendu;

	// Rendu normal
	if (!cible.creer((int)LARGEUR, (int)HAUTEUR)) return 1;
	// Carte de profondeur : RGBA32F
	if (!carteProfondeur.creer((int)LARGEUR, (int)HAUTEUR, glx::kRgba32f, GL_RGBA, GL_FLOAT)) return 1;
	// Carte d'epaisseur : RGBA32F
	if (!carteEpaisseur.creer((int)LARGEUR, (int)HAUTEUR, glx::kRgba32f, GL_RGBA, GL_FLOAT))return 1;
	if (!rendu.initialiser(boiteMin.x, boiteMin.y, boiteMin.z, boiteMax.x, boiteMax.y, boiteMax.z)) return 1;

	Camera camera;
	bool continuer = true;
	bool pause = false, pasUnique = false;
	bool afficherCarteProfondeur = false;
	bool afficherCarteEpaisseur = false;
	bool positionsAEnvoyer = true;                    // vrai des que les positions ont change
	int numeroImage = 0;
	float msSimulation = 0.0f;
	sf::Clock horloge;
	float fpsAffiche = 0.0f;
	sf::Vector2i sourisPrecedente = sf::Mouse::getPosition(fenetre);

	while (continuer) {
		// ---- Evenements
		while (const std::optional evenement = fenetre.pollEvent()) {
			if (evenement->is<sf::Event::Closed>()) {
				continuer = false;
			}
			else if (const auto* touche = evenement->getIf<sf::Event::KeyPressed>()) {
				switch (touche->code) {
				case sf::Keyboard::Key::Escape: continuer = false; break;
				case sf::Keyboard::Key::Space:  pause = !pause; break;
				case sf::Keyboard::Key::N:      if (pause) pasUnique = true; break;
				case sf::Keyboard::Key::R:
					if (enreg.actif) arreterEnregistrement();
					initialiserSimulation();
					numeroImage = 0;
					positionsAEnvoyer = true;
					break;
				case sf::Keyboard::Key::E:
					if (enreg.actif) arreterEnregistrement(); else demarrerEnregistrement();
					break;
				case sf::Keyboard::Key::D: afficherCarteProfondeur = !afficherCarteProfondeur;
					afficherCarteEpaisseur = false;
					break;

				case sf::Keyboard::Key::T: afficherCarteEpaisseur = !afficherCarteEpaisseur;
					afficherCarteProfondeur = false;
					break;
				default: break;
				}
			}
			else if (const auto* molette = evenement->getIf<sf::Event::MouseWheelScrolled>()) {
				camera.distance = std::clamp(camera.distance * std::pow(0.9f, molette->delta), 0.5f, 50.0f);
			}
		}

		// ---- Camera (glisser avec le bouton gauche)
		const sf::Vector2i souris = sf::Mouse::getPosition(fenetre);
		if (fenetre.hasFocus() && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
			camera.yaw -= (souris.x - sourisPrecedente.x) * 0.006f;
			camera.pitch = std::clamp(camera.pitch + (souris.y - sourisPrecedente.y) * 0.006f, -1.5f, 1.5f);
		}
		sourisPrecedente = souris;

		// ---- Simulation : exactement 1 image de video par tour de boucle
		if (!pause || pasUnique) {
			pasUnique = false;
			const auto t0 = std::chrono::steady_clock::now();
			avancerUneImage();
			msSimulation = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
			numeroImage++;
			positionsAEnvoyer = true;
			if (numeroImage % 60 == 0) verifierVoisins();

			if (enreg.actif) {
				ecrireFrame(enreg.prochaine++);
				if (enreg.prochaine >= Cfg::framesMaxEnregistrement) arreterEnregistrement();
			}
		}

		// ---- CPU -> GPU : positions des particules dans le VBO
		if (positionsAEnvoyer) {
			rendu.envoyerPositions(reinterpret_cast<const float*>(positions.data()), positions.size());
			positionsAEnvoyer = false;
		}

		// ---- Camera -> parametres des shaders (meme base avant/droite/haut que le path tracer)
		const Projecteur proj(camera, sf::Vector2u{ LARGEUR, HAUTEUR });
		glx::ParametresCamera cam{};
		cam.pos[0] = proj.pos.x;       cam.pos[1] = proj.pos.y;       cam.pos[2] = proj.pos.z;
		cam.avant[0] = proj.avant.x;   cam.avant[1] = proj.avant.y;   cam.avant[2] = proj.avant.z;
		cam.droite[0] = proj.droite.x; cam.droite[1] = proj.droite.y; cam.droite[2] = proj.droite.z;
		cam.haut[0] = proj.haut.x;     cam.haut[1] = proj.haut.y;     cam.haut[2] = proj.haut.z;
		cam.fovDeg = camera.fovDeg;
		cam.aspect = (float)LARGEUR / (float)HAUTEUR;
		// Adapter le plan lointain au zoom de la camera. La scene tient dans cette distance avec ta boite actuelle.
		cam.loin = camera.distance + 5.0f;

		// Rendu : FBO hors ecran, puis copie vers la fenetre
        // PASSE 1 : CARTE DE PROFONDEUR
		carteProfondeur.lierEtEffacer(1.0f, 1.0f, 1.0f);
		rendu.dessinerCarteProfondeur(cam, Cfg::rayonRendu);

		// PASSE 2 : CARTE D'EPAISSEUR
		carteEpaisseur.lierEtEffacer(0.0f, 0.0f, 0.0f);
		rendu.dessinerCarteEpaisseur(cam, Cfg::rayonRendu);

		// PASSE 3 : RENDU NORMAL
		cible.lierEtEffacer(0.07f, 0.08f, 0.10f );
		rendu.dessiner(cam, Cfg::rayonRendu);

		// AFFICHAGE
		if (afficherCarteProfondeur) { carteProfondeur.afficherSurFenetre((int)fenetre.getSize().x, (int)fenetre.getSize().y); }
		else if (afficherCarteEpaisseur) { carteEpaisseur.afficherSurFenetre((int)fenetre.getSize().x, (int)fenetre.getSize().y); }
		else { cible.afficherSurFenetre( (int)fenetre.getSize().x, (int)fenetre.getSize().y); }

		fenetre.display();

		const float dtReel = horloge.restart().asSeconds();
		fpsAffiche = 0.9f * fpsAffiche + 0.1f * (dtReel > 0.0f ? 1.0f / dtReel : 0.0f);
		if (numeroImage % 10 == 0 || pause) {
			char titre[160];
			std::snprintf(titre, sizeof titre, "flex | %zu particules | image %d | sim %.1f ms | %.0f fps%s%s",
				positions.size(), numeroImage, msSimulation, fpsAffiche,
				pause ? " | PAUSE" : "", enreg.actif ? " | REC" : "");
			fenetre.setTitle(titre);
		}
	}

	// Liberer les objets OpenGL AVANT de fermer la fenetre (close() detruit le contexte)
	carteProfondeur.detruire();
	carteEpaisseur.detruire();
	rendu.detruire();
	cible.detruire();
	fenetre.close();
	return 0;
}