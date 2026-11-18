// =====================================================================================
//  flex_gl.h  --  OpenGL 3.3 pour flex.cpp : chargeur de fonctions, shaders, FBO,
//                 rendu des particules en billboards (spheres "impostors").
//
//  A placer a cote de flex.cpp. Aucune dependance a Vec3 : on recoit des float* (x,y,z,x,y,z...).
//  Pas de GLEW/glad : les fonctions OpenGL > 1.1 sont chargees via sf::Context::getFunction.
// =====================================================================================
#pragma once

#include <SFML/Window.hpp>
#include <SFML/OpenGL.hpp>     // inclut windows.h + GL/gl.h (OpenGL 1.1 seulement sous Windows)

#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>

namespace glx {

	// -------------------------------------------------------------------------------------
	//  1) Chargeur de fonctions OpenGL (> 1.1)
	//     X-macro : la liste est ecrite UNE fois, puis servira a declarer ET a charger.
	// -------------------------------------------------------------------------------------
#define GLX_FONCTIONS(X)                                                                          \
	X(void,   glGenBuffers,              (GLsizei, GLuint*))                                       \
	X(void,   glDeleteBuffers,           (GLsizei, const GLuint*))                                 \
	X(void,   glBindBuffer,              (GLenum, GLuint))                                         \
	X(void,   glBufferData,              (GLenum, std::ptrdiff_t, const void*, GLenum))            \
	X(void,   glBufferSubData,           (GLenum, std::ptrdiff_t, std::ptrdiff_t, const void*))    \
	X(void,   glGenVertexArrays,         (GLsizei, GLuint*))                                       \
	X(void,   glDeleteVertexArrays,      (GLsizei, const GLuint*))                                 \
	X(void,   glBindVertexArray,         (GLuint))                                                 \
	X(void,   glEnableVertexAttribArray, (GLuint))                                                 \
	X(void,   glVertexAttribPointer,     (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
	X(void,   glVertexAttribDivisor,     (GLuint, GLuint))                                         \
	X(void,   glDrawArraysInstanced,     (GLenum, GLint, GLsizei, GLsizei))                        \
	X(GLuint, glCreateShader,            (GLenum))                                                 \
	X(void,   glShaderSource,            (GLuint, GLsizei, const char* const*, const GLint*))      \
	X(void,   glCompileShader,           (GLuint))                                                 \
	X(void,   glGetShaderiv,             (GLuint, GLenum, GLint*))                                 \
	X(void,   glGetShaderInfoLog,        (GLuint, GLsizei, GLsizei*, char*))                       \
	X(void,   glDeleteShader,            (GLuint))                                                 \
	X(GLuint, glCreateProgram,           ())                                                       \
	X(void,   glAttachShader,            (GLuint, GLuint))                                         \
	X(void,   glLinkProgram,             (GLuint))                                                 \
	X(void,   glGetProgramiv,            (GLuint, GLenum, GLint*))                                 \
	X(void,   glGetProgramInfoLog,       (GLuint, GLsizei, GLsizei*, char*))                       \
	X(void,   glDeleteProgram,           (GLuint))                                                 \
	X(void,   glUseProgram,              (GLuint))                                                 \
	X(GLint,  glGetUniformLocation,      (GLuint, const char*))                                    \
	X(void,   glUniform1f,               (GLint, GLfloat))                                         \
	X(void,   glUniform3f,               (GLint, GLfloat, GLfloat, GLfloat))                       \
	X(void,   glGenFramebuffers,         (GLsizei, GLuint*))                                       \
	X(void,   glDeleteFramebuffers,      (GLsizei, const GLuint*))                                 \
	X(void,   glBindFramebuffer,         (GLenum, GLuint))                                         \
	X(void,   glFramebufferTexture2D,    (GLenum, GLenum, GLenum, GLuint, GLint))                  \
	X(GLenum, glCheckFramebufferStatus,  (GLenum))                                                 \
	X(void,   glBlitFramebuffer,         (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum))

#define GLX_DECLARER(R, N, P) using PFN_##N = R (APIENTRY*) P; inline PFN_##N N = nullptr;
	GLX_FONCTIONS(GLX_DECLARER)
#undef GLX_DECLARER

		// Constantes absentes de gl.h (OpenGL 1.1). Prefixe k pour ne jamais entrer en conflit avec un glext.h.
		constexpr GLenum kArrayBuffer = 0x8892, kStreamDraw = 0x88E0, kStaticDraw = 0x88E4;
	constexpr GLenum kFragmentShader = 0x8B30, kVertexShader = 0x8B31;
	constexpr GLenum kCompileStatus = 0x8B81, kLinkStatus = 0x8B82, kInfoLogLength = 0x8B84;
	constexpr GLenum kFramebuffer = 0x8D40, kReadFramebuffer = 0x8CA8, kDrawFramebuffer = 0x8CA9;
	constexpr GLenum kColorAttachment0 = 0x8CE0, kDepthAttachment = 0x8D00, kFramebufferComplete = 0x8CD5;
	constexpr GLenum kRgba8 = 0x8058;
	constexpr GLenum kRgba32f = 0x8814;

	constexpr GLenum kDepthComponent24 = 0x81A6;
	constexpr GLenum kClampToEdge = 0x812F;

	// A appeler UNE fois, apres la creation de la fenetre (il faut un contexte OpenGL actif).
	inline bool charger() {
		bool ok = true;
#define GLX_CHARGER(R, N, P)                                                              \
	N = reinterpret_cast<PFN_##N>(sf::Context::getFunction(#N));                          \
	if (!N) { std::cerr << "Fonction OpenGL introuvable : " #N "\n"; ok = false; }
		GLX_FONCTIONS(GLX_CHARGER)
#undef GLX_CHARGER
			return ok;
	}

	// Affiche les erreurs OpenGL en attente (utile pour deboguer). Limite a quelques messages.
	inline void verifierErreurs(const char* contexte) {
		static int restant = 10;
		for (GLenum e = glGetError(); e != GL_NO_ERROR && restant > 0; e = glGetError(), restant--)
			std::cerr << "Erreur OpenGL 0x" << std::hex << e << std::dec << " apres : " << contexte << "\n";
	}

	// -------------------------------------------------------------------------------------
	//  2) Shaders : compilation + edition de liens, avec affichage du log en cas d'erreur
	// -------------------------------------------------------------------------------------
	inline GLuint compilerShader(GLenum type, const std::string& source, const char* nom) {
		const GLuint s = glCreateShader(type);
		const char* texte = source.c_str();
		glShaderSource(s, 1, &texte, nullptr);
		glCompileShader(s);
		GLint ok = 0;
		glGetShaderiv(s, kCompileStatus, &ok);
		if (!ok) {
			GLint n = 0; glGetShaderiv(s, kInfoLogLength, &n);
			std::string log((size_t)(n > 1 ? n : 1), '\0');
			glGetShaderInfoLog(s, (GLsizei)log.size(), nullptr, log.data());
			std::cerr << "--- Erreur de compilation (" << nom << ") ---\n" << log << "\n";
			glDeleteShader(s);
			return 0;
		}
		return s;
	}

	inline GLuint creerProgramme(const std::string& vs, const std::string& fs, const char* nom) {
		const GLuint v = compilerShader(kVertexShader, vs, nom);
		const GLuint f = compilerShader(kFragmentShader, fs, nom);
		if (!v || !f) return 0;
		const GLuint p = glCreateProgram();
		glAttachShader(p, v);
		glAttachShader(p, f);
		glLinkProgram(p);
		glDeleteShader(v);       // marques a supprimer : ils partiront avec le programme
		glDeleteShader(f);
		GLint ok = 0;
		glGetProgramiv(p, kLinkStatus, &ok);
		if (!ok) {
			GLint n = 0; glGetProgramiv(p, kInfoLogLength, &n);
			std::string log((size_t)(n > 1 ? n : 1), '\0');
			glGetProgramInfoLog(p, (GLsizei)log.size(), nullptr, log.data());
			std::cerr << "--- Erreur d'edition de liens (" << nom << ") ---\n" << log << "\n";
			glDeleteProgram(p);
			return 0;
		}
		return p;
	}

	// -------------------------------------------------------------------------------------
	//  3) FBO hors ecran : une texture couleur + une texture de PROFONDEUR (echantillonnable
	//     plus tard : c'est elle que l'etape "lissage de profondeur" lira).
	// -------------------------------------------------------------------------------------
	struct CibleDeRendu {
		GLuint fbo = 0, couleur = 0, profondeur = 0;
		int largeur = 0, hauteur = 0;

		bool creer(int w, int h, GLenum formatInterneCouleur = kRgba8, GLenum formatCouleur = GL_RGBA, GLenum typeCouleur = GL_UNSIGNED_BYTE) {
			largeur = w; hauteur = h;

			auto texture = [&](GLenum formatInterne, GLenum format, GLenum type) {
				GLuint t = 0;
				glGenTextures(1, &t);
				glBindTexture(GL_TEXTURE_2D, t);
				glTexImage2D(GL_TEXTURE_2D, 0, (GLint)formatInterne, w, h, 0, format, type, nullptr);
				// NEAREST obligatoire : le filtre par defaut utilise des mipmaps, que la texture n'a pas
				// -> texture "incomplete" -> echantillonnage noir dans un shader.
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)kClampToEdge);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)kClampToEdge);
				return t;
				};
			couleur = texture(formatInterneCouleur, formatCouleur, typeCouleur);
			profondeur = texture(kDepthComponent24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);

			glGenFramebuffers(1, &fbo);
			glBindFramebuffer(kFramebuffer, fbo);
			glFramebufferTexture2D(kFramebuffer, kColorAttachment0, GL_TEXTURE_2D, couleur, 0);
			glFramebufferTexture2D(kFramebuffer, kDepthAttachment, GL_TEXTURE_2D, profondeur, 0);
			const GLenum etat = glCheckFramebufferStatus(kFramebuffer);
			glBindFramebuffer(kFramebuffer, 0);
			if (etat != kFramebufferComplete) {
				std::cerr << "FBO incomplet : 0x" << std::hex << etat << std::dec << "\n";
				return false;
			}
			return true;
		}

		// Les prochains appels de dessin iront dans ce FBO ; on l'efface.
		void lierEtEffacer(float r, float g, float b) const {
			glBindFramebuffer(kFramebuffer, fbo);
			glViewport(0, 0, largeur, hauteur);
			glClearColor(r, g, b, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}

		// Copie la texture couleur du FBO vers la fenetre (met a l'echelle si les tailles different).
		void afficherSurFenetre(int largeurFenetre, int hauteurFenetre) const {
			glBindFramebuffer(kReadFramebuffer, fbo);
			glBindFramebuffer(kDrawFramebuffer, 0);
			glBlitFramebuffer(0, 0, largeur, hauteur, 0, 0, largeurFenetre, hauteurFenetre, GL_COLOR_BUFFER_BIT, GL_LINEAR);
			glBindFramebuffer(kFramebuffer, 0);
		}

		void detruire() {
			if (fbo) glDeleteFramebuffers(1, &fbo);
			if (couleur) glDeleteTextures(1, &couleur);
			if (profondeur) glDeleteTextures(1, &profondeur);
			fbo = couleur = profondeur = 0;
		}
	};

	// -------------------------------------------------------------------------------------
	//  4) Camera envoyee aux shaders : meme convention que pathTracer.fs
	//     (position + base orthonormee avant / droite / haut + fov vertical)
	// -------------------------------------------------------------------------------------
	struct ParametresCamera {
		float pos[3], avant[3], droite[3], haut[3];
		float fovDeg = 45.0f, aspect = 16.0f / 9.0f;
		float proche = 0.05f, loin = 100.0f;
	};

	// -------------------------------------------------------------------------------------
	//  5) Shaders GLSL 330
	// -------------------------------------------------------------------------------------
	// Bloc commun : passage monde -> repere camera ("vue") -> espace de decoupage (clip).
	// Repere vue : x = droite, y = haut, z = distance DEVANT la camera (positive).
	// Projection en perspective ecrite "a la main" (pas de matrice) :
	//     clip.x = x / (tan(fov/2) * aspect)     clip.y = y / tan(fov/2)
	//     clip.w = z                              clip.z = A*z + B   avec A=(f+n)/(f-n), B=-2fn/(f-n)
	// Apres la division par w : ndc.z = A + B/z  (profondeur OpenGL classique).
	static const char* kGlslCamera = R"(#version 330 core
uniform vec3  uCamPos;
uniform vec3  uCamAvant;
uniform vec3  uCamDroite;
uniform vec3  uCamHaut;
uniform float uTanDemiFov;
uniform float uAspect;
uniform float uProche;
uniform float uLoin;

float coefA() { return (uLoin + uProche) / (uLoin - uProche); }
float coefB() { return -2.0 * uLoin * uProche / (uLoin - uProche); }

vec3 versVue(vec3 p) {
    vec3 r = p - uCamPos;
    return vec3(dot(r, uCamDroite), dot(r, uCamHaut), dot(r, uCamAvant));
}
vec4 versClip(vec3 v) {
    return vec4(v.x / (uTanDemiFov * uAspect), v.y / uTanDemiFov, coefA() * v.z + coefB(), v.z);
}
)";

	// Particules : un quad (4 sommets) par particule, replique par instanciation.
	//  - location 0 : coin du quad (-1..1), identique pour toutes les instances
	//  - location 1 : centre de la particule, change a chaque instance (diviseur = 1)
	static const char* kVsParticule = R"(
layout(location = 0) in vec2 aCoin;
layout(location = 1) in vec3 aCentre;
uniform float uRayon;

out vec2 vCoin;
flat out float vProfondeurCentre;

void main() {
    vec3 v = versVue(aCentre);
    vProfondeurCentre = v.z;
    vCoin = aCoin;
    v.xy += aCoin * uRayon;              // le quad reste face a la camera : c'est un billboard
    gl_Position = versClip(v);
}
)";

	// Le quad est transforme en SPHERE : on rejette les pixels hors du disque, on reconstruit la
	// normale et la profondeur de la sphere, et on ecrit cette profondeur dans gl_FragDepth.
	// (Approximation classique : on traite le rayon de vue comme parallele a l'axe de la camera.)
	static const char* kFsParticule = R"(
in vec2 vCoin;
flat in float vProfondeurCentre;
uniform float uRayon;
out vec4 FragColor;

void main() {
    float d2 = dot(vCoin, vCoin);
    if (d2 > 1.0) discard;
    float zSphere = sqrt(1.0 - d2);                       // 0 au bord, 1 au centre

    // Normale en repere MONDE : vers la camera = -avant
    vec3 N = normalize(uCamDroite * vCoin.x + uCamHaut * vCoin.y - uCamAvant * zSphere);

    // Profondeur reelle du point de la sphere (plus proche que le centre)
    float z = vProfondeurCentre - zSphere * uRayon;
    gl_FragDepth = 0.5 * (coefA() + coefB() / z) + 0.5;

    // Eclairage de DEBUG (temporaire : sert juste a verifier la 3D)
    vec3  L = normalize(vec3(0.4, 1.0, 0.3));
    float diffus = max(dot(N, L), 0.0);
    FragColor = vec4(vec3(0.15, 0.45, 0.95) * (0.25 + 0.75 * diffus), 1.0);
}
)";

	static const char* kFsProfondeur = R"(
in vec2 vCoin;
flat in float vProfondeurCentre;

uniform float uRayon;

out vec4 FragColor;

void main()
{
    // Reconstituer le disque de la sphere
    float d2 = dot(vCoin, vCoin);

    if (d2 > 1.0)
        discard;

    // Position du point visible sur la sphere
    float zSphere = sqrt(max(0.0, 1.0 - d2));

    // Profondeur lineaire dans le repere camera
    float z = vProfondeurCentre - zSphere * uRayon;

    if (z <= uProche || z >= uLoin)
        discard;

    // Profondeur materielle OpenGL pour le depth test
    gl_FragDepth =
        0.5 * (coefA() + coefB() / z) + 0.5;

    // Profondeur lineaire normalisee pour la texture couleur
    float profondeur01 =
        (z - uProche) / (uLoin - uProche);

    FragColor = vec4(
        profondeur01,
        profondeur01,
        profondeur01,
        1.0
    );
}
)";

	static const char* kFsEpaisseur = R"(
in vec2 vCoin;
flat in float vProfondeurCentre;

uniform float uRayon;

out vec4 FragColor;

void main()
{
    float d2 = dot(vCoin, vCoin);

    if (d2 > 1.0)
        discard;

    // Verifier que la sphere touche le volume de vision
    if (vProfondeurCentre + uRayon < uProche ||
        vProfondeurCentre - uRayon > uLoin)
        discard;

    // Demi-longueur de la corde de la sphere
    float zSphere = sqrt(max(0.0, 1.0 - d2));

    // Epaisseur traversee par le rayon de camera
    float epaisseur =
        2.0 * uRayon * zSphere;

    // La valeur est additionnee aux contributions
    // des autres particules.
    FragColor = vec4(
        epaisseur,
        epaisseur,
        epaisseur,
        0.0
    );
}
)";

	// Aretes de la boite
	static const char* kVsLigne = R"(
layout(location = 0) in vec3 aPosition;
void main() { gl_Position = versClip(versVue(aPosition)); }
)";
	static const char* kFsLigne = R"(
out vec4 FragColor;
void main() { FragColor = vec4(0.5, 0.5, 0.5, 1.0); }
)";


	// -------------------------------------------------------------------------------------
	//  6) Rendu des particules : VBO + VAO + programmes
	// -------------------------------------------------------------------------------------
	class RenduParticules {
	public:
		bool initialiser(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
			progParticules = creerProgramme(std::string(kGlslCamera) + kVsParticule,
				std::string(kGlslCamera) + kFsParticule, "particules");
			progLignes = creerProgramme(std::string(kGlslCamera) + kVsLigne,
				std::string(kGlslCamera) + kFsLigne, "lignes");
			progProfondeur = creerProgramme(
				std::string(kGlslCamera) + kVsParticule,
				std::string(kGlslCamera) + kFsProfondeur, "carte_profondeur");
			progEpaisseur = creerProgramme(
				std::string(kGlslCamera) + kVsParticule,
				std::string(kGlslCamera) + kFsEpaisseur, "carte_epaisseur");

			if (!progParticules || !progLignes || !progProfondeur || !progEpaisseur) return false;

			// --- Particules : VAO = "cablage" des buffers vers les attributs du shader
			glGenVertexArrays(1, &vaoParticules);
			glBindVertexArray(vaoParticules);

			const float coins[8] = { -1, -1,   1, -1,   -1, 1,   1, 1 };   // TRIANGLE_STRIP : 2 triangles
			glGenBuffers(1, &vboCoins);
			glBindBuffer(kArrayBuffer, vboCoins);
			glBufferData(kArrayBuffer, sizeof coins, coins, kStaticDraw);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

			glGenBuffers(1, &vboPositions);                                 // rempli par envoyerPositions()
			glBindBuffer(kArrayBuffer, vboPositions);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			glVertexAttribDivisor(1, 1);                                    // 1 valeur par INSTANCE, pas par sommet

			// --- Boite : 12 aretes = 24 sommets
			float s[24 * 3];
			int k = 0;
			for (int a = 0; a < 8; a++)
				for (int bit = 0; bit < 3; bit++) {
					const int b = a ^ (1 << bit);
					if (b < a) continue;                                   // chaque arete une seule fois
					for (int m : { a, b }) {
						s[k++] = (m & 1) ? maxX : minX;
						s[k++] = (m & 2) ? maxY : minY;
						s[k++] = (m & 4) ? maxZ : minZ;
					}
				}
			glGenVertexArrays(1, &vaoBoite);
			glBindVertexArray(vaoBoite);
			glGenBuffers(1, &vboBoite);
			glBindBuffer(kArrayBuffer, vboBoite);
			glBufferData(kArrayBuffer, sizeof s, s, kStaticDraw);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

			glBindVertexArray(0);
			glBindBuffer(kArrayBuffer, 0);
			verifierErreurs("RenduParticules::initialiser");
			return true;
		}

		// xyz : n * 3 floats contigus (par ex. reinterpret_cast<const float*>(positions.data()))
		void envoyerPositions(const float* xyz, std::size_t n) {
			const std::ptrdiff_t octets = (std::ptrdiff_t)(n * 3 * sizeof(float));
			glBindBuffer(kArrayBuffer, vboPositions);
			if (n > capacite) {                                            // agrandir
				glBufferData(kArrayBuffer, octets, xyz, kStreamDraw);
				capacite = n;
			}
			else {
				// "orphaning" : on realloue le meme espace pour ne pas attendre que le GPU ait fini
				// de lire l'image precedente, puis on ecrit dedans.
				glBufferData(kArrayBuffer, (std::ptrdiff_t)(capacite * 3 * sizeof(float)), nullptr, kStreamDraw);
				glBufferSubData(kArrayBuffer, 0, octets, xyz);
			}
			glBindBuffer(kArrayBuffer, 0);
			nbParticules = n;
		}

		// A appeler quand le FBO de destination est deja lie (voir CibleDeRendu::lierEtEffacer)
		void dessiner(const ParametresCamera& c, float rayon) const {
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LESS);

			glUseProgram(progLignes);
			envoyerCamera(progLignes, c);
			glBindVertexArray(vaoBoite);
			glDrawArrays(GL_LINES, 0, 24);

			glUseProgram(progParticules);
			envoyerCamera(progParticules, c);
			glUniform1f(glGetUniformLocation(progParticules, "uRayon"), rayon);
			glBindVertexArray(vaoParticules);
			glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)nbParticules);

			glBindVertexArray(0);
			glUseProgram(0);
			verifierErreurs("RenduParticules::dessiner");
		}

		void dessinerCarteProfondeur(const ParametresCamera& c, float rayon) const {

			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LESS);
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);

			glUseProgram(progProfondeur);
			envoyerCamera(progProfondeur, c);
			glUniform1f(glGetUniformLocation(progProfondeur, "uRayon"),rayon);

			glBindVertexArray(vaoParticules);
			glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,4,(GLsizei)nbParticules);

			glBindVertexArray(0);
			glUseProgram(0);
		}

		void dessinerCarteEpaisseur(const ParametresCamera& c, float rayon) const {

			// Toutes les particules doivent contribuer
			// meme si elles sont derriere une autre.
			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);

			// Additionner les contributions
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);

			glUseProgram(progEpaisseur);
			envoyerCamera(progEpaisseur, c);
			glUniform1f(glGetUniformLocation(progEpaisseur, "uRayon"), rayon);

			glBindVertexArray(vaoParticules);
			glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)nbParticules);

			glBindVertexArray(0);
			glUseProgram(0);

			// Restaurer les etats OpenGL
			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE);
			glEnable(GL_DEPTH_TEST);
		}

		void detruire() {
			if (vboCoins) glDeleteBuffers(1, &vboCoins);
			if (vboPositions) glDeleteBuffers(1, &vboPositions);
			if (vboBoite) glDeleteBuffers(1, &vboBoite);
			if (vaoParticules) glDeleteVertexArrays(1, &vaoParticules);
			if (vaoBoite) glDeleteVertexArrays(1, &vaoBoite);
			if (progParticules) glDeleteProgram(progParticules);
			if (progLignes) glDeleteProgram(progLignes);
			if (progProfondeur) glDeleteProgram(progProfondeur);
			if (progEpaisseur) glDeleteProgram(progEpaisseur);
			vboCoins = vboPositions = vboBoite = vaoParticules = vaoBoite = progParticules = progLignes = progProfondeur = progEpaisseur = 0;
		}

	private:
		static void envoyerCamera(GLuint p, const ParametresCamera& c) {
			glUniform3f(glGetUniformLocation(p, "uCamPos"), c.pos[0], c.pos[1], c.pos[2]);
			glUniform3f(glGetUniformLocation(p, "uCamAvant"), c.avant[0], c.avant[1], c.avant[2]);
			glUniform3f(glGetUniformLocation(p, "uCamDroite"), c.droite[0], c.droite[1], c.droite[2]);
			glUniform3f(glGetUniformLocation(p, "uCamHaut"), c.haut[0], c.haut[1], c.haut[2]);
			glUniform1f(glGetUniformLocation(p, "uTanDemiFov"), std::tan(0.5f * c.fovDeg * 3.14159265f / 180.0f));
			glUniform1f(glGetUniformLocation(p, "uAspect"), c.aspect);
			glUniform1f(glGetUniformLocation(p, "uProche"), c.proche);
			glUniform1f(glGetUniformLocation(p, "uLoin"), c.loin);
		}

		GLuint progParticules = 0, progLignes = 0;
		GLuint progProfondeur = 0, progEpaisseur = 0;
		GLuint vaoParticules = 0, vaoBoite = 0;
		GLuint vboCoins = 0, vboPositions = 0, vboBoite = 0;
		std::size_t capacite = 0, nbParticules = 0;
	};

} // namespace glx