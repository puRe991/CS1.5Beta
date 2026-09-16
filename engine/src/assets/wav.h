#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Minimal RIFF/WAVE loader: supports uncompressed PCM (format tag 1),
// 8-bit unsigned or 16-bit signed, mono or stereo — the variants GoldSrc's
// own sound assets actually ship as. No ADPCM/MP3/float, no extensible
// format chunk beyond reading past it.
struct WavSound {
    uint32_t sampleRate = 0;
    uint16_t channels = 0;    // 1 or 2
    uint16_t bitsPerSample = 0; // 8 or 16
    // Interleaved PCM samples, native endianness, in the source bit depth
    // (caller/mixer converts as needed).
    std::vector<uint8_t> pcm;
};

bool loadWAV(const std::string& path, WavSound& out);

// Same parse, from an in-memory buffer (used by tests against synthetic
// fixtures, and available to any future in-memory asset source).
bool parseWAV(const uint8_t* data, size_t size, WavSound& out);
