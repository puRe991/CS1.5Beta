#include "test_framework.h"
#include "fixtures.h"

#include "assets/wad.h"

TEST(wad_open_missing_file_fails) {
    WadFile wad;
    CHECK(!wad.open("/nonexistent/path/does_not_exist.wad"));
}

TEST(wad_open_rejects_bad_magic) {
    std::vector<uint8_t> bogus(64, 0);
    std::memcpy(bogus.data(), "NOPE", 4);
    std::string path = fixtures::tempFilePath(".wad");
    fixtures::writeFile(path, bogus);

    WadFile wad;
    CHECK(!wad.open(path));
}

TEST(wad_reads_and_decodes_solid_texture) {
    std::string path = fixtures::buildWadWithSolidTexture("BRICK1", 8, 8, /*paletteIndex=*/1,
                                                            200, 100, 50);
    WadFile wad;
    CHECK(wad.open(path));
    CHECK_EQ(wad.textureCount(), (size_t)1);
    CHECK_EQ(wad.textureName(0), std::string("BRICK1"));

    WadTexture tex;
    CHECK(wad.decodeTexture(0, tex));
    CHECK_EQ(tex.width, (uint32_t)8);
    CHECK_EQ(tex.height, (uint32_t)8);
    CHECK_EQ(tex.rgba.size(), (size_t)8 * 8 * 4);

    // Every texel should decode to the palette color we set at index 1, fully opaque.
    CHECK_EQ(tex.rgba[0], (uint8_t)200);
    CHECK_EQ(tex.rgba[1], (uint8_t)100);
    CHECK_EQ(tex.rgba[2], (uint8_t)50);
    CHECK_EQ(tex.rgba[3], (uint8_t)255);
}

TEST(wad_transparent_texture_masks_index_255) {
    // Names starting with '{' are the transparent-texture convention:
    // palette index 255 becomes alpha 0 instead of opaque.
    std::string path = fixtures::buildWadWithSolidTexture("{FENCE", 8, 8, /*paletteIndex=*/255,
                                                            10, 20, 30);
    WadFile wad;
    CHECK(wad.open(path));

    WadTexture tex;
    CHECK(wad.decodeTexture(0, tex));
    CHECK_EQ(tex.rgba[3], (uint8_t)0); // alpha masked out
}

TEST(wad_decode_out_of_range_index_fails) {
    std::string path = fixtures::buildWadWithSolidTexture("BRICK1", 8, 8, 1, 1, 2, 3);
    WadFile wad;
    CHECK(wad.open(path));

    WadTexture tex;
    CHECK(!wad.decodeTexture(5, tex));
}

int main() { return RUN_ALL_TESTS(); }
