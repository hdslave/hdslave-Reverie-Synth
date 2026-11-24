// Desktop faceplate. Drawing and input share the same native-pixel geometry.
#pragma once

#include "hdslave_surface.h"
#include "ui/desktop_art.h"

namespace reverie
{
struct Surface
{
    using Raster = hdui::Raster;
    using Ink = hdui::Ink;
    using Cell = Screen::Cell;
    using Spot = hdslave_tile_hotspot_t;

    static constexpr SurfaceMetrics metrics {
        640, 432,
        { {429, 5, 17, 18}, {448, 5, 166, 18}, {616, 5, 17, 18},
          {429, 26, 100, 16}, {532, 26, 101, 16}, {429, 45, 204, 16} }
    };
    static constexpr int headingY[] = {66, 204, 318};
    static constexpr int chordTop = 80;
    static constexpr SurfaceRect motionLens {324, 83, 120, 118};
    static constexpr SurfaceRect keyboard {4, chordTop + 108, 312, 14};
    static constexpr SurfaceRect scopes[] = { {4, 221, 154, 24}, {162, 221, 154, 24} };
    static constexpr SurfaceRect response {324, 221, 148, 45};
    static constexpr SurfaceRect filterEnvelope {324, 270, 148, 45};
    static constexpr SurfaceRect ampEnvelope {4, 335, 148, 44};
    static constexpr SurfaceRect lfo {4, 384, 148, 45};

    static Cell cellAt (int page, const Cell& source)
    {
        Cell c = source;
        c.x += (page % 2) * 320;
        switch (page)
        {
            case Screen::chordsPage:
            case Screen::motionPage:
                c.y += chordTop;
                break;
            case Screen::oscPage:
                c.y = c.y >= 107 ? 306 : 246 + (c.y - 35) / 14 * 12;
                c.h = 12;
                break;
            case Screen::filterPage:
                c.y = c.y < 78 ? 233 + (c.y - 13) / 16 * 12 : 283 + (c.y - 78) / 16 * 14;
                c.h = c.y < 283 ? 12 : 14;
                break;
            case Screen::ampPage:
                c.y = c.y < 74 ? 348 + (c.y - 14) / 16 * 15 : 395 + (c.y - 74) / 16 * 12;
                c.h = c.y < 395 ? 15 : 12;
                break;
            case Screen::fxPage:
                c.h = c.y == 106 ? 14 : 12;
                c.y = c.y == 106 ? 417 : 333 + c.y / 15 * 12;
                break;
        }
        return c;
    }

    // Truncate proportionally, including the ellipsis, so one control can never paint on its neighbor.
    static void fitText (const Raster& r, int x, int y, const char* value, int width, Ink color, int size = 10)
    {
        if (r.measure (value, size) <= width)
        {
            r.text (x, y, value, color, size);
            return;
        }
        char s[128];
        std::snprintf (s, sizeof (s) - 3, "%s", value);
        size_t n = std::strlen (s);
        while (n && r.measure (s, size) + r.measure ("...", size) > width) s[--n] = 0;
        std::strcat (s, "...");
        r.text (x, y, s, color, size);
    }

    static void header (const Raster& r, const Screen& s, const SurfaceHeader& state)
    {
        using namespace hdui;
        std::copy_n (desktopArt::masthead, std::size (desktopArt::masthead), r.px);
        const auto& progression = progressionTable[s.choice (ParamId::progression)];
        char key[8], mode[22];
        s.formatted (ParamId::key, key, sizeof (key));
        const int mood = s.choice (ParamId::mood);
        if (mood) std::snprintf (mode, sizeof (mode), "Mood %+d", s.choice (ParamId::moodShift));
        else std::snprintf (mode, sizeof (mode), "%s %s", key, progression.minor ? "minor" : "major");
        const int keyW = r.measure (mode, 10) + 14;
        r.gel (13, 44, keyW, 15, 0xff1f728b, 0xfff4ffff, 0xff8adcdf, 0xffc3eff4);
        r.centre (13 + keyW / 2, 47, mode, ink::deep, 10);
        const int px = 20 + keyW;
        r.text (px, 47, mood ? moodNames[mood] : progression.name, ink::deep, 12);
        r.text (px, 46, mood ? moodNames[mood] : progression.name, ink::white, 12);

        char clearText[24];
        if (state.mappedCc) std::snprintf (clearText, sizeof (clearText), "Clear CC (%d)", state.mappedCc);
        else std::snprintf (clearText, sizeof (clearText), "Clear CC");
        const char* labels[] = {"<", state.preset, ">", state.learning ? "Learning..." : "MIDI Learn", clearText};
        for (int i = 0; i < (int) SurfaceControl::Readout; ++i)
        {
            const auto& b = metrics.controls[i];
            const bool active = (i == (int) SurfaceControl::Learn && state.learning)
                             || (i == (int) SurfaceControl::Preset && state.presetsOpen);
            const bool hot = i == (int) state.hovered;
            r.gel (b.x, b.y, b.w, b.h, active ? ink::teal : 0xff1b7898,
                   active ? 0xffedffc6 : hot ? 0xffdcffff : 0xffa3eafb,
                   active ? 0xff9ed45b : hot ? 0xff218ca8 : 0xff12698c,
                   active ? 0xffd6ffaf : 0xff2794ae);
            const int size = i <= (int) SurfaceControl::Next ? 12 : 10;
            const int x = b.x + std::max (3, (b.w - r.measure (labels[i], size)) / 2);
            const int y = b.y + (b.h - size) / 2;
            fitText (r, x, y + 1, labels[i], b.w - (x - b.x) - 2, active ? ink::ice : ink::deep, size);
            fitText (r, x, y, labels[i], b.w - (x - b.x) - 2, active ? ink::deep : ink::white, size);
        }
        const auto& b = metrics.controls[(int) SurfaceControl::Readout];
        r.gel (b.x, b.y, b.w, b.h, 0xff6fbed4, 0xff215476, 0xff093d5d, 0xff12536d);
        fitText (r, b.x + 6, b.y + 3, state.readout, b.w - 12, state.learning ? ink::lime : ink::ice);
    }

    static void heading (const Raster& r, int page, const char* name)
    {
        using namespace hdui;
        const int x = (page % 2) * 320, y = headingY[page / 2];
        // Light travels through the same sheet beneath both halves of the rail.
        for (int j = 0; j < 15; ++j)
        {
            const Ink tint = j < 7 ? mix (0xff83c9d7, 0xff258199, j * 30)
                                   : mix (0xff146879, 0xff40979e, (j - 7) * 29);
            for (int i = 0; i < 320; ++i)
                r.blend (x + i, y + j, tint, 245 - (i * 58 / 320));
        }
        r.box (x, y, 320, 1, 0xffb4eef5);
        r.box (x, y + 14, 320, 1, 0xff39798c);
        const int cx = x + 13, cy = y + 7;
        const Ink glow = 0xffe8ffbe;
        switch (page)
        {
            case Screen::chordsPage:
                for (int i = 0; i < 4; ++i) r.box (cx - 5 + (i % 2) * 6, cy - 4 + (i / 2) * 5, 5, 4, glow);
                break;
            case Screen::motionPage:
                r.ring (cx, cy, 5, glow); r.disc (cx + 3, cy - 3, 2, ink::white);
                break;
            case Screen::oscPage:
                for (int i = -6; i < 6; ++i)
                    r.line (cx + i, cy + int (3 * std::sin (i * .65)), cx + i + 1, cy + int (3 * std::sin ((i + 1) * .65)), glow);
                break;
            case Screen::filterPage:
                r.line (cx - 6, cy - 2, cx, cy - 2, glow); r.line (cx, cy - 2, cx + 2, cy - 4, glow);
                r.line (cx + 2, cy - 4, cx + 6, cy + 4, glow);
                break;
            case Screen::ampPage:
                r.line (cx - 6, cy + 4, cx - 3, cy - 4, glow); r.line (cx - 3, cy - 4, cx, cy, glow);
                r.line (cx, cy, cx + 3, cy, glow); r.line (cx + 3, cy, cx + 6, cy + 4, glow);
                break;
            case Screen::fxPage:
                for (int i = 0; i < 4; ++i) r.box (cx - 5 + i * 3, cy + 2 - i * 2, 2, 3 + i * 2, glow);
                break;
        }
        r.text (x + 25, y + 3, name, ink::deep, 12);
        r.text (x + 25, y + 2, name, ink::white, 12);
    }

    static const auto& chassis()
    {
        using namespace hdui;
        // Stationary glass, printed section titles and control beds are baked once.
        static const auto base = [] {
            std::vector<Ink> px (size_t (metrics.width) * metrics.height);
            Raster r {px.data(), metrics.width, metrics.height};
            for (int y = headingY[0]; y < r.h; ++y)
                for (int x = 0; x < r.w; ++x)
                    r.px[y * r.w + x] = desktopArt::glass[(y - headingY[0]) / 2 * 320 + x / 2];
            constexpr const char* titles[] = {"Chord pads", "Motion", "Oscillators", "Filter & envelope", "Amplitude & LFO", "Effects & output"};
            for (int page = 0; page < Screen::pageCount; ++page)
            {
                heading (r, page, titles[page]);
                const auto layout = Screen::layout (page);
                for (int i = 0; i < layout.count; ++i)
                {
                    const Cell c = cellAt (page, layout.cells[i]);
                    for (int j = 0; j < c.h - 1; ++j)
                        for (int xx = 0; xx < c.w - 1; ++xx)
                            r.blend (c.x + xx, c.y + j, ink::white, 162 - j * 68 / c.h);
                    r.box (c.x, c.y + c.h - 1, c.w - 1, 1, 0xff95bec6);
                    r.text (c.x + 3, c.y + (c.h - 10) / 2, c.label, ink::label, 10);
                }
            }
            r.box (319, 66, 1, r.h - 66, 0xff8bbbc5);
            return px;
        }();
        return base;
    }

    static void parameter (const Raster& r, const Screen& s, const Cell& c)
    {
        using namespace hdui;
        const bool enabled = s.controlEnabled (c.id);
        if (!enabled)
        {
            r.box (c.x, c.y, c.w - 1, c.h - 1, Screen::mutedBed);
            r.box (c.x, c.y + c.h - 1, c.w - 1, 1, Screen::mutedRule);
            r.text (c.x + 3, c.y + (c.h - 10) / 2, c.label, Screen::mutedText, 10);
        }
        const auto& p = paramTable[c.id];
        const int labelW = r.measure (c.label, 10);
        char value[64];
        s.formatted (c.id, value, sizeof (value));
        const int room = c.w - labelW - 8;
        const int size = c.h >= 15 && r.measure (value, 12) <= room ? 12 : 10;
        const int vx = c.x + std::max (labelW + 5, c.w - 3 - r.measure (value, size));
        fitText (r, vx, c.y + (c.h - size) / 2, value, c.x + c.w - vx - 1, enabled ? ink::deep : Screen::mutedText, size);
        if (p.kind == Kind::continuous)
        {
            const float unit = (s.live (c.id) - p.min) / (p.max - p.min);
            r.box (c.x, c.y + c.h - 1, int ((c.w - 2) * unit), 1, 0xff64abb5);
        }
    }

    static void CLAP_ABI paint (const clap_plugin_t* plugin, uint32_t* pixels, const SurfaceHeader* state)
    {
        using namespace hdui;
        const Screen& s = of (plugin).screen;
        const uint32_t sounding = s.tel.sounding.load (std::memory_order_relaxed);
        Raster r {pixels, metrics.width, metrics.height};
        const auto& base = chassis();
        std::copy (base.begin() + 66 * r.w, base.end(), pixels + 66 * r.w);
        header (r, s, *state);

        Raster pads {pixels + chordTop * r.w, r.w, r.h - chordTop};
        s.padGrid (pads, s.tel.padsHeld.load (std::memory_order_relaxed));
        s.keyboard (r, keyboard.x, keyboard.y, keyboard.w, keyboard.h, sounding);
        s.field (r, motionLens.x, motionLens.y, motionLens.w, motionLens.h, sounding, desktopArt::lens);
        for (int i = 0; i < 2; ++i)
        {
            const auto& b = scopes[i];
            s.scope (r, b.x, b.y, b.w, b.h, i);
        }
        s.response (r, response.x, response.y, response.w, response.h);
        s.envelope (r, filterEnvelope.x, filterEnvelope.y, filterEnvelope.w, filterEnvelope.h, ParamId::fltAttack,
                    s.tel.fltEnv.load (std::memory_order_relaxed), ink::lime, sounding != 0);
        s.envelope (r, ampEnvelope.x, ampEnvelope.y, ampEnvelope.w, ampEnvelope.h, ParamId::ampAttack,
                    s.tel.ampEnv.load (std::memory_order_relaxed), ink::foam, sounding != 0);
        s.lfoPicture (r, lfo.x, lfo.y, lfo.w, lfo.h);
        Screen::section (r, 477, 221, 636, "Filter response");
        Screen::section (r, 477, 270, 636, "Filter envelope");
        Screen::section (r, 157, 335, 316, "Amplitude envelope");
        Screen::section (r, 157, 383, 316, "LFO");
        for (int y = 0; y < 63; ++y)
            std::copy_n (desktopArt::tank + y * 103, 103, pixels + (335 + y) * r.w + 324);
        r.frame (324, 335, 103, 63, ink::guide);
        s.outputMeter (r, 324, 399);
        Screen::section (r, 432, 334, 636, "Ensemble");
        Screen::section (r, 432, 358, 636, "Tempo delay");
        Screen::section (r, 432, 382, 636, "Shimmer reverb");
        Screen::section (r, 385, 419, 422, "Phaser");
        for (int page = 0; page < Screen::pageCount; ++page)
        {
            const auto layout = Screen::layout (page);
            for (int i = 0; i < layout.count; ++i)
                parameter (r, s, cellAt (page, layout.cells[i]));
        }
    }

    static bool CLAP_ABI controlEnabled (const clap_plugin_t* plugin, clap_id id)
    {
        const int row = paramIndex (id);
        return row >= 0 && of (plugin).screen.controlEnabled (row);
    }

    static bool CLAP_ABI hitTest (const clap_plugin_t* plugin, int x, int y, Spot* out)
    {
        if (!out || x < 0 || y < headingY[0] || x >= metrics.width || y >= metrics.height) return false;
        auto spot = [&] (SurfaceRect b, clap_id id, clap_id idY = CLAP_INVALID_ID, bool absolute = false) {
            *out = { HDSLAVE_HOTSPOT_PARAM, id, idY, b.x, b.y, b.w, b.h, 0, absolute, true };
            return true;
        };
        auto note = [&] (SurfaceRect b, int midi) {
            *out = { HDSLAVE_HOTSPOT_NOTE_PAD, CLAP_INVALID_ID, CLAP_INVALID_ID, b.x, b.y, b.w, b.h, midi, false, false };
            return true;
        };
        const int row = y < headingY[1] ? 0 : y < headingY[2] ? 1 : 2;
        const int page = row * 2 + (x >= 320);
        const auto layout = Screen::layout (page);
        for (int i = 0; i < layout.count; ++i)
        {
            const auto c = cellAt (page, layout.cells[i]);
            const SurfaceRect b {c.x, c.y, c.w, c.h};
            if (b.contains (x, y)) return of (plugin).screen.controlEnabled (c.id) && spot (b, paramTable[c.id].id);
        }

        if (page == Screen::chordsPage)
        {
            for (int i = 0; i < padSlots; ++i)
            {
                const SurfaceRect pad {int16_t (4 + (i % 4) * 42), int16_t (chordTop + 3 + (3 - i / 4) * 25), 40, 24};
                if (pad.contains (x, y)) return note (pad, 36 + i);
            }
            if (keyboard.contains (x, y))
            {
                // Same C1..B6 keyboard as the live note display. Black keys overlap white ones.
                constexpr int whiteNotes[] = {0, 2, 4, 5, 7, 9, 11};
                constexpr int kw = keyboard.w / 42, left = keyboard.x + (keyboard.w - 42 * kw) / 2;
                for (int wi = 0; wi < 42; ++wi)
                {
                    const int midi = 24 + wi / 7 * 12 + whiteNotes[wi % 7];
                    const SurfaceRect black {int16_t (left + (wi + 1) * kw - 2), keyboard.y, 4, keyboard.h * 3 / 5};
                    if (wi % 7 != 2 && wi % 7 != 6 && black.contains (x, y)) return note (black, midi + 1);
                }
                if (x >= left && x < left + kw * 42)
                {
                    const int wi = (x - left) / kw;
                    return note ({int16_t (left + wi * kw), keyboard.y, kw, keyboard.h},
                                 24 + wi / 7 * 12 + whiteNotes[wi % 7]);
                }
            }
        }
        if (motionLens.contains (x, y))
            return spot ({motionLens.x + 15, motionLens.y + 8, 90, 90}, ParamId::motionX, ParamId::motionY, true);
        for (int i = 0; i < 2; ++i)
            if (scopes[i].contains (x, y)) return spot (scopes[i], ParamId::osc (i, 1), ParamId::osc (i, 0));
        if (response.contains (x, y)) return spot (response, ParamId::cutoff, ParamId::resonance);
        if (lfo.contains (x, y)) return spot (lfo, ParamId::lfoRate, ParamId::lfoCutoff);
        auto adsr = [&] (SurfaceRect b, clap_id first) {
            const int third = (x - b.x) * 3 / b.w;
            return spot (b, first + (third == 2 ? 3 : third), first + 2);
        };
        if (ampEnvelope.contains (x, y)) return adsr (ampEnvelope, ParamId::ampAttack);
        if (filterEnvelope.contains (x, y)) return adsr (filterEnvelope, ParamId::fltAttack);
        return false;
    }
};

const clap_plugin_hdslave_surface_t surfaceExtension {Surface::metrics, Surface::paint, Surface::hitTest, nullptr, Surface::controlEnabled};
} // namespace reverie
