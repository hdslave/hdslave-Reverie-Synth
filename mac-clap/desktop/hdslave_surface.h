// Private contract between the desktop wrapper and an instrument's faceplate.
// All rectangles and hotspots use native surface pixels. Main thread only.
#pragma once

#include "hdslave_tiles.h"

inline constexpr char CLAP_EXT_HDSLAVE_SURFACE[] = "com.hdslave.surface/3";

enum class SurfaceControl : uint8_t { Previous, Preset, Next, Learn, Clear, Readout, Count };

struct SurfaceRect
{
    int16_t x, y, w, h;
    constexpr bool contains (int px, int py) const
    {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

struct SurfaceMetrics
{
    int width, height;
    SurfaceRect controls[(int) SurfaceControl::Count];
};

struct SurfaceHeader
{
    const char* preset;  // UTF-8, borrowed for this paint
    const char* readout;
    int mappedCc;
    bool learning, presetsOpen;
    SurfaceControl hovered;
};

struct clap_plugin_hdslave_surface_t
{
    SurfaceMetrics metrics;
    void (CLAP_ABI* paint) (const clap_plugin_t*, uint32_t* pixels, const SurfaceHeader*);
    bool (CLAP_ABI* hit_test) (const clap_plugin_t*, int x, int y, hdslave_tile_hotspot_t*);
    void (CLAP_ABI* action) (const clap_plugin_t*, int32_t id, int32_t wheel, bool shift);
    bool (CLAP_ABI* control_enabled) (const clap_plugin_t*, clap_id);
    // Poll once per GUI tick. The shell tracks its own header and overlays.
    bool (CLAP_ABI* content_changed) (const clap_plugin_t*);
};
