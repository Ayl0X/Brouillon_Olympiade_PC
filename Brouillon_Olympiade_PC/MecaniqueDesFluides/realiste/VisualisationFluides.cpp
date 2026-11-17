// Visualisation 3D (raylib) de particules SPH dans un recipient cylindrique ouvert.
// Experience : chute d'une balle (g_rayonBalle = 35 mm, g_massBalle = 12 g) dans l'eau.
//
// PROBLEME : on ne peut pas reduire les particules d'eau sans casser la physique SPH, donc la vraie balle
// (35 mm) serait plus petite qu'une particule. SOLUTION : similitude de Froude.
// On simule la meme experience agrandie d'un facteur L (echelle) :
//     longueurs x L      masse x L^3 (meme masse volumique que la vraie balle)
//     vitesses  x sqrt(L)   temps x sqrt(L)   accelerations x 1   gravite INCHANGEE
// Dans ces conditions la balle agrandie dans de l'eau normale se comporte comme la vraie balle dans la vraie eau
// (flottabilite, enfoncement, rebond : tout ce qui est gouverne par la gravite et l'inertie).
// L est calcule automatiquement pour que le rayon de la balle vaille NB_ESPACEMENTS_RAYON_BALLE espacements
// de particules. Le CSV et l'affichage sont en unites REELLES : y = 0 a l'eau calme (NIVEAU_EAU_CALME),
// balle lachee de HAUTEUR_LACHER_REELLE (0.78 m) ; la chute libre donne bien a = 9.81 m/s2.
//
// La balle est un amas RIGIDE DE PARTICULES SPH : ses particules sont dans les memes tableaux que l'eau,
// donc elles passent par le meme calcul (densite, pression, viscosite, gravite). La force du fluide sur la
// balle est la somme des forces subies par ses particules ; elle deplace la balle comme un seul corps.
//
// Commandes :
//   clic gauche + glisser : tourner la camera     molette : zoom
//   B      : fait apparaitre la balle (au-dessus du recipient) / la fait disparaitre
//   ESPACE : pause        R : reinitialiser
//
// Necessite OpenGL 3.3 (raylib desktop par defaut) : shaders en GLSL 330.

#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include "raylib.h"
#include "..\..\Constantes.h"          // g_gravite, g_rayonBalle, g_massBalle (comme MecaniqueDesFluides.cpp)
#include "MecaniqueDesFluides.h"

extern float masse;                    // masse d'une particule SPH (definie dans MecaniqueDesFluides.cpp)
extern float rayonDeLissage;           // rayon du noyau SPH (h)
extern float densiteCible;             // masse volumique de repos de l'eau simulee (pression nulle en dessous)
extern float multiplicateurPression;   // raideur de l'equation d'etat : P = k * (rho - densiteCible)

// --- Recipient cylindrique (metres) : ouvert en haut ---
static const float DIAMETRE = 8.0f;             // diametre du recipient (m) : plus etroit => plus d'eau en hauteur pour le meme nombre de particules
static const float RAYON_RECIPIENT = DIAMETRE / 2.0f;
static const float hauteur = 20.0f;
static const float CENTRE_X = RAYON_RECIPIENT;
static const float CENTRE_Z = RAYON_RECIPIENT;

// --- Particules d'eau (inchangees : memes reglages SPH) ---
static const int   NB_PARTICULES_MAX = 100000;  // plafond du nombre de particules d'eau (l'espacement grossit si le volume ne tient pas)
static float       ESPACEMENT = 0.1f;           // pas de la grille initiale : calcule dans initialiser()
static const float NIVEAU_EAU = 3.0f;           // hauteur de remplissage initiale (m)
static const float ESPACEMENT_CIBLE = 0.12f;    // espacement voulu entre particules (m) : garde le rapport espacement / rayon de lissage (0.35) du reglage SPH
static float       RAYON_AFFICHAGE = 0.1f;      // recalcule dans initialiser()
static const int   RESOLUTION_SPHERE = 8;

// Raideur : avec la formule de pression du moteur, la force effective est la moitie de k, donc la vitesse du son
// simulee vaut c = sqrt(k/2). Condition de stabilite explicite : c * DT / h < ~0.3 (h = rayon de lissage = 0.35).
// k = 400 -> c = 14 m/s ; DT = 1/180 -> c*DT/h = 0.22. Compression hydrostatique : environ 5 % par metre de profondeur.
static const float RAIDEUR_PRESSION = 400.0f;
static const float DT = 1.0f / 180.0f;
// Ralenti : la simulation avance de VITESSE_RALENTI secondes simulees par seconde reelle (0.25 = x0,25),
// quel que soit le nombre d'images par seconde. Le pas de temps DT reste TOUJOURS le meme : c'est le nombre de pas
// par image qui varie (accumulateur de temps). MAX_PAS_PAR_IMAGE evite l'emballement si l'ordinateur est trop lent
// (la simulation ralentit alors encore, mais DT ne change pas).
static const float VITESSE_RALENTI = 0.25f;
static const int   MAX_PAS_PAR_IMAGE = 6;
static const float VITESSE_MAX_COULEUR = 6.0f;
static const float INV_VITESSE_MAX_COULEUR = 1.0f / VITESSE_MAX_COULEUR;

// --- Balle (similitude de Froude) ---
static const float NB_ESPACEMENTS_RAYON_BALLE = 6.0f;   // rayon de la balle simulee = 6 espacements de particules
static const float NIVEAU_EAU_CALME = 2.5f;              // hauteur de l'eau au repos dans la simulation (m, echelle simulation) = ORIGINE (y = 0) des coordonnees reelles
static const float HAUTEUR_LACHER_REELLE = 0.78f;        // hauteur de chute en METRES REELS : le centre de la balle part a y = +0.78 m au-dessus de l'eau calme
static const float VITESSE_MAX_BALLE = 40.0f;            // securite numerique (m/s, echelle simulation)
static const float DENSITE_EAU_REELLE = 1000.0f;         // kg/m3 : sert a calculer le rapport de masse volumique balle/eau reel
static const float PI_F = 3.14159265f;

// --- Enregistrement CSV ---
static const bool  CSV_UNITES_REELLES = true;   // true : temps, vitesse, position reconvertis a l'echelle de la vraie balle
// false : valeurs brutes de la simulation

static int   nFluide = 0;                       // particules d'eau = indices [0, nFluide) ; balle = indices a la suite
static bool  balleActive = false;
static bool  rayonPlafonne = false;
static float rayonBalle = 0.0f, masseBalle = 1.0f;
static float echelle = 1.0f;                    // L : rayonBalle / g_rayonBalle
static float bx = 0, by = 0, bz = 0;            // centre de la balle
static float bvx = 0, bvy = 0, bvz = 0;         // vitesse de la balle
static float bax = 0, bay = 0, baz = 0;         // acceleration de la balle
static float fFluideX = 0, fFluideY = 0, fFluideZ = 0;   // force du fluide sur la balle (pression + viscosite)
static float tempsBalle = 0.0f;                 // temps de simulation ecoule depuis l'apparition de la balle (s)
static std::vector<Vec3> decalages;             // position des particules de la balle par rapport a son centre
static std::ofstream fichierCSV;

static void fermerCSV()
{
	if (fichierCSV.is_open()) fichierCSV.close();
}

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------
static const char* VS_INSTANCING = R"GLSL(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in mat4 instanceTransform;   // une matrice par particule

uniform mat4 mvp;

out vec3 fragNormal;
out float fragVitesse;

void main()
{
    vec3 centre = instanceTransform[3].xyz;
    fragVitesse = instanceTransform[0][0];
    fragNormal = vertexNormal;
    gl_Position = mvp * vec4(vertexPosition + centre, 1.0);
}
)GLSL";

static const char* FS_INSTANCING = R"GLSL(
#version 330
in vec3 fragNormal;
in float fragVitesse;

out vec4 finalColor;

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
    finalColor = vec4(degrade(fragVitesse), 1.0);
}
)GLSL";

// Masse volumique SPH d'un reseau cubique d'espacement e : meme formule que UpdateDensites()
// (noyau de Spiky d'ordre 2, somme sur les voisins + la particule elle-meme).
static float densiteDeRepos(float e)
{
	const float h = rayonDeLissage;
	const float k = 15.0f / (2.0f * PI_F * h * h * h * h * h);
	const int   n = (int)(h / e) + 1;
	float somme = 0.0f;
	for (int i = -n; i <= n; i++)
		for (int j = -n; j <= n; j++)
			for (int l = -n; l <= n; l++) {
				const float d = e * std::sqrt((float)(i * i + j * j + l * l));
				if (d < h) somme += k * (h - d) * (h - d);
			}
	return masse * somme;
}

// Remplit le cylindre couche par couche jusqu'a NIVEAU_EAU (ou NB_PARTICULES_MAX)
static void initialiser()
{
	positions.clear();
	velocites.clear();
	balleActive = false;
	decalages.clear();
	fermerCSV();

	auto bruit = []() { return ((rand() % 1000) / 1000.0f - 0.5f) * 0.02f; };

	const float m = 0.1f;   // marge de placement (fixe)
	const float rMax = RAYON_RECIPIENT - m - 0.05f;

	// Espacement : on resserre la grille jusqu'a pouvoir loger NB_PARTICULES_MAX particules sous NIVEAU_EAU
	auto compter = [&](float e) {
		const int n = (int)(rMax / e);
		int parCouche = 0;
		for (int i = -n; i <= n; i++)
			for (int j = -n; j <= n; j++)
				if ((i * e) * (i * e) + (j * e) * (j * e) <= rMax * rMax) parCouche++;
		const int couches = (int)((NIVEAU_EAU - (m + 0.05f)) / e) + 1;
		return parCouche * couches;
		};
	// Espacement fixe (ESPACEMENT_CIBLE) : le nombre de particules decoule de la geometrie (volume d'eau).
	// Il n'est grossi que si le plafond NB_PARTICULES_MAX serait depasse.
	ESPACEMENT = ESPACEMENT_CIBLE;
	while (compter(ESPACEMENT) > NB_PARTICULES_MAX) ESPACEMENT *= 1.01f;
	const int n = (int)(rMax / ESPACEMENT);

	// CORRECTION IMPORTANTE : densiteCible valait 1.0 alors que le reseau d'eau a une masse volumique SPH
	// d'environ 280. La pression (max(0, k*(rho - densiteCible))) etait donc enorme partout : l'eau se comportait
	// comme un gaz sans surface libre ni masse volumique de repos, et "flotter" n'avait aucun sens.
	// On fixe la densite cible a celle du reseau : pression nulle au repos, l'eau devient un liquide.
	densiteCible = densiteDeRepos(ESPACEMENT);
	multiplicateurPression = RAIDEUR_PRESSION;

	// Particules affichees petites : rayon = 0.1 pour un espacement de 0.17
	RAYON_AFFICHAGE = 0.1f * ESPACEMENT / 0.17f;

	int k = 0;
	for (float y = m + 0.05f; y <= NIVEAU_EAU && k < NB_PARTICULES_MAX; y += ESPACEMENT) {
		for (int i = -n; i <= n; i++) {
			for (int j = -n; j <= n; j++) {
				const float dx = i * ESPACEMENT, dz = j * ESPACEMENT;
				if (dx * dx + dz * dz > rMax * rMax || k >= NB_PARTICULES_MAX) continue;
				positions.push_back(Vec3(CENTRE_X + dx + bruit(), y + bruit(), CENTRE_Z + dz + bruit()));
				velocites.push_back(Vec3(0.0f, 0.0f, 0.0f));
				k++;
			}
		}
	}
	nFluide = k;
	preparerNoyaux();
}

// Parois (sol, "plafond" anti-eclaboussures, paroi laterale) : uniquement pour l'eau
static void contenirDansLeCylindre()
{
	const float m = RAYON_AFFICHAGE;
	const float rLim = RAYON_RECIPIENT - m;
	for (int i = 0; i < nFluide; i++) {
		if (positions[i].y < m) { positions[i].y = m;            velocites[i].y *= -0.5f; }
		if (positions[i].y > hauteur - m) { positions[i].y = hauteur - m; velocites[i].y *= -0.5f; }

		const float dx = positions[i].x - CENTRE_X;
		const float dz = positions[i].z - CENTRE_Z;
		const float d = std::sqrt(dx * dx + dz * dz);
		if (d > rLim) {
			const float nx = dx / d, nz = dz / d;
			positions[i].x = CENTRE_X + nx * rLim;
			positions[i].z = CENTRE_Z + nz * rLim;
			const float vn = velocites[i].x * nx + velocites[i].z * nz;
			if (vn > 0.0f) {
				velocites[i].x -= 1.5f * vn * nx;
				velocites[i].z -= 1.5f * vn * nz;
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Balle = amas rigide de particules SPH, a l'echelle L
// ---------------------------------------------------------------------------
static void basculerBalle()
{
	if (balleActive) {   // disparition : on retire ses particules
		positions.erase(positions.begin() + nFluide, positions.end());
		velocites.erase(velocites.begin() + nFluide, velocites.end());
		balleActive = false;
		fermerCSV();
		return;
	}

	// Rayon simule : NB_ESPACEMENTS_RAYON_BALLE espacements (borne pour rester dans le recipient)
	const float rVoulu = NB_ESPACEMENTS_RAYON_BALLE * ESPACEMENT;
	const float rNominal = std::clamp(rVoulu, 3.0f * ESPACEMENT, 0.5f * RAYON_RECIPIENT);
	rayonPlafonne = (rNominal != rVoulu);

	// Reseau cubique de particules : chaque particule occupe un cube ESPACEMENT^3, donc le nuage
	// recouvre la sphere de rayon rNominal (rayon de coupure = rNominal - ESPACEMENT/2).
	decalages.clear();
	const float rc = rNominal - 0.5f * ESPACEMENT;
	const int   n = (int)(rc / ESPACEMENT);
	for (int i = -n; i <= n; i++)
		for (int j = -n; j <= n; j++)
			for (int l = -n; l <= n; l++) {
				const float x = i * ESPACEMENT, y = j * ESPACEMENT, z = l * ESPACEMENT;
				if (x * x + y * y + z * z <= rc * rc) decalages.push_back(Vec3(x, y, z));
			}
	const float nbPart = (float)decalages.size();

	// Rayon effectif = rayon de la sphere qui a le volume du nuage (c'est ce volume que le fluide "voit")
	const float volumeNuage = nbPart * ESPACEMENT * ESPACEMENT * ESPACEMENT;
	rayonBalle = std::cbrt(3.0f * volumeNuage / (4.0f * PI_F));

	// Facteur d'agrandissement par rapport a la vraie balle (35 mm)
	echelle = rayonBalle / g_rayonBalle;

	// MASSE : on conserve le rapport de masse volumique balle / eau de l'experience reelle.
	// L'eau deplacee par le nuage a pour masse nbPart * masse (quelle que soit l'unite de masse volumique
	// de la simulation) ; la poussee d'Archimede vaut donc exactement nbPart * masse * g.
	const float volumeReel = 4.0f / 3.0f * PI_F * g_rayonBalle * g_rayonBalle * g_rayonBalle;
	const float rapportDensite = (g_massBalle / volumeReel) / DENSITE_EAU_REELLE;   // ~0.067 : la balle flotte
	masseBalle = rapportDensite * nbPart * masse;

	// apparait au-dessus du bord du recipient, au centre, sans vitesse
	bx = CENTRE_X; bz = CENTRE_Z;
	by = NIVEAU_EAU_CALME + HAUTEUR_LACHER_REELLE * echelle;   // 0.78 m reels x L au-dessus de l'eau calme
	bvx = bvy = bvz = 0.0f;
	bax = bay = baz = 0.0f;
	fFluideX = fFluideY = fFluideZ = 0.0f;
	tempsBalle = 0.0f;

	for (size_t k = 0; k < decalages.size(); k++) {
		positions.push_back(Vec3(bx + decalages[k].x, by + decalages[k].y, bz + decalages[k].z));
		velocites.push_back(Vec3(0.0f, 0.0f, 0.0f));
	}

	// Fichier CSV
	fermerCSV();
	fichierCSV.open("balle.csv", std::ios::trunc);
	fichierCSV << std::setprecision(8);
	if (CSV_UNITES_REELLES)
		fichierCSV << "temps_s,ax_m_s2,ay_m_s2,az_m_s2,vx_m_s,vy_m_s,vz_m_s,x_m,y_m,z_m\n";
	else
		fichierCSV << "temps_s_sim,ax,ay,az,vx,vy,vz,x,y,z\n";

	balleActive = true;
}

// A appeler JUSTE APRES pasDeSimulation(dt).
// Pendant le pas, les particules de la balle ont ete traitees comme l'eau : gravite + pression + viscosite.
// Leur variation de vitesse donne la force du fluide ; on l'applique au corps entier, puis on le "rigidifie".
static void majBalle(float dt)
{
	const int nb = (int)decalages.size();
	if (nb == 0 || (int)positions.size() != nFluide + nb) return;

	// 1) force du fluide = somme masse * (dv/dt + g) sur les particules de la balle (la gravite est retiree)
	float fx = 0.0f, fy = 0.0f, fz = 0.0f;
	for (int k = 0; k < nb; k++) {
		const Vec3& v = velocites[nFluide + k];
		fx += masse * (v.x - bvx) / dt;
		fy += masse * ((v.y - bvy) / dt + g_gravite);
		fz += masse * (v.z - bvz) / dt;
	}
	fFluideX = fx; fFluideY = fy; fFluideZ = fz;

	// 2) mouvement du corps rigide : fluide + son propre poids (gravite NORMALE : c'est la similitude de Froude
	//    qui assure le bon comportement, pas une gravite modifiee)
	bax = fx / masseBalle;
	bay = fy / masseBalle - g_gravite;
	baz = fz / masseBalle;
	bvx += bax * dt;
	bvy += bay * dt;
	bvz += baz * dt;
	const float vit = std::sqrt(bvx * bvx + bvy * bvy + bvz * bvz);
	if (vit > VITESSE_MAX_BALLE) {
		const float s = VITESSE_MAX_BALLE / vit;
		bvx *= s; bvy *= s; bvz *= s;
	}
	bx += bvx * dt; by += bvy * dt; bz += bvz * dt;
	tempsBalle += dt;

	// 3) sol et paroi laterale (le recipient est ouvert en haut)
	if (by < rayonBalle) { by = rayonBalle; if (bvy < 0.0f) bvy *= -0.2f; }
	const float dx = bx - CENTRE_X, dz = bz - CENTRE_Z;
	const float dist = std::sqrt(dx * dx + dz * dz);
	const float lim = RAYON_RECIPIENT - rayonBalle - 0.05f;
	if (dist > lim) {
		const float nx = dx / dist, nz = dz / dist;
		bx = CENTRE_X + nx * lim; bz = CENTRE_Z + nz * lim;
		const float vn = bvx * nx + bvz * nz;
		if (vn > 0.0f) { bvx -= 1.2f * vn * nx; bvz -= 1.2f * vn * nz; }
	}

	// 4) les particules de la balle suivent le corps rigide
	for (int k = 0; k < nb; k++) {
		positions[nFluide + k] = Vec3(bx + decalages[k].x, by + decalages[k].y, bz + decalages[k].z);
		velocites[nFluide + k] = Vec3(bvx, bvy, bvz);
	}

	// 5) enregistrement CSV. Conversion a l'echelle reelle (similitude de Froude, g inchangee) :
	//    longueurs / L, vitesses / sqrt(L), temps / sqrt(L), accelerations inchangees (chute libre : -9.81 m/s2).
	//    Origine : y = 0 a la surface de l'eau calme ; x et z mesures depuis l'axe du recipient.
	if (fichierCSV.is_open()) {
		const float fL = CSV_UNITES_REELLES ? 1.0f / echelle : 1.0f;
		const float fSL = CSV_UNITES_REELLES ? 1.0f / std::sqrt(echelle) : 1.0f;
		const float oy = CSV_UNITES_REELLES ? NIVEAU_EAU_CALME : 0.0f;
		const float ox = CSV_UNITES_REELLES ? CENTRE_X : 0.0f;
		const float oz = CSV_UNITES_REELLES ? CENTRE_Z : 0.0f;
		fichierCSV << tempsBalle * fSL << ','
			<< bax << ',' << bay << ',' << baz << ','
			<< bvx * fSL << ',' << bvy * fSL << ',' << bvz * fSL << ','
			<< (bx - ox) * fL << ',' << (by - oy) * fL << ',' << (bz - oz) * fL << '\n';
	}
}

// Remplit le tableau d'instances (position + vitesse normalisee) pour les particules d'EAU uniquement
static void remplirInstances(std::vector<Matrix>& instances, int nb)
{
	if ((int)instances.size() != nb) instances.assign(nb, Matrix{});

	for (int i = 0; i < nb; i++) {
		const float vx = velocites[i].x, vy = velocites[i].y, vz = velocites[i].z;
		float t = std::sqrt(vx * vx + vy * vy + vz * vz) * INV_VITESSE_MAX_COULEUR;
		if (!(t == t)) t = 0.0f;
		Matrix& m = instances[i];
		m.m0 = std::min(t, 1.0f);
		m.m12 = positions[i].x;
		m.m13 = positions[i].y;
		m.m14 = positions[i].z;
	}
}

int visualisationFluides() {
	InitWindow(1200, 800, "SPH - chute d'une balle dans l'eau (raylib, instancing)");
	SetTargetFPS(60);

	initialiser();

	Mesh sphere = GenMeshSphere(RAYON_AFFICHAGE, RESOLUTION_SPHERE, RESOLUTION_SPHERE);
	Shader shader = LoadShaderFromMemory(VS_INSTANCING, FS_INSTANCING);
	shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
	shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(shader, "instanceTransform");
	Material materiau = LoadMaterialDefault();
	materiau.shader = shader;

	std::vector<Matrix> instances;
	instances.reserve(NB_PARTICULES_MAX);

	float angle = 0.6f, elevation = 0.25f, distance = 38.0f;
	bool pause = false;
	float tempsAccumule = 0.0f;                    // temps simule a rattraper (s)

	Camera3D camera = { 0 };
	camera.up = { 0.0f, 1.0f, 0.0f };
	camera.fovy = 45.0f;
	camera.projection = CAMERA_PERSPECTIVE;

	while (!WindowShouldClose())
	{
		// --- Entrees ---
		if (IsKeyPressed(KEY_SPACE)) pause = !pause;
		if (IsKeyPressed(KEY_R)) initialiser();
		if (IsKeyPressed(KEY_B)) basculerBalle();

		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
			Vector2 d = GetMouseDelta();
			angle -= d.x * 0.005f;
			elevation = std::clamp(elevation + d.y * 0.005f, -1.4f, 1.4f);
		}
		distance = std::clamp(distance - GetMouseWheelMove() * 1.5f, 5.0f, 150.0f);

		const Vector3 centre = { CENTRE_X, hauteur / 2.0f, CENTRE_Z };
		camera.target = centre;
		camera.position = {
			centre.x + distance * std::cos(elevation) * std::sin(angle),
			centre.y + distance * std::sin(elevation),
			centre.z + distance * std::cos(elevation) * std::cos(angle) };

		// --- Simulation ---
		if (!pause) {
			// temps reel ecoule depuis la derniere image (borne pour eviter un gros saut apres un blocage)
			tempsAccumule += std::min(GetFrameTime(), 0.1f) * VITESSE_RALENTI;
			int pas = 0;
			while (tempsAccumule >= DT && pas < MAX_PAS_PAR_IMAGE) {
				pasDeSimulation(DT);                 // eau ET particules de la balle : meme logique
				if (balleActive) majBalle(DT);
				contenirDansLeCylindre();
				tempsAccumule -= DT;
				pas++;
			}
			if (tempsAccumule >= DT) tempsAccumule = 0.0f;   // trop lent : on abandonne le retard (DT inchange)
		}

		// --- Rendu ---
		remplirInstances(instances, nFluide);

		BeginDrawing();
		ClearBackground(Color{ 20, 22, 30, 255 });

		BeginMode3D(camera);
		DrawMeshInstanced(sphere, materiau, instances.data(), (int)instances.size());

		if (balleActive) {
			const Vector3 c = { bx, by, bz };
			DrawSphereEx(c, rayonBalle, 24, 24, Color{ 235, 120, 40, 255 });
		}
		EndMode3D();

		DrawFPS(10, 10);
		DrawText(TextFormat("Particules d'eau : %d", nFluide), 10, 35, 20, RAYWHITE);
		DrawText(TextFormat("Recipient : diametre %.1f m, hauteur %.1f m (ouvert)", DIAMETRE, hauteur), 10, 60, 20, RAYWHITE);
		DrawText(pause ? "PAUSE (espace)" : "B : balle | espace : pause | R : reset | souris : camera", 10, 85, 20, LIGHTGRAY);
		EndDrawing();
	}

	fermerCSV();
	UnloadMesh(sphere);
	UnloadMaterial(materiau);
	CloseWindow();
	return 0;
}