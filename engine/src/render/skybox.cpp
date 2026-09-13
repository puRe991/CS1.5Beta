#include "skybox.h"
#include "../assets/tga.h"

#include <algorithm>
#include <cctype>
#include <dirent.h>

namespace {
struct Vertex { float x, y, z, u, v; };

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// GoldSrc filenames were case-insensitive on their original Windows
// filesystem; ours isn't, so scan the directory for a case-insensitive
// match instead of assuming the on-disk casing matches the skyname exactly.
std::string findCaseInsensitive(const std::string& dir, const std::string& wantedLower) {
    DIR* d = opendir(dir.c_str());
    if (!d) return "";
    std::string found;
    while (dirent* entry = readdir(d)) {
        if (toLower(entry->d_name) == wantedLower) {
            found = entry->d_name;
            break;
        }
    }
    closedir(d);
    return found;
}

GLuint uploadFace(const TgaImage& img) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
    return id;
}
} // namespace

bool Skybox::load(const std::string& envDir, const std::string& skyname) {
    if (skyname.empty()) return false;

    // up/dn/lf/rt/ft/bk map onto a cube centered on the origin using the
    // engine's existing world axes (X forward, Y left, Z up): ft/bk are the
    // +X/-X faces, lf/rt are +Y/-Y, up/dn are +Z/-Z.
    static const char* kSuffixes[6] = {"up", "dn", "lf", "rt", "ft", "bk"};
    std::string envDirPath = envDir + "/gfx/env";
    TgaImage images[6];
    for (int i = 0; i < 6; ++i) {
        std::string wanted = toLower(skyname + kSuffixes[i] + ".tga");
        std::string actual = findCaseInsensitive(envDirPath, wanted);
        if (actual.empty() || !loadTGA(envDirPath + "/" + actual, images[i])) return false;
    }
    for (int i = 0; i < 6; ++i) faceTex_[i] = uploadFace(images[i]);

    constexpr float S = 1.0f; // unit cube; scaled up via the MVP's model matrix
    // Each face: 2 triangles, CCW when viewed from inside the cube.
    std::vector<Vertex> verts;
    auto quad = [&](Vertex a, Vertex b, Vertex c, Vertex d) {
        verts.push_back(a); verts.push_back(b); verts.push_back(c);
        verts.push_back(a); verts.push_back(c); verts.push_back(d);
    };
    // up (+Z)
    quad({-S,-S, S,0,0}, { S,-S, S,1,0}, { S, S, S,1,1}, {-S, S, S,0,1});
    // dn (-Z)
    quad({-S, S,-S,0,0}, { S, S,-S,1,0}, { S,-S,-S,1,1}, {-S,-S,-S,0,1});
    // lf (+Y)
    quad({-S, S,-S,0,0}, {-S, S, S,1,0}, { S, S, S,1,1}, { S, S,-S,0,1});
    // rt (-Y)
    quad({ S,-S,-S,0,0}, { S,-S, S,1,0}, {-S,-S, S,1,1}, {-S,-S,-S,0,1});
    // ft (+X)
    quad({ S, S,-S,0,0}, { S, S, S,1,0}, { S,-S, S,1,1}, { S,-S,-S,0,1});
    // bk (-X)
    quad({-S,-S,-S,0,0}, {-S,-S, S,1,0}, {-S, S, S,1,1}, {-S, S,-S,0,1});

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(Vertex)), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    valid_ = true;
    return true;
}

void Skybox::draw(const Shader& shader, Vec3f) const {
    if (!valid_) return;

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    GLint posLoc = shader.attribLocation("aPos");
    GLint texLoc = shader.attribLocation("aTexCoord");
    glEnableVertexAttribArray(posLoc);
    glEnableVertexAttribArray(texLoc);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));

    for (int i = 0; i < 6; ++i) {
        glBindTexture(GL_TEXTURE_2D, faceTex_[i]);
        glDrawArrays(GL_TRIANGLES, i * 6, 6);
    }

    glDisableVertexAttribArray(posLoc);
    glDisableVertexAttribArray(texLoc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

Skybox::~Skybox() {
    for (GLuint t : faceTex_) if (t) glDeleteTextures(1, &t);
    if (vbo_) glDeleteBuffers(1, &vbo_);
}
