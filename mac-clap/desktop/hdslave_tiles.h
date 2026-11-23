#pragma once

#include <clap/clap.h>
#include <cstdint>

#ifdef HDSLAVE_DESKTOP_SHELL
#include "melodee_screen.h"
#endif

static const char CLAP_EXT_HDSLAVE_TILES[] = "com.hdslave.tiles/1";

enum HdslaveHotspotKind : uint8_t
{
    HDSLAVE_HOTSPOT_NONE = 0,
    HDSLAVE_HOTSPOT_PARAM = 1,      // Drag vertically (and horizontally if param_id_y != CLAP_INVALID_ID), wheel, double-click reset, click cycles choice/toggle
    HDSLAVE_HOTSPOT_BIT_TOGGLE = 2, // Click/drag toggles bit (custom_id = bit index) in integer bitmask param_id
    HDSLAVE_HOTSPOT_BUTTON = 3,     // Click fires momentary button parameter param_id
    HDSLAVE_HOTSPOT_NOTE_PAD = 4,   // Mouse down/up triggers MIDI note (custom_id = MIDI note 0..127)
    HDSLAVE_HOTSPOT_ACTION = 5,     // Click triggers custom screen action via action(plugin, tile, custom_id, shift)
};

typedef struct hdslave_tile_hotspot
{
    uint8_t kind;
    clap_id param_id;
    clap_id param_id_y;
    int16_t x, y, w, h;
    int32_t custom_id;
    bool is_absolute;
    bool invert_y;
} hdslave_tile_hotspot_t;

typedef struct clap_plugin_hdslave_tiles
{
    // Returns total number of 320x218 MPC screen tiles and recommended grid layout (cols x rows).
    uint32_t (CLAP_ABI* count) (const clap_plugin_t* plugin, int* out_cols, int* out_rows);

    // Short title for tile (0 .. count-1).
    const char* (CLAP_ABI* title) (const clap_plugin_t* plugin, uint32_t tile);

    // Paints 320x218 MPC screen tile into canvas->pixels and fills knobs_out[3] with that tile's K1-K3 parameter IDs.
    bool (CLAP_ABI* paint) (const clap_plugin_t* plugin, uint32_t tile, const melodee_screen_canvas_t* canvas, clap_id knobs_out[3]);

    // Hit-tests (x, y) in 320x218 tile coordinates and fills out_spot.
    bool (CLAP_ABI* hit_test) (const clap_plugin_t* plugin, uint32_t tile, int x, int y, hdslave_tile_hotspot_t* out_spot);

    // Moves the tile's internal MPC cursor/focus to the control at (x, y).
    void (CLAP_ABI* focus) (const clap_plugin_t* plugin, uint32_t tile, int x, int y, bool shift);

    // Executes a custom screen action (when hit_test returned HDSLAVE_HOTSPOT_ACTION), or wheel delta when wheel_delta != 0.
    void (CLAP_ABI* action) (const clap_plugin_t* plugin, uint32_t tile, int32_t custom_id, int32_t wheel_delta, bool shift);

    // Active screen height without the MPC hardware K1-K3 footer panel (defaults to MELODEE_SCREEN_HEIGHT = 218 when 0).
    uint32_t content_h;
} clap_plugin_hdslave_tiles_t;
