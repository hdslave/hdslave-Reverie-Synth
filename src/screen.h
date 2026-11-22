// Reverie's page on Melodee's screen (melodee_screen.h): 320 x 218 under the soft keys.
//
//   CHORDS  the sixteen pads as they sit on the MPC (pad 1 bottom left), each with the chord it plays in this key
//           and lit while held; the progression, key, voicing and the rest of the chord section; a keyboard
//           showing the notes that sound (or the last chord's).
//   MOTION  the X / Y field: the path, its centre (X, Y), a light for every note on it; the path's controls and
//           the four destinations.
//   OSC     both oscillators, each drawn at the place its Position (and Motion) has it; width, air and drift.
//   FILTER  the filter's response with the cutoff where the newest note has it, and the filter envelope.
//   AMP     the amplitude envelope and the LFO.
//   FX      ensemble, delay, reverb and shimmer, under an aurora that follows the output.
// The wheel moves between controls, Push edits one (and flips a switch at once), Minus and Plus turn the pages;
// K1-K3 hold the control picked and the next two (on MOTION: X, Y and Depth to begin with). Shift shows the
// controls. Nothing is redrawn while nothing changes; while notes sound the page moves at 25 frames a second.
#pragma once

#include "chords.h"
#include "engine.h"
#include "params.h"
#include "ui/page.h"
#include "waves.h"

#include <atomic>
#include <cmath>
#include <iterator>

namespace reverie
{
class Screen final
{
    using Raster = hdui::Raster;
    using Host = hdui::Host;
    using Ink = hdui::Ink;

public:
    Screen (const std::atomic<float>* values, const Telemetry& telemetry) : values (values), tel (telemetry)
    {
        for (int i = 0; i < paramCount; ++i) seen[i] = paramTable[i].def;
    }

    enum Page { chordsPage, motionPage, oscPage, filterPage, ampPage, fxPage, pageCount };

    void knobs (uint32_t ids[3]) const
    {
        const Layout l = layout (page);
        for (int k = 0; k < 3; ++k)
        {
            const int c = focus[page] + k;
            ids[k] = c < l.count ? l.cells[c].id : hdui::noParam;
        }
    }

    bool animating() const { return animated; }
    int currentPage() const { return page; }
    void showPage (int p) { page = std::clamp (p, 0, pageCount - 1); touched = true; }

    bool input (hdui::Key key, int amount, bool shift, hdui::Edits& out)
    {
        using hdui::Key;
        touched = true;
        if (key == Key::nudge && ! editing)
        {
            page = (page + (amount > 0 ? 1 : -1) + pageCount) % pageCount;
            return true;
        }
        const Layout l = layout (page);
        int& f = focus[page];
        f = std::clamp (f, 0, l.count - 1);
        const Cell& cell = l.cells[f];
        const Param& p = paramTable[cell.id];
        if (key == Key::push)
        {
            if (p.unit == Unit::onOff)
            {
                out.set (cell.id, live (cell.id) >= 0.5f ? 0.0f : 1.0f);
                return true;
            }
            editing = ! editing;
            return true;
        }
        if (! editing && key == Key::wheel)
        {
            f = std::clamp (f + amount, 0, l.count - 1);
            return true;
        }
        if (editing && (key == Key::wheel || key == Key::nudge))
        {
            const float delta = p.kind == Kind::continuous ? (p.max - p.min) / (key == Key::nudge ? 16.0f : (shift ? 1024.0f : 128.0f))
                                                           : (key == Key::nudge ? std::max (1.0f, std::round ((p.max - p.min) / 8.0f)) : 1.0f);
            const float before = live (cell.id);
            const float next = clampParam (p, before + delta * (float) amount);
            if (next != before) out.set (cell.id, next);
            return true;
        }
        return false;
    }

    bool paint (const Host& h, uint32_t* pixels, int stride, bool whole)
    {
        whole |= first;
        first = false;
        bool params = false;
        for (int i = 0; i < paramCount; ++i)
        {
            const float v = values[i].load (std::memory_order_relaxed);
            params |= v != seen[i];
            seen[i] = v;
        }
        const uint32_t sounding = tel.sounding.load (std::memory_order_relaxed);
        const uint32_t held = tel.padsHeld.load (std::memory_order_relaxed);
        const uint32_t strikes = tel.strikes.load (std::memory_order_relaxed);
        const float tail = tel.reverb.load (std::memory_order_relaxed);
        animated = sounding != 0 || tail > 1e-4f;
        const bool frame = animated && (h.nowMs - frameAt >= 40 || h.nowMs < frameAt);
        const bool notesMoved = sounding != shownSounding || held != shownHeld || strikes != shownStrikes;
        bool pots = false;
        for (int k = 0; k < 3; ++k)
            pots |= h.pot[k] != lastHost.pot[k] || h.engaged[k] != lastHost.engaged[k];
        const bool chrome = whole || page != shownPage || touched;
        if (! frame && ! pots && ! chrome && ! params && ! notesMoved && h.shift == lastHost.shift && h.accent == lastHost.accent
            && ! (wasAnimated && ! animated))
            return false;
        if (frame || notesMoved) frameAt = h.nowMs;
        wasAnimated = animated;
        nowMs = h.nowMs;
        accent = h.accent ? (h.accent | 0xff000000u) : hdui::ink::violet;
        bool drew = false;
        if (chrome || params || frame || notesMoved)
            drew |= picture.region<320, 26> (pixels, stride, whole, 0, 0, [&] (Raster& r) { header (r, sounding); });
        // The whole body when what it shows changed; between, only the pictures that move.
        if (chrome || params || notesMoved)
            drew |= picture.region<320, 142> (pixels, stride, whole, 0, 26, [&] (Raster& r) { body (r, sounding, held); });
        else if (frame)
            drew |= moving (pixels, stride, sounding, held);
        if (chrome || params || pots || h.shift != lastHost.shift || h.accent != lastHost.accent)
            drew |= picture.region<320, 48> (pixels, stride, whole, 0, 170, [&] (Raster& r) {
                if (h.shift) legend (r);
                else cards (r, h);
            });
        shownPage = page;
        shownSounding = sounding, shownHeld = held, shownStrikes = strikes;
        lastHost = h;
        touched = false;
        return drew;
    }

private:
    // --- Layout -----------------------------------------------------------------------------------------------------
    struct Cell { uint32_t id; int16_t x, y, w, h; const char* label; };
    struct Layout { const Cell* cells; int count; };

    static Layout layout (int p)
    {
        using namespace ParamId;
        constexpr int16_t L = 182, R = 250, W = 66, H = 18;
        static const Cell chordCells[] = {
            { progression, L, 2, 134, H, "PROG" }, { voicing, L, 22, 134, H, "VOICING" }, { key, L, 42, W, H, "KEY" },
            { octave, R, 42, W, H, "OCT" }, { strum, L, 62, W, H, "STRUM" }, { humanize, R, 62, W, H, "HUMAN" },
            { bass, L, 82, W, H, "BASS" }, { velocity, R, 82, W, H, "VEL" }, { latch, L, 102, W, H, "LATCH" },
            { glide, R, 102, W, H, "GLIDE" },
        };
        constexpr int16_t ML = 146, MR = 232, MW = 84, MH = 14;
        auto mrow = [] (int r) { return (int16_t) (2 + 15 * r); };
        static const Cell motionCells[] = {
            { motionX, ML, mrow (0), MW, MH, "X" }, { motionY, MR, mrow (0), MW, MH, "Y" },
            { motionDepth, ML, mrow (1), MW, MH, "DEPTH" }, { motionShape, MR, mrow (1), MW, MH, "PATH" },
            { motionRate, ML, mrow (2), MW, MH, "RATE" }, { motionSpread, MR, mrow (2), MW, MH, "SPREAD" },
            { motionSync, ML, mrow (3), MW, MH, "SYNC" }, { motionDiv, MR, mrow (3), MW, MH, "DIV" },
            { motionRetrig, ML, mrow (4), MW, MH, "RETRIG" },
            { xDest1, ML, mrow (5), MW, MH, "X>1" }, { xAmt1, MR, mrow (5), MW, MH, "AMT" },
            { xDest2, ML, mrow (6), MW, MH, "X>2" }, { xAmt2, MR, mrow (6), MW, MH, "AMT" },
            { yDest1, ML, mrow (7), MW, MH, "Y>1" }, { yAmt1, MR, mrow (7), MW, MH, "AMT" },
            { yDest2, ML, mrow (8), MW, MH, "Y>2" }, { yAmt2, MR, mrow (8), MW, MH, "AMT" },
        };
        static Cell oscCells[20];
        static bool made = false;
        if (! made)
        {
            made = true;
            // WAVE across the panel, then POS LEVEL / OCT SEMI / FINE UNISON / DETUNE
            static const char* const labels[8] = { "WAVE", "POS", "OCT", "SEMI", "FINE", "UNISON", "DETUNE", "LEVEL" };
            static const int order[8] = { 0, 1, 7, 2, 3, 4, 5, 6 };
            int n = 0;
            for (int o = 0; o < 2; ++o)
                for (int i = 0; i < 8; ++i)
                {
                    const int f = order[i];
                    const int16_t x = (int16_t) (4 + 158 * o + (i == 0 ? 0 : 78 * ((i - 1) % 2)));
                    const int16_t y = (int16_t) (40 + 16 * (i == 0 ? 0 : 1 + (i - 1) / 2));
                    oscCells[n++] = { ParamId::osc (o, (uint32_t) f), x, y, (int16_t) (i == 0 ? 154 : 76), 15, labels[f] };
                }
            oscCells[n++] = { width, 4, 120, 76, 18, "WIDTH" };
            oscCells[n++] = { air, 82, 120, 76, 18, "AIR" };
            oscCells[n++] = { airTone, 162, 120, 76, 18, "A.TONE" };
            oscCells[n++] = { drift, 240, 120, 76, 18, "DRIFT" };
        }
        constexpr int16_t FL = 160, FR = 238, FW = 76;
        static const Cell filterCells[] = {
            { filterType, FL, 2, FW, H, "FILTER" }, { cutoff, FR, 2, FW, H, "CUTOFF" }, { resonance, FL, 22, FW, H, "RESO" },
            { filterEnv, FR, 22, FW, H, "ENV" }, { keyTrack, FL, 42, FW, H, "KEYTRK" }, { velCutoff, FR, 42, FW, H, "VEL" },
            { fltAttack, FL, 76, FW, H, "ATTACK" }, { fltDecay, FR, 76, FW, H, "DECAY" }, { fltSustain, FL, 96, FW, H, "SUST" },
            { fltRelease, FR, 96, FW, H, "REL" },
        };
        static const Cell ampCells[] = {
            { ampAttack, FL, 6, FW, H, "ATTACK" }, { ampDecay, FR, 6, FW, H, "DECAY" }, { ampSustain, FL, 26, FW, H, "SUST" },
            { ampRelease, FR, 26, FW, H, "REL" },
            { lfoShape, FL, 76, FW, H, "SHAPE" }, { lfoRate, FR, 76, FW, H, "RATE" }, { lfoDelay, FL, 96, FW, H, "DELAY" },
            { lfoPitch, FR, 96, FW, H, "PITCH" }, { lfoCutoff, FL, 116, FW, H, "CUTOFF" }, { lfoWave, FR, 116, FW, H, "WAVE" },
        };
        constexpr int16_t XW = 102;
        auto fxc = [] (int c) { return (int16_t) (4 + 105 * c); };
        auto fxr = [] (int r) { return (int16_t) (46 + 19 * r); };
        static const Cell fxCells[] = {
            { ensMix, fxc (0), fxr (0), XW, H, "ENSEM" }, { ensDepth, fxc (1), fxr (0), XW, H, "DEPTH" }, { ensRate, fxc (2), fxr (0), XW, H, "RATE" },
            { dlyMix, fxc (0), fxr (1), XW, H, "DELAY" }, { dlyTime, fxc (1), fxr (1), XW, H, "TIME" }, { dlyFeedback, fxc (2), fxr (1), XW, H, "FDBK" },
            { revMix, fxc (0), fxr (2), XW, H, "REVERB" }, { revSize, fxc (1), fxr (2), XW, H, "SIZE" }, { revDecay, fxc (2), fxr (2), XW, H, "DECAY" },
            { revDamp, fxc (0), fxr (3), XW, H, "TONE" }, { revPredelay, fxc (1), fxr (3), XW, H, "PREDLY" }, { shimmer, fxc (2), fxr (3), XW, H, "SHIMR" },
            { volume, fxc (0), fxr (4), XW, H, "VOLUME" },
        };
        switch (p)
        {
            case chordsPage: return { chordCells, (int) std::size (chordCells) };
            case motionPage: return { motionCells, (int) std::size (motionCells) };
            case oscPage: return { oscCells, 20 };
            case filterPage: return { filterCells, (int) std::size (filterCells) };
            case ampPage: return { ampCells, (int) std::size (ampCells) };
            default: return { fxCells, (int) std::size (fxCells) };
        }
    }

    float live (uint32_t id) const { return values[id].load (std::memory_order_relaxed); }
    int choice (uint32_t id) const { return (int) std::lround (live (id)); }
    void formatted (uint32_t id, char* s, uint32_t n) const { formatParam (paramTable[id], live (id), s, n); }

    // The pictures that move while notes sound, each in its own region (the same places body() draws them).
    bool moving (uint32_t* pixels, int stride, uint32_t sounding, uint32_t held)
    {
        bool drew = false;
        switch (page)
        {
            case chordsPage:
                drew |= picture.region<176, 114> (pixels, stride, false, 0, 26, [&] (Raster& r) { padGrid (r, held); });
                drew |= picture.region<320, 22> (pixels, stride, false, 0, 26 + 120, [&] (Raster& r) { keyboard (r, 4, 2, 312, 18, sounding); });
                break;
            case motionPage:
                drew |= picture.region<142, 142> (pixels, stride, false, 0, 26, [&] (Raster& r) { field (r, 4, 2, 136, 136, sounding); });
                break;
            case oscPage:
                drew |= picture.region<320, 40> (pixels, stride, false, 0, 26, [&] (Raster& r) { scope (r, 4, 2, 154, 36, 0), scope (r, 162, 2, 154, 36, 1); });
                break;
            case filterPage:
                drew |= picture.region<160, 142> (pixels, stride, false, 0, 26, [&] (Raster& r) {
                    response (r, 4, 2, 152, 62);
                    envelope (r, 4, 72, 152, 66, ParamId::fltAttack, tel.fltEnv.load (std::memory_order_relaxed), hdui::ink::cyan, sounding != 0);
                });
                break;
            case ampPage:
                drew |= picture.region<160, 142> (pixels, stride, false, 0, 26, [&] (Raster& r) {
                    envelope (r, 4, 2, 152, 62, ParamId::ampAttack, tel.ampEnv.load (std::memory_order_relaxed), hdui::ink::rose, sounding != 0);
                    lfoPicture (r, 4, 72, 152, 66);
                });
                break;
            default: drew |= picture.region<320, 44> (pixels, stride, false, 0, 26, [&] (Raster& r) { aurora (r, 4, 2, 312, 40); }); break;
        }
        return drew;
    }

    // Pictures that never change, made once: the header's sky and the X / Y field's glow.
    struct Backdrops
    {
        Ink sky[320 * 26];
        Ink field[136 * 136];
        Backdrops()
        {
            using namespace hdui;
            for (int y = 0; y < 26; ++y)
                for (int x = 0; x < 320; ++x)
                    sky[y * 320 + x] = mix (0xff120c3a, ink::bg, std::clamp ((y * 256) / 26 + (bayer[y & 3][x & 3] - 8), 0, 255));
            for (int y = 0; y < 136; ++y)
                for (int x = 0; x < 136; ++x)
                {
                    const float dx = (float) (x - 68) / 136.0f, dy = (float) (y - 68) / 136.0f;
                    const int t = std::clamp ((int) (std::sqrt (dx * dx + dy * dy) * 420.0f) + bayer[y & 3][x & 3] - 8, 0, 255);
                    field[y * 136 + x] = mix (0xff1d1452, ink::field, t);
                }
        }
    };
    static const Backdrops& backdrops()
    {
        static const Backdrops b;
        return b;
    }
    static void blit (const Raster& r, int x0, int y0, const Ink* src, int w, int h)
    {
        for (int y = 0; y < h; ++y)
            if (y0 + y >= 0 && y0 + y < r.h)
            {
                const int from = std::max (0, -x0), to = std::min (w, r.w - x0);
                if (to > from) std::memcpy (r.px + (y0 + y) * r.w + x0 + from, src + y * w + from, (size_t) (to - from) * sizeof (Ink));
            }
    }

    // --- Header -----------------------------------------------------------------------------------------------------
    void header (const Raster& r, uint32_t sounding) const
    {
        using namespace hdui;
        // a sky: indigo to night, a few stars that breathe while notes sound
        blit (r, 0, 0, backdrops().sky, 320, 26);
        static const int stars[][2] = { { 112, 21 }, { 141, 19 }, { 9, 21 }, { 304, 5 }, { 268, 20 }, { 189, 3 }, { 230, 13 }, { 92, 22 } };
        for (int i = 0; i < 8; ++i)
        {
            const float tw = sounding ? 0.5f + 0.5f * std::sin ((float) nowMs * 0.004f + (float) i * 1.7f) : 0.35f;
            r.blend (stars[i][0], stars[i][1], ink::ice, (int) (80 + 170 * tw));
        }
        r.large (6, 1, "REVERIE", ink::violet, ink::cyan, 15);
        // the pages
        static const char* const names[pageCount] = { "CHORDS", "MOTION", "OSC", "FILTER", "AMP", "FX" };
        int x = 118;
        for (int p = 0; p < pageCount; ++p)
        {
            const int w = 6 * (int) std::strlen (names[p]);
            if (p == page)
            {
                r.box (x - 2, 1, w + 4, 11, ink::selected);
                r.box (x - 2, 11, w + 4, 1, accent);
            }
            r.text (x, 1, names[p], p == page ? ink::paper : ink::muted, 6);
            x += w + 7;
        }
        // where the chords are: the progression, its key and the voicing
        const int prog = std::clamp (choice (ParamId::progression), 0, progressionCount - 1);
        char s[64];
        std::snprintf (s, sizeof (s), "%s  %s  %s", progressionTable[prog].name, keyName (choice (ParamId::key), progressionTable[prog].minor),
                       voicingChoices[std::clamp (choice (ParamId::voicing), 0, Voicing::count - 1)]);
        r.text (118, 14, s, ink::soft, 6);
    }

    // --- Body -------------------------------------------------------------------------------------------------------
    void body (const Raster& r, uint32_t sounding, uint32_t held)
    {
        using namespace hdui;
        r.box (0, 0, 320, 142, ink::bg);
        switch (page)
        {
            case chordsPage: padGrid (r, held), keyboard (r, 4, 122, 312, 18, sounding); break;
            case motionPage: field (r, 4, 2, 136, 136, sounding); break;
            case oscPage: scope (r, 4, 2, 154, 36, 0), scope (r, 162, 2, 154, 36, 1); break;
            case filterPage:
                response (r, 4, 2, 152, 62);
                envelope (r, 4, 72, 152, 66, ParamId::fltAttack, tel.fltEnv.load (std::memory_order_relaxed), ink::cyan, sounding != 0);
                break;
            case ampPage:
                envelope (r, 4, 2, 152, 62, ParamId::ampAttack, tel.ampEnv.load (std::memory_order_relaxed), ink::rose, sounding != 0);
                lfoPicture (r, 4, 72, 152, 66);
                break;
            default: aurora (r, 4, 2, 312, 40); break;
        }
        const Layout l = layout (page);
        for (int c = 0; c < l.count; ++c) cell (r, l.cells[c], c == focus[page], c == focus[page] && editing);
    }

    void cell (const Raster& r, const Cell& c, bool focused, bool edited) const
    {
        using namespace hdui;
        const Param& p = paramTable[c.id];
        r.box (c.x, c.y, c.w, c.h, focused ? ink::selected : ink::card);
        if (edited) r.frame (c.x, c.y, c.w, c.h, accent);
        else if (focused) r.box (c.x, c.y + c.h - 1, c.w, 1, accent);
        const int ty = c.y + (c.h - 12) / 2;
        char s[40];
        formatted (c.id, s, sizeof (s));
        // the label and the value at 6 pixels a letter, else closer (the value first), else the value cut
        const int labelLen = (int) std::strlen (c.label), valueLen = (int) std::strlen (s);
        int labelAdv = 6, adv = 6;
        auto fits = [&] { return 6 + labelAdv * labelLen + 4 + adv * valueLen <= c.w; };
        if (! fits()) adv = 5;
        if (! fits()) labelAdv = 5;
        if (! fits()) s[std::max (1, (c.w - 10 - labelAdv * labelLen) / adv)] = 0;
        r.text (c.x + 3, ty, c.label, focused ? ink::soft : ink::muted, labelAdv);
        const bool off = (p.unit == Unit::onOff && live (c.id) < 0.5f) || (p.unit == Unit::glideTime && live (c.id) <= 0.0f);
        r.right (c.x + c.w - 3, ty, s, off ? ink::parked : ink::paper, adv);
        if (p.kind == Kind::continuous && c.h >= 16)
        {
            const float unit = (live (c.id) - p.min) / (p.max - p.min);
            const int w = c.w - 6, x0 = c.x + 3, y = c.y + c.h - 3;
            const int origin = p.min < 0 ? x0 + w / 2 : x0, at = x0 + (int) std::lround (unit * (float) (w - 1));
            r.box (x0, y, w, 1, ink::grid);
            r.box (std::min (origin, at), y, std::abs (at - origin) + 1, 1, focused ? accent : ink::rule);
        }
    }

    // The sixteen pads, pad 1 bottom left (as on the MPC); 9-16 the lifted chords.
    void padGrid (const Raster& r, uint32_t held) const // in the body's place (4, 2)
    {
        using namespace hdui;
        const int prog = std::clamp (choice (ParamId::progression), 0, progressionCount - 1);
        const int key = choice (ParamId::key);
        const bool minor = progressionTable[prog].minor;
        const int last = tel.lastSlot.load (std::memory_order_relaxed);
        const ParsedProgression& pp = parsedProgressions()[prog];
        for (int slot = 0; slot < padSlots; ++slot)
        {
            const int col = slot % 4, row = 3 - slot / 4;
            const int x = 4 + 44 * col, y = 2 + 28 * row;
            const bool lift = slot >= 8, on = (held >> slot) & 1u;
            Ink base = lift ? 0xff1c1240 : ink::card;
            if (on)
            {
                const float pulse = 0.75f + 0.25f * std::sin ((float) nowMs * 0.006f);
                base = mix (base, lift ? ink::cyan : accent, (int) (150 * pulse));
            }
            r.box (x, y, 42, 26, base);
            if (slot == last) r.frame (x, y, 42, 26, on ? ink::paper : ink::soft);
            else r.box (x, y + 25, 42, 1, lift ? ink::deep : ink::rule);
            char root[12];
            const ChordSymbol& c = pp.chords[slot % 8];
            chordRootText (c, key, minor, root, sizeof (root));
            const char* q = chordQualityText (c);
            r.text (x + 3, y + 2, root, on ? ink::bg : ink::paper, (int) std::strlen (root) > 5 ? 6 : 7);
            const int qa = (int) std::strlen (q) * 6 > 38 ? 5 : 6;
            r.text (x + 3, y + 13, q[0] ? q : "maj", on ? ink::bg : (lift ? ink::cyan : ink::soft), qa);
            if (lift) r.text (x + 35, y + 2, "^", on ? ink::bg : ink::parked, 6);
        }
    }

    // Notes C1 to C7; the sounding ones lit (the bass gold), else the last chord's, dim.
    void keyboard (const Raster& r, int x0, int y0, int w, int h, uint32_t sounding)
    {
        using namespace hdui;
        constexpr int low = 24, high = 96;
        const int whites = 42, kw = w / whites, left = x0 + (w - kw * whites) / 2;
        float lit[128] {};
        bool any = false;
        for (int i = 0; i < maxVoices; ++i)
            if ((sounding >> i) & 1u)
            {
                const int n = tel.notes[i].load (std::memory_order_relaxed);
                if (n >= 0 && n < 128) lit[n] = std::max (lit[n], std::min (1.0f, 0.25f + tel.levels[i].load (std::memory_order_relaxed) * 1.5f)), any = true;
            }
        int lowest = 128;
        if (! any)
        {
            const int slot = tel.lastSlot.load (std::memory_order_relaxed);
            if (slot >= 0 && slot < padSlots)
            {
                refreshVoicings();
                const ChordNotes& c = cachedSet.slot[slot];
                const int shift = 12 * choice (ParamId::octave);
                for (int i = 0; i < c.count; ++i)
                {
                    const int n = c.note[i] + shift;
                    if (n >= 0 && n < 128) lit[n] = 0.2f;
                }
            }
        }
        for (int n = low; n <= high; ++n)
            if (lit[n] > 0.0f) { lowest = n; break; }
        static const int whiteOf[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
        for (int n = low; n < high; ++n)
        {
            const int pc = n % 12;
            if (whiteOf[pc] < 0) continue;
            const int wi = (n / 12 - low / 12) * 7 + whiteOf[pc];
            const int x = left + wi * kw;
            const Ink lightInk = n == lowest && any ? ink::gold : mix (ink::violet, ink::cyan, std::clamp ((n - 48) * 8, 0, 255));
            r.box (x, y0, kw - 1, h, lit[n] > 0.0f ? mix (0xff6e6690, lightInk, (int) (255 * lit[n])) : 0xff6e6690);
        }
        for (int n = low; n < high; ++n)
        {
            const int pc = n % 12;
            if (whiteOf[pc] >= 0) continue;
            const int wi = (n / 12 - low / 12) * 7 + whiteOf[pc - 1];
            const int x = left + wi * kw + kw - 2;
            const Ink lightInk = n == lowest && any ? ink::gold : mix (ink::violet, ink::cyan, std::clamp ((n - 48) * 8, 0, 255));
            r.box (x, y0, 4, h * 3 / 5, lit[n] > 0.0f ? mix (ink::bg, lightInk, (int) (255 * lit[n])) : ink::bg);
        }
        r.box (x0, y0 - 2, w, 1, ink::rule);
    }

    void refreshVoicings()
    {
        const int p = choice (ParamId::progression), v = choice (ParamId::voicing), k = choice (ParamId::key);
        if (p == cachedProg && v == cachedVoicing && k == cachedKey) return;
        voiceProgression (p, v, k, cachedSet);
        cachedProg = p, cachedVoicing = v, cachedKey = k;
    }

    // The X / Y field: its grid, the path around (X, Y), the notes on it.
    void field (const Raster& r, int x0, int y0, int w, int h, uint32_t sounding) const
    {
        using namespace hdui;
        if (w == 136 && h == 136) blit (r, x0, y0, backdrops().field, 136, 136);
        else r.box (x0, y0, w, h, ink::field);
        for (int i = 1; i < 8; ++i)
        {
            r.dotted (x0 + i * w / 8, y0, x0 + i * w / 8, y0 + h - 1, i == 4 ? ink::rule : ink::grid, 2);
            r.dotted (x0, y0 + i * h / 8, x0 + w - 1, y0 + i * h / 8, i == 4 ? ink::rule : ink::grid, 2);
        }
        r.frame (x0, y0, w, h, ink::rule);
        auto px = [&] (float v) { return x0 + (int) std::lround ((v + 1.0f) * 0.5f * (float) (w - 1)); };
        auto py = [&] (float v) { return y0 + (int) std::lround ((1.0f - v) * 0.5f * (float) (h - 1)); };
        const float cx = live (ParamId::motionX), cy = live (ParamId::motionY);
        const int shape = choice (ParamId::motionShape);
        const float depth = shape == Motion::off ? 0.0f : live (ParamId::motionDepth);
        if (depth > 0.0f)
        {
            int lx = 0, ly = 0;
            for (int i = 0; i <= 160; ++i)
            {
                float mx, my;
                motionPoint (shape, (float) i / 160.0f, mx, my);
                const int x = px (std::clamp (cx + depth * mx, -1.0f, 1.0f)), y = py (std::clamp (cy + depth * my, -1.0f, 1.0f));
                if (i > 0) r.line (lx, ly, x, y, mix (ink::deep, ink::aurora, 110));
                lx = x, ly = y;
            }
        }
        // the centre
        const int ccx = px (cx), ccy = py (cy);
        r.box (ccx - 4, ccy, 9, 1, ink::gold);
        r.box (ccx, ccy - 4, 1, 9, ink::gold);
        // the path's own point, then each note's
        r.ring (px (tel.pathX.load (std::memory_order_relaxed)), py (tel.pathY.load (std::memory_order_relaxed)), 4, ink::soft);
        for (int i = 0; i < maxVoices; ++i)
            if ((sounding >> i) & 1u)
            {
                const float lv = std::min (1.0f, tel.levels[i].load (std::memory_order_relaxed) * 1.6f);
                const int n = tel.notes[i].load (std::memory_order_relaxed);
                const Ink c = mix (ink::violet, ink::cyan, std::clamp ((n - 40) * 6, 0, 255));
                const int x = px (tel.x[i].load (std::memory_order_relaxed)), y = py (tel.y[i].load (std::memory_order_relaxed));
                r.disc (x, y, 3, mix (ink::field, c, (int) (90 + 160 * lv)));
                r.dot (x, y, ink::paper);
            }
        r.text (x0 + 3, y0 + h - 13, motionChoices[std::clamp (shape, 0, Motion::count - 1)], ink::muted, 6);
    }

    // An oscillator's wave where its Position is now (the newest note's, else the knob's).
    void scope (const Raster& r, int x0, int y0, int w, int h, int o) const
    {
        using namespace hdui;
        r.box (x0, y0, w, h, ink::field);
        r.box (x0, y0 + h / 2, w, 1, ink::grid);
        const int wave = std::clamp (choice (ParamId::osc (o, 0)), 0, waveCount - 1);
        const bool sounding = tel.sounding.load (std::memory_order_relaxed) != 0;
        const float pos = sounding ? tel.morph[o].load (std::memory_order_relaxed) : live (ParamId::osc (o, 1));
        const float xf = pos * (float) (waveFrames - 1);
        const int f = std::min ((int) xf, waveFrames - 2);
        const float m = xf - (float) f;
        const WaveTable& a = WaveBank::get().table (wave, f, 2);
        const WaveTable& b = WaveBank::get().table (wave, f + 1, 2);
        const float level = live (ParamId::osc (o, 7));
        int ly = 0;
        for (int x = 0; x < w; ++x)
        {
            const uint32_t i = (uint32_t) ((float) x / (float) w * (float) a.length);
            const float v = (a.data[i] + (b.data[i] - a.data[i]) * m) * 2.2f;
            const int y = y0 + h / 2 - (int) std::lround (std::clamp (v, -1.0f, 1.0f) * (float) (h / 2 - 3));
            const Ink c = mix (ink::violet, ink::cyan, x * 255 / w);
            const Ink c2 = level > 0.0f ? c : ink::parked;
            if (x > 0) r.line (x0 + x - 1, ly, x0 + x, y, c2);
            r.blend (x0 + x, y + 1, c2, 70);
            ly = y;
        }
        char s[32];
        std::snprintf (s, sizeof (s), "OSC %d  %s", o + 1, waveChoices[wave]);
        r.text (x0 + 3, y0 + 2, s, ink::soft, 6);
    }

    // The filter's response, 20 Hz to 20 kHz, -36 to +18 dB; the newest note's cutoff marked.
    void response (const Raster& r, int x0, int y0, int w, int h) const
    {
        using namespace hdui;
        r.box (x0, y0, w, h, ink::field);
        const int type = choice (ParamId::filterType);
        const float fc = cutoffHz (live (ParamId::cutoff));
        const float k = 2.0f - 1.94f * live (ParamId::resonance);
        auto db = [&] (float hz) {
            const float wv = hz / fc;
            auto mag = [&] (float kk) { const float re = 1.0f - wv * wv, im = kk * wv; return 1.0f / std::sqrt (re * re + im * im); };
            float m;
            switch (type)
            {
                case Filter::lp24: m = mag (1.4142136f) * mag (k); break;
                case Filter::bp: m = k * wv * mag (k); break;
                case Filter::hp: m = wv * wv * mag (k); break;
                default: m = mag (k); break;
            }
            return 20.0f * std::log10 (std::max (m, 1e-6f));
        };
        for (int i = 1; i < 3; ++i) r.dotted (x0 + i * w / 3, y0, x0 + i * w / 3, y0 + h - 1, ink::grid, 2);
        const int zero = y0 + (int) ((18.0f / 54.0f) * (float) h);
        r.box (x0, zero, w, 1, ink::grid);
        int ly = 0;
        for (int x = 0; x < w; ++x)
        {
            const float hz = 20.0f * std::pow (1000.0f, (float) x / (float) (w - 1));
            const float d = std::clamp (db (hz), -36.0f, 18.0f);
            const int y = y0 + (int) std::lround ((18.0f - d) / 54.0f * (float) (h - 1));
            for (int yy = y; yy < y0 + h; ++yy) r.blend (x0 + x, yy, ink::violet, std::max (0, 60 - (yy - y) * 2));
            if (x > 0) r.line (x0 + x - 1, ly, x0 + x, y, ink::cyan);
            ly = y;
        }
        if (tel.sounding.load (std::memory_order_relaxed) != 0)
        {
            const float live = tel.cutoff.load (std::memory_order_relaxed);
            const int x = x0 + (int) std::lround (std::log (std::max (live, 20.0f) / 20.0f) / std::log (1000.0f) * (float) (w - 1));
            r.dotted (x, y0, x, y0 + h - 1, ink::gold, 2);
        }
        r.text (x0 + 3, y0 + 2, filterChoices[std::clamp (type, 0, 3)], ink::soft, 6);
    }

    // An ADSR from the four ids from `first`, with where the newest note is.
    void envelope (const Raster& r, int x0, int y0, int w, int h, uint32_t first, float now, Ink c, bool sounding) const
    {
        using namespace hdui;
        r.box (x0, y0, w, h, ink::field);
        const float a = live (first), d = live (first + 1), s = live (first + 2), rl = live (first + 3);
        const float total = a + d + 0.35f + rl + 1e-3f;
        const int wa = (int) ((float) (w - 8) * a / total), wd = (int) ((float) (w - 8) * d / total);
        const int ws = (int) ((float) (w - 8) * 0.35f / total), wr = (w - 8) - wa - wd - ws;
        const int top = y0 + 6, bottom = y0 + h - 4, sy = bottom - (int) ((float) (bottom - top) * s);
        const int x1 = x0 + 4, x2 = x1 + wa, x3 = x2 + wd, x4 = x3 + ws, x5 = x4 + wr;
        auto curve = [&] (int xa, int ya, int xb, int yb, bool exp) {
            int lx = xa, ly = ya;
            const int n = std::max (1, xb - xa);
            for (int i = 1; i <= n; ++i)
            {
                const float t = (float) i / (float) n;
                const float e = exp ? 1.0f - std::exp (-4.0f * t) : t;
                const int x = xa + i, y = ya + (int) std::lround ((float) (yb - ya) * (exp ? e / (1.0f - std::exp (-4.0f)) : e));
                r.line (lx, ly, x, y, c);
                for (int yy = y + 1; yy < bottom; ++yy) r.blend (x, yy, c, 26);
                lx = x, ly = y;
            }
        };
        curve (x1, bottom, x2, top, false);
        curve (x2, top, x3, sy, true);
        curve (x3, sy, x4, sy, false);
        curve (x4, sy, x5, bottom, true);
        if (sounding)
        {
            const int y = bottom - (int) ((float) (bottom - top) * std::clamp (now, 0.0f, 1.0f));
            r.box (x0 + 1, y, 3, 1, ink::paper);
            r.disc (x0 + w - 5, y, 2, ink::paper);
        }
        r.text (x0 + 3, y0 + 2, first == ParamId::ampAttack ? "AMP ENV" : "FILTER ENV", ink::soft, 6);
    }

    void lfoPicture (const Raster& r, int x0, int y0, int w, int h) const
    {
        using namespace hdui;
        r.box (x0, y0, w, h, ink::field);
        r.box (x0, y0 + h / 2, w, 1, ink::grid);
        const int shape = choice (ParamId::lfoShape);
        const float cycles = 1.0f + 5.0f * live (ParamId::lfoRate);
        const float fade = std::max (0.02f, live (ParamId::lfoDelay));
        int ly = 0;
        for (int x = 0; x < w; ++x)
        {
            const float t = (float) x / (float) w;
            const float ph = t * cycles;
            float v;
            if (shape == LfoShape::triangle) v = 1.0f - 4.0f * std::fabs (ph - std::floor (ph) - 0.5f);
            else if (shape == LfoShape::drift) v = 0.6f * std::sin (ph * 6.28f) + 0.4f * std::sin (ph * 2.7f + 1.0f);
            else v = std::sin (ph * 6.2831853f);
            v *= std::min (1.0f, t / fade);
            const int y = y0 + h / 2 - (int) std::lround (v * (float) (h / 2 - 6));
            if (x > 0) r.line (x0 + x - 1, ly, x0 + x, y, ink::aurora);
            ly = y;
        }
        r.text (x0 + 3, y0 + 2, "LFO", ink::soft, 6);
    }

    // FX: an aurora band that follows the output.
    void aurora (const Raster& r, int x0, int y0, int w, int h) const
    {
        using namespace hdui;
        const float level = std::min (1.0f, tel.reverb.load (std::memory_order_relaxed) * 2.5f);
        const float t = (float) nowMs * 0.001f;
        for (int x = 0; x < w; ++x)
        {
            const float u = (float) x / (float) w;
            const float wave = 0.5f + 0.25f * std::sin (u * 9.0f + t * 0.9f) + 0.15f * std::sin (u * 23.0f - t * 1.7f);
            const float crest = (float) h * (0.85f - (0.15f + 0.6f * level) * wave);
            for (int y = 0; y < h; ++y)
            {
                const float d = (float) y - crest;
                int a = d < 0.0f ? (int) (std::max (0.0f, 1.0f + d / 14.0f) * 140.0f * (0.35f + level)) : (int) (std::max (0.0f, 1.0f - d / 5.0f) * 200.0f * (0.3f + level));
                a = std::clamp (a + bayer[y & 3][x & 3] - 8, 0, 255);
                const Ink c = mix (ink::aurora, ink::violet, std::clamp ((int) (u * 255.0f + 60.0f * std::sin (t + u * 4.0f)), 0, 255));
                r.dot (x0 + x, y0 + y, mix (ink::bg, c, a));
            }
        }
        r.text (x0 + 3, y0 + 2, "ENSEMBLE  DELAY  REVERB", ink::soft, 6);
    }

    // --- K1-K3 ------------------------------------------------------------------------------------------------------
    void cards (const Raster& r, const Host& h) const
    {
        using namespace hdui;
        const Layout l = layout (page);
        for (int k = 0; k < 3; ++k)
        {
            const int x = 4 + 105 * k;
            const int c = focus[page] + k;
            r.box (x, 3, 102, 42, ink::card);
            const char tag[] = { 'K', char ('1' + k), 0 };
            r.text (x + 4, 6, tag, ink::muted);
            if (c >= l.count) continue;
            const uint32_t id = l.cells[c].id;
            const auto& p = paramTable[id];
            r.text (x + 22, 6, p.name, ink::paper, (int) std::strlen (p.name) * 7 > 78 ? 6 : 7);
            char s[40];
            formatted (id, s, sizeof (s));
            if ((int) std::strlen (s) > 14) s[14] = 0;
            r.text (x + 4, 20, s, ink::ice);
            const int start = x + 5, w = 92, rail = 37;
            const float unit = (live (id) - p.min) / (p.max - p.min);
            const int at = start + (int) std::lround (unit * (float) (w - 1));
            r.box (start, rail, w, 2, ink::rule);
            const int origin = p.min < 0 ? start + (w - 1) / 2 : start;
            r.box (std::min (at, origin), rail, std::abs (at - origin) + 1, 2, accent);
            r.box (at, rail - 2, 2, 6, accent);
            if (h.pot[k] >= 0 && ! h.engaged[k])
            {
                const int pos = start + std::clamp ((int) h.pot[k], 0, 127) * (w - 1) / 127;
                r.box (pos - 2, rail - 3, 5, 7, ink::card);
                r.frame (pos - 2, rail - 3, 5, 7, ink::paper);
            }
        }
    }

    void legend (const Raster& r) const
    {
        using namespace hdui;
        const char* top[3] = { "WHEEL", "PUSH", "MINUS / PLUS" };
        const char* plain[3] = { "Move", "Edit / flip", "Page" };
        const char* shifted[3] = { "", "", "" };
        if (editing) plain[0] = "Value", shifted[0] = "+SH Fine", plain[1] = "Done", plain[2] = "Coarse value";
        if (page == chordsPage) shifted[2] = "Pads A01-A16";
        for (int k = 0; k < 3; ++k)
        {
            const int x = 4 + 105 * k;
            r.box (x, 3, 102, 42, ink::field);
            r.box (x, 3, 2, 42, ink::violet);
            r.text (x + 6, 6, top[k], ink::muted);
            r.text (x + 6, 19, plain[k], ink::paper);
            r.text (x + 6, 31, shifted[k], ink::soft, 6);
        }
    }

    const std::atomic<float>* values;
    const Telemetry& tel;
    float seen[paramCount] {};
    hdui::Picture picture;
    int page = chordsPage;
    int focus[pageCount] {};
    bool editing = false, first = true, touched = false, animated = false, wasAnimated = false;
    int shownPage = -1;
    uint32_t shownSounding = 0, shownHeld = 0, shownStrikes = 0;
    uint64_t frameAt = 0, nowMs = 0;
    Host lastHost {};
    Ink accent = hdui::ink::violet;
    VoicingSet cachedSet {};
    int cachedProg = -1, cachedVoicing = -1, cachedKey = -1;
};
} // namespace reverie
