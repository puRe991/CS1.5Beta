#include "world_mesh.h"

#include <algorithm>
#include <map>

namespace {
struct Vertex {
    float x, y, z, u, v, lmU, lmV;
};

constexpr int kAtlasSize = 2048;

// Simple shelf packer: places rects left-to-right, wrapping to a new row
// when the current one runs out of width. Good enough for a one-shot map
// load; doesn't need to be space-optimal.
struct ShelfPacker {
    int cursorX = 0, cursorY = 0, shelfHeight = 0;
    bool pack(int w, int h, int& outX, int& outY) {
        if (cursorX + w > kAtlasSize) {
            cursorX = 0;
            cursorY += shelfHeight;
            shelfHeight = 0;
        }
        if (cursorY + h > kAtlasSize) return false; // atlas full
        outX = cursorX;
        outY = cursorY;
        cursorX += w;
        shelfHeight = std::max(shelfHeight, h);
        return true;
    }
};
} // namespace

void WorldMesh::build(const BspMap& map, const std::vector<GLuint>& texIds) {
    // --- Pack every face's lightmap into one shared atlas texture first, so
    // the render loop only ever binds one lightmap texture regardless of how
    // many faces/base-textures there are. ---
    std::vector<uint8_t> atlas((size_t)kAtlasSize * kAtlasSize * 3, 255); // white default (no-lightmap faces sample this)
    ShelfPacker packer;
    packer.cursorX = 2; // reserve a small always-white corner for no-lightmap faces
    struct AtlasRect { int x, y; bool valid; };
    std::vector<AtlasRect> atlasRects(map.faces().size());

    for (size_t fi = 0; fi < map.faces().size(); ++fi) {
        const BspFace& face = map.faces()[fi];
        if (face.lightmapRGB.empty() || face.lightmapWidth == 0 || face.lightmapHeight == 0) {
            atlasRects[fi] = {0, 0, false};
            continue;
        }
        int ax, ay;
        if (!packer.pack((int)face.lightmapWidth, (int)face.lightmapHeight, ax, ay)) {
            atlasRects[fi] = {0, 0, false}; // atlas exhausted — fall back to white
            continue;
        }
        for (uint32_t row = 0; row < face.lightmapHeight; ++row) {
            const uint8_t* src = &face.lightmapRGB[(size_t)row * face.lightmapWidth * 3];
            uint8_t* dst = &atlas[((size_t)(ay + row) * kAtlasSize + ax) * 3];
            std::copy(src, src + face.lightmapWidth * 3, dst);
        }
        atlasRects[fi] = {ax, ay, true};
    }

    glGenTextures(1, &lightmapAtlas_);
    glBindTexture(GL_TEXTURE_2D, lightmapAtlas_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, kAtlasSize, kAtlasSize, 0, GL_RGB, GL_UNSIGNED_BYTE, atlas.data());

    // --- Bucket triangulated vertices by base texture, same as before, now
    // also carrying each vertex's lightmap UV within the shared atlas. ---
    std::map<GLuint, std::vector<Vertex>> byTexture;

    for (size_t fi = 0; fi < map.faces().size(); ++fi) {
        const BspFace& face = map.faces()[fi];
        if (face.vertices.size() < 3) continue;

        GLuint texId = 0;
        float texW = 64, texH = 64;
        if (face.textureIndex >= 0 && (size_t)face.textureIndex < texIds.size()) {
            texId = texIds[face.textureIndex];
            texW = (float)map.textures()[face.textureIndex].width;
            texH = (float)map.textures()[face.textureIndex].height;
        }
        if (texW <= 0) texW = 1;
        if (texH <= 0) texH = 1;

        const AtlasRect& rect = atlasRects[fi];

        auto toVertex = [&](size_t i) {
            float lmU = 0.5f / kAtlasSize, lmV = 0.5f / kAtlasSize; // the reserved white corner
            if (rect.valid && !face.lightmapTexCoords.empty()) {
                float localU = face.lightmapTexCoords[i * 2 + 0];
                float localV = face.lightmapTexCoords[i * 2 + 1];
                lmU = (rect.x + localU) / (float)kAtlasSize;
                lmV = (rect.y + localV) / (float)kAtlasSize;
            }
            return Vertex{
                face.vertices[i].x, face.vertices[i].y, face.vertices[i].z,
                face.texCoords[i * 2 + 0] / texW, face.texCoords[i * 2 + 1] / texH,
                lmU, lmV
            };
        };

        // Triangle fan: (0, i, i+1) for i in [1, n-2].
        auto& bucket = byTexture[texId];
        for (size_t i = 1; i + 1 < face.vertices.size(); ++i) {
            bucket.push_back(toVertex(0));
            bucket.push_back(toVertex(i));
            bucket.push_back(toVertex(i + 1));
        }
    }

    std::vector<Vertex> all;
    groups_.clear();
    for (auto& [texId, verts] : byTexture) {
        groups_.push_back(DrawGroup{texId, (GLsizei)all.size(), (GLsizei)verts.size()});
        all.insert(all.end(), verts.begin(), verts.end());
    }

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(all.size() * sizeof(Vertex)), all.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void WorldMesh::draw(const Shader& shader) const {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    GLint posLoc = shader.attribLocation("aPos");
    GLint texLoc = shader.attribLocation("aTexCoord");
    GLint lmLoc = shader.attribLocation("aLightmapCoord");
    glEnableVertexAttribArray(posLoc);
    glEnableVertexAttribArray(texLoc);
    glEnableVertexAttribArray(lmLoc);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glVertexAttribPointer(lmLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(5 * sizeof(float)));

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, lightmapAtlas_);
    glActiveTexture(GL_TEXTURE0);

    for (const auto& group : groups_) {
        glBindTexture(GL_TEXTURE_2D, group.texId);
        glDrawArrays(GL_TRIANGLES, group.start, group.count);
    }

    glDisableVertexAttribArray(posLoc);
    glDisableVertexAttribArray(texLoc);
    glDisableVertexAttribArray(lmLoc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

WorldMesh::~WorldMesh() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (lightmapAtlas_) glDeleteTextures(1, &lightmapAtlas_);
}
