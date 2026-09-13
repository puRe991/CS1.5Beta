#include "tga.h"

#include <cstdio>

bool loadTGA(const std::string& path, TgaImage& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    uint8_t header[18];
    if (std::fread(header, 1, sizeof(header), f) != sizeof(header)) {
        std::fclose(f);
        return false;
    }

    uint8_t idLength = header[0];
    uint8_t colorMapType = header[1];
    uint8_t dataType = header[2];
    uint16_t width = header[12] | (header[13] << 8);
    uint16_t height = header[14] | (header[15] << 8);
    uint8_t bpp = header[16];
    uint8_t descriptor = header[17];

    if (colorMapType != 0 || dataType != 2 || (bpp != 24 && bpp != 32)) {
        std::fclose(f); // unsupported variant (colormapped/RLE/grayscale) — not needed for skyboxes
        return false;
    }

    std::fseek(f, idLength, SEEK_CUR);

    int bytesPerPixel = bpp / 8;
    std::vector<uint8_t> raw((size_t)width * height * bytesPerPixel);
    if (std::fread(raw.data(), 1, raw.size(), f) != raw.size()) {
        std::fclose(f);
        return false;
    }
    std::fclose(f);

    bool topToBottom = (descriptor & 0x20) != 0; // bit 5: 1 = origin at top
    out.width = width;
    out.height = height;
    out.rgba.resize((size_t)width * height * 4);

    for (uint32_t y = 0; y < height; ++y) {
        uint32_t srcRow = topToBottom ? y : (height - 1 - y);
        const uint8_t* src = &raw[(size_t)srcRow * width * bytesPerPixel];
        uint8_t* dst = &out.rgba[(size_t)y * width * 4];
        for (uint32_t x = 0; x < width; ++x) {
            // TGA stores BGR(A).
            dst[x * 4 + 0] = src[x * bytesPerPixel + 2];
            dst[x * 4 + 1] = src[x * bytesPerPixel + 1];
            dst[x * 4 + 2] = src[x * bytesPerPixel + 0];
            dst[x * 4 + 3] = bytesPerPixel == 4 ? src[x * bytesPerPixel + 3] : 255;
        }
    }
    return true;
}
