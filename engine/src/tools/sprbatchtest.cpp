// Batch-loads every .spr path given on the command line and reports
// pass/fail + frame/format info, to verify the parser across a whole
// sprite set (muzzle flashes, smoke, HUD elements) at once.
#include <cstdio>
#include "../assets/spr.h"

const char* renderModeName(SprRenderMode m) {
    switch (m) {
        case SprRenderMode::Normal: return "normal";
        case SprRenderMode::Additive: return "additive";
        case SprRenderMode::IndexAlpha: return "indexalpha";
        case SprRenderMode::AlphaTest: return "alphatest";
    }
    return "?";
}

int main(int argc, char** argv) {
    int ok = 0, failed = 0;
    for (int i = 1; i < argc; ++i) {
        SprModel sprite;
        if (sprite.load(argv[i]) && !sprite.frames().empty()) {
            ++ok;
            const SprFrame& f0 = sprite.frames()[0];
            std::printf("OK   %-40s frames=%-3zu mode=%-10s %ux%u\n",
                        argv[i], sprite.frames().size(), renderModeName(sprite.renderMode()), f0.width, f0.height);
        } else {
            ++failed;
            std::printf("FAIL %-40s\n", argv[i]);
        }
    }
    std::printf("\n%d ok, %d failed (of %d)\n", ok, failed, argc - 1);
    return failed > 0 ? 1 : 0;
}
