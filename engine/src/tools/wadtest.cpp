#include <cstdio>
#include "../assets/wad.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: wadtest <file.wad>\n");
        return 1;
    }

    WadFile wad;
    if (!wad.open(argv[1])) {
        std::fprintf(stderr, "failed to open %s\n", argv[1]);
        return 1;
    }

    std::printf("textures found: %zu\n", wad.textureCount());

    size_t decoded = 0, failed = 0;
    for (size_t i = 0; i < wad.textureCount(); ++i) {
        WadTexture tex;
        if (wad.decodeTexture(i, tex)) {
            ++decoded;
            if (i < 5) {
                std::printf("  [%zu] %-16s %ux%u\n", i, tex.name.c_str(), tex.width, tex.height);
            }
        } else {
            ++failed;
            std::printf("  [%zu] %-16s FAILED to decode\n", i, wad.textureName(i).c_str());
        }
    }

    std::printf("decoded: %zu, failed: %zu\n", decoded, failed);
    return failed > 0 ? 1 : 0;
}
