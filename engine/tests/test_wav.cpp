#include "test_framework.h"
#include "fixtures.h"

#include "assets/wav.h"

namespace {

void put32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x & 0xff));
    v.push_back((uint8_t)((x >> 8) & 0xff));
    v.push_back((uint8_t)((x >> 16) & 0xff));
    v.push_back((uint8_t)((x >> 24) & 0xff));
}
void put16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back((uint8_t)(x & 0xff));
    v.push_back((uint8_t)((x >> 8) & 0xff));
}
void putTag(std::vector<uint8_t>& v, const char* tag) {
    v.insert(v.end(), tag, tag + 4);
}

// Builds a synthetic canonical PCM WAV file in memory.
std::vector<uint8_t> buildWav(uint16_t channels, uint32_t sampleRate, uint16_t bitsPerSample,
                               const std::vector<uint8_t>& pcm) {
    std::vector<uint8_t> fmtChunk;
    put16(fmtChunk, 1); // PCM
    put16(fmtChunk, channels);
    put32(fmtChunk, sampleRate);
    uint32_t byteRate = sampleRate * channels * (bitsPerSample / 8);
    put32(fmtChunk, byteRate);
    put16(fmtChunk, (uint16_t)(channels * (bitsPerSample / 8))); // block align
    put16(fmtChunk, bitsPerSample);

    std::vector<uint8_t> out;
    putTag(out, "RIFF");
    uint32_t riffSize = 4 + (8 + (uint32_t)fmtChunk.size()) + (8 + (uint32_t)pcm.size());
    put32(out, riffSize);
    putTag(out, "WAVE");

    putTag(out, "fmt ");
    put32(out, (uint32_t)fmtChunk.size());
    out.insert(out.end(), fmtChunk.begin(), fmtChunk.end());

    putTag(out, "data");
    put32(out, (uint32_t)pcm.size());
    out.insert(out.end(), pcm.begin(), pcm.end());
    return out;
}

} // namespace

TEST(wav_parses_mono_16bit) {
    std::vector<uint8_t> pcm;
    for (int i = 0; i < 8; ++i) { pcm.push_back((uint8_t)i); pcm.push_back(0); }
    std::vector<uint8_t> file = buildWav(1, 44100, 16, pcm);

    WavSound sound;
    CHECK(parseWAV(file.data(), file.size(), sound));
    CHECK_EQ(sound.channels, (uint16_t)1);
    CHECK_EQ(sound.sampleRate, (uint32_t)44100);
    CHECK_EQ(sound.bitsPerSample, (uint16_t)16);
    CHECK_EQ(sound.pcm.size(), pcm.size());
}

TEST(wav_parses_stereo_8bit) {
    std::vector<uint8_t> pcm = {10, 20, 30, 40, 50, 60};
    std::vector<uint8_t> file = buildWav(2, 22050, 8, pcm);

    WavSound sound;
    CHECK(parseWAV(file.data(), file.size(), sound));
    CHECK_EQ(sound.channels, (uint16_t)2);
    CHECK_EQ(sound.bitsPerSample, (uint16_t)8);
    CHECK_EQ(sound.pcm.size(), (size_t)6);
}

TEST(wav_rejects_bad_magic) {
    std::vector<uint8_t> file = buildWav(1, 44100, 16, {1, 2, 3, 4});
    file[0] = 'X'; // corrupt "RIFF"

    WavSound sound;
    CHECK(!parseWAV(file.data(), file.size(), sound));
}

TEST(wav_rejects_non_pcm_format) {
    std::vector<uint8_t> file = buildWav(1, 44100, 16, {1, 2, 3, 4});
    // The format tag is the first two bytes of the fmt chunk body, which
    // starts right after "RIFF"(4) + size(4) + "WAVE"(4) + "fmt "(4) + size(4).
    size_t fmtTagOffset = 4 + 4 + 4 + 4 + 4;
    file[fmtTagOffset] = 3; // IEEE float, not PCM

    WavSound sound;
    CHECK(!parseWAV(file.data(), file.size(), sound));
}

TEST(wav_truncates_partial_trailing_frame) {
    // 3 bytes of 16-bit mono PCM is one and a half frames; the parser should
    // keep only the whole frame.
    std::vector<uint8_t> pcm = {1, 2, 3};
    std::vector<uint8_t> file = buildWav(1, 44100, 16, pcm);

    WavSound sound;
    CHECK(parseWAV(file.data(), file.size(), sound));
    CHECK_EQ(sound.pcm.size(), (size_t)2);
}

TEST(wav_open_missing_file_fails) {
    WavSound sound;
    CHECK(!loadWAV("/nonexistent/path/does_not_exist.wav", sound));
}

TEST(wav_round_trips_through_disk) {
    std::vector<uint8_t> pcm;
    for (int i = 0; i < 100; ++i) { pcm.push_back((uint8_t)(i * 3)); pcm.push_back((uint8_t)(i * 7)); }
    std::vector<uint8_t> file = buildWav(1, 48000, 16, pcm);
    std::string path = fixtures::tempFilePath(".wav");
    fixtures::writeFile(path, file);

    WavSound sound;
    CHECK(loadWAV(path, sound));
    CHECK_EQ(sound.sampleRate, (uint32_t)48000);
    CHECK(sound.pcm == pcm);
}

int main() { return RUN_ALL_TESTS(); }
