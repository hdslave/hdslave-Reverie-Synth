// A 16-bit stereo WAV writer for the tests' renders.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

inline bool writeWav (const char* path, const std::vector<float>& left, const std::vector<float>& right, int rate)
{
    FILE* f = std::fopen (path, "wb");
    if (f == nullptr)
        return false;
    const uint32_t frames = (uint32_t) left.size(), bytes = frames * 4;
    auto u32 = [f] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [f] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (1); u16 (2); u32 ((uint32_t) rate); u32 ((uint32_t) rate * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (bytes);
    for (uint32_t i = 0; i < frames; ++i)
        for (float s : { left[i], right[i] })
        {
            const float c = s > 1.0f ? 1.0f : (s < -1.0f ? -1.0f : s);
            u16 ((uint16_t) (int16_t) std::lrint (c * 32767.0f));
        }
    std::fclose (f);
    return true;
}
