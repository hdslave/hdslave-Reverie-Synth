/*
 * SPDX-License-Identifier: MIT
 *
 * Embedded custom body-screen CLAP extension ("org.sixteen.screen/1").
 *
 * A CLAP plugin may return a pointer to `sixteen_plugin_screen_t` from
 * `plugin->get_extension(plugin, SIXTEEN_EXT_SCREEN)` to draw its own
 * body canvas inside the host's standard screen chrome (StatusRow on top,
 * ContextBar below it, and the 6-column KnobStrip on the bottom).
 *
 * Canvas geometry:
 *   - Fixed 780 x 285 physical pixels (BodyTemplate::Full = 1248 x 456 lpx at 0.625 scale).
 *   - Pixel format: 32-bit little-endian 0xAARRGGBB (straight alpha, opaque = 0xFFRRGGBB).
 *   - All calls happen on the UI / message thread; never called on the audio thread.
 *
 * Controls owned by the plugin while its page is open:
 *   - Touchscreen down / move / up inside the 780 x 285 body canvas (0..779 x 0..284).
 *   - Main selector encoder turn & push.
 *   - Active 6-knob bank returned via `knobs(...)`: the host displays those 6 CLAP parameters
 *     in its native 6-column KnobStrip and routes rotary encoder turns to CLAP parameter updates.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIXTEEN_EXT_SCREEN "org.sixteen.screen/1"

#define SIXTEEN_SCREEN_WIDTH  780
#define SIXTEEN_SCREEN_HEIGHT 285
#define SIXTEEN_KNOB_COUNT    6
#define SIXTEEN_TAB_NAME_SIZE 24

/* Sentinel for an unbound / empty knob column in `knobs(...)` (matches CLAP_INVALID_ID). */
#define SIXTEEN_INVALID_PARAM_ID ((uint32_t)0xFFFFFFFFu)

enum sixteen_touch_phase
{
    SIXTEEN_TOUCH_DOWN = 0,
    SIXTEEN_TOUCH_MOVE = 1,
    SIXTEEN_TOUCH_UP   = 2
};

typedef struct sixteen_screen_surface
{
    uint32_t* pixels; /* 0xAARRGGBB, at least stride * height pixels */
    uint16_t width;   /* SIXTEEN_SCREEN_WIDTH (780) */
    uint16_t height;  /* SIXTEEN_SCREEN_HEIGHT (285) */
    uint16_t stride;  /* row stride in pixels (>= width) */
    uint32_t now_ms;  /* monotonic host UI time in milliseconds */
} sixteen_screen_surface_t;

typedef struct sixteen_plugin_screen
{
    /* Optional lifecycle hooks when the plugin page opens / closes on screen. May be NULL. */
    void (*on_open)(const void* clap_plugin);
    void (*on_close)(const void* clap_plugin);

    /*
     * Render the 780 x 285 body into `surface->pixels`.
     * Return true if any pixel changed (host bumps its blit serial and redraws the body rect);
     * return false if the surface is unchanged since the last render (host skips texture upload
     * and redraw). The host preserves the surface pixels between calls while the page stays open.
     * Passing `force_full = true` requires the plugin to paint the complete surface.
     */
    bool (*render)(const void* clap_plugin, const sixteen_screen_surface_t* surface, bool force_full);

    /*
     * Touch input in body-local physical pixels (x in 0..779, y in 0..284).
     * Return true if consumed / state changed. May be NULL.
     */
    bool (*touch)(const void* clap_plugin, int32_t phase, int32_t x, int32_t y);

    /*
     * Main selector rotary turn (`delta` detents, positive = clockwise) and push.
     * Return true if consumed / state changed. May be NULL.
     */
    bool (*selector_turn)(const void* clap_plugin, int32_t delta);
    bool (*selector_push)(const void* clap_plugin);

    /*
     * Report the 6 CLAP parameter IDs currently bound to hardware knobs 1..6 and the strip tab
     * title (NUL-terminated, max SIXTEEN_TAB_NAME_SIZE bytes including NUL).
     * Fill unused columns with SIXTEEN_INVALID_PARAM_ID.
     * Return true to override the default 6-per-page CLAP parameter pager. May be NULL.
     */
    bool (*knobs)(const void* clap_plugin, uint32_t out_param_ids[SIXTEEN_KNOB_COUNT],
                  char out_tab[SIXTEEN_TAB_NAME_SIZE]);
} sixteen_plugin_screen_t;

#ifdef __cplusplus
}
#endif
