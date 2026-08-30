#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "camera.h"
#include "mat4.h"
#include "assets/bsp.h"

namespace {

GLuint uploadTexture(const BspTexture& tex) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    if (!tex.rgba.empty()) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.rgba.data());
    } else {
        uint8_t pixels[16] = {
            255,0,255,255,  0,0,0,255,
            0,0,0,255,      255,0,255,255,
        };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }
    return id;
}

Vec3f parseOrigin(const std::string& s) {
    Vec3f v{0, 0, 0};
    std::sscanf(s.c_str(), "%f %f %f", &v.x, &v.y, &v.z);
    return v;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <map.bsp> <wad_dir>\n", argv[0]);
        return 1;
    }
    std::string mapPath = argv[1];
    std::string wadDir = argv[2];

    BspMap map;
    if (!map.load(mapPath, {wadDir})) {
        std::fprintf(stderr, "failed to load map: %s\n", mapPath.c_str());
        return 1;
    }
    std::printf("loaded map: %zu faces, %zu textures\n", map.faces().size(), map.textures().size());

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

    std::vector<GLuint> texIds;
    texIds.reserve(map.textures().size());
    for (const auto& tex : map.textures()) texIds.push_back(uploadTexture(tex));

    Camera camera;
    for (const auto& ent : map.entities()) {
        const std::string* classname = ent.get("classname");
        if (classname && (*classname == "info_player_start" || *classname == "info_player_deathmatch")) {
            const std::string* origin = ent.get("origin");
            if (origin) {
                Vec3f o = parseOrigin(*origin);
                camera.x = o.x;
                camera.y = o.y;
                camera.z = o.z; // feet/origin, matching the hull collision test point
            }
            const std::string* angle = ent.get("angle");
            if (angle) camera.yaw = (float)std::atof(angle->c_str());
            break;
        }
    }

    Uint64 lastTicks = SDL_GetPerformanceCounter();
    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            } else if (event.type == SDL_MOUSEMOTION) {
                camera.look((float)event.motion.xrel, (float)event.motion.yrel);
            }
        }

        Uint64 nowTicks = SDL_GetPerformanceCounter();
        float dt = (float)(nowTicks - lastTicks) / (float)SDL_GetPerformanceFrequency();
        lastTicks = nowTicks;

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        float forward = 0.0f, strafe = 0.0f, up = 0.0f;
        if (keys[SDL_SCANCODE_W]) forward += 1.0f;
        if (keys[SDL_SCANCODE_S]) forward -= 1.0f;
        if (keys[SDL_SCANCODE_D]) strafe += 1.0f;
        if (keys[SDL_SCANCODE_A]) strafe -= 1.0f;
        if (keys[SDL_SCANCODE_SPACE]) up += 1.0f;
        if (keys[SDL_SCANCODE_LCTRL]) up -= 1.0f;

        float dx, dy, dz;
        camera.wishDelta(forward, strafe, up, dt, dx, dy, dz);

        // Resolve each axis independently against the map's player hull so
        // movement slides along walls instead of stopping dead on contact.
        Vec3 candidate{camera.x, camera.y, camera.z};
        candidate.x += dx;
        if (map.pointInSolid(candidate)) candidate.x = camera.x;
        candidate.y += dy;
        if (map.pointInSolid(candidate)) candidate.y = camera.y;
        candidate.z += dz;
        if (map.pointInSolid(candidate)) candidate.z = camera.z;
        camera.x = candidate.x;
        camera.y = candidate.y;
        camera.z = candidate.z;

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

        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(proj.m);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(view.m);

        for (const auto& face : map.faces()) {
            GLuint texId = (face.textureIndex >= 0 && (size_t)face.textureIndex < texIds.size()) ? texIds[face.textureIndex] : 0;
            float texW = 64, texH = 64;
            if (face.textureIndex >= 0 && (size_t)face.textureIndex < map.textures().size()) {
                texW = (float)map.textures()[face.textureIndex].width;
                texH = (float)map.textures()[face.textureIndex].height;
            }
            glBindTexture(GL_TEXTURE_2D, texId);

            glBegin(GL_POLYGON);
            for (size_t i = 0; i < face.vertices.size(); ++i) {
                float u = face.texCoords[i * 2 + 0] / (texW > 0 ? texW : 1);
                float v = face.texCoords[i * 2 + 1] / (texH > 0 ? texH : 1);
                glTexCoord2f(u, v);
                glVertex3f(face.vertices[i].x, face.vertices[i].y, face.vertices[i].z);
            }
            glEnd();
        }

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
