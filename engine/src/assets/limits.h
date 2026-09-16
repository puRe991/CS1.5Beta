#pragma once

#include <cstdint>

// Upper bound on a decoded texture's width/height. Real GoldSrc assets stay
// far below this; the cap exists so a corrupt dimension field in a WAD, BSP or
// MDL can't make a loader allocate (or read) gigabytes.
constexpr uint32_t kMaxTextureDim = 4096;

// Upper bounds for parsing untrusted WAV files: a single RIFF chunk can't
// claim to be bigger than this, the whole file can't be bigger than this,
// and the sample rate field can't be an absurd value — all just sanity caps
// against a corrupt/malicious header, not real content limits.
constexpr uint32_t kMaxAudioChunkSize = 256u * 1024u * 1024u;
constexpr size_t kMaxAudioFileSize = 256u * 1024u * 1024u;
constexpr uint32_t kMaxAudioSampleRate = 192000;
