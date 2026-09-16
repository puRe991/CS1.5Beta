#include "render.h"

#include <SDL2/SDL.h>

#include <cstdio>
#include <cstring>

namespace {

// Fallback shown for a texture whose pixels couldn't be resolved.
const uint8_t kMissingTexture[16] = {
    255,0,255,255,  0,0,0,255,
    0,0,0,255,      255,0,255,255,
};

GLuint uploadRGBA(const uint8_t* rgba, uint32_t width, uint32_t height) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    if (rgba && width > 0 && height > 0) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, kMissingTexture);
    }
    return id;
}

// Texture dimensions to divide raw texel-unit UVs by. Falls back to 64 (the
// most common GoldSrc texture size) when the index doesn't resolve.
template <typename TextureList>
void texSizeFor(const TextureList& textures, int index, float& outW, float& outH) {
    outW = 64.0f;
    outH = 64.0f;
    if (index >= 0 && (size_t)index < textures.size()) {
        if (textures[index].width > 0) outW = (float)textures[index].width;
        if (textures[index].height > 0) outH = (float)textures[index].height;
    }
}

GLuint texIdFor(const std::vector<GLuint>& texIds, int index) {
    return (index >= 0 && (size_t)index < texIds.size()) ? texIds[index] : 0;
}

} // namespace

GLuint uploadTexture(const BspTexture& tex) {
    return uploadRGBA(tex.rgba.empty() ? nullptr : tex.rgba.data(), tex.width, tex.height);
}

GLuint uploadTexture(const MdlTexture& tex) {
    return uploadRGBA(tex.rgba.empty() ? nullptr : tex.rgba.data(), tex.width, tex.height);
}

std::vector<GLuint> uploadTextures(const std::vector<BspTexture>& textures) {
    std::vector<GLuint> ids;
    ids.reserve(textures.size());
    for (const auto& tex : textures) ids.push_back(uploadTexture(tex));
    return ids;
}

std::vector<GLuint> uploadTextures(const std::vector<MdlTexture>& textures) {
    std::vector<GLuint> ids;
    ids.reserve(textures.size());
    for (const auto& tex : textures) ids.push_back(uploadTexture(tex));
    return ids;
}

void drawBspFaces(const BspMap& map, const std::vector<GLuint>& texIds) {
    for (const auto& face : map.faces()) {
        float texW, texH;
        texSizeFor(map.textures(), face.textureIndex, texW, texH);
        glBindTexture(GL_TEXTURE_2D, texIdFor(texIds, face.textureIndex));

        glBegin(GL_POLYGON);
        for (size_t i = 0; i < face.vertices.size(); ++i) {
            glTexCoord2f(face.texCoords[i * 2 + 0] / texW, face.texCoords[i * 2 + 1] / texH);
            glVertex3f(face.vertices[i].x, face.vertices[i].y, face.vertices[i].z);
        }
        glEnd();
    }
}

void drawMdlTriangles(const MdlModel& model, const std::vector<GLuint>& texIds) {
    // Batched into one GL_TRIANGLES run per texture; the triangle list is
    // already grouped by mesh, so switches are rare.
    GLuint currentTex = (GLuint)-1;
    glBegin(GL_TRIANGLES);
    for (const auto& tri : model.triangles()) {
        GLuint texId = texIdFor(texIds, tri.textureIndex);
        if (texId != currentTex) {
            glEnd();
            glBindTexture(GL_TEXTURE_2D, texId);
            currentTex = texId;
            glBegin(GL_TRIANGLES);
        }

        float texW, texH;
        texSizeFor(model.textures(), tri.textureIndex, texW, texH);
        for (const MdlVertex* v : {&tri.a, &tri.b, &tri.c}) {
            glTexCoord2f(v->u / texW, v->v / texH);
            glVertex3f(v->x, v->y, v->z);
        }
    }
    glEnd();
}

bool saveScreenshotBMP(const std::string& path, int width, int height) {
    if (width <= 0 || height <= 0) return false;

    const size_t stride = (size_t)width * 3;
    std::vector<uint8_t> pixels(stride * height);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    std::vector<uint8_t> flipped(pixels.size());
    for (int y = 0; y < height; ++y) {
        std::memcpy(&flipped[y * stride], &pixels[(height - 1 - y) * stride], stride);
    }

    SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(flipped.data(), width, height, 24, (int)stride,
                                                 0x0000FF, 0x00FF00, 0xFF0000, 0);
    if (!surf) {
        std::fprintf(stderr, "screenshot: SDL_CreateRGBSurfaceFrom failed: %s\n", SDL_GetError());
        return false;
    }

    bool ok = SDL_SaveBMP(surf, path.c_str()) == 0;
    if (!ok) std::fprintf(stderr, "screenshot: SDL_SaveBMP failed: %s\n", SDL_GetError());
    SDL_FreeSurface(surf);
    return ok;
}
