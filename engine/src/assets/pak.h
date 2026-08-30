#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Reader for Quake-style PAK archives (used to bundle GoldSrc assets).
class PakFile {
public:
    struct Entry {
        std::string name; // e.g. "textures/cs_office.wad"
        uint32_t offset;
        uint32_t length;
    };

    // Opens a .pak file and reads its directory. Returns false on failure.
    bool open(const std::string& path);

    const Entry* find(const std::string& name) const;
    const std::vector<Entry>& entries() const { return entries_; }

    // Reads the full contents of an entry into memory.
    bool readEntry(const Entry& entry, std::vector<uint8_t>& outData) const;

private:
    std::string path_;
    std::vector<Entry> entries_;
};
