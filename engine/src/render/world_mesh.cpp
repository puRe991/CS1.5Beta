#include "world_mesh.h"

#include <map>

namespace {
struct Vertex {
    float x, y, z, u, v;
};
} // namespace

void WorldMesh::build(const BspMap& map, const std::vector<GLuint>& texIds) {
    // Bucket triangulated vertices by texture id first, so each texture's
    // triangles end up contiguous in the final buffer (one draw call each).
    std::map<GLuint, std::vector<Vertex>> byTexture;

    for (const auto& face : map.faces()) {
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

        auto toVertex = [&](size_t i) {
            return Vertex{
                face.vertices[i].x, face.vertices[i].y, face.vertices[i].z,
                face.texCoords[i * 2 + 0] / texW, face.texCoords[i * 2 + 1] / texH
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
    glEnableVertexAttribArray(posLoc);
    glEnableVertexAttribArray(texLoc);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));

    for (const auto& group : groups_) {
        glBindTexture(GL_TEXTURE_2D, group.texId);
        glDrawArrays(GL_TRIANGLES, group.start, group.count);
    }

    glDisableVertexAttribArray(posLoc);
    glDisableVertexAttribArray(texLoc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

WorldMesh::~WorldMesh() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
}
