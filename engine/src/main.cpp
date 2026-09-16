#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <cstdio>
#include <vector>

#include "camera.h"
#include "mat4.h"
#include "entities.h"
#include "player.h"
#include "assets/bsp.h"
#include "assets/mdl.h"
#include "render/render.h"
#include "ui/ui.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <map.bsp> <wad_dir> [viewmodel.mdl] [screenshot_out.bmp]\n", argv[0]);
        return 1;
    }
    std::string mapPath = argv[1];
    std::string wadDir = argv[2];
    std::string viewModelPath = argc >= 4 ? argv[3] : "";
    std::string screenshotPath = argc >= 5 ? argv[4] : "";

    BspMap map;
    if (!map.load(mapPath, {wadDir})) {
        std::fprintf(stderr, "failed to load map: %s\n", mapPath.c_str());
        return 1;
    }
    std::printf("loaded map: %zu faces, %zu textures\n", map.faces().size(), map.textures().size());

    MdlModel viewModel;
    bool hasViewModel = false;
    if (!viewModelPath.empty()) {
        hasViewModel = viewModel.load(viewModelPath);
        if (!hasViewModel) {
            std::fprintf(stderr, "failed to load view model: %s (continuing without it)\n", viewModelPath.c_str());
        } else {
            std::printf("loaded view model: %zu triangles, %zu textures\n", viewModel.triangles().size(), viewModel.textures().size());
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    const int kWidth = 1280, kHeight = 720;
    SDL_Window* window = SDL_CreateWindow(
        "cs15engine",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kWidth, kHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetSwapInterval(1); // vsync
    SDL_SetRelativeMouseMode(SDL_TRUE);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    std::vector<GLuint> texIds = uploadTextures(map.textures());
    std::vector<GLuint> viewModelTexIds;
    if (hasViewModel) viewModelTexIds = uploadTextures(viewModel.textures());

    EntitySystem entities;
    entities.build(map);
    std::printf("entities: %zu spawns (CT/T), %zu bomb targets, %zu buy zones\n",
                entities.spawns.size(), entities.bombTargets.size(), entities.buyZones.size());

    Camera camera;
    {
        // Prefer a Counter-Terrorist spawn (matches the historical
        // info_player_start = CT convention), fall back to any spawn point,
        // or the map origin if the entity lump had none at all.
        const SpawnPoint* chosen = nullptr;
        for (const auto& sp : entities.spawns) {
            if (sp.team == Team::CT) { chosen = &sp; break; }
        }
        if (!chosen && !entities.spawns.empty()) chosen = &entities.spawns[0];
        if (chosen) {
            camera.x = chosen->origin.x;
            camera.y = chosen->origin.y;
            camera.z = chosen->origin.z; // feet/origin, matching the hull collision test point
            camera.yaw = chosen->yaw;
        }
    }

    Uint64 lastTicks = SDL_GetPerformanceCounter();
    bool running = true;

    PlayerState player;
    bool spaceWasDown = false;

    // --- Weapon test state ---
    constexpr int kMagazineSize = 30;
    int ammoInMag = kMagazineSize;
    bool mouseWasDown = false;
    std::vector<Vec3> impactMarks;
    constexpr size_t kMaxImpactMarks = 64;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r) {
                ammoInMag = kMagazineSize;
            } else if (event.type == SDL_MOUSEMOTION) {
                camera.look((float)event.motion.xrel, (float)event.motion.yrel);
            }
        }

        Uint64 nowTicks = SDL_GetPerformanceCounter();
        float dt = (float)(nowTicks - lastTicks) / (float)SDL_GetPerformanceFrequency();
        lastTicks = nowTicks;

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        float forward = 0.0f, strafe = 0.0f;
        if (keys[SDL_SCANCODE_W]) forward += 1.0f;
        if (keys[SDL_SCANCODE_S]) forward -= 1.0f;
        if (keys[SDL_SCANCODE_D]) strafe += 1.0f;
        if (keys[SDL_SCANCODE_A]) strafe -= 1.0f;
        bool spaceDown = keys[SDL_SCANCODE_SPACE];
        bool jumpPressed = spaceDown && !spaceWasDown;
        spaceWasDown = spaceDown;

        playerStep(camera, player, map, PlayerInput{forward, strafe, jumpPressed}, dt);

#ifdef CS15_DEBUG_PHYSICS
        static float debugTimer = 0.0f;
        debugTimer += dt;
        if (debugTimer >= 0.5f) {
            debugTimer = 0.0f;
            std::fprintf(stderr, "t=%.1f z=%.2f velZ=%.1f grounded=%d\n",
                         (float)SDL_GetTicks() / 1000.0f, camera.z, player.velocityZ, player.grounded);
        }
#endif

        glViewport(0, 0, kWidth, kHeight);
        glClearColor(0.4f, 0.6f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Mat4 proj = perspective(90.0f, (float)kWidth / kHeight, 4.0f, 8192.0f);

        constexpr float kEyeHeight = 64.0f; // eye offset above the collision origin

        float yawRad = camera.yaw * 3.14159265f / 180.0f;
        float pitchRad = camera.pitch * 3.14159265f / 180.0f;
        Vec3f eye{camera.x, camera.y, camera.z + kEyeHeight};
        Vec3f forwardDir{
            std::cos(yawRad) * std::cos(pitchRad),
            std::sin(yawRad) * std::cos(pitchRad),
            std::sin(pitchRad)
        };
        Vec3f center{eye.x + forwardDir.x, eye.y + forwardDir.y, eye.z + forwardDir.z};
        Mat4 view = lookAt(eye, center, Vec3f{0, 0, 1});

        // --- Shooting: left click fires a hitscan trace, leaves an impact mark ---
        bool mouseDown = SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT);
        if (mouseDown && !mouseWasDown && ammoInMag > 0) {
            --ammoInMag;
            Vec3 traceStart{eye.x, eye.y, eye.z};
            constexpr float kRange = 4096.0f;
            Vec3 traceEnd{eye.x + forwardDir.x * kRange, eye.y + forwardDir.y * kRange, eye.z + forwardDir.z * kRange};
            Vec3 hit;
            if (map.traceLine(traceStart, traceEnd, hit)) {
                if (impactMarks.size() >= kMaxImpactMarks) impactMarks.erase(impactMarks.begin());
                impactMarks.push_back(hit);
            }
        }
        mouseWasDown = mouseDown;

        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(proj.m);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(view.m);

        drawBspFaces(map, texIds);

        // Bullet impact marks: small dark points on whatever they hit.
        glDisable(GL_TEXTURE_2D);
        glPointSize(6.0f);
        glColor3f(0.05f, 0.05f, 0.05f);
        glBegin(GL_POINTS);
        for (const auto& mark : impactMarks) glVertex3f(mark.x, mark.y, mark.z);
        glEnd();
        glColor3f(1.0f, 1.0f, 1.0f);
        glEnable(GL_TEXTURE_2D);

        if (hasViewModel) {
            // Classic FPS trick: separate (narrower) projection so the model
            // isn't fisheye-distorted at close range, and cleared depth so
            // it never clips into world geometry.
            glClear(GL_DEPTH_BUFFER_BIT);

            Mat4 viewModelProj = perspective(50.0f, (float)kWidth / kHeight, 1.0f, 512.0f);
            glMatrixMode(GL_PROJECTION);
            glLoadMatrixf(viewModelProj.m);
            glMatrixMode(GL_MODELVIEW);

            Mat4 rotOnly = lookAt(Vec3f{0, 0, 0}, forwardDir, Vec3f{0, 0, 1});
            glLoadMatrixf(rotOnly.m);
            glTranslatef(18.0f, -6.0f, -8.0f); // forward, right, down, in view-local (world-axis) units
            // View models are authored with their barrel along -Y, not along
            // the world-forward X axis used everywhere else — reorient once.
            glRotatef(90.0f, 0.0f, 0.0f, 1.0f);

            drawMdlTriangles(viewModel, viewModelTexIds);

            glMatrixMode(GL_PROJECTION);
            glLoadMatrixf(proj.m);
            glMatrixMode(GL_MODELVIEW);
        }

        // --- HUD: crosshair + ammo counter ---
        int mouseXForHud, mouseYForHud;
        SDL_GetMouseState(&mouseXForHud, &mouseYForHud);
        uiBeginFrame(mouseXForHud, mouseYForHud, false, kWidth, kHeight);
        float cx = kWidth / 2.0f, cy = kHeight / 2.0f;
        uiDrawRect(cx - 8, cy - 1, 16, 2, kColorWhite);
        uiDrawRect(cx - 1, cy - 8, 2, 16, kColorWhite);
        char ammoStr[32];
        std::snprintf(ammoStr, sizeof(ammoStr), "%d / %d", ammoInMag, kMagazineSize);
        uiDrawText(kWidth - uiTextWidth(ammoStr, 2.5f) - 24, kHeight - 48, ammoStr, kColorWhite, 2.5f);

        // Zone indicators, driven by the real func_bomb_target/func_buyzone
        // brush bounds parsed from the map's entity lump.
        Vec3 feet{camera.x, camera.y, camera.z};
        bool inBombsite = false, inBuyzone = false;
        for (const auto& zone : entities.bombTargets) if (pointInZone(zone, feet)) inBombsite = true;
        for (const auto& zone : entities.buyZones) if (pointInZone(zone, feet)) inBuyzone = true;
        if (inBombsite) {
            const char* msg = "BOMBSITE";
            uiDrawText((kWidth - uiTextWidth(msg, 2.0f)) / 2.0f, 24, msg, Color{1.0f, 0.3f, 0.2f, 1.0f}, 2.0f);
        }
        if (inBuyzone) {
            const char* msg = "BUY ZONE";
            uiDrawText((kWidth - uiTextWidth(msg, 2.0f)) / 2.0f, 48, msg, Color{0.3f, 0.8f, 1.0f, 1.0f}, 2.0f);
        }

        uiEndFrame();

        SDL_GL_SwapWindow(window);

        if (!screenshotPath.empty()) {
            saveScreenshotBMP(screenshotPath, kWidth, kHeight);
            running = false; // one-shot verification run
        }
    }

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
