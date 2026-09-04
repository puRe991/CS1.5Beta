// Batch-loads every .mdl path given on the command line and reports
// pass/fail + triangle/texture counts, to verify the parser across a
// whole asset set (e.g. all weapon view/world/pickup models) at once.
#include <cstdio>
#include "../assets/mdl.h"

int main(int argc, char** argv) {
    int ok = 0, failed = 0;
    for (int i = 1; i < argc; ++i) {
        MdlModel model;
        if (model.load(argv[i]) && !model.triangles().empty()) {
            ++ok;
            std::printf("OK   %-40s tris=%-6zu tex=%zu\n", argv[i], model.triangles().size(), model.textures().size());
        } else {
            ++failed;
            std::printf("FAIL %-40s\n", argv[i]);
        }
    }
    std::printf("\n%d ok, %d failed (of %d)\n", ok, failed, argc - 1);
    return failed > 0 ? 1 : 0;
}
