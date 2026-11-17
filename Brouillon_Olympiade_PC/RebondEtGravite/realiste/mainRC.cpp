#include "raylib.h"
#include "rebondCoef.h"
#include "..\..\Constantes.h"
#include "..\..\Math.h"
#include "raymath.h"
#include <rlgl.h>
#include <fstream>
#include <iomanip>

static void UpdateFreeCamera(Camera3D* camera)
{
    // Déplacement clavier
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

        Vector3 direction = Vector3Subtract(
            camera->target,
            camera->position
        );

        float distance = Vector3Length(direction);

        float yaw = atan2f(direction.x, direction.z);
        float pitch = asinf(direction.y / distance);

        yaw -= d.x * sensitivity;
        pitch -= d.y * sensitivity;

        pitch = Clamp(pitch, -1.5f, 1.5f);

        direction.x = sinf(yaw) * cosf(pitch);
        direction.y = sinf(pitch);
        direction.z = cosf(yaw) * cosf(pitch);

        camera->target = Vector3Add(
            camera->position,
            Vector3Scale(direction, distance)
        );
    }
}

int rebondCoef() {
    InitWindow(1280, 720, "Rebond et gravite");
    SetTargetFPS(60);

    Camera3D camera = {};
    camera.position = { 0.25f, 0.25f, 2.0f };
    camera.target = { 0.0f, 0.5f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // --- Parametres (metres) ---
    const float hauteurChute = 0.78f;
    const float angleDeg = 12.0f;
    const float angle = angleDeg * DEG2RAD;
    const int   sousPas = 20;

    Vec3 normale = { -sinf(angle), cosf(angle), 0.0f };

    particule p;

    // --- Enregistrement CSV ---
    const float logDt = 1.0f / 30.0f;         // 30 echantillons par seconde
    std::ofstream csv("rebond.csv");
    csv << std::fixed << std::setprecision(5);
    csv << "t,ax,ay,az,vx,vy,vz,px,py,pz\n";

    float simTime = 0.0f;                     // chronometre de simulation
    float nextLog = 0.0f;                     // prochain instant d'echantillonnage
    float prevLogTime = 0.0f;                 // pour calculer l'acceleration
    Vec3  prevLogVel = { 0.0f, 0.0f, 0.0f };

    auto reset = [&]() {
        p.position = { 0.0f, hauteurChute + g_rayonBalle / cosf(angle), 0.0f };
        p.velocite = { 0.0f, 0.0f, 0.0f };

        simTime = 0.0f;                       // le chrono repart a zero
        nextLog = 0.0f;
        prevLogTime = 0.0f;
        prevLogVel = { 0.0f, 0.0f, 0.0f };
        };
    reset();

    while (!WindowShouldClose()) {

        UpdateFreeCamera(&camera);
        if (IsKeyPressed(KEY_R)) reset();

        float dt = GetFrameTime();
        if (dt > 0.033f) dt = 0.033f;
        float h = dt / sousPas;

        for (int i = 0; i < sousPas; i++) {
            Vec3 yv = graviteCoef({ p.position.y, p.velocite.y }, h);
            p.position.y = yv.x;
            p.velocite.y = yv.y;

            p.position.x += p.velocite.x * h;
            p.position.z += p.velocite.z * h;

            collisionCoef(p, normale, g_rayonBalle);

            // avance du chrono et ecriture a 30 Hz (verifie a chaque sous-pas
            // pour que les instants soient precis meme avec 60 FPS)
            simTime += h;
            if (simTime >= nextLog) {
                float dtLog = simTime - prevLogTime;
                Vec3 acc = { 0.0f, 0.0f, 0.0f };
                if (dtLog > 1e-6f) {
                    acc.x = (p.velocite.x - prevLogVel.x) / dtLog;
                    acc.y = (p.velocite.y - prevLogVel.y) / dtLog;
                    acc.z = (p.velocite.z - prevLogVel.z) / dtLog;
                }

                csv << simTime << ','
                    << acc.x << ',' << acc.y << ',' << acc.z << ','
                    << p.velocite.x << ',' << p.velocite.y << ',' << p.velocite.z << ','
                    << p.position.x << ',' << p.position.y << ',' << p.position.z << '\n';

                prevLogTime = simTime;
                prevLogVel = { p.velocite.x, p.velocite.y, p.velocite.z };
                nextLog += logDt;
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);
        DrawGrid(20, 1.0f);

        rlPushMatrix();
        rlRotatef(angleDeg, 0.0f, 0.0f, 1.0f);
        DrawCube({ 0.0f, -0.05f, 0.0f }, 20.0f, 0.1f, 20.0f, LIGHTGRAY);
        DrawCubeWires({ 0.0f, -0.05f, 0.0f }, 20.0f, 0.1f, 20.0f, GRAY);
        rlPopMatrix();

        DrawSphere({ p.position.x, p.position.y, p.position.z }, g_rayonBalle, RED);
        EndMode3D();

        DrawText(TextFormat("Vitesse : %.2f m/s", Vector2Length({ p.velocite.x, p.velocite.y })), 10, 35, 20, DARKGRAY);
        DrawText(TextFormat("Chrono : %.2f s", simTime), 10, 85, 20, DARKGRAY);  // affichage du chrono
        DrawText("R : relancer", 10, 60, 20, DARKGRAY);
        DrawFPS(10, 10);
        EndDrawing();
    }

    csv.close();
    CloseWindow();
    return 0;
}