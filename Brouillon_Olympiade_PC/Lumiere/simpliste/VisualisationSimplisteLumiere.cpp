#include "raylib.h"
#include "rlgl.h"
#include <cmath>
#include <algorithm>
#include "..\..\Constantes.h"

int main() {
    InitWindow(1280, 720, "Rebond et gravite");
    SetTargetFPS(60);

    Camera3D camera = {};
    camera.position = { 10.0f, 8.0f, 10.0f };
    camera.target = { 0.0f, 2.0f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    const float rayon = 0.3f;
    float angle = 0.6f, elevation = 0.35f, distance = 11.0f;

    particule p;
    p.position = { 0.0f, 4.0f, 0.0f };

    // --- Shader Phong ---
    Shader shader = LoadShader("Lumiere\\Simpliste\\Phong.vs", "Lumiere\\Simpliste\\Phong.fs");
    int locViewPos = GetShaderLocation(shader, "viewPos");
    int locLightPos = GetShaderLocation(shader, "lightPos");
    int locLightColor = GetShaderLocation(shader, "lightColor");
    int locObjectColor = GetShaderLocation(shader, "objectColor");

    float lightColor[3] = { 1.0f, 1.0f, 1.0f };
    SetShaderValue(shader, locLightColor, lightColor, SHADER_UNIFORM_VEC3);

    // Couleurs des objets (vec3, valeurs 0..1)
    float couleurSphere[3] = { 1.0f, 0.2f, 0.2f };
    float couleurSol[3] = { 0.75f, 0.75f, 0.75f };

    // --- Modèles avec normales (au lieu de DrawSphere) ---
    Model sphere = LoadModelFromMesh(GenMeshSphere(rayon, 32, 32));
    sphere.materials[0].shader = shader;
    sphere.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = RED;

    Model sol = LoadModelFromMesh(GenMeshPlane(20.0f, 20.0f, 1, 1));
    sol.materials[0].shader = shader;
    sol.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = LIGHTGRAY;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.033f) dt = 0.033f;

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 d = GetMouseDelta();
            angle -= d.x * 0.005f;
            elevation = std::clamp(elevation + d.y * 0.005f, -1.4f, 1.4f);
        }
        distance = std::clamp(distance - GetMouseWheelMove() * 0.8f, 4.0f, 40.0f);

        Vector3 centre = { p.position.x, p.position.y, p.position.z };
        camera.target = centre;
        camera.position = {
            centre.x + distance * std::cos(elevation) * std::sin(angle),
            centre.y + distance * std::sin(elevation),
            centre.z + distance * std::cos(elevation) * std::cos(angle) };

        float t = (float)GetTime();
        Vector3 lightPos = { 5, 5, 0 };

        float cam[3] = { camera.position.x, camera.position.y, camera.position.z };
        float lp[3] = { lightPos.x, lightPos.y, lightPos.z };
        SetShaderValue(shader, locViewPos, cam, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, locLightPos, lp, SHADER_UNIFORM_VEC3);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);
        SetShaderValue(shader, locObjectColor, couleurSol, SHADER_UNIFORM_VEC3);
        DrawModel(sol, { 0, 0, 0 }, 1.0f, WHITE);

        SetShaderValue(shader, locObjectColor, couleurSphere, SHADER_UNIFORM_VEC3);
        DrawModel(sphere, centre, 1.0f, WHITE);

        DrawSphere(lightPos, 0.15f, YELLOW);   // repère visuel de la lumière
        EndMode3D();
        DrawFPS(10, 10);
        EndDrawing();
    }

    UnloadModel(sphere);
    UnloadModel(sol);
    UnloadShader(shader);
    CloseWindow();
    return 0;
}