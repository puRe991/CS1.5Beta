#include "test_framework.h"
#include "fixtures.h"

#include "assets/pak.h"

TEST(pak_open_missing_file_fails) {
    PakFile pak;
    CHECK(!pak.open("/nonexistent/path/does_not_exist.pak"));
}

TEST(pak_open_rejects_bad_magic) {
    std::vector<uint8_t> bogus(64, 0);
    std::memcpy(bogus.data(), "NOPE", 4);
    std::string path = fixtures::tempFilePath(".pak");
    fixtures::writeFile(path, bogus);

    PakFile pak;
    CHECK(!pak.open(path));
}

TEST(pak_reads_directory_and_entries) {
    std::vector<uint8_t> dataA = {'h', 'e', 'l', 'l', 'o'};
    std::vector<uint8_t> dataB = {1, 2, 3, 4, 5, 6, 7, 8};

    std::string path = fixtures::buildPak({
        {"textures/foo.wad", dataA},
        {"maps/bar.bsp", dataB},
    });

    PakFile pak;
    CHECK(pak.open(path));
    CHECK_EQ(pak.entries().size(), (size_t)2);

    const PakFile::Entry* foo = pak.find("textures/foo.wad");
    CHECK(foo != nullptr);
    if (foo) {
        CHECK_EQ(foo->length, dataA.size());
        std::vector<uint8_t> out;
        CHECK(pak.readEntry(*foo, out));
        CHECK(out == dataA);
    }

    CHECK(pak.find("does/not/exist") == nullptr);

    const PakFile::Entry* bar = pak.find("maps/bar.bsp");
    CHECK(bar != nullptr);
    if (bar) {
        std::vector<uint8_t> out;
        CHECK(pak.readEntry(*bar, out));
        CHECK(out == dataB);
    }
}

int main() { return RUN_ALL_TESTS(); }
