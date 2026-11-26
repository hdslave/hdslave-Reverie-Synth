#pragma once

#include "../desktop/panel_renderer.h"
#include <array>
#include <cstdio>

namespace
{
// Compare every destination pixel to direct area sampling, including the untouched margins.
bool checkPanelScaling()
{
    using hdslave_desktop::Canvas;
    using hdslave_desktop::Rect;
    constexpr int width = 51, height = 39;
    const Rect full { 0, 0, width, height };
    struct Geometry { int sw, sh, scale; Rect dst, clip; };
    const Geometry cases[] = {
        { 7, 5, 1, { -2, 3, 29, 19 }, full },
        { 7, 5, 1, { 4, -3, 29, 19 }, { 8, 2, 9, 7 } },
        { 17, 12, 1, { 0, 0, 29, 19 }, full },
        { 39, 23, 1, { 0, 0, 11, 7 }, full },
        { 7, 5, 2, { 1, -2, 19, 13 }, { 1, 1, 11, 6 } },
        { 1, 1, 2, { 3, 1, 11, 7 }, full },
        { 7, 5, 3, { 0, 0, 7, 5 }, full },
        { 7, 5, 1, { 3, 1, 19, 13 }, { 25, 17, 2, 2 } },
    };
    struct Span { int first, last; uint32_t weight; };
    auto span = [] (int pos, int dest, int source) -> Span {
        const float step = float (source) / float (dest);
        const float start = pos * step, end = start + step;
        const int first = std::clamp (int (start), 0, source - 1), last = std::clamp (int (end), 0, source - 1);
        const int weight = first == last ? 0 : std::clamp (int ((end - float (last)) / step * 256.0f), 0, 256);
        return { first, last, uint32_t (weight) };
    };
    Canvas canvas;
    std::array<uint32_t, width * height> pixels;
    uint32_t seed = 4107;
    for (const auto& g : cases)
    {
        const int scale = g.scale;
        const Rect physical { g.dst.x * scale, g.dst.y * scale, g.dst.w * scale, g.dst.h * scale };
        std::vector<uint32_t> source (g.sw * g.sh);
        for (int frame = 0; frame < 2; ++frame)
        {
            for (auto& p : source) p = (seed = seed * 1664525u + 1013904223u);
            constexpr uint32_t guard = 0x12345678;
            pixels.fill (guard);
            canvas.begin (pixels.data(), width, height, scale);
            canvas.setClip (g.clip);
            canvas.blitSharp (source.data(), g.sw, g.sh, g.dst);
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                {
                    uint32_t expected = guard;
                    if (physical.contains (x, y) && g.clip.contains (x / scale, y / scale))
                    {
                        const auto h = span (x - physical.x, physical.w, g.sw);
                        const auto v = span (y - physical.y, physical.h, g.sh);
                        auto row = [&] (int i) { return Canvas::blend (source[i * g.sw + h.first], source[i * g.sw + h.last], h.weight); };
                        expected = Canvas::blend (row (v.first), row (v.last), v.weight) | 0xff000000u;
                    }
                    if (pixels[y * width + x] != expected)
                    {
                        std::fprintf (stderr, "FAIL panel sampling at (%d,%d), source %dx%d, scale %d, frame %d\n", x, y, g.sw, g.sh, scale, frame);
                        return false;
                    }
                }
        }
    }
    return true;
}
} // namespace
