#include "wav.h"

#include <cstdio>
#include <cstring>

#include "limits.h"

namespace {

uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
uint16_t rd16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

} // namespace

bool parseWAV(const uint8_t* data, size_t size, WavSound& out) {
    // RIFF header: "RIFF" <u32 size> "WAVE"
    if (size < 12) return false;
    if (std::memcmp(data, "RIFF", 4) != 0) return false;
    if (std::memcmp(data + 8, "WAVE", 4) != 0) return false;

    bool haveFmt = false;
    uint16_t formatTag = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    const uint8_t* pcmData = nullptr;
    uint32_t pcmSize = 0;

    size_t pos = 12;
    while (pos + 8 <= size) {
        char id[5] = {0, 0, 0, 0, 0};
        std::memcpy(id, data + pos, 4);
        uint32_t chunkSize = rd32(data + pos + 4);
        size_t body = pos + 8;
        // A truncated/corrupt chunk size must not walk the cursor past the
        // buffer — bail rather than read (or seek) out of bounds.
        if (chunkSize > kMaxAudioChunkSize || body + chunkSize > size) {
            if (std::memcmp(id, "data", 4) == 0 && body <= size) {
                // Some encoders write a "data" size larger than what's
                // actually in the file (streamed/truncated write). Clamp to
                // what's actually available rather than rejecting outright.
                chunkSize = (uint32_t)(size - body);
            } else {
                break;
            }
        }

        if (std::memcmp(id, "fmt ", 4) == 0) {
            if (chunkSize < 16) return false;
            formatTag = rd16(data + body + 0);
            channels = rd16(data + body + 2);
            sampleRate = rd32(data + body + 4);
            bitsPerSample = rd16(data + body + 14);
            haveFmt = true;
        } else if (std::memcmp(id, "data", 4) == 0) {
            pcmData = data + body;
            pcmSize = chunkSize;
        }

        // Chunks are word-aligned: a chunk with an odd size has one pad byte.
        pos = body + chunkSize + (chunkSize & 1);
    }

    if (!haveFmt || !pcmData) return false;
    if (formatTag != 1 /* PCM */) return false;
    if (channels != 1 && channels != 2) return false;
    if (bitsPerSample != 8 && bitsPerSample != 16) return false;
    if (sampleRate == 0 || sampleRate > kMaxAudioSampleRate) return false;

    // Trim to a whole number of sample frames so the mixer never reads a
    // partial frame off the end.
    uint32_t bytesPerFrame = channels * (bitsPerSample / 8);
    uint32_t usableSize = bytesPerFrame ? (pcmSize / bytesPerFrame) * bytesPerFrame : 0;

    out.sampleRate = sampleRate;
    out.channels = channels;
    out.bitsPerSample = bitsPerSample;
    out.pcm.assign(pcmData, pcmData + usableSize);
    return true;
}

bool loadWAV(const std::string& path, WavSound& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    if (size < 0 || (size_t)size > kMaxAudioFileSize) {
        std::fclose(f);
        return false;
    }
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf((size_t)size);
    bool ok = std::fread(buf.data(), 1, buf.size(), f) == buf.size();
    std::fclose(f);
    if (!ok) return false;
    return parseWAV(buf.data(), buf.size(), out);
}
