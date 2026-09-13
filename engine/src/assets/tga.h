#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Minimal TGA loader: supports uncompressed (type 2) 24/32-bit true-color
// images only, which covers the classic GoldSrc skybox assets — no RLE
// (type 10), no colormapped/grayscale variants. Good enough for its one
// use case (skybox faces), not a general-purpose image loader.
struct TgaImage {
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> rgba; // top-to-bottom row order, RGBA8
};

bool loadTGA(const std::string& path, TgaImage& out);
