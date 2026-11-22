// A PNG writer for the tests' screen pictures: 8-bit RGB, stored (uncompressed) deflate blocks, so it needs nothing
// but this file. Optionally each pixel drawn as a scale x scale square.
#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

inline bool writePng (const char* path, const uint32_t* argb, int width, int height, int stride, int scale = 1)
{
    const int w = width * scale, h = height * scale;
    std::vector<uint8_t> raw;
    raw.reserve ((size_t) (w * 3 + 1) * (size_t) h);
    for (int y = 0; y < h; ++y)
    {
        raw.push_back (0); // no filter
        for (int x = 0; x < w; ++x)
        {
            const uint32_t c = argb[(y / scale) * stride + x / scale];
            raw.push_back ((uint8_t) (c >> 16)), raw.push_back ((uint8_t) (c >> 8)), raw.push_back ((uint8_t) c);
        }
    }
    std::vector<uint8_t> z { 0x78, 0x01 };
    for (size_t at = 0; at < raw.size() || at == 0;)
    {
        const size_t n = std::min<size_t> (65535, raw.size() - at);
        const bool last = at + n >= raw.size();
        z.push_back (last ? 1 : 0);
        z.push_back ((uint8_t) n), z.push_back ((uint8_t) (n >> 8)), z.push_back ((uint8_t) ~n), z.push_back ((uint8_t) (~n >> 8));
        z.insert (z.end(), raw.begin() + (long) at, raw.begin() + (long) (at + n));
        at += n;
        if (last)
            break;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw)
        a = (a + v) % 65521, b = (b + a) % 65521;
    const uint32_t adler = (b << 16) | a;
    for (int s = 24; s >= 0; s -= 8)
        z.push_back ((uint8_t) (adler >> s));

    auto crc = [] (const std::vector<uint8_t>& d) {
        uint32_t c = 0xffffffffu;
        for (uint8_t v : d)
        {
            c ^= v;
            for (int k = 0; k < 8; ++k)
                c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
        }
        return ~c;
    };
    std::FILE* f = std::fopen (path, "wb");
    if (f == nullptr)
        return false;
    auto u32 = [f] (uint32_t v) { const uint8_t b4[4] = { (uint8_t) (v >> 24), (uint8_t) (v >> 16), (uint8_t) (v >> 8), (uint8_t) v }; std::fwrite (b4, 1, 4, f); };
    auto chunk = [&] (const char* type, const std::vector<uint8_t>& data) {
        u32 ((uint32_t) data.size());
        std::vector<uint8_t> all (type, type + 4);
        all.insert (all.end(), data.begin(), data.end());
        std::fwrite (all.data(), 1, all.size(), f);
        u32 (crc (all));
    };
    static const uint8_t signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    std::fwrite (signature, 1, 8, f);
    std::vector<uint8_t> header;
    for (uint32_t v : { (uint32_t) w, (uint32_t) h })
        for (int s = 24; s >= 0; s -= 8)
            header.push_back ((uint8_t) (v >> s));
    header.insert (header.end(), { 8, 2, 0, 0, 0 });
    chunk ("IHDR", header);
    chunk ("IDAT", z);
    chunk ("IEND", {});
    return std::fclose (f) == 0;
}
