// Loads a .mdl file, prints its hitboxes (bone, HITGROUP id, body part,
// bind-pose world-space AABB) — a text-only counterpart to modelshot for
// verifying hitbox parsing + bone-world transforms against real assets
// without needing a display.
#include <cstdio>
#include "../assets/mdl.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: hitboxtest <model.mdl>\n");
        return 1;
    }

    MdlModel model;
    if (!model.load(argv[1])) {
        std::fprintf(stderr, "failed to load %s\n", argv[1]);
        return 1;
    }

    std::printf("%s: %zu hitbox(es)\n", argv[1], model.hitboxes().size());
    std::vector<WorldHitbox> posed = model.poseHitboxes(-1, 0.0f); // bind pose
    for (size_t i = 0; i < posed.size(); ++i) {
        const WorldHitbox& hb = posed[i];
        std::printf("  [%zu] bone=%d part=%-10s world mins=(%.1f %.1f %.1f) maxs=(%.1f %.1f %.1f)\n",
                    i, hb.bone, bodyPartName(hb.part),
                    hb.mins[0], hb.mins[1], hb.mins[2],
                    hb.maxs[0], hb.maxs[1], hb.maxs[2]);
    }
    return posed.empty() && !model.hitboxes().empty() ? 1 : 0;
}
