// Tracks the last complete frame submitted to the desktop window.
#pragma once

#include "panel_renderer.h"

#include <cstring>
#include <vector>

namespace hdslave_desktop
{

class PanelPresentation
{
public:
    int width = 0, height = 0, scale = 0;
    std::vector<uint32_t> pixels;

    template <class Invalidate>
    void publish (const uint32_t* source, int w, int h, int density, Rect candidate, Invalidate invalidate)
    {
        if (!source || w <= 0 || h <= 0 || density <= 0) return;
        const Rect bounds {0, 0, w / density, h / density};
        if (w != width || h != height || density != scale)
        {
            pixels.assign (source, source + size_t (w) * h);
            width = w;
            height = h;
            scale = density;
            invalidate (bounds);
            return;
        }

        candidate = candidate.intersect (bounds);
        if (candidate.empty()) return;
        const int x = candidate.x * scale, count = candidate.w * scale;
        const size_t bytes = size_t (count) * sizeof (uint32_t);
        Rect pending {};
        // Separate horizontal bands keep independent animations from invalidating
        // the stationary controls between them. Emit adjoining equal spans together.
        for (int y = candidate.y; y < candidate.y + candidate.h;)
        {
            const int end = std::min (y + 16, candidate.y + candidate.h);
            int left = count, right = 0, top = end * scale, bottom = y * scale;
            for (int row = y * scale; row < end * scale; ++row)
            {
                const size_t offset = size_t (row) * w + x;
                const uint32_t* src = source + offset;
                uint32_t* dst = pixels.data() + offset;
                if (!std::memcmp (src, dst, bytes)) continue;
                int first = 0, last = count;
                while (first < left && src[first] == dst[first]) ++first;
                while (last > right && src[last - 1] == dst[last - 1]) --last;
                left = first;
                right = last;
                top = std::min (top, row);
                bottom = row + 1;
                std::memcpy (dst, src, bytes);
            }
            if (left < right)
            {
                const int x0 = (x + left) / scale, y0 = top / scale;
                const Rect changed {x0, y0, (x + right + scale - 1) / scale - x0,
                                    (bottom + scale - 1) / scale - y0};
                if (!pending.empty() && pending.x == changed.x && pending.w == changed.w &&
                    pending.y + pending.h == changed.y)
                    pending.h += changed.h;
                else
                {
                    if (!pending.empty()) invalidate (pending);
                    pending = changed;
                }
            }
            y = end;
        }
        if (!pending.empty()) invalidate (pending);
    }
};
} // namespace hdslave_desktop
