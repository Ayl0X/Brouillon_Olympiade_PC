#include "raylib.h"
#include "rlgl.h"
#include <cmath>
#include <algorithm>
#include "phong.h"

// Unites de la scene : 1 unite = 1 cm
int phong() {
    InitWindow(1280, 720, "Balle de liege - 2 lumieres");
    SetTargetFPS(60);

    // ---------- Parametres de la scene (en cm) ----------
    const float rayonBalle = 3.0f;    // balle de 6 cm de diametre
    const float rayonSupport = 100.0f;    // petit socle blanc circulaire
    const float distLumiere1 = 20.0f;   // lumiere 1 : a 20 cm de la surface de la balle
    const float distLumiere2 = 2.0f;    // lumiere 2 : a 2 cm de la surface de la balle

    Vector3 centreBalle = { 0.0f, rayonBalle, 0.0f };   // la balle repose sur le support (y = 0)

    // Lumiere 1 : sur l'axe X passant par le centre de la balle
    Vector3 lumiere1 = { centreBalle.x + rayonBalle + distLumiere1, centreBalle.y, 0.0f };

    // Lumiere 2 : sur l'axe Z passant par le centre de la balle
    Vector3 lumiere2 = { 0.0f, centreBalle.y + 4, centreBalle.z + rayonBalle + distLumiere2 };

    // ---------- Camera orbitale ----------
    Camera3D camera = {};
    camera.target = centreBalle;
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float angle = 0.8f, elevation = 0.35f, distance = 32.0f;

    // ---------- Shader Phong ----------
    Shader shader = LoadShader("Lumiere\\Simpliste\\Phong.vs", "Lumiere\\Simpliste\\Phong.fs");
    int locViewPos = GetShaderLocation(shader, "viewPos");
    int locLightPos = GetShaderLocation(shader, "lightPos[0]");
    int locLightColor = GetShaderLocation(shader, "lightColor[0]");
    int locObjectColor = GetShaderLocation(shader, "objectColor");
    int locSpecStrength = GetShaderLocation(shader, "specularStrength");
    int locShininess = GetShaderLocation(shader, "shininess");
    int locShadow = GetShaderLocation(shader, "receiveShadow");
    int locBallCenter = GetShaderLocation(shader, "ballCenter");
    int locBallRadius = GetShaderLocation(shader, "ballRadius");

    float lp[6] = { lumiere1.x, lumiere1.y, lumiere1.z,
                    lumiere2.x, lumiere2.y, lumiere2.z };
    SetShaderValueV(shader, locLightPos, lp, SHADER_UNIFORM_VEC3, 2);

    // Intensite de chaque lumiere (multiplie la couleur blanc chaud).
    // Avec l'attenuation du shader, la lumiere a 20 cm est tres affaiblie : on la compense.
    const float intensite[2] = { 3.0f, 1.0f };
    const float teinte[3] = { 1.00f, 0.96f, 0.90f };
    bool allumee[2] = { true, true };

    float cb[3] = { centreBalle.x, centreBalle.y, centreBalle.z };
    SetShaderValue(shader, locBallCenter, cb, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, locBallRadius, &rayonBalle, SHADER_UNIFORM_FLOAT);

    // ---------- Materiaux ----------
    // Liege : marron mat, reflet tres faible et tres etale
    float couleurLiege[3] = { 0.60f, 0.42f, 0.26f };
    float specLiege = 0.04f, shinLiege = 6.0f;
    // Support : blanc, leger reflet
    float couleurSupport[3] = { 0.95f, 0.95f, 0.95f };
    float specSupport = 0.30f, shinSupport = 32.0f;

    // ---------- Modeles ----------
    Model balle = LoadModelFromMesh(GenMeshSphere(rayonBalle, 64, 64));
    balle.materials[0].shader = shader;

    // Cylindre : base en y = 0, hauteur vers +y -> decale de -1 pour que le dessus soit en y = 0
    Model support = LoadModelFromMesh(GenMeshCylinder(rayonSupport, 1.0f, 64));
    support.materials[0].shader = shader;

    while (!WindowShouldClose()) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 d = GetMouseDelta();
            angle -= d.x * 0.005f;
            elevation = std::clamp(elevation + d.y * 0.005f, -0.2f, 1.4f);
        }
        distance = std::clamp(distance - GetMouseWheelMove() * 2.0f, 8.0f, 80.0f);

        if (IsKeyPressed(KEY_ONE)) allumee[0] = !allumee[0];
        if (IsKeyPressed(KEY_TWO)) allumee[1] = !allumee[1];

        camera.position = {
            centreBalle.x + distance * std::cos(elevation) * std::sin(angle),
            centreBalle.y + distance * std::sin(elevation),
            centreBalle.z + distance * std::cos(elevation) * std::cos(angle) };

        float cam[3] = { camera.position.x, camera.position.y, camera.position.z };
        SetShaderValue(shader, locViewPos, cam, SHADER_UNIFORM_VEC3);

        float lc[6];
        for (int i = 0; i < 2; i++) {
            float k = allumee[i] ? intensite[i] : 0.0f;
            lc[i * 3 + 0] = teinte[0] * k;
            lc[i * 3 + 1] = teinte[1] * k;
            lc[i * 3 + 2] = teinte[2] * k;
        }
        SetShaderValueV(shader, locLightColor, lc, SHADER_UNIFORM_VEC3, 2);

        BeginDrawing();
        ClearBackground({ 40, 40, 46, 255 });
        BeginMode3D(camera);

        // Support blanc (recoit l'ombre de la balle)
        int un = 1, zero = 0;
        SetShaderValue(shader, locObjectColor, couleurSupport, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, locSpecStrength, &specSupport, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, locShininess, &shinSupport, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, locShadow, &un, SHADER_UNIFORM_INT);
        DrawModel(support, { 0.0f, -1.0f, 0.0f }, 1.0f, WHITE);

        // Balle de liege
        SetShaderValue(shader, locObjectColor, couleurLiege, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, locSpecStrength, &specLiege, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, locShininess, &shinLiege, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, locShadow, &zero, SHADER_UNIFORM_INT);
        DrawModel(balle, centreBalle, 1.0f, WHITE);

        EndMode3D();

        DrawFPS(10, 10);
        DrawText("Souris : orbite / molette : zoom   |   [1] lumiere X (20 cm)   [2] lumiere Z (2 cm)", 10, 690, 18, RAYWHITE);
        EndDrawing();
    }

    UnloadModel(balle);
    UnloadModel(support);
    UnloadShader(shader);
    CloseWindow();
    return 0;
}
