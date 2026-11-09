// Visualisation 3D (raylib) de particules SPH dans un pave droit redimensionnable.
// Rendu par INSTANCING : toutes les particules sont dessinees en un seul appel (DrawMeshInstanced).
// La couleur (degrade selon la vitesse) est calculee dans le fragment shader.
// Simulation SPH 3D (RMecaniqueDesFluides.cpp) : x largeur, y hauteur, z profondeur.
//
// Commandes :
//   clic gauche + glisser : tourner la camera     molette : zoom
//   fleches gauche/droite : largeur (x) -/+
//   fleches bas/haut      : profondeur (z) -/+
//   Page Bas / Page Haut  : hauteur (y) -/+
//   ESPACE : pause        R : reinitialiser
//
// Necessite OpenGL 3.3 (raylib desktop par defaut) : shaders en GLSL 330.

#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include "raylib.h"
#include "RMecaniqueDesFluides.h"

// --- Dimensions du pave droit (en metres), modifiables pendant l'execution ---
static float largeur = 10.0f;   // x
static float hauteur = 6.0f;   // y (le sol est y = 0)
static float profondeur = 10.0f;   // z

static const float LARGEUR_MIN = 1.0f, LARGEUR_MAX = 30.0f;
static const float HAUTEUR_MIN = 1.5f, HAUTEUR_MAX = 12.0f;
static const float PROFONDEUR_MIN = 1.0f, PROFONDEUR_MAX = 30.0f;
static const float VITESSE_REDIMENSIONNEMENT = 4.0f; // m/s

// --- Particules : 200 x 200 = 40000 ---
static const int   COLONNES = 100;
static const int   LIGNES = 200;
static const int   NB_PARTICULES = COLONNES * LIGNES;
static const float ESPACEMENT = 0.17f;  // ~ densiteCible = 35 avec rayonDeLissage = 0.35
static const float RAYON_AFFICHAGE = 0.1f;
static const int   RESOLUTION_SPHERE = 8;   // avec l'instancing, 8x8 ne coute presque rien de plus

static const float DT = 1.0f / 120.0f;  // le pas de simulation prevoit 1/120 s (prediction)
static const int   SOUS_PAS_PAR_IMAGE = 2;
static const float VITESSE_MAX_COULEUR = 6.0f; // vitesse (m/s) affichee en rouge
static const float INV_VITESSE_MAX_COULEUR = 1.0f / VITESSE_MAX_COULEUR;
static const float MARGE_PAROI = RAYON_AFFICHAGE; // doit rester = marge de contenirDansLeBoite()

static std::vector<float> decalageZ;    // position z normalisee dans [-0.5, 0.5], mise a l'echelle de la profondeur

// ---------------------------------------------------------------------------
// Shaders (embarques dans le code : pas de fichiers a deplacer)
//
// Astuce : DrawMeshInstanced n'envoie qu'une matrice par instance. On s'en sert
// comme d'un simple conteneur de donnees :
//   instanceTransform[3].xyz  = position de la particule (colonne de translation)
//   instanceTransform[0][0]   = vitesse normalisee dans [0, 1] (pour la couleur)
// Le reste de la matrice est ignore par le shader.
// ---------------------------------------------------------------------------
static const char* VS_INSTANCING = R"GLSL(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in mat4 instanceTransform;   // une matrice par particule (voir l'astuce ci-dessus)

uniform mat4 mvp;

out vec3 fragNormal;
out float fragVitesse;

void main()
{
    vec3 centre = instanceTransform[3].xyz;
    fragVitesse = instanceTransform[0][0];
    fragNormal = vertexNormal;                       // pas de rotation : normale monde = normale modele
    gl_Position = mvp * vec4(vertexPosition + centre, 1.0);
}
)GLSL";

static const char* FS_INSTANCING = R"GLSL(
#version 330
in vec3 fragNormal;
in float fragVitesse;

out vec4 finalColor;

// Degrade bleu -> vert -> orange -> rouge pour t dans [0, 1] (memes couleurs que sur CPU)
vec3 degrade(float t)
{
    const vec3 bleu   = vec3( 40.0, 110.0, 255.0) / 255.0;
    const vec3 vert   = vec3( 60.0, 210.0,  90.0) / 255.0;
    const vec3 orange = vec3(255.0, 160.0,  30.0) / 255.0;
    const vec3 rouge  = vec3(240.0,  40.0,  40.0) / 255.0;

    t = clamp(t, 0.0, 1.0) * 3.0;
    if (t < 1.0) return mix(bleu, vert, t);
    if (t < 2.0) return mix(vert, orange, t - 1.0);
    return mix(orange, rouge, t - 2.0);
}

void main()
{
    // Eclairage simple : une lumiere directionnelle fixe + ambiante
    vec3 n = normalize(fragNormal);
    float diffus = max(dot(n, normalize(vec3(0.4, 1.0, 0.6))), 0.0);
    float lum = 0.45 + 0.55 * diffus;
    finalColor = vec4(degrade(fragVitesse), 1.0);
}
)GLSL";

static void initialiser()
{
	positions.assign(NB_PARTICULES, Vec3(0.0f, 0.0f, 0.0f));
	velocites.assign(NB_PARTICULES, Vec3(0.0f, 0.0f, 0.0f));
	decalageZ.assign(NB_PARTICULES, 0.0f);

	// petit bruit pour eviter une grille parfaitement symetrique
	auto bruit = []() { return ((rand() % 1000) / 1000.0f - 0.5f) * 0.02f; };

	int k = 0;
	for (int ligne = 0; ligne < LIGNES; ligne++) {
		for (int col = 0; col < COLONNES; col++, k++) {
			for (int prof = 0; prof < 1; prof++) {
				positions[k].x = 0.4f + col * ESPACEMENT + bruit();
				positions[k].y = 0.4f + ligne * ESPACEMENT + bruit();
				positions[k].z = 0.4f + prof * ESPACEMENT + bruit();
			}
		}
	}
	preparerNoyaux();
}

// Garde les particules dans le pave (utile surtout quand on le redimensionne)
static void contenirDansLeBoite()
{
	const float m = RAYON_AFFICHAGE;
	for (size_t i = 0; i < positions.size(); i++) {
		if (positions[i].x < m) { positions[i].x = m;           velocites[i].x *= -0.5f; }
		if (positions[i].x > largeur - m) { positions[i].x = largeur - m; velocites[i].x *= -0.5f; }
		if (positions[i].y < m) { positions[i].y = m;           velocites[i].y *= -0.5f; }
		if (positions[i].y > hauteur - m) { positions[i].y = hauteur - m; velocites[i].y *= -0.5f; }
		if (positions[i].z < m) { positions[i].z = m;           velocites[i].z *= -0.5f; }
		if (positions[i].z > profondeur - m) { positions[i].z = profondeur - m; velocites[i].z *= -0.5f; }
	}
}

// Remplit le tableau d'instances (position + vitesse normalisee) pour DrawMeshInstanced
static void remplirInstances(std::vector<Matrix>& instances)
{
	const size_t nb = positions.size();
	if (instances.size() != nb) instances.assign(nb, Matrix{});   // tout a 0 : seuls 4 champs servent

	for (size_t i = 0; i < nb; i++) {
		const float vx = velocites[i].x, vy = velocites[i].y, vz = velocites[i].z;
		float t = std::sqrt(vx * vx + vy * vy + vz * vz) * INV_VITESSE_MAX_COULEUR;
		if (!(t == t)) t = 0.0f;                 // NaN -> bleu
		Matrix& m = instances[i];
		m.m0 = std::min(t, 1.0f);                // vitesse normalisee   -> instanceTransform[0][0]
		m.m12 = positions[i].x;                  // translation x        -> instanceTransform[3].x
		m.m13 = positions[i].y;                  // translation y
		m.m14 = positions[i].z;                  // translation z
	}
}

int main()
{
	InitWindow(1200, 800, "SPH - fluide 3D (raylib, instancing)");
	SetTargetFPS(60);   // retire cette ligne pour mesurer les gains reels (sinon plafonne a 60 FPS)

	initialiser();

	// --- Mesh + materiau instancie (apres InitWindow : il faut le contexte OpenGL) ---
	Mesh sphere = GenMeshSphere(RAYON_AFFICHAGE, RESOLUTION_SPHERE, RESOLUTION_SPHERE);
	Shader shader = LoadShaderFromMemory(VS_INSTANCING, FS_INSTANCING);
	shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
	shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(shader, "instanceTransform");
	Material materiau = LoadMaterialDefault();
	materiau.shader = shader;

	std::vector<Matrix> instances;
	instances.reserve(NB_PARTICULES);

	float angle = 0.6f, elevation = 0.35f, distance = 11.0f;
	bool pause = false;

	Camera3D camera = { 0 };
	camera.up = { 0.0f, 1.0f, 0.0f };
	camera.fovy = 45.0f;
	camera.projection = CAMERA_PERSPECTIVE;

	while (!WindowShouldClose())
	{
		const float dtImage = GetFrameTime();

		// --- Entrees ---
		if (IsKeyPressed(KEY_SPACE)) pause = !pause;
		if (IsKeyPressed(KEY_R)) initialiser();

		const float dd = VITESSE_REDIMENSIONNEMENT * dtImage;
		if (IsKeyDown(KEY_RIGHT))     largeur += dd;
		if (IsKeyDown(KEY_LEFT))      largeur -= dd;
		if (IsKeyDown(KEY_UP))        profondeur += dd;
		if (IsKeyDown(KEY_DOWN))      profondeur -= dd;
		if (IsKeyDown(KEY_PAGE_UP))   hauteur += dd;
		if (IsKeyDown(KEY_PAGE_DOWN)) hauteur -= dd;
		largeur = std::clamp(largeur, LARGEUR_MIN, LARGEUR_MAX);
		hauteur = std::clamp(hauteur, HAUTEUR_MIN, HAUTEUR_MAX);
		profondeur = std::clamp(profondeur, PROFONDEUR_MIN, PROFONDEUR_MAX);

		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
			Vector2 d = GetMouseDelta();
			angle -= d.x * 0.005f;
			elevation = std::clamp(elevation + d.y * 0.005f, -1.4f, 1.4f);
		}
		distance = std::clamp(distance - GetMouseWheelMove() * 0.8f, 4.0f, 40.0f);

		// La camera suit le centre du pave, qui change avec ses dimensions
		const Vector3 centre = { largeur / 2.0f, hauteur / 2.0f, profondeur / 2.0f };
		camera.target = centre;
		camera.position = {
			centre.x + distance * std::cos(elevation) * std::sin(angle),
			centre.y + distance * std::sin(elevation),
			centre.z + distance * std::cos(elevation) * std::cos(angle) };

		// --- Simulation ---
		if (!pause) {
			for (int s = 0; s < SOUS_PAS_PAR_IMAGE; s++) {
				pasDeSimulation(DT);
				contenirDansLeBoite();
			}
		}

		// --- Rendu ---
		remplirInstances(instances);

		BeginDrawing();
		ClearBackground(Color{ 20, 22, 30, 255 });

		BeginMode3D(camera);
		// Toutes les particules en UN seul appel de dessin
		DrawMeshInstanced(sphere, materiau, instances.data(), (int)instances.size());
		// pave droit (fils de fer)
		DrawCubeWiresV(centre, Vector3{ largeur, hauteur, profondeur }, RAYWHITE);
		// sol
		//DrawPlane(Vector3{ largeur / 2.0f, 0.0f, profondeur / 2.0f }, Vector2{ largeur * 2.0f, profondeur * 6.0f }, Color{ 40, 44, 56, 255 });
		DrawGrid(20, 1.0f);
		EndMode3D();

		DrawFPS(10, 10);
		DrawText(TextFormat("Particules : %d", (int)positions.size()), 10, 35, 20, RAYWHITE);
		DrawText(TextFormat("Pave : %.2f x %.2f x %.2f m", largeur, hauteur, profondeur), 10, 60, 20, RAYWHITE);
		DrawText("fleches gauche/droite : largeur | haut/bas : profondeur | PgUp/PgDn : hauteur", 10, 85, 20, LIGHTGRAY);
		DrawText(pause ? "PAUSE (espace)" : "espace : pause | R : reset | souris : camera", 10, 110, 20, LIGHTGRAY);
		EndDrawing();
	}

	UnloadMesh(sphere);
	UnloadMaterial(materiau);   // libere aussi le shader
	CloseWindow();
	return 0;
}