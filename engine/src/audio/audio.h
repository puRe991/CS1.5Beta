#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "../mat4.h"
#include "../assets/wav.h"

using SDL_AudioDeviceID = uint32_t;

// Minimal software mixer: opens one SDL audio device (S16 stereo), loads
// WAV clips into an in-memory cache, and mixes any number of simultaneous
// one-shot or looping voices in the SDL audio callback. No SDL_mixer/OpenAL
// dependency — same "own the format parser and the playback path" approach
// as the rest of engine/src/assets.
//
// 3D placement is deliberately simple: linear distance falloff for volume,
// and a stereo pan derived from the sound's position relative to the
// listener's right vector — good enough for directional gunfire/footstep
// cues, not HRTF-accurate spatialization.
class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Opens the default output device. Safe to call in headless/no-audio
    // environments: returns false and every other method becomes a no-op
    // rather than crashing, so callers don't need to branch on it everywhere.
    bool init();
    void shutdown();
    bool available() const { return device_ != 0; }

    // Loads (or returns the cached handle for) a WAV file. Returns -1 if the
    // file doesn't exist or fails to parse — callers should treat that as
    // "no sound available" rather than a fatal error, since game audio
    // assets aren't shipped with this engine.
    int loadSound(const std::string& path);

    // Listener pose for 3D voices, normally set once per frame from the
    // camera.
    void setListener(Vec3f pos, Vec3f forward, Vec3f right);

    // Non-positional playback (UI clicks, menu music) — full volume
    // regardless of listener position.
    void play2D(int sound, float volume = 1.0f, bool loop = false);

    // Positional playback, attenuated by distance from the listener and
    // panned left/right by direction. maxDistance is where volume reaches 0.
    void play3D(int sound, Vec3f pos, float volume = 1.0f, float maxDistance = 1000.0f, bool loop = false);

    void setMasterVolume(float v) { masterVolume_ = v; }
    float masterVolume() const { return masterVolume_; }

private:
    struct Voice {
        int sound = -1;
        double srcPos = 0.0; // fractional frame index into the source PCM
        float volume = 1.0f;
        float pan = 0.0f; // -1 (left) .. +1 (right), 0 for 2D sounds
        bool loop = false;
        bool positional = false;
        Vec3f worldPos{0, 0, 0};
        float maxDistance = 1000.0f;
        bool active = false;
    };

    static void sdlCallback(void* userdata, uint8_t* stream, int len);
    void mix(int16_t* out, int frames);

    SDL_AudioDeviceID device_ = 0;
    int deviceSampleRate_ = 44100;

    std::vector<WavSound> cache_;
    std::vector<Voice> voices_;
    std::mutex voicesMutex_; // guards voices_ between the app thread and the SDL audio thread

    Vec3f listenerPos_{0, 0, 0};
    Vec3f listenerForward_{1, 0, 0};
    Vec3f listenerRight_{0, 1, 0};
    float masterVolume_ = 1.0f;

    void startVoice(Voice v);
};
