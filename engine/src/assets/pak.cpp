#include "pak.h"

#include <cstdio>
#include <cstring>

namespace {

struct PakHeader {
    char magic[4]; // "PACK"
    int32_t dirOffset;
    int32_t dirLength;
};

struct PakDirEntry {
    char name[56];
    int32_t filePos;
    int32_t fileLen;
};

} // namespace

bool PakFile::open(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    PakHeader header;
    if (std::fread(&header, sizeof(header), 1, f) != 1) {
        std::fclose(f);
        return false;
    }
    if (std::memcmp(header.magic, "PACK", 4) != 0) {
        std::fclose(f);
        return false;
    }

    int32_t numEntries = header.dirLength / (int32_t)sizeof(PakDirEntry);
    std::fseek(f, header.dirOffset, SEEK_SET);

    entries_.clear();
    entries_.reserve(numEntries);
    for (int32_t i = 0; i < numEntries; ++i) {
        PakDirEntry dirEntry;
        if (std::fread(&dirEntry, sizeof(dirEntry), 1, f) != 1) break;

        Entry entry;
        entry.name.assign(dirEntry.name, strnlen(dirEntry.name, sizeof(dirEntry.name)));
        entry.offset = (uint32_t)dirEntry.filePos;
        entry.length = (uint32_t)dirEntry.fileLen;
        entries_.push_back(std::move(entry));
    }

    std::fclose(f);
    path_ = path;
    return true;
}

const PakFile::Entry* PakFile::find(const std::string& name) const {
    for (const auto& entry : entries_) {
        if (entry.name == name) return &entry;
    }
    return nullptr;
}

bool PakFile::readEntry(const Entry& entry, std::vector<uint8_t>& outData) const {
    FILE* f = std::fopen(path_.c_str(), "rb");
    if (!f) return false;

    std::fseek(f, (long)entry.offset, SEEK_SET);
    outData.resize(entry.length);
    size_t read = std::fread(outData.data(), 1, entry.length, f);
    std::fclose(f);

    return read == entry.length;
}
