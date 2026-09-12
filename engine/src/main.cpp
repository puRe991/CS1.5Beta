#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "camera.h"
#include "mat4.h"
#include "entities.h"
#include "game_state.h"
#include "weapons.h"
#include "assets/bsp.h"
#include "assets/mdl.h"
#include "ui/ui.h"
#include "render/glext.h"
#include "render/shader.h"
#include "render/world_mesh.h"
#include "render/skybox.h"
#include "render/particles.h"
#include "render/decals.h"

namespace {
const char* kWorldVertexShader = R"(#version 120
attribute vec3 aPos;
attribute vec2 aTexCoord;
attribute vec2 aLightmapCoord;
varying vec2 vTexCoord;
varying vec2 vLightmapCoord;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vTexCoord = aTexCoord;
    vLightmapCoord = aLightmapCoord;
}
)";

const char* kWorldFragmentShader = R"(#version 120
varying vec2 vTexCoord;
varying vec2 vLightmapCoord;
uniform sampler2D uTexture;
uniform sampler2D uLightmap;
void main() {
    vec4 base = texture2D(uTexture, vTexCoord);
    vec3 light = texture2D(uLightmap, vLightmapCoord).rgb;
    gl_FragColor = vec4(base.rgb * light, base.a);
}
)";

const char* kSkyVertexShader = R"(#version 120
attribute vec3 aPos;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vTexCoord = aTexCoord;
}
)";

const char* kSkyFragmentShader = R"(#version 120
varying vec2 vTexCoord;
uniform sampler2D uTexture;
void main() {
    gl_FragColor = texture2D(uTexture, vTexCoord);
}
)";
} // namespace

namespace {

GLuint uploadTextureRGBA(const uint8_t* rgba, uint32_t width, uint32_t height) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    if (rgba) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    } else {
        // Missing texture (e.g. a base WAD this map depends on isn't
        // present): a magenta/black checkerboard sized to the texture's
        // own declared dimensions, not a fixed tiny swatch — a 2x2
        // placeholder tiled with GL_REPEAT across a large face blows up
        // into giant, screen-filling blocks that read as a rendering bug
        // rather than a missing-asset indicator. Sized properly, it tiles
        // at roughly the same visual density a real texture would.
        uint32_t w = width > 0 ? width : 16;
        uint32_t h = height > 0 ? height : 16;
        std::vector<uint8_t> pixels((size_t)w * h * 4);
        constexpr uint32_t kCell = 8;
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                bool dark = ((x / kCell) + (y / kCell)) % 2 == 0;
                size_t p = ((size_t)y * w + x) * 4;
                pixels[p + 0] = dark ? 0 : 255;
                pixels[p + 1] = 0;
                pixels[p + 2] = dark ? 0 : 255;
                pixels[p + 3] = 255;
            }
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    }
    return id;
}

GLuint uploadTexture(const BspTexture& tex) {
    return uploadTextureRGBA(tex.rgba.empty() ? nullptr : tex.rgba.data(), tex.width, tex.height);
}

GLuint uploadTexture(const MdlTexture& tex) {
    return uploadTextureRGBA(tex.rgba.empty() ? nullptr : tex.rgba.data(), tex.width, tex.height);
}

Vec3f parseOrigin(const std::string& s) {
    Vec3f v{0, 0, 0};
    std::sscanf(s.c_str(), "%f %f %f", &v.x, &v.y, &v.z);
    return v;
}

void drawTexturedQuad(GLuint tex, float x, float y, float w, float h) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(1, 0); glVertex2f(x + w, y);
    glTexCoord2f(1, 1); glVertex2f(x + w, y + h);
    glTexCoord2f(0, 1); glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void drawMdlTriangles(const std::vector<MdlTriangle>& triangles, const std::vector<MdlTexture>& textures,
                       const std::vector<GLuint>& texIds) {
    GLuint currentTex = (GLuint)-1;
    glBegin(GL_TRIANGLES);
    for (const auto& tri : triangles) {
        GLuint texId = (tri.textureIndex >= 0 && (size_t)tri.textureIndex < texIds.size()) ? texIds[tri.textureIndex] : 0;
        if (texId != currentTex) {
            glEnd();
            glBindTexture(GL_TEXTURE_2D, texId);
            currentTex = texId;
            glBegin(GL_TRIANGLES);
        }
        float texW = 64, texH = 64;
        if (tri.textureIndex >= 0 && (size_t)tri.textureIndex < textures.size()) {
            texW = (float)textures[tri.textureIndex].width;
            texH = (float)textures[tri.textureIndex].height;
        }
        for (const MdlVertex* v : {&tri.a, &tri.b, &tri.c}) {
            glTexCoord2f(v->u / texW, v->v / texH);
            glVertex3f(v->x, v->y, v->z);
        }
    }
    glEnd();
}

} // namespace

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

    // Radar: the real per-map overview image CS ships (cstrike/overviews/<map>.bmp).
    // World-space bounds come from our own BSP parse (worldspawn's model bounds),
    // not from the overview's own zoom/origin metadata — that sidesteps needing to
    // reverse-engineer the original engine's exact (undocumented) scale constant,
    // and still self-consistently places the player dot on the real map image.
    std::string mapBaseName = mapPath;
    if (size_t slash = mapBaseName.find_last_of("/\\"); slash != std::string::npos) mapBaseName = mapBaseName.substr(slash + 1);
    if (size_t dot = mapBaseName.find_last_of('.'); dot != std::string::npos) mapBaseName = mapBaseName.substr(0, dot);
    std::string radarPath = wadDir + "/overviews/" + mapBaseName + ".bmp";
    SDL_Surface* radarSurfaceRaw = SDL_LoadBMP(radarPath.c_str());
    bool hasRadar = false;
    Vec3 worldMins{0, 0, 0}, worldMaxs{0, 0, 0};
    if (!map.models().empty()) {
        worldMins = map.models()[0].mins;
        worldMaxs = map.models()[0].maxs;
    }

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

    if (!loadGLExtensions()) {
        std::fprintf(stderr, "failed to load GL shader/VBO extensions\n");
        return 1;
    }
    Shader worldShader;
    if (!worldShader.load(kWorldVertexShader, kWorldFragmentShader)) {
        std::fprintf(stderr, "failed to build world shader\n");
        return 1;
    }
    Shader skyShader;
    if (!skyShader.load(kSkyVertexShader, kSkyFragmentShader)) {
        std::fprintf(stderr, "failed to build sky shader\n");
        return 1;
    }

    Skybox skybox;
    for (const auto& ent : map.entities()) {
        const std::string* classname = ent.get("classname");
        if (classname && *classname == "worldspawn") {
            const std::string* skyname = ent.get("skyname");
            if (skyname && skybox.load(wadDir, *skyname)) {
                std::printf("loaded skybox: %s\n", skyname->c_str());
            } else if (skyname) {
                std::printf("skybox '%s' not found (continuing without one)\n", skyname->c_str());
            }
            break;
        }
    }

    std::vector<GLuint> texIds;
    texIds.reserve(map.textures().size());
    for (const auto& tex : map.textures()) texIds.push_back(uploadTexture(tex));

    WorldMesh worldMesh;
    worldMesh.build(map, texIds);

    GLuint radarTexId = 0;
    if (radarSurfaceRaw) {
        SDL_Surface* rgba = SDL_ConvertSurfaceFormat(radarSurfaceRaw, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(radarSurfaceRaw);
        if (rgba) {
            radarTexId = uploadTextureRGBA((const uint8_t*)rgba->pixels, rgba->w, rgba->h);
            hasRadar = true;
            SDL_FreeSurface(rgba);
        }
    } else {
        std::printf("no radar overview found at %s (continuing without one)\n", radarPath.c_str());
    }

    std::vector<GLuint> viewModelTexIds;
    if (hasViewModel) {
        viewModelTexIds.reserve(viewModel.textures().size());
        for (const auto& tex : viewModel.textures()) viewModelTexIds.push_back(uploadTexture(tex));
    }

    // --- View model animation: idle/draw/shoot/reload sequences, played
    // back via MdlModel::pose() instead of the static bind pose. Sequence
    // indices are re-looked-up whenever the model changes (equipWeapon);
    // any that's missing from a given model just stays -1 and that state
    // silently falls back to whatever's already posed (or the bind pose,
    // if none of them exist).
    struct ViewAnimSeqs {
        int idle = -1, draw = -1, shoot = -1, reload = -1;
    };
    enum class ViewAnimState { Idle, Draw, Shoot, Reload };
    ViewAnimSeqs viewSeqs;
    ViewAnimState viewAnimState = ViewAnimState::Idle;
    float viewAnimTime = 0.0f;
    auto lookUpViewAnimSeqs = [&]() {
        viewSeqs.idle = viewModel.findSequence("idle1");
        viewSeqs.draw = viewModel.findSequence("draw");
        viewSeqs.shoot = viewModel.findSequence("shoot1");
        viewSeqs.reload = viewModel.findSequence("reload");
        viewAnimState = viewSeqs.draw >= 0 ? ViewAnimState::Draw : ViewAnimState::Idle;
        viewAnimTime = 0.0f;
    };
    if (hasViewModel) lookUpViewAnimSeqs();

    // Swaps the equipped weapon: reloads the view model and its textures,
    // freeing the previous ones. Used by the buy menu.
    auto equipWeapon = [&](const std::string& modelPath) {
        for (GLuint t : viewModelTexIds) glDeleteTextures(1, &t);
        viewModelTexIds.clear();
        hasViewModel = viewModel.load(modelPath);
        if (hasViewModel) {
            viewModelTexIds.reserve(viewModel.textures().size());
            for (const auto& tex : viewModel.textures()) viewModelTexIds.push_back(uploadTexture(tex));
            lookUpViewAnimSeqs();
        }
        return hasViewModel;
    };

    // Particle effects: muzzle flash, bullet-impact smoke, explosion — real
    // GoldSrc .spr sprites, loaded once and reused for every spawn. Each
    // load can fail gracefully (loadEffect returns -1, spawn() then no-ops)
    // so a missing sprites/ directory just means no effects, not a crash.
    ParticleSystem particles;
    int fxMuzzleFlash = particles.loadEffect(wadDir + "/sprites/muzzleflash2.spr");
    int fxSmoke = particles.loadEffect(wadDir + "/sprites/smokepuff.spr");
    int fxExplosion = particles.loadEffect(wadDir + "/sprites/fexplo.spr");

    // Decals: bullet holes and blood, using the real GoldSrc decal
    // textures (decals.wad's "{shot*"/"{blood*" lumps — a grayscale ramp
    // used as an alpha mask, tinted here since the palette carries no
    // actual color) instead of the plain dark dot this replaces. A few
    // variants each, picked randomly per spawn like the original engine.
    DecalSystem decals;
    std::vector<int> bulletHoleDecals = decals.loadGroup(wadDir + "/decals.wad",
        {"{shot1", "{shot2", "{shot3", "{shot4", "{shot5"}, 20, 20, 20);
    std::vector<int> bloodDecals = decals.loadGroup(wadDir + "/decals.wad",
        {"{blood1", "{blood2", "{blood3", "{blood4", "{blood5", "{blood6"}, 90, 4, 4);

    EntitySystem entities;
    entities.build(map);
    std::printf("entities: %zu spawns (CT/T), %zu bomb targets, %zu buy zones\n",
                entities.spawns.size(), entities.bombTargets.size(), entities.buyZones.size());

    // Player joins CT by default (matches the historical info_player_start =
    // CT convention). 'N' switches teams at runtime for testing, since
    // there's no team-select screen or other players to balance against yet.
    PlayerState player;

    Camera camera;
    {
        Vec3 origin{0, 0, 0};
        float yaw = 0.0f;
        pickSpawnForTeam(entities, player.team, origin, yaw);
        camera.x = origin.x;
        camera.y = origin.y;
        camera.z = origin.z; // feet/origin, matching the hull collision test point
        camera.yaw = yaw;
    }

    // --- Third-person player body: a real MDL player skin (urban/terror),
    // animated with the same pose()/sequence system as the view model, so
    // that system actually drives a renderer instead of only ever being
    // exercised by the first-person weapon. 'V' toggles the view; the
    // model reloads whenever the team changes (matching the historical
    // CT=urban/T=terror skins — no team-select or per-player skin system
    // exists yet, so this is the one body per side).
    MdlModel playerBodyModel;
    std::vector<GLuint> playerBodyTexIds;
    bool hasPlayerBodyModel = false;
    bool thirdPerson = false;
    int bodyIdleSeq = -1, bodyRunSeq = -1;
    float bodyAnimTime = 0.0f;
    auto equipPlayerBodyModel = [&](Team team) {
        for (GLuint t : playerBodyTexIds) glDeleteTextures(1, &t);
        playerBodyTexIds.clear();
        std::string path = wadDir + "/models/player/" + (team == Team::CT ? "urban/urban.mdl" : "terror/terror.mdl");
        hasPlayerBodyModel = playerBodyModel.load(path);
        if (hasPlayerBodyModel) {
            playerBodyTexIds.reserve(playerBodyModel.textures().size());
            for (const auto& tex : playerBodyModel.textures()) playerBodyTexIds.push_back(uploadTexture(tex));
            bodyIdleSeq = playerBodyModel.findSequence("idle1");
            bodyRunSeq = playerBodyModel.findSequence("run");
        }
        return hasPlayerBodyModel;
    };
    equipPlayerBodyModel(player.team);

    Uint64 lastTicks = SDL_GetPerformanceCounter();
    bool running = true;

    // --- Player physics state ---
    constexpr float kGravity = 800.0f;   // units/sec^2, Source-ish
    constexpr float kJumpSpeed = 300.0f; // units/sec, initial upward velocity
    float velocityZ = 0.0f;
    bool grounded = false;
    bool spaceWasDown = false;

    // --- Weapon test state ---
    constexpr int kMagazineSize = 30;
    int ammoInMag = kMagazineSize;
    bool mouseWasDown = false;

    // --- Damage flash (screen reddens briefly when hurt) ---
    int lastHealth = kMaxHealth;
    float damageFlashTimer = 0.0f;
    constexpr float kDamageFlashDuration = 0.35f;

    // --- Muzzle flash, drawn at the view model's real attachment point ---
    constexpr float kMuzzleFlashDuration = 0.05f;
    float muzzleFlashTimer = 0.0f;

    // --- Health/death/respawn + round loop ---
    RoundState round;
    bool bombExplosionSpawned = false; // guards the one-shot explosion particle per detonation
    bool hWasDown = false; // 'H' is a debug key to test damage/death without needing fall damage
    bool nWasDown = false; // 'N' is a debug key to test team switching
    std::srand((unsigned)SDL_GetTicks());

    // --- Buy menu ---
    bool buyMenuOpen = false;
    bool bWasDown = false;
    std::string currentWeaponName = "AK47";

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                if (buyMenuOpen) {
                    buyMenuOpen = false;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                } else {
                    running = false;
                }
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r) {
                ammoInMag = kMagazineSize;
                if (viewSeqs.reload >= 0) {
                    viewAnimState = ViewAnimState::Reload;
                    viewAnimTime = 0.0f;
                }
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_v) {
                thirdPerson = !thirdPerson;
            } else if (event.type == SDL_MOUSEMOTION && !buyMenuOpen) {
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
        bool jumpPressed = spaceDown && !spaceWasDown && player.alive;
        spaceWasDown = spaceDown;

        bool hDown = keys[SDL_SCANCODE_H];
        if (hDown && !hWasDown) {
            damagePlayer(player, 25); // debug key: test damage/death/respawn
            // No hittable NPCs exist yet to splatter blood near, so this
            // demonstrates the decal on the one damageable thing there is:
            // a splash on the floor at the player's own feet.
            decals.spawn(bloodDecals, Vec3f{camera.x, camera.y, camera.z}, Vec3f{0, 0, 1}, 10.0f);
        }
        hWasDown = hDown;

        bool nDown = keys[SDL_SCANCODE_N];
        if (nDown && !nWasDown) {
            // Debug key: switch team and teleport to a spawn of the new
            // team, since there's no team-select UI or other players yet.
            player.team = (player.team == Team::CT) ? Team::T : Team::CT;
            equipPlayerBodyModel(player.team);
            Vec3 origin{0, 0, 0};
            float yaw = 0.0f;
            pickSpawnForTeam(entities, player.team, origin, yaw);
            camera.x = origin.x;
            camera.y = origin.y;
            camera.z = origin.z;
            camera.yaw = yaw;
            camera.pitch = 0.0f;
            velocityZ = 0.0f;
            player.health = kMaxHealth;
            player.alive = true;
        }
        nWasDown = nDown;

        if (!player.alive || buyMenuOpen) { forward = 0.0f; strafe = 0.0f; }

        float dx, dy, dzUnused;
        camera.wishDelta(forward, strafe, 0.0f, dt, dx, dy, dzUnused);

        // Resolve X/Y independently against the map's player hull so
        // movement slides along walls instead of stopping dead on contact.
        Vec3 candidate{camera.x, camera.y, camera.z};
        candidate.x += dx;
        if (map.pointInSolid(candidate)) candidate.x = camera.x;
        candidate.y += dy;
        if (map.pointInSolid(candidate)) candidate.y = camera.y;

        // Ground check: probe just below the resolved feet position.
        Vec3 groundProbe = candidate;
        groundProbe.z -= 2.0f;
        grounded = map.pointInSolid(groundProbe);

        if (jumpPressed && grounded) {
            velocityZ = kJumpSpeed;
            grounded = false;
        } else if (grounded && velocityZ <= 0.0f) {
            velocityZ = 0.0f;
        } else {
            velocityZ -= kGravity * dt;
        }

        candidate.z += velocityZ * dt;
        if (map.pointInSolid(candidate)) {
            if (velocityZ < 0.0f) {
                grounded = true;
                // Fall damage: rough approximation of the classic engines'
                // formula (only speeds past a threshold hurt, then it scales
                // with how far past that threshold you were).
                constexpr float kFallDamageThreshold = 500.0f; // units/sec
                constexpr float kFallDamageScale = 0.15f;
                if (-velocityZ > kFallDamageThreshold) {
                    int dmg = (int)((-velocityZ - kFallDamageThreshold) * kFallDamageScale);
                    if (dmg > 0) damagePlayer(player, dmg);
                }
            }
            velocityZ = 0.0f;
            candidate.z = camera.z; // cancel this step's vertical move, snap to prior floor/ceiling
        }

        camera.x = candidate.x;
        camera.y = candidate.y;
        camera.z = candidate.z;

        Vec3 respawnOrigin;
        float respawnYaw = 0.0f;
        if (updateRound(round, player, entities, dt, respawnOrigin, respawnYaw)) {
            camera.x = respawnOrigin.x;
            camera.y = respawnOrigin.y;
            camera.z = respawnOrigin.z;
            camera.yaw = respawnYaw;
            camera.pitch = 0.0f;
            velocityZ = 0.0f;
            ammoInMag = kMagazineSize;
            bombExplosionSpawned = false; // new round: allow the next detonation to spawn its effect
        }
        if (round.endReason == "BOMB_EXPLODED" && !bombExplosionSpawned) {
            bombExplosionSpawned = true;
            particles.spawn(fxExplosion,
                             Vec3f{(float)round.bombPosition.x, (float)round.bombPosition.y, (float)round.bombPosition.z},
                             1.5f, 1.2f);
        }

        if (player.health < lastHealth) damageFlashTimer = kDamageFlashDuration;
        lastHealth = player.health;
        if (damageFlashTimer > 0.0f) damageFlashTimer -= dt;
        if (muzzleFlashTimer > 0.0f) muzzleFlashTimer -= dt;
        particles.update(dt);
        viewAnimTime += dt;
        bodyAnimTime += dt;

        // Zone checks, driven by the real func_bomb_target/func_buyzone
        // brush bounds parsed from the map's entity lump.
        Vec3 feet{camera.x, camera.y, camera.z};
        bool inBombsite = false, inBuyzone = false;
        for (const auto& zone : entities.bombTargets) if (pointInZone(zone, feet)) inBombsite = true;
        for (const auto& zone : entities.buyZones) if (pointInZone(zone, feet)) inBuyzone = true;

        bool bDown = keys[SDL_SCANCODE_B];
        if (bDown && !bWasDown) {
            if (buyMenuOpen) {
                buyMenuOpen = false;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            } else if (inBuyzone && player.alive && round.phase == RoundPhase::Live) {
                buyMenuOpen = true;
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
        }
        bWasDown = bDown;

        // Plant/defuse: hold E. Progress resets the instant you let go, move
        // out of range, or aren't on the right team — no partial carry-over.
        bool eHeld = keys[SDL_SCANCODE_E] && player.alive && round.phase == RoundPhase::Live && !buyMenuOpen;

        bool planting = eHeld && player.team == Team::T && !round.bombPlanted && inBombsite;
        if (planting) {
            round.plantProgress += dt;
            if (round.plantProgress >= kPlantDuration) {
                round.bombPlanted = true;
                round.bombTimer = kBombTimerDuration;
                round.bombPosition = feet;
                round.plantProgress = 0.0f;
            }
        } else {
            round.plantProgress = 0.0f;
        }

        float bombDist = round.bombPlanted
            ? std::sqrt((feet.x - round.bombPosition.x) * (feet.x - round.bombPosition.x) +
                        (feet.y - round.bombPosition.y) * (feet.y - round.bombPosition.y) +
                        (feet.z - round.bombPosition.z) * (feet.z - round.bombPosition.z))
            : 1e9f;
        bool defusing = eHeld && player.team == Team::CT && round.bombPlanted && bombDist <= kDefuseRadius;
        if (defusing) {
            round.defuseProgress += dt;
            if (round.defuseProgress >= kDefuseDuration) {
                endRound(round, player, "BOMB_DEFUSED");
                round.defuseProgress = 0.0f;
            }
        } else {
            round.defuseProgress = 0.0f;
        }

#ifdef CS15_DEBUG_PHYSICS
        static float debugTimer = 0.0f;
        debugTimer += dt;
        if (debugTimer >= 0.5f) {
            debugTimer = 0.0f;
            std::fprintf(stderr, "t=%.1f x=%.1f y=%.1f z=%.2f hp=%d alive=%d phase=%d round=%d team=%s inBombsite=%d planted=%d bombT=%.1f plantP=%.1f defP=%.1f ctScore=%d tScore=%d endReason=%s\n",
                         (float)SDL_GetTicks() / 1000.0f, camera.x, camera.y, camera.z,
                         player.health, player.alive, (int)round.phase, round.roundNumber,
                         player.team == Team::CT ? "CT" : "T", inBombsite,
                         round.bombPlanted, round.bombTimer, round.plantProgress, round.defuseProgress,
                         round.ctScore, round.tScore,
                         round.endReason.c_str());
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
        // Third person: a simple chase camera pulled back behind the
        // player along the same look direction (no collision against
        // world geometry yet, so it can clip into walls in tight spots).
        Vec3f viewEye = eye;
        if (thirdPerson) {
            constexpr float kChaseDist = 60.0f;
            viewEye = Vec3f{eye.x - forwardDir.x * kChaseDist,
                            eye.y - forwardDir.y * kChaseDist,
                            eye.z - forwardDir.z * kChaseDist + 20.0f};
        }
        Mat4 view = lookAt(viewEye, thirdPerson ? eye : center, Vec3f{0, 0, 1});

        // --- Shooting: left click fires a hitscan trace, leaves an impact mark ---
        bool mouseDown = SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT);
        if (mouseDown && !mouseWasDown && ammoInMag > 0 && player.alive && !buyMenuOpen) {
            --ammoInMag;
            muzzleFlashTimer = kMuzzleFlashDuration;
            if (viewSeqs.shoot >= 0) {
                viewAnimState = ViewAnimState::Shoot;
                viewAnimTime = 0.0f;
            }
            // World-space muzzle flash particle, a little in front of the
            // eye along the aim direction — there's no true world-space
            // muzzle attachment point available here (the view model is
            // drawn in its own screen-locked overlay pass, not as a real
            // world object), so this is a close approximation rather than
            // the exact barrel tip.
            particles.spawn(fxMuzzleFlash,
                             Vec3f{eye.x + forwardDir.x * 20.0f, eye.y + forwardDir.y * 20.0f, eye.z + forwardDir.z * 20.0f},
                             0.15f, 0.06f);
            Vec3 traceStart{eye.x, eye.y, eye.z};
            constexpr float kRange = 4096.0f;
            Vec3 traceEnd{eye.x + forwardDir.x * kRange, eye.y + forwardDir.y * kRange, eye.z + forwardDir.z * kRange};
            Vec3 hit;
            Vec3 hitNormal;
            if (map.traceLine(traceStart, traceEnd, hit, &hitNormal)) {
                particles.spawn(fxSmoke, Vec3f{(float)hit.x, (float)hit.y, (float)hit.z}, 0.35f, 0.6f);
                decals.spawn(bulletHoleDecals, Vec3f{(float)hit.x, (float)hit.y, (float)hit.z},
                             Vec3f{(float)hitNormal.x, (float)hitNormal.y, (float)hitNormal.z}, 6.0f);
            }
        }
        mouseWasDown = mouseDown;

        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(proj.m);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(view.m);

        // Skybox first, depth test off so it can't occlude (or be occluded
        // out of order by) anything, then re-enabled for the world.
        if (skybox.valid()) {
            constexpr float kSkyboxHalfSize = 4000.0f;
            Mat4 skyModel = multiply(translate(eye), scale(kSkyboxHalfSize));
            Mat4 skyMvp = multiply(multiply(proj, view), skyModel);
            glDisable(GL_DEPTH_TEST);
            skyShader.use();
            skyShader.setMat4("uMVP", skyMvp);
            skyShader.setInt("uTexture", 0);
            skybox.draw(skyShader, eye);
            glUseProgram(0);
            glEnable(GL_DEPTH_TEST);
        }

        // World geometry: shader + VBO pipeline (see render/world_mesh.*).
        // Model matrix is identity — BSP face vertices are already world space.
        // PVS culling: computed from the actual render viewpoint (viewEye —
        // the chase camera's position in third person, not the player's own
        // eye), since that's genuinely what the frustum can see from.
        Mat4 mvp = multiply(proj, view);
        std::vector<bool> visibleFaces = map.computeVisibleFaces(Vec3{viewEye.x, viewEye.y, viewEye.z});
        worldShader.use();
        worldShader.setMat4("uMVP", mvp);
        worldShader.setInt("uTexture", 0);
        worldShader.setInt("uLightmap", 1);
        worldMesh.draw(worldShader, visibleFaces);
        glUseProgram(0); // back to the fixed-function pipeline for everything below

        // Decals: real bullet-hole/blood textures glued to the surfaces
        // they hit, replacing the old plain dark impact dots.
        decals.draw();

        // Particle effects: billboarded to face the real camera (not the
        // view model's screen-locked axes), same right/up construction the
        // main view matrix itself uses.
        {
            Vec3f camRight = normalize(cross(forwardDir, Vec3f{0, 0, 1}));
            Vec3f camUp = cross(camRight, forwardDir);
            particles.draw(camRight, camUp);
        }

        // Third-person player body: a real animated MDL skin instead of
        // the first-person view model overlay, drawn as an ordinary world
        // object (same proj/view already active from the world/particle
        // pass above) at the player's own position and facing.
        if (thirdPerson && hasPlayerBodyModel) {
            bool isMoving = (forward != 0.0f || strafe != 0.0f) && player.alive;
            int activeBodySeq = isMoving && bodyRunSeq >= 0 ? bodyRunSeq : bodyIdleSeq;
            std::vector<MdlTriangle> bodyTriangles;
            if (activeBodySeq >= 0) {
                const MdlSequence& seq = playerBodyModel.sequences()[activeBodySeq];
                float frame = std::fmod(bodyAnimTime * seq.fps, (float)seq.numFrames);
                bodyTriangles = playerBodyModel.pose(activeBodySeq, frame);
            } else {
                bodyTriangles = playerBodyModel.triangles();
            }

            glMatrixMode(GL_MODELVIEW);
            glLoadMatrixf(view.m);
            glTranslatef(camera.x, camera.y, camera.z);
            glRotatef(camera.yaw, 0.0f, 0.0f, 1.0f);
            drawMdlTriangles(bodyTriangles, playerBodyModel.textures(), playerBodyTexIds);
            glLoadMatrixf(view.m); // restore: undo the translate/rotate for whatever draws next
        }

        if (hasViewModel && !thirdPerson) {
            // Classic FPS trick: separate (narrower) projection so the model
            // isn't fisheye-distorted at close range, and cleared depth so
            // it never clips into world geometry.
            glClear(GL_DEPTH_BUFFER_BIT);

            Mat4 viewModelProj = perspective(50.0f, (float)kWidth / kHeight, 1.0f, 512.0f);
            glMatrixMode(GL_PROJECTION);
            glLoadMatrixf(viewModelProj.m);
            glMatrixMode(GL_MODELVIEW);

            // Fixed reference axis, NOT the player's actual forwardDir: the
            // view model is locked to the screen regardless of look
            // direction (like any FPS weapon), so this must not depend on
            // where the camera is currently facing — using forwardDir here
            // previously rotated the whole view model (including its
            // "forward" translate offset below) around the camera as the
            // player looked around, occasionally spinning it clean out of
            // the frustum.
            Mat4 rotOnly = lookAt(Vec3f{0, 0, 0}, Vec3f{1, 0, 0}, Vec3f{0, 0, 1});
            glLoadMatrixf(rotOnly.m);
            glTranslatef(18.0f, -6.0f, -8.0f); // forward, right, down, in view-local (world-axis) units
            // View models are authored with their barrel along -Y, not along
            // the world-forward X axis used everywhere else — reorient once.
            glRotatef(90.0f, 0.0f, 0.0f, 1.0f);

            // Pick the sequence for the current animation state, falling
            // back to the bind pose if the model has none of these at all
            // (e.g. a prop with no sequences) — one-shot states (draw/
            // shoot/reload) that finish revert to idle, looping ones just
            // keep advancing.
            int activeSeq = -1;
            switch (viewAnimState) {
                case ViewAnimState::Draw:   activeSeq = viewSeqs.draw; break;
                case ViewAnimState::Shoot:  activeSeq = viewSeqs.shoot; break;
                case ViewAnimState::Reload: activeSeq = viewSeqs.reload; break;
                case ViewAnimState::Idle:   activeSeq = viewSeqs.idle; break;
            }
            std::vector<MdlTriangle> posedTriangles;
            if (activeSeq >= 0) {
                const MdlSequence& seq = viewModel.sequences()[activeSeq];
                float frame = viewAnimTime * seq.fps;
                bool finished = frame >= (float)(seq.numFrames - 1);
                // Idle is always looped in-game regardless of the
                // sequence's own STUDIO_LOOPING flag — GoldSrc view-model
                // idle animations are conventionally not flagged looping
                // in the file, but the client always loops them anyway.
                if (seq.looping || viewAnimState == ViewAnimState::Idle) {
                    frame = std::fmod(frame, (float)seq.numFrames);
                } else if (finished && viewAnimState != ViewAnimState::Idle) {
                    // One-shot animation done: drop back to idle (or the
                    // bind pose, if this model has no idle sequence).
                    viewAnimState = ViewAnimState::Idle;
                    viewAnimTime = 0.0f;
                    activeSeq = viewSeqs.idle;
                    frame = 0.0f;
                } else {
                    frame = std::min(frame, (float)(seq.numFrames - 1));
                }
                posedTriangles = (activeSeq >= 0) ? viewModel.pose(activeSeq, frame) : viewModel.triangles();
            } else {
                posedTriangles = viewModel.triangles();
            }
            drawMdlTriangles(posedTriangles, viewModel.textures(), viewModelTexIds);

            // Muzzle flash: drawn at the view model's own attachment point
            // (attachment 0 — GoldSrc models rarely name these, they're
            // referenced by index) using whatever transform is already on
            // the matrix stack, so it lines up with the barrel exactly as
            // rendered without needing to re-derive a world-space position.
            if (muzzleFlashTimer > 0.0f && !viewModel.attachments().empty()) {
                // Drawn as a small billboard quad (facing the camera, since
                // this is the rotation-only view-model matrix stack) rather
                // than a GL_POINTS sprite — point-size support is spotty
                // across GL implementations, a quad renders everywhere.
                const MdlAttachment& muzzle = viewModel.attachments()[0];
                constexpr float kFlashRadius = 6.0f;
                glDisable(GL_TEXTURE_2D);
                glDisable(GL_DEPTH_TEST);
                glColor4f(1.0f, 0.9f, 0.4f, 1.0f);
                glBegin(GL_TRIANGLE_FAN);
                glVertex3f(muzzle.x, muzzle.y - kFlashRadius, muzzle.z - kFlashRadius);
                glVertex3f(muzzle.x, muzzle.y + kFlashRadius, muzzle.z - kFlashRadius);
                glVertex3f(muzzle.x, muzzle.y + kFlashRadius, muzzle.z + kFlashRadius);
                glVertex3f(muzzle.x, muzzle.y - kFlashRadius, muzzle.z + kFlashRadius);
                glEnd();
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_TEXTURE_2D);
            }

            glMatrixMode(GL_PROJECTION);
            glLoadMatrixf(proj.m);
            glMatrixMode(GL_MODELVIEW);
        }

        // --- HUD: crosshair + ammo counter ---
        int mouseXForHud, mouseYForHud;
        SDL_GetMouseState(&mouseXForHud, &mouseYForHud);
        bool hudMouseDown = buyMenuOpen && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT));
        uiBeginFrame(mouseXForHud, mouseYForHud, hudMouseDown, kWidth, kHeight);

        if (damageFlashTimer > 0.0f) {
            float alpha = (damageFlashTimer / kDamageFlashDuration) * 0.4f;
            uiDrawRect(0, 0, kWidth, kHeight, Color{0.8f, 0.0f, 0.0f, alpha});
        }

        float cx = kWidth / 2.0f, cy = kHeight / 2.0f;
        uiDrawRect(cx - 8, cy - 1, 16, 2, kColorWhite);
        uiDrawRect(cx - 1, cy - 8, 2, 16, kColorWhite);
        char ammoStr[32];
        std::snprintf(ammoStr, sizeof(ammoStr), "%d / %d", ammoInMag, kMagazineSize);
        uiDrawText(kWidth - uiTextWidth(ammoStr, 2.5f) - 24, kHeight - 48, ammoStr, kColorWhite, 2.5f);
        uiDrawText(kWidth - uiTextWidth(currentWeaponName, 1.4f) - 24, kHeight - 76, currentWeaponName, Color{0.7f, 0.7f, 0.7f, 1.0f}, 1.4f);

        // Health + round timer.
        char hpStr[32];
        std::snprintf(hpStr, sizeof(hpStr), "%d HP", player.health);
        Color hpColor = player.health > 50 ? Color{0.4f, 1.0f, 0.4f, 1.0f} : Color{1.0f, 0.4f, 0.3f, 1.0f};
        uiDrawText(24, kHeight - 48, hpStr, hpColor, 2.5f);

        int timeLeft = (int)(round.phase == RoundPhase::Live ? round.timeRemaining : 0.0f);
        char timerStr[16];
        std::snprintf(timerStr, sizeof(timerStr), "%d:%02d", timeLeft / 60, timeLeft % 60);
        uiDrawText((kWidth - uiTextWidth(timerStr, 2.5f)) / 2.0f, kHeight - 48, timerStr, kColorWhite, 2.5f);

        if (round.phase == RoundPhase::Intermission) {
            std::string bigMsg = "ROUND OVER";
            Color bigColor = Color{1.0f, 0.3f, 0.2f, 1.0f};
            if (round.endReason == "DEATH") { bigMsg = "YOU DIED"; }
            else if (round.endReason == "TIME") { bigMsg = "CT WIN - TIME"; bigColor = Color{0.4f, 0.6f, 1.0f, 1.0f}; }
            else if (round.endReason == "BOMB_EXPLODED") { bigMsg = "T WIN - BOMB DETONATED"; bigColor = Color{1.0f, 0.8f, 0.3f, 1.0f}; }
            else if (round.endReason == "BOMB_DEFUSED") { bigMsg = "CT WIN - BOMB DEFUSED"; bigColor = Color{0.4f, 0.6f, 1.0f, 1.0f}; }
            float w = uiTextWidth(bigMsg, 3.0f);
            uiDrawText((kWidth - w) / 2.0f, kHeight / 2.0f - 60, bigMsg, bigColor, 3.0f);

            char nextStr[48];
            std::snprintf(nextStr, sizeof(nextStr), "ROUND %d IN %.0f...", round.roundNumber + 1, round.intermissionRemaining);
            uiDrawText((kWidth - uiTextWidth(nextStr, 2.0f)) / 2.0f, kHeight / 2.0f, nextStr, kColorWhite, 2.0f);
        }

        // Zone indicators (zones themselves were already resolved earlier,
        // before the B-key buy-menu-open check that needs them).
        if (inBombsite && !round.bombPlanted) {
            const char* msg = player.team == Team::T ? "BOMBSITE - HOLD E TO PLANT" : "BOMBSITE";
            uiDrawText((kWidth - uiTextWidth(msg, 2.0f)) / 2.0f, 24, msg, Color{1.0f, 0.3f, 0.2f, 1.0f}, 2.0f);
        }
        if (inBuyzone && !buyMenuOpen) {
            const char* msg = "BUY ZONE - PRESS B TO BUY";
            uiDrawText((kWidth - uiTextWidth(msg, 2.0f)) / 2.0f, 48, msg, Color{0.3f, 0.8f, 1.0f, 1.0f}, 2.0f);
        }

        if (round.plantProgress > 0.0f) {
            char msg[32];
            std::snprintf(msg, sizeof(msg), "PLANTING... %d%%", (int)(round.plantProgress / kPlantDuration * 100));
            uiDrawText((kWidth - uiTextWidth(msg, 2.0f)) / 2.0f, 72, msg, Color{1.0f, 0.6f, 0.2f, 1.0f}, 2.0f);
        }
        if (round.defuseProgress > 0.0f) {
            char msg[32];
            std::snprintf(msg, sizeof(msg), "DEFUSING... %d%%", (int)(round.defuseProgress / kDefuseDuration * 100));
            uiDrawText((kWidth - uiTextWidth(msg, 2.0f)) / 2.0f, 72, msg, Color{0.3f, 0.8f, 1.0f, 1.0f}, 2.0f);
        }
        if (round.bombPlanted && round.phase == RoundPhase::Live) {
            char msg[32];
            std::snprintf(msg, sizeof(msg), "BOMB: %.0fs", round.bombTimer);
            uiDrawText((kWidth - uiTextWidth(msg, 2.5f)) / 2.0f, 96, msg, Color{1.0f, 0.2f, 0.2f, 1.0f}, 2.5f);
        }

        char moneyStr[32];
        std::snprintf(moneyStr, sizeof(moneyStr), "$%d", player.money);
        uiDrawText(24, 24, moneyStr, Color{0.4f, 1.0f, 0.4f, 1.0f}, 2.0f);

        const char* teamName = player.team == Team::CT ? "COUNTER-TERRORIST" : "TERRORIST";
        Color teamColor = player.team == Team::CT ? Color{0.4f, 0.6f, 1.0f, 1.0f} : Color{1.0f, 0.8f, 0.3f, 1.0f};
        uiDrawText(kWidth / 2.0f - uiTextWidth(teamName, 1.2f) / 2.0f, kHeight - 24, teamName, teamColor, 1.2f);

        // --- Radar: real per-map overview image, player dot from our own BSP bounds ---
        {
            constexpr float kRadarSize = 200.0f;
            float rx = kWidth - kRadarSize - 16, ry = 16;
            uiDrawRect(rx - 2, ry - 2, kRadarSize + 4, kRadarSize + 4, Color{0, 0, 0, 0.6f});
            if (hasRadar) {
                drawTexturedQuad(radarTexId, rx, ry, kRadarSize, kRadarSize);
            } else {
                uiDrawRect(rx, ry, kRadarSize, kRadarSize, Color{0.15f, 0.15f, 0.15f, 0.8f});
            }

            float spanX = worldMaxs.x - worldMins.x, spanY = worldMaxs.y - worldMins.y;
            if (spanX > 1.0f && spanY > 1.0f) {
                float u = (camera.x - worldMins.x) / spanX;
                float v = 1.0f - (camera.y - worldMins.y) / spanY; // image Y grows downward
                float dotX = rx + u * kRadarSize, dotY = ry + v * kRadarSize;
                uiDrawRect(dotX - 3, dotY - 3, 6, 6, teamColor);
            }
        }

        // --- Buy menu overlay ---
        if (buyMenuOpen) {
            uiDrawRect(0, 0, kWidth, kHeight, Color{0, 0, 0, 0.6f});
            uiDrawText(24, 24, "BUY MENU", kColorWhite, 3.0f);
            char moneyBig[32];
            std::snprintf(moneyBig, sizeof(moneyBig), "MONEY: $%d", player.money);
            uiDrawText(24, 64, moneyBig, Color{0.4f, 1.0f, 0.4f, 1.0f}, 2.0f);

            float by = 120;
            for (int i = 0; i < kWeaponCatalogCount; ++i) {
                const WeaponDef& w = kWeaponCatalog[i];
                bool canAfford = player.money >= w.price;
                Color bg = canAfford ? Color{0.15f, 0.16f, 0.2f, 1.0f} : Color{0.3f, 0.15f, 0.15f, 1.0f};

                char label[64];
                std::snprintf(label, sizeof(label), "%-10s $%d", w.name, w.price);
                if (uiButton(24, by, 300, 36, label, bg) && canAfford) {
                    player.money -= w.price;
                    if (equipWeapon(wadDir + "/models/" + w.viewModel)) {
                        currentWeaponName = w.name;
                        ammoInMag = w.magazineSize;
                    }
                    buyMenuOpen = false;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }
                by += 44;
            }

            uiDrawText(24, by + 12, "ESC TO CLOSE", Color{0.6f, 0.6f, 0.6f, 1.0f}, 1.5f);
        }

        // --- Scoreboard (hold Tab) ---
        if (keys[SDL_SCANCODE_TAB]) {
            float sx = kWidth / 2.0f - 220, sy = 140, sw = 440;
            uiDrawRect(sx, sy, sw, 200, Color{0, 0, 0, 0.75f});
            uiDrawText(sx + (sw - uiTextWidth("SCOREBOARD", 2.0f)) / 2.0f, sy + 12, "SCOREBOARD", kColorWhite, 2.0f);

            char ctLine[32];
            std::snprintf(ctLine, sizeof(ctLine), "COUNTER-TERRORISTS: %d", round.ctScore);
            uiDrawText(sx + 20, sy + 56, ctLine, Color{0.4f, 0.6f, 1.0f, 1.0f}, 1.6f);

            char tLine[32];
            std::snprintf(tLine, sizeof(tLine), "TERRORISTS: %d", round.tScore);
            uiDrawText(sx + 20, sy + 88, tLine, Color{1.0f, 0.8f, 0.3f, 1.0f}, 1.6f);

            uiDrawRect(sx + 20, sy + 128, sw - 40, 1, Color{0.4f, 0.4f, 0.4f, 1.0f});

            char youLine[48];
            std::snprintf(youLine, sizeof(youLine), "YOU (%s)  $%d  %d HP", player.team == Team::CT ? "CT" : "T", player.money, player.health);
            uiDrawText(sx + 20, sy + 144, youLine, kColorWhite, 1.4f);

            char roundLine[32];
            std::snprintf(roundLine, sizeof(roundLine), "ROUND %d", round.roundNumber);
            uiDrawText(sx + 20, sy + 168, roundLine, Color{0.6f, 0.6f, 0.6f, 1.0f}, 1.4f);
        }

        uiEndFrame();

        SDL_GL_SwapWindow(window);

        if (!screenshotPath.empty()) {
            std::vector<uint8_t> pixels(kWidth * kHeight * 3);
            glReadPixels(0, 0, kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            std::vector<uint8_t> flipped(kWidth * kHeight * 3);
            for (int y = 0; y < kHeight; ++y) {
                std::memcpy(&flipped[y * kWidth * 3], &pixels[(kHeight - 1 - y) * kWidth * 3], kWidth * 3);
            }
            SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(flipped.data(), kWidth, kHeight, 24, kWidth * 3, 0x0000FF, 0x00FF00, 0xFF0000, 0);
            if (surf) SDL_SaveBMP(surf, screenshotPath.c_str());
            if (surf) SDL_FreeSurface(surf);
            running = false; // one-shot verification run
        }
    }

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
