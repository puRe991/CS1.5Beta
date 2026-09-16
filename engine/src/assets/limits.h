#pragma once

#include <cstdint>

// Upper bound on a decoded texture's width/height. Real GoldSrc assets stay
// far below this; the cap exists so a corrupt dimension field in a WAD, BSP or
// MDL can't make a loader allocate (or read) gigabytes.
constexpr uint32_t kMaxTextureDim = 4096;
