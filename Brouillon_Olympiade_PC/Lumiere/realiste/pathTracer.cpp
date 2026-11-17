#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "pathTracer.h"

#include <cstdio>

// Configuration

static constexpr int INITIAL_WIDTH = 1280;
static constexpr int INITIAL_HEIGHT = 720;
static constexpr int TARGET_FPS = 60;

static const char* FRAGMENT_SHADER_PATH = "Lumiere/Realiste/pathTracer.fs";

// Camera uniforms
struct CameraUniformLocations
{
    int resolution = -1;
    int position = -1;
    int forward = -1;
    int right = -1;
    int up = -1;
    int fov = -1;
    int sampleIndex = -1;
    int previousFrame = -1;
};

static CameraUniformLocations GetCameraUniformLocations(Shader shader)
{
    CameraUniformLocations loc;

    loc.resolution = GetShaderLocation(shader, "uResolution");
    loc.position = GetShaderLocation(shader, "uCameraPosition");
    loc.forward = GetShaderLocation(shader, "uCameraForward");
    loc.right = GetShaderLocation(shader, "uCameraRight");
    loc.up = GetShaderLocation(shader, "uCameraUp");
    loc.fov = GetShaderLocation(shader, "uFov");
    loc.sampleIndex = GetShaderLocation(shader, "uSampleIndex");
    loc.previousFrame = GetShaderLocation(shader, "uPreviousFrame");

    return loc;
}

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

static void SendCameraUniforms( Shader shader, const CameraUniformLocations& loc, const Camera3D& camera, int width, int height, int sampleIndex) {

    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    Vector3 correctedUp = Vector3Normalize(Vector3CrossProduct(right, forward));

    float resolution[2] = {
        static_cast<float>(width),
        static_cast<float>(height)
    };

    float position[3] = {
        camera.position.x,
        camera.position.y,
        camera.position.z
    };

    float forwardArray[3] = { forward.x, forward.y, forward.z };
    float rightArray[3] = { right.x, right.y, right.z };
    float upArray[3] = { correctedUp.x, correctedUp.y, correctedUp.z };
    float fov = camera.fovy;
    int sample = sampleIndex;

    if (loc.resolution >= 0) SetShaderValue(shader, loc.resolution, resolution, SHADER_UNIFORM_VEC2);
    if (loc.position >= 0) SetShaderValue(shader, loc.position, position, SHADER_UNIFORM_VEC3);
    if (loc.forward >= 0) SetShaderValue(shader, loc.forward, forwardArray, SHADER_UNIFORM_VEC3);
    if (loc.right >= 0) SetShaderValue(shader, loc.right, rightArray, SHADER_UNIFORM_VEC3);
    if (loc.up >= 0) SetShaderValue(shader, loc.up, upArray, SHADER_UNIFORM_VEC3);
    if (loc.fov >= 0) SetShaderValue(shader, loc.fov, &fov, SHADER_UNIFORM_FLOAT);
    if (loc.sampleIndex >= 0) SetShaderValue(shader, loc.sampleIndex, &sampleIndex, SHADER_UNIFORM_INT);
}

// Accumulation helpers
static void ClearAccumulation(RenderTexture2D& a, RenderTexture2D& b) {
    BeginTextureMode(a);
    ClearBackground(BLACK);
    EndTextureMode();

    BeginTextureMode(b);
    ClearBackground(BLACK);
    EndTextureMode();
}

static bool CameraHasChanged( const Camera3D& current, const Camera3D& previous, float epsilon = 0.0001f) {
    const bool positionChanged = Vector3Distance(current.position, previous.position) > epsilon;
    const bool targetChanged = Vector3Distance(current.target, previous.target) > epsilon;
    const bool upChanged = Vector3Distance(current.up, previous.up) > epsilon;
    const bool fovyChanged = fabsf(current.fovy - previous.fovy) > epsilon;

    return positionChanged || targetChanged || upChanged || fovyChanged;
}
static RenderTexture2D LoadFloatRenderTexture(int width, int height)
{
    RenderTexture2D target = { 0 };

    target.id = rlLoadFramebuffer();

    if (target.id == 0)
        return target;

    rlEnableFramebuffer(target.id);

    target.texture.id = rlLoadTexture(
        nullptr,
        width,
        height,
        PIXELFORMAT_UNCOMPRESSED_R16G16B16A16,
        1
    );

    target.texture.width = width;
    target.texture.height = height;
    target.texture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    target.texture.mipmaps = 1;

    if (target.texture.id > 0)
    {
        rlFramebufferAttach(
            target.id,
            target.texture.id,
            RL_ATTACHMENT_COLOR_CHANNEL0,
            RL_ATTACHMENT_TEXTURE2D,
            0
        );
    }

    bool complete =
        target.texture.id > 0 && rlFramebufferComplete(target.id);

    rlDisableFramebuffer();

    if (!complete)
    {
        if (target.texture.id > 0)
            rlUnloadTexture(target.texture.id);

        rlUnloadFramebuffer(target.id);

        TraceLog(LOG_ERROR, "Impossible de creer la texture flottante.");
        return { 0 };
    }

    return target;
}


// Main
int pathTracer()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(INITIAL_WIDTH, INITIAL_HEIGHT, "Path tracer - GLSL 330");
    SetTargetFPS(TARGET_FPS);

    // Caméra contrôlée depuis le C++.
    Camera3D camera = {};
    camera.position = { 10.0f, 8.0f, 10.0f };
    camera.target = { 0.0f, 2.0f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Camera3D previousCamera = camera;

    // Charger le shader fragment GLSL.
    Shader shader = LoadShader(nullptr, FRAGMENT_SHADER_PATH);

    if (shader.id == 0)
    {
        TraceLog(LOG_ERROR, "Impossible de charger le shader : %s",
            FRAGMENT_SHADER_PATH);
        CloseWindow();
        return 1;
    }

    CameraUniformLocations uniforms = GetCameraUniformLocations(shader);

    if (uniforms.resolution < 0 ||
        uniforms.position < 0 ||
        uniforms.forward < 0 ||
        uniforms.right < 0 ||
        uniforms.up < 0 ||
        uniforms.fov < 0 ||
        uniforms.sampleIndex < 0 ||
        uniforms.previousFrame < 0)
    {
        TraceLog(LOG_WARNING,
            "Au moins un uniforme attendu est absent ou inutilise dans raytracer.fs.");
    }

    int width = GetScreenWidth();
    int height = GetScreenHeight();

    RenderTexture2D accumulationA = LoadFloatRenderTexture(width, height);
    RenderTexture2D accumulationB = LoadFloatRenderTexture(width, height);

    if (accumulationA.id == 0 || accumulationB.id == 0)
    {
        TraceLog(LOG_ERROR, "Impossible de creer les textures d'accumulation.");

        if (accumulationA.id != 0) UnloadRenderTexture(accumulationA);
        if (accumulationB.id != 0) UnloadRenderTexture(accumulationB);

        UnloadShader(shader);
        CloseWindow();
        return 1;
    }

    SetTextureFilter(accumulationA.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(accumulationB.texture, TEXTURE_FILTER_POINT);

    ClearAccumulation(accumulationA, accumulationB);

    int sampleCount = 0;
    bool readA = true;

    Texture2D lastRenderedTexture = accumulationA.texture;

    while (!WindowShouldClose())
    {
        // Mettre à jour la caméra
        UpdateFreeCamera(&camera);

        // Réinitialiser l'accumulation si la caméra bouge
        if (CameraHasChanged(camera, previousCamera))
        {
            sampleCount = 0;
            readA = true;
            ClearAccumulation(accumulationA, accumulationB);
        }

        previousCamera = camera;

        // Choisir texture source et texture destination
        RenderTexture2D& previousFrame = readA ? accumulationA : accumulationB;
        RenderTexture2D& nextFrame = readA ? accumulationB : accumulationA;


        // Envoyer les autres uniformes
        SendCameraUniforms(shader, uniforms, camera, width, height, sampleCount);

        BeginTextureMode(nextFrame);
        ClearBackground(BLACK);

        BeginShaderMode(shader);

        // IMPORTANT : déclarer la texture précédente ici
        SetShaderValueTexture(
            shader,
            uniforms.previousFrame,
            previousFrame.texture
        );

        DrawRectangle(0, 0, width, height, WHITE);

        EndShaderMode();
        EndTextureMode();

        // Afficher le résultat calculé
        lastRenderedTexture = nextFrame.texture;

        // Préparer l'échantillon suivant
        readA = !readA;
        sampleCount++;

        // Afficher le résultat à l'écran
        BeginDrawing();
        ClearBackground(BLACK);

        // Les RenderTexture2D de raylib sont affichées à l'envers
        // verticalement : source.height négatif pour corriger cela.
        Rectangle source = {0.0f, 0.0f, static_cast<float>(lastRenderedTexture.width), -static_cast<float>(lastRenderedTexture.height)};
        DrawTextureRec(lastRenderedTexture, source, { 0.0f, 0.0f }, WHITE);

        DrawFPS(10, 10);
        DrawText(TextFormat("Echantillons : %d", sampleCount), 10, 35, 20, GREEN);
        DrawText("WASD + souris : deplacement camera | Echap : quitter", 10, height - 28, 16, LIGHTGRAY);

        EndDrawing();
    }

    // Nettoyage
    UnloadRenderTexture(accumulationA);
    UnloadRenderTexture(accumulationB);
    UnloadShader(shader);

    CloseWindow();
    return 0;
}
