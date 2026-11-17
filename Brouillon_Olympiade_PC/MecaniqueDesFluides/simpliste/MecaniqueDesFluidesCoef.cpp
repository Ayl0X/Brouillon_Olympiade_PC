#include "MecaniqueDesFluidesCoef.h"
#include "..\..\Constantes.h"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <fstream>
#include <iomanip>

// renvoie une force en N
float frottementDeLeau(float& v) {
    float rho = 1000.0f; // densite de l'eau en kg/m^3
    float S = pi * g_rayonBalle * g_rayonBalle; // surface frontale de la balle en m^2
    float force = 0.5f * rho * g_coefficientDeTrainee * S * v * v; // force de trainee en N
    return force;
}

// volume de la balle immerge (calotte spherique), en m^3
// yCentre : hauteur du centre de la balle, hauteurSurface : hauteur de la surface de l'eau
float volumeImmerge(float yCentre, float hauteurSurface) {
    float r = g_rayonBalle;
    float h = hauteurSurface - (yCentre - r);   // profondeur immergee depuis le bas de la balle
    if (h <= 0.0f) return 0.0f;                 // balle entierement hors de l'eau
    if (h >= 2.0f * r) h = 2.0f * r;            // balle entierement immergee
    return pi * h * h * (3.0f * r - h) / 3.0f;  // volume d'une calotte spherique de hauteur h
}

// renvoie la poussee d'Archimede en N (dirigee vers le haut)
float poussee_d_archimede(float volumeImmerge) {
    float rho = 1000.0f; // densite de l'eau en kg/m^3
    float gravite = 9.81f; // m/s^2
    return rho * volumeImmerge * gravite;
}

static void UpdateFreeCamera(Camera3D* camera)
{
    // Deplacement clavier
    float speed = 0.2f;

    Vector3 forward = Vector3Normalize(Vector3Subtract(camera->target, camera->position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera->up));

    if (IsKeyDown(KEY_W)) camera->position = Vector3Add(camera->position, Vector3Scale(forward, speed));
    if (IsKeyDown(KEY_S)) camera->position = Vector3Subtract(camera->position, Vector3Scale(forward, speed));
    if (IsKeyDown(KEY_D)) camera->position = Vector3Add(camera->position, Vector3Scale(right, speed));
    if (IsKeyDown(KEY_A)) camera->position = Vector3Subtract(camera->position, Vector3Scale(right, speed));
    if (IsKeyDown(KEY_SPACE)) camera->position.y += speed;
    if (IsKeyDown(KEY_LEFT_SHIFT)) camera->position.y -= speed;

    // Rotation uniquement avec le clic gauche
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        Vector2 d = GetMouseDelta();

        float sensitivity = 0.003f;

        Vector3 direction = Vector3Subtract(camera->target, camera->position);
        float distance = Vector3Length(direction);

        float yaw = atan2f(direction.x, direction.z);
        float pitch = asinf(direction.y / distance);

        yaw -= d.x * sensitivity;
        pitch -= d.y * sensitivity;

        pitch = Clamp(pitch, -1.5f, 1.5f);

        direction.x = sinf(yaw) * cosf(pitch);
        direction.y = sinf(pitch);
        direction.z = cosf(yaw) * cosf(pitch);

        camera->target = Vector3Add(camera->position, Vector3Scale(direction, distance));
    }
}

int mecaniqueDesFluidesCoef() {
    InitWindow(1280, 720, "Rebond et gravite");
    SetTargetFPS(60);

    Camera3D camera = {};
    camera.position = { 0.25f, 0.25f, 2.0f };
    camera.target = { 0.0f, 0.5f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // --- Parametres (metres) ---
    const float hauteurChute = 0.5f;
    const int   sousPas = 20; // sous-pas de simulation par frame (precision)

    // --- Eau : cylindre pose sur la grille (y = 0), centre sur l'origine ---
    const float rayonEau = 0.105f;   // diametre 21 cm
    const float hauteurEau = 0.10f;  // hauteur 10 cm

    // --- Physique ---
    const float gravite = 9.81f;     // m/s^2

    // --- Enregistrement CSV (meme format que la simulation SPH) ---
    // Origine : y = 0 a la surface de l'eau (hauteurEau) ; x et z mesures depuis l'axe du cylindre (deja l'origine).
    // Le temps part de 0 au moment ou la balle est lachee (au lancement et a chaque appui sur R).
    const float dureeMaxCsv = 30.0f;   // arret de l'enregistrement apres 30 s (0 = illimite)
    std::ofstream fichierCSV;
    float temps = 0.0f;

    particule p;
    auto reset = [&]() {
        p.position = { 0.0f, hauteurChute + g_rayonBalle, 0.0f };
        p.velocite = { 0.0f, 0.0f, 0.0f };
        temps = 0.0f;
        if (fichierCSV.is_open()) fichierCSV.close();
        fichierCSV.open("balle_modele.csv", std::ios::trunc);
        fichierCSV << std::setprecision(8);
        fichierCSV << "temps_s,ax_m_s2,ay_m_s2,az_m_s2,vx_m_s,vy_m_s,vz_m_s,x_m,y_m,z_m\n";
        };
    reset();

    while (!WindowShouldClose()) {

        UpdateFreeCamera(&camera);
        if (IsKeyPressed(KEY_R)) reset();

        // --- Simulation ---
        const float dt = (1.0f / 60.0f) / sousPas;
        bool dansLeau = false;

        for (int i = 0; i < sousPas; i++) {
            // la balle est dans l'eau si son centre est dans le cylindre
            float distHorizontale = sqrtf(p.position.x * p.position.x + p.position.z * p.position.z);
            dansLeau = (distHorizontale <= rayonEau) && (p.position.y >= 0.0f) && (p.position.y <= hauteurEau);

            // vitesse au debut du sous-pas : sert a calculer l'acceleration nette (gravite + poussee + trainee)
            const Vec3 vAvant = p.velocite;

            // gravite
            p.velocite.y -= gravite * dt;

            // poussee d'Archimede : vers le haut, proportionnelle au volume immerge
            if (distHorizontale <= rayonEau) {
                float Vimm = volumeImmerge(p.position.y, hauteurEau);
                if (Vimm > 0.0f) {
                    float poussee = poussee_d_archimede(Vimm);   // N
                    p.velocite.y += (poussee / g_massBalle) * dt;
                }
            }

            // frottement de l'eau : oppose a la vitesse
            if (dansLeau) {
                float v = sqrtf(p.velocite.x * p.velocite.x +
                    p.velocite.y * p.velocite.y +
                    p.velocite.z * p.velocite.z);
                if (v > 1e-6f) {
                    float force = frottementDeLeau(v);   // N
                    float dv = (force / g_massBalle) * dt;     // variation de vitesse
                    if (dv > v) dv = v;                  // la trainee ne peut pas inverser la vitesse
                    float k = dv / v;
                    p.velocite.x -= p.velocite.x * k;
                    p.velocite.y -= p.velocite.y * k;
                    p.velocite.z -= p.velocite.z * k;
                }
            }

            // integration
            p.position.x += p.velocite.x * dt;
            p.position.y += p.velocite.y * dt;
            p.position.z += p.velocite.z * dt;

            // enregistrement CSV : acceleration nette = variation de vitesse / dt
            temps += dt;
            if (fichierCSV.is_open()) {
                if (dureeMaxCsv > 0.0f && temps > dureeMaxCsv) {
                    fichierCSV.close();
                }
                else {
                    fichierCSV << temps << ','
                        << (p.velocite.x - vAvant.x) / dt << ','
                        << (p.velocite.y - vAvant.y) / dt << ','
                        << (p.velocite.z - vAvant.z) / dt << ','
                        << p.velocite.x << ',' << p.velocite.y << ',' << p.velocite.z << ','
                        << p.position.x << ',' << (p.position.y - hauteurEau) << ',' << p.position.z << '\n';
                }
            }
        }

        // --- Rendu ---
        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);
        DrawGrid(20, 1.0f);

        DrawSphere({ p.position.x, p.position.y, p.position.z }, g_rayonBalle, RED);

        // eau dessinee en dernier pour la transparence
        DrawCylinder({ 0.0f, 0.0f, 0.0f }, rayonEau, rayonEau, hauteurEau, 64, Fade(BLUE, 0.5f));

        EndMode3D();

        DrawText(TextFormat("Vitesse : %.2f m/s", Vector2Length({ p.velocite.x, p.velocite.y })), 10, 35, 20, DARKGRAY);
        DrawText(dansLeau ? "Dans l'eau" : "Hors de l'eau", 10, 85, 20, dansLeau ? BLUE : DARKGRAY);
        DrawText("R : relancer", 10, 60, 20, DARKGRAY);
        DrawFPS(10, 10);
        EndDrawing();
    }

    if (fichierCSV.is_open()) fichierCSV.close();
    CloseWindow();
    return 0;
}