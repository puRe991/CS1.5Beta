#include "audio.h"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::init() {
    if (device_ != 0) return true;

    SDL_AudioSpec want{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = &AudioEngine::sdlCallback;
    want.userdata = this;

    SDL_AudioSpec have{};
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (dev == 0) {
        std::fprintf(stderr, "AudioEngine: SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return false;
    }

    device_ = dev;
    deviceSampleRate_ = have.freq;
    SDL_PauseAudioDevice(device_, 0);
    return true;
}

void AudioEngine::shutdown() {
    if (device_ == 0) return;
    SDL_CloseAudioDevice(device_);
    device_ = 0;
    cache_.clear();
    voices_.clear();
}

int AudioEngine::loadSound(const std::string& path) {
    WavSound sound;
    if (!loadWAV(path, sound)) return -1;
    cache_.push_back(std::move(sound));
    return (int)cache_.size() - 1;
}

void AudioEngine::setListener(Vec3f pos, Vec3f forward, Vec3f right) {
    listenerPos_ = pos;
    listenerForward_ = forward;
    listenerRight_ = right;
}

void AudioEngine::startVoice(Voice v) {
    if (!available()) return;
    if (v.sound < 0 || v.sound >= (int)cache_.size()) return;
    std::lock_guard<std::mutex> lock(voicesMutex_);
    // Reuse a finished slot if there is one, rather than growing forever
    // under sustained fire.
    for (Voice& slot : voices_) {
        if (!slot.active) {
            slot = v;
            slot.active = true;
            return;
        }
    }
    v.active = true;
    voices_.push_back(v);
}

void AudioEngine::play2D(int sound, float volume, bool loop) {
    Voice v;
    v.sound = sound;
    v.volume = volume;
    v.loop = loop;
    v.positional = false;
    startVoice(v);
}

void AudioEngine::play3D(int sound, Vec3f pos, float volume, float maxDistance, bool loop) {
    Voice v;
    v.sound = sound;
    v.volume = volume;
    v.loop = loop;
    v.positional = true;
    v.worldPos = pos;
    v.maxDistance = maxDistance <= 0.0f ? 1.0f : maxDistance;
    startVoice(v);
}

void AudioEngine::sdlCallback(void* userdata, uint8_t* stream, int len) {
    AudioEngine* self = static_cast<AudioEngine*>(userdata);
    int frames = len / (int)sizeof(int16_t) / 2; // stereo S16
    self->mix(reinterpret_cast<int16_t*>(stream), frames);
}

void AudioEngine::mix(int16_t* out, int frames) {
    std::vector<int32_t> accum(frames * 2, 0);

    std::lock_guard<std::mutex> lock(voicesMutex_);
    for (Voice& voice : voices_) {
        if (!voice.active) continue;
        const WavSound& snd = cache_[voice.sound];
        if (snd.pcm.empty()) {
            voice.active = false;
            continue;
        }

        float distVol = 1.0f;
        float pan = voice.pan;
        if (voice.positional) {
            Vec3f d = sub(voice.worldPos, listenerPos_);
            float dist = std::sqrt(dot(d, d));
            distVol = std::clamp(1.0f - dist / voice.maxDistance, 0.0f, 1.0f);
            if (dist > 1e-3f) {
                Vec3f dir = {d.x / dist, d.y / dist, d.z / dist};
                pan = std::clamp(dot(dir, listenerRight_), -1.0f, 1.0f);
            } else {
                pan = 0.0f;
            }
        }
        float gain = voice.volume * masterVolume_ * distVol;
        if (gain <= 0.0f) continue;
        // Equal-power-ish pan (cheap linear approximation is fine here).
        float leftGain = gain * (1.0f - std::max(0.0f, pan));
        float rightGain = gain * (1.0f + std::min(0.0f, pan));

        uint32_t srcFrameCount = (uint32_t)snd.pcm.size() / (snd.channels * (snd.bitsPerSample / 8));
        double srcRateRatio = (double)snd.sampleRate / (double)deviceSampleRate_;
        int bytesPerSample = snd.bitsPerSample / 8;

        for (int i = 0; i < frames; ++i) {
            uint32_t srcFrame = (uint32_t)voice.srcPos;
            if (srcFrame >= srcFrameCount) {
                if (voice.loop && srcFrameCount > 0) {
                    voice.srcPos = std::fmod(voice.srcPos, (double)srcFrameCount);
                    srcFrame = (uint32_t)voice.srcPos;
                } else {
                    voice.active = false;
                    break;
                }
            }

            const uint8_t* frame = &snd.pcm[(size_t)srcFrame * snd.channels * bytesPerSample];
            int32_t l, r;
            if (snd.bitsPerSample == 16) {
                int16_t s0 = (int16_t)(frame[0] | (frame[1] << 8));
                if (snd.channels == 2) {
                    int16_t s1 = (int16_t)(frame[2] | (frame[3] << 8));
                    l = s0; r = s1;
                } else {
                    l = r = s0;
                }
            } else {
                // 8-bit WAV PCM is unsigned, centered at 128.
                int32_t s0 = ((int32_t)frame[0] - 128) * 256;
                if (snd.channels == 2) {
                    int32_t s1 = ((int32_t)frame[1] - 128) * 256;
                    l = s0; r = s1;
                } else {
                    l = r = s0;
                }
            }

            accum[i * 2 + 0] += (int32_t)(l * leftGain);
            accum[i * 2 + 1] += (int32_t)(r * rightGain);

            voice.srcPos += srcRateRatio;
        }
    }

    for (int i = 0; i < frames * 2; ++i) {
        out[i] = (int16_t)std::clamp(accum[i], -32768, 32767);
    }
}
