#include "raylib.h"
#include "RRebondEtGravite.h"
#include "..\..\Constantes.h"

int mainRR() {
    InitWindow(1280, 720, "Rebond et gravite");
    SetTargetFPS(60);

    Camera3D camera = {};
    camera.position = { 10.0f, 8.0f, 10.0f };
    camera.target = { 0.0f, 2.0f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    const float rayon = 0.3f;

    particule p;
    p.position = { 0.0f, 8.0f, 0.0f };
    p.velocite = { 0.0f, 0.0f, 0.0f };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.033f) dt = 0.033f;          // évite les gros pas de temps

        // Gravité : ta fonction est 1D, on l'applique donc à l'axe Y uniquement
        Vec3 yv = Rgravite({ p.position.y, p.velocite.y }, dt);
        p.position.y = yv.x;
        p.velocite.y = yv.y;

        // Mouvement horizontal (X et Z)
        p.position.x += p.velocite.x * dt;
        p.position.z += p.velocite.z * dt;
        Rcollision(p);                          // rebond sur le sol y = 0

        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);
        DrawGrid(20, 1.0f);
        // + rayon pour que la sphère repose sur le sol au lieu d'y être à moitié enfoncée
        DrawSphere({ p.position.x, p.position.y + rayon, p.position.z }, rayon, RED);
        EndMode3D();
        DrawFPS(10, 10);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}