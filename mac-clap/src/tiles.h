#pragma once

#include "hdslave_tiles.h"
#include "screen.h"

namespace reverie
{

struct TilesAccess
{
    static constexpr int kTileCount = Screen::pageCount; // 6 tiles: 3x2 grid

    static uint32_t CLAP_ABI count (const clap_plugin_t*, int* out_cols, int* out_rows)
    {
        if (out_cols) *out_cols = 3;
        if (out_rows) *out_rows = 2;
        return kTileCount;
    }

    static const char* CLAP_ABI title (const clap_plugin_t*, uint32_t tile)
    {
        static constexpr const char* kTitles[kTileCount] = {
            "01 // CHORDS & PADS",
            "02 // MOTION X/Y",
            "03 // OSCILLATORS",
            "04 // FILTER & ENV",
            "05 // AMP & LFO",
            "06 // ENSEMBLE / DELAY / REVERB"
        };
        return tile < kTileCount ? kTitles[tile] : "";
    }

    static bool CLAP_ABI paint (const clap_plugin_t* plugin, uint32_t tile, const melodee_screen_frame_t* frame, clap_id knobs_out[3])
    {
        if (tile >= kTileCount || !frame || !frame->pixels) return false;
        auto& p = of (plugin);
        const int savedPage = p.screen.page;
        p.screen.page = (int) tile;
        p.screen.shownPage = -1;
        melodee_screen_frame_t c = *frame;
        c.redraw_all = 1;
        const hdui::Host h = hostOf (&c);
        const bool drew = p.screen.paint (h, c.pixels, (int) c.stride, true);
        if (knobs_out)
            screenControls (plugin, knobs_out);
        p.screen.page = savedPage;
        return drew;
    }

    static bool CLAP_ABI hit_test (const clap_plugin_t* plugin, uint32_t tile, int x, int y, hdslave_tile_hotspot_t* out_spot)
    {
        if (!out_spot || tile >= kTileCount) return false;
        *out_spot = {};
        out_spot->param_id = CLAP_INVALID_ID;
        out_spot->param_id_y = CLAP_INVALID_ID;

        // 1. Parameter cells from Screen::layout(tile) (offset vertically by Screen::headH = 59)
        const Screen::Layout l = Screen::layout ((int) tile);
        for (int i = 0; i < l.count; ++i)
        {
            const auto& c = l.cells[i];
            const int cx = c.x, cy = Screen::headH + c.y, cw = c.w, ch = c.h;
            if (x >= cx && x < cx + cw && y >= cy && y < cy + ch)
            {
                out_spot->kind = HDSLAVE_HOTSPOT_PARAM;
                out_spot->param_id = c.id;
                out_spot->x = (int16_t) cx;
                out_spot->y = (int16_t) cy;
                out_spot->w = (int16_t) cw;
                out_spot->h = (int16_t) ch;
                return true;
            }
        }

        // 3. Visual interactive widgets in the body (y: 59..182)
        const int by = y - Screen::headH;
        if (by >= 0 && by < Screen::bodyH)
        {
            if (tile == Screen::chordsPage)
            {
                // 4x4 MPC Chord Pad Grid (x: 4..168, by: 3..98)
                if (x >= 4 && x < 168 && by >= 3 && by < 99)
                {
                    const int col = std::clamp ((x - 4) / 41, 0, 3);
                    const int rowFromTop = std::clamp ((by - 3) / 24, 0, 3);
                    const int pad = (3 - rowFromTop) * 4 + col; // Pad 1 (0) is bottom-left
                    out_spot->kind = HDSLAVE_HOTSPOT_NOTE_PAD;
                    out_spot->custom_id = 60 + pad;
                    out_spot->x = (int16_t) (4 + col * 41);
                    out_spot->y = (int16_t) (Screen::headH + 3 + rowFromTop * 24);
                    out_spot->w = 40;
                    out_spot->h = 22;
                    return true;
                }
                // Mini keyboard (x: 4..168, by: 101..121)
                if (x >= 4 && x < 168 && by >= 101 && by < 122)
                {
                    const int note = 48 + std::clamp ((x - 4) * 36 / 164, 0, 35);
                    out_spot->kind = HDSLAVE_HOTSPOT_NOTE_PAD;
                    out_spot->custom_id = note;
                    out_spot->x = 4;
                    out_spot->y = (int16_t) (Screen::headH + 101);
                    out_spot->w = 164;
                    out_spot->h = 20;
                    return true;
                }
            }
            else if (tile == Screen::motionPage)
            {
                // 2D Glass Lens X/Y Field (x: 4..124, by: 3..121)
                if (x >= 4 && x < 124 && by >= 3 && by < 121)
                {
                    out_spot->kind = HDSLAVE_HOTSPOT_PARAM;
                    out_spot->param_id = ParamId::motionX;
                    out_spot->param_id_y = ParamId::motionY;
                    out_spot->x = 19;
                    out_spot->y = (int16_t) (Screen::headH + 11);
                    out_spot->w = 90;
                    out_spot->h = 90;
                    out_spot->is_absolute = true;
                    out_spot->invert_y = true;
                    return true;
                }
            }
            else if (tile == Screen::oscPage)
            {
                // Osc 1 & Osc 2 wavetable scopes (by: 2..34)
                if (by >= 2 && by < 35)
                {
                    if (x >= 4 && x < 158)
                    {
                        out_spot->kind = HDSLAVE_HOTSPOT_PARAM;
                        out_spot->param_id = ParamId::osc (0, 1); // Osc 1 Pos
                        out_spot->param_id_y = ParamId::osc (0, 0); // Osc 1 Wave
                        out_spot->x = 4;
                        out_spot->y = (int16_t) (Screen::headH + 2);
                        out_spot->w = 154;
                        out_spot->h = 32;
                        out_spot->invert_y = true;
                        return true;
                    }
                    if (x >= 162 && x < 316)
                    {
                        out_spot->kind = HDSLAVE_HOTSPOT_PARAM;
                        out_spot->param_id = ParamId::osc (1, 1); // Osc 2 Pos
                        out_spot->param_id_y = ParamId::osc (1, 0); // Osc 2 Wave
                        out_spot->x = 162;
                        out_spot->y = (int16_t) (Screen::headH + 2);
                        out_spot->w = 154;
                        out_spot->h = 32;
                        out_spot->invert_y = true;
                        return true;
                    }
                }
            }
            else if (tile == Screen::filterPage && x >= 4 && x < 152)
            {
                // Filter frequency response plot (by: 13..61) -> 2D Cutoff & Resonance
                if (by >= 13 && by < 62)
                {
                    out_spot->kind = HDSLAVE_HOTSPOT_PARAM;
                    out_spot->param_id = ParamId::cutoff;
                    out_spot->param_id_y = ParamId::resonance;
                    out_spot->x = 4;
                    out_spot->y = (int16_t) (Screen::headH + 13);
                    out_spot->w = 147;
                    out_spot->h = 48;
                    out_spot->invert_y = true;
                    return true;
                }
                // Filter envelope plot (by: 78..120)
                if (by >= 78 && by < 121)
                {
                    out_spot->kind = HDSLAVE_HOTSPOT_PARAM;
                    out_spot->param_id = x < 54 ? ParamId::fltAttack : (x < 104 ? ParamId::fltDecay : ParamId::fltRelease);
                    out_spot->param_id_y = ParamId::fltSustain;
                    out_spot->x = 4;
                    out_spot->y = (int16_t) (Screen::headH + 78);
                    out_spot->w = 147;
                    out_spot->h = 42;
                    out_spot->invert_y = true;
                    return true;
                }
            }
            else if (tile == Screen::ampPage && x >= 4 && x < 152)
            {
                // Amp envelope plot (by: 14..60)
                if (by >= 14 && by < 61)
                {
                    out_spot->kind = HDSLAVE_HOTSPOT_PARAM;
                    out_spot->param_id = x < 54 ? ParamId::ampAttack : (x < 104 ? ParamId::ampDecay : ParamId::ampRelease);
                    out_spot->param_id_y = ParamId::ampSustain;
                    out_spot->x = 4;
                    out_spot->y = (int16_t) (Screen::headH + 14);
                    out_spot->w = 147;
                    out_spot->h = 46;
                    out_spot->invert_y = true;
                    return true;
                }
                // LFO plot (by: 74..120)
                if (by >= 74 && by < 121)
                {
                    out_spot->kind = HDSLAVE_HOTSPOT_PARAM;
                    out_spot->param_id = ParamId::lfoRate;
                    out_spot->param_id_y = ParamId::lfoCutoff;
                    out_spot->x = 4;
                    out_spot->y = (int16_t) (Screen::headH + 74);
                    out_spot->w = 147;
                    out_spot->h = 46;
                    out_spot->invert_y = true;
                    return true;
                }
            }
        }
        return false;
    }

    static void CLAP_ABI focus (const clap_plugin_t* plugin, uint32_t tile, int x, int y, bool)
    {
        if (tile >= kTileCount) return;
        auto& p = of (plugin);
        p.screen.page = (int) tile;
        const Screen::Layout l = Screen::layout ((int) tile);
        for (int i = 0; i < l.count; ++i)
        {
            const auto& c = l.cells[i];
            const int cx = c.x, cy = Screen::headH + c.y;
            if (x >= cx && x < cx + c.w && y >= cy && y < cy + c.h)
            {
                p.screen.focus[tile] = i;
                p.screen.touched = true;
                break;
            }
        }
    }

    static void CLAP_ABI action (const clap_plugin_t*, uint32_t, int32_t, int32_t, bool) {}
};

const clap_plugin_hdslave_tiles_t tilesExtension {
    TilesAccess::count,
    TilesAccess::title,
    TilesAccess::paint,
    TilesAccess::hit_test,
    TilesAccess::focus,
    TilesAccess::action,
    183,
};

} // namespace reverie
