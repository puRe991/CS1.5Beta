// Standalone tool: load a BSP map, render one frame from the player start,
// save a screenshot as BMP. Requires a display (run under Xvfb).
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <cstdio>
#include <cstdlib>

#include "../assets/bsp.h"
#include "../mat4.h"
#include "../render/render.h"

namespace {

Vec3f parseOrigin(const std::string& s) {
    Vec3f v{0, 0, 0};
    std::sscanf(s.c_str(), "%f %f %f", &v.x, &v.y, &v.z);
    return v;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: mapshot <map.bsp> <wad_dir> <out.bmp>\n");
        return 1;
    }
    std::string mapPath = argv[1];
    std::string wadDir = argv[2];
    std::string outPath = argv[3];

    BspMap map;
    if (!map.load(mapPath, {wadDir})) {
        std::fprintf(stderr, "failed to load %s\n", mapPath.c_str());
        return 1;
    }
    std::printf("faces: %zu, textures: %zu\n", map.faces().size(), map.textures().size());

    Vec3f eye{0, 0, 64};
    float yawDeg = 0.0f;
    for (const auto& ent : map.entities()) {
        const std::string* classname = ent.get("classname");
        if (classname && (*classname == "info_player_start" || *classname == "info_player_deathmatch")) {
            const std::string* origin = ent.get("origin");
            if (origin) eye = parseOrigin(*origin);
            const std::string* angle = ent.get("angle");
            if (angle) yawDeg = (float)std::atof(angle->c_str());
            break;
        }
    }
    eye.z += 64.0f; // eye height above the entity's ground origin

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    const int W = 1280, H = 720;
    SDL_Window* window = SDL_CreateWindow("mapshot", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_OPENGL);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    if (!ctx) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glViewport(0, 0, W, H);
    glClearColor(0.4f, 0.6f, 0.9f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Mat4 proj = perspective(90.0f, (float)W / H, 4.0f, 8192.0f);
    float yawRad = yawDeg * 3.14159265f / 180.0f;
    Vec3f forward{std::cos(yawRad), std::sin(yawRad), 0.0f};
    Vec3f center{eye.x + forward.x, eye.y + forward.y, eye.z + forward.z};
    Mat4 view = lookAt(eye, center, Vec3f{0, 0, 1});

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(proj.m);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(view.m);

    std::vector<GLuint> texIds = uploadTextures(map.textures());
    drawBspFaces(map, texIds);

    SDL_GL_SwapWindow(window);

    if (!saveScreenshotBMP(outPath, W, H)) return 1;
    std::printf("saved %s\n", outPath.c_str());

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
