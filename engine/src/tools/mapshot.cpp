// Standalone tool: load a BSP map, render one frame from the player start,
// save a screenshot as BMP. Requires a display (run under Xvfb).
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include "../assets/bsp.h"
#include "../mat4.h"

namespace {

Vec3f parseOrigin(const std::string& s) {
    Vec3f v{0, 0, 0};
    std::sscanf(s.c_str(), "%f %f %f", &v.x, &v.y, &v.z);
    return v;
}

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
        // Missing texture: 2x2 magenta/black checker so gaps are obvious, not invisible.
        uint8_t pixels[16] = {
            255,0,255,255,  0,0,0,255,
            0,0,0,255,      255,0,255,255,
        };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }
    return id;
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

    std::vector<GLuint> texIds;
    texIds.reserve(map.textures().size());
    for (const auto& tex : map.textures()) texIds.push_back(uploadTexture(tex));

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

    std::vector<uint8_t> pixels(W * H * 3);
    glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // glReadPixels gives bottom-up rows; flip to top-down for a normal image file.
    std::vector<uint8_t> flipped(W * H * 3);
    for (int y = 0; y < H; ++y) {
        std::memcpy(&flipped[y * W * 3], &pixels[(H - 1 - y) * W * 3], W * 3);
    }

    SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(flipped.data(), W, H, 24, W * 3,
        0x0000FF, 0x00FF00, 0xFF0000, 0);
    if (!surf || SDL_SaveBMP(surf, outPath.c_str()) != 0) {
        std::fprintf(stderr, "SDL_SaveBMP failed: %s\n", SDL_GetError());
        return 1;
    }
    std::printf("saved %s\n", outPath.c_str());

    SDL_FreeSurface(surf);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
