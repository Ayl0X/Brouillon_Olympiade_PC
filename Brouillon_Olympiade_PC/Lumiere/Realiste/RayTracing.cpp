#include "raylib.h"
#include "rlgl.h"
#include <cmath>
#include <algorithm>
#include "..\..\Constantes.h"
#include "..\..\Math.h"

static const float proche = 1.0f;   // distance du plan proche
static const int   nbPoints = 30;

int screenWidth = 1280;
int screenHeight = 720;

// Caméra dont on visualise les rayons
Camera3D cam = {
    { 0.0f, 2.0f, 0.0f },   // position
    { 0.0f, 2.0f, -1.0f },  // cible
    { 0.0f, 1.0f, 0.0f },   // up
    60.0f,                  // fovy
    CAMERA_PERSPECTIVE
};

int main() {
    InitWindow(screenWidth, screenHeight, "Rebond et gravite");
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

    // --- Shader ---
    Shader shader = LoadShader(nullptr, "Lumiere\\Realiste\\raytracer.fs");

    int resolutionLoc = GetShaderLocation(shader, "uResolution");
    int positionLoc = GetShaderLocation(shader, "uCameraPosition");
    int forwardLoc = GetShaderLocation(shader, "uCameraForward");
    int rightLoc = GetShaderLocation(shader, "uCameraRight");
    int upLoc = GetShaderLocation(shader, "uCameraUp");
    int fovLoc = GetShaderLocation(shader, "uFov");

    float resolution[2] = {
        (float)GetScreenWidth(),
        (float)GetScreenHeight()
    };

    float position[3] = { 0, 0, 0 };
    float forward[3] = { 0, 0, -1 };
    float right[3] = { 1, 0, 0 };
    float up[3] = { 0, 1, 0 };

    float fov = 60.0f;

    SetShaderValue(shader, resolutionLoc, resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, positionLoc, position, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, forwardLoc, forward, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, rightLoc, right, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, upLoc, up, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, fovLoc, &fov, SHADER_UNIFORM_FLOAT);

    /*
    float lightColor[3] = { 1.0f, 1.0f, 1.0f };

    // Couleurs des objets (vec3, valeurs 0..1)
    float couleurSphere[3] = { 1.0f, 0.2f, 0.2f };
    float couleurSol[3] = { 0.75f, 0.75f, 0.75f };

    // --- Modèles avec normales (au lieu de DrawSphere) ---
    Model sphere = LoadModelFromMesh(GenMeshSphere(rayon, 32, 32));
    sphere.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = RED;

    Model sol = LoadModelFromMesh(GenMeshPlane(20.0f, 20.0f, 1, 1));
    sol.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = LIGHTGRAY;*/

    while (!WindowShouldClose()) {
        /*
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
        */
        BeginDrawing();
        ClearBackground(RAYWHITE);

        float resolution[2] = {(float)screenWidth, (float)screenHeight};

        BeginShaderMode(shader);
        /*BeginMode3D(camera);
        DrawModel(sol, { 0, 0, 0 }, 1.0f, WHITE);

        DrawModel(sphere, centre, 1.0f, WHITE);

        DrawSphere(lightPos, 0.15f, YELLOW);   // repère visuel de la lumière*/
        DrawRectangle( 0, 0, 2000, 1000, WHITE);
        EndMode3D();
        EndShaderMode();
        EndDrawing();
    }
    UnloadShader(shader);
    CloseWindow();
    return 0;
}