// Standalone tool: load an MDL character model, render one frame, save as BMP.
// Requires a display (run under Xvfb).
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../assets/mdl.h"
#include "../mat4.h"

namespace {

GLuint uploadTexture(const MdlTexture& tex) {
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
        uint8_t pixels[16] = {255,0,255,255, 0,0,0,255, 0,0,0,255, 255,0,255,255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }
    return id;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: modelshot <model.mdl> <out.bmp>\n");
        return 1;
    }

    MdlModel model;
    if (!model.load(argv[1])) {
        std::fprintf(stderr, "failed to load %s\n", argv[1]);
        return 1;
    }
    std::printf("triangles: %zu, textures: %zu\n", model.triangles().size(), model.textures().size());

    // Bounding box, to frame the camera regardless of model scale (GoldSrc units vary).
    float minX=1e9f,minY=1e9f,minZ=1e9f,maxX=-1e9f,maxY=-1e9f,maxZ=-1e9f;
    for (const auto& tri : model.triangles()) {
        for (const MdlVertex* v : {&tri.a, &tri.b, &tri.c}) {
            minX = std::min(minX, v->x); maxX = std::max(maxX, v->x);
            minY = std::min(minY, v->y); maxY = std::max(maxY, v->y);
            minZ = std::min(minZ, v->z); maxZ = std::max(maxZ, v->z);
        }
    }
    Vec3f center{(minX+maxX)/2, (minY+maxY)/2, (minZ+maxZ)/2};
    float radius = std::max({maxX-minX, maxY-minY, maxZ-minZ}) * 0.75f + 1.0f;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError()); return 1; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    const int W = 800, H = 800;
    SDL_Window* window = SDL_CreateWindow("modelshot", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_OPENGL);
    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    if (!ctx) { std::fprintf(stderr, "GL context failed: %s\n", SDL_GetError()); return 1; }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glViewport(0, 0, W, H);
    glClearColor(0.25f, 0.25f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Mat4 proj = perspective(60.0f, 1.0f, 1.0f, 4096.0f);
    // Standard Quake/GoldSrc forward is +X; view from front-left so both face and side read.
    Vec3f eye{center.x - radius * 1.6f, center.y - radius * 1.6f, center.z + radius * 0.3f};
    Mat4 view = lookAt(eye, center, Vec3f{0, 0, 1});

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(proj.m);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(view.m);

    std::vector<GLuint> texIds;
    for (const auto& tex : model.textures()) texIds.push_back(uploadTexture(tex));

    GLuint currentTex = (GLuint)-1;
    glBegin(GL_TRIANGLES);
    for (const auto& tri : model.triangles()) {
        GLuint texId = (tri.textureIndex >= 0 && (size_t)tri.textureIndex < texIds.size()) ? texIds[tri.textureIndex] : 0;
        if (texId != currentTex) {
            glEnd();
            glBindTexture(GL_TEXTURE_2D, texId);
            currentTex = texId;
            glBegin(GL_TRIANGLES);
        }
        float texW = 64, texH = 64;
        if (tri.textureIndex >= 0 && (size_t)tri.textureIndex < model.textures().size()) {
            texW = (float)model.textures()[tri.textureIndex].width;
            texH = (float)model.textures()[tri.textureIndex].height;
        }
        for (const MdlVertex* v : {&tri.a, &tri.b, &tri.c}) {
            glTexCoord2f(v->u / texW, v->v / texH);
            glVertex3f(v->x, v->y, v->z);
        }
    }
    glEnd();

    SDL_GL_SwapWindow(window);

    std::vector<uint8_t> pixels(W * H * 3);
    glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    std::vector<uint8_t> flipped(W * H * 3);
    for (int y = 0; y < H; ++y) std::memcpy(&flipped[y*W*3], &pixels[(H-1-y)*W*3], W*3);

    SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(flipped.data(), W, H, 24, W*3, 0x0000FF, 0x00FF00, 0xFF0000, 0);
    if (!surf || SDL_SaveBMP(surf, argv[2]) != 0) {
        std::fprintf(stderr, "SDL_SaveBMP failed: %s\n", SDL_GetError());
        return 1;
    }
    std::printf("saved %s\n", argv[2]);
    return 0;
}
