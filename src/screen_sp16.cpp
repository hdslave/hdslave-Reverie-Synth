#include "screen_sp16.h"

#include "presets.h"
#include "sp16_art.h"

namespace reverie
{
namespace
{
using namespace hdui;
using namespace ParamId;

constexpr int w = SIXTEEN_SCREEN_WIDTH, h = SIXTEEN_SCREEN_HEIGHT;
constexpr int left = 12, right = w - left;
constexpr int tabWidth = 122, tabStep = 127, tabTop = 56, tabBottom = 86, bodyTop = 115;
constexpr int contentY = 121;
constexpr int padX = 13, padY = contentY, padW = 116, padH = 34, padDX = 121, padDY = 37;
constexpr int globeX = 13, globeY = contentY, globeSize = 150;
constexpr int globeCenterX = globeX + 75, globeCenterY = globeY + 67, globeRadius = 56;
constexpr int bankWidth = 106, bankStep = 108, bankY = tabBottom + 2, bankH = 26;
constexpr int soundsX = 310, soundsY = 10, soundsW = 105, soundsH = 38;
constexpr int soundCols = 5, soundX = 13, soundY = 123, soundDX = 152, soundDY = 35, soundW = 146, soundH = 33;

struct Bank
{
    const char* name;
    const char* strip;
    std::array<uint32_t, 6> ids;
};

constexpr auto none = SIXTEEN_INVALID_PARAM_ID;
constexpr Bank chordBanks[] = {
    { "PROGRESSION", "CHORDS / PROG", { progression, voicing, key, octave, strum, latch } },
    { "FEEL", "CHORDS / FEEL", { humanize, bass, velocity, glide, none, none } },
};
constexpr Bank motionBanks[] = {
    { "PATH", "MOTION / PATH", { motionX, motionY, motionDepth, motionShape, motionRate, motionSpread } },
    { "CLOCK", "MOTION / CLOCK", { motionSync, motionDiv, motionRetrig, none, none, none } },
    { "X ROUTES", "MOTION / X ROUTES", { xDest1, xAmt1, xDest2, xAmt2, none, none } },
    { "Y ROUTES", "MOTION / Y ROUTES", { yDest1, yAmt1, yDest2, yAmt2, none, none } },
};
constexpr Bank oscBanks[] = {
    { "MORPH", "OSC / MORPH", { osc1Wave, osc1Pos, osc1Level, osc2Wave, osc2Pos, osc2Level } },
    { "OSC 1", "OSC / OSC 1", { osc1Octave, osc1Semi, osc1Fine, osc1Unison, osc1Detune, none } },
    { "OSC 2", "OSC / OSC 2", { osc2Octave, osc2Semi, osc2Fine, osc2Unison, osc2Detune, none } },
    { "VOICE", "OSC / VOICE", { width, air, airTone, drift, none, none } },
};
constexpr Bank filterBanks[] = {
    { "RESPONSE", "FILTER / RESPONSE", { filterType, cutoff, resonance, filterEnv, keyTrack, velCutoff } },
    { "ENVELOPE", "FILTER / ENVELOPE", { fltAttack, fltDecay, fltSustain, fltRelease, none, none } },
};
constexpr Bank ampBanks[] = {
    { "ENVELOPE", "AMP / ENVELOPE", { ampAttack, ampDecay, ampSustain, ampRelease, volume, none } },
    { "LFO", "AMP / LFO", { lfoShape, lfoRate, lfoDelay, lfoPitch, lfoCutoff, lfoWave } },
};
constexpr Bank fxBanks[] = {
    { "ENS / DELAY", "FX / ENS + DELAY", { ensMix, ensDepth, ensRate, dlyMix, dlyTime, dlyFeedback } },
    { "REVERB", "FX / REVERB", { revMix, revSize, revDecay, revDamp, revPredelay, shimmer } },
};

struct Page
{
    const char* tab;
    const char* heading;
    const Bank* banks;
    int count;
};

constexpr Page pages[] = {
    { "CHORDS", "Chord pads", chordBanks, std::size (chordBanks) },
    { "MOTION", "Motion field", motionBanks, std::size (motionBanks) },
    { "OSC", "Wavetable oscillators", oscBanks, std::size (oscBanks) },
    { "FILTER", "Filter", filterBanks, std::size (filterBanks) },
    { "AMP", "Amplitude & modulation", ampBanks, std::size (ampBanks) },
    { "FX", "Stereo effects", fxBanks, std::size (fxBanks) },
};
static_assert (std::size (pages) == Screen::pageCount);

constexpr bool coversParameters()
{
    int counts[paramCount] {};
    for (const auto& p : pages)
        for (int b = 0; b < p.count; ++b)
            for (auto id : p.banks[b].ids)
            {
                if (id == none) continue;
                if (id >= paramCount) return false;
                ++counts[id];
            }
    for (int count : counts)
        if (count != 1) return false;
    return true;
}
static_assert (coversParameters(), "Each parameter has one six-encoder bank");

int bankX (const Page& p, int bank) { return right - bankWidth - (p.count - 1 - bank) * bankStep; }

template<int Size> struct Font;
template<> struct Font<11> { static constexpr auto widths = sp16Art::width11, mask = sp16Art::mask11;
                             static constexpr auto offsets = sp16Art::offset11; };
template<> struct Font<14> { static constexpr auto widths = sp16Art::width14, mask = sp16Art::mask14;
                             static constexpr auto offsets = sp16Art::offset14; };
template<> struct Font<18> { static constexpr auto widths = sp16Art::width18, mask = sp16Art::mask18;
                             static constexpr auto offsets = sp16Art::offset18; };
template<> struct Font<23> { static constexpr auto widths = sp16Art::width23, mask = sp16Art::mask23;
                             static constexpr auto offsets = sp16Art::offset23; };

unsigned glyph (char ch) { return ch >= ' ' && ch <= '~' ? unsigned (ch - ' ') : unsigned ('?' - ' '); }

template<int Size>
int textWidth (const char* text)
{
    int total = 0;
    for (; *text; ++text) total += Font<Size>::widths[glyph (*text)];
    return total;
}

template<int Size>
void text (const Raster& r, int x, int y, const char* s, Ink color)
{
    for (; *s; ++s)
    {
        const unsigned c = glyph (*s);
        const int wide = Font<Size>::widths[c];
        const auto* m = Font<Size>::mask + Font<Size>::offsets[c];
        for (int j = 0; j < Size + 3; ++j)
            for (int i = 0; i < wide; ++i, ++m)
                if (*m) r.blend (x + i, y + j, color, *m + (*m == 255));
        x += wide;
    }
}

template<int Size>
void centered (const Raster& r, int middle, int y, const char* s, Ink color)
{
    text<Size> (r, middle - textWidth<Size> (s) / 2, y, s, color);
}

bool inside (int x, int y, int x0, int y0, int width, int height)
{
    return x >= x0 && x < x0 + width && y >= y0 && y < y0 + height;
}

// The large plates are already in the host's opaque ARGB layout. Alpha is
// needed only at the curved edges of the small overlaid controls.
void sprite (const Raster& r, int x, int y, int width, int height, const Ink* pixels)
{
    for (int row = 0; row < height; ++row)
        for (int col = 0; col < width; ++col)
        {
            const Ink c = *pixels++;
            const unsigned a = c >> 24;
            if (a == 255) r.dot (x + col, y + row, c);
            else if (a != 0) r.blend (x + col, y + row, c, int (a));
        }
}

void section (const Raster& r, int x, int y, int width, const char* title)
{
    text<14> (r, x + 6, y, title, ink::text);
    const int start = x + textWidth<14> (title) + 18;
    r.box (start, y + 10, x + width - start - 8, 1, 0xff8bbbc8);
}

void chordText (const ChordSymbol& chord, int key, bool minor, char (&label)[32])
{
    char root[12];
    chordRootText (chord, key, minor, root, sizeof (root));
    const char* suffix = chordQualityText (chord);
    // Put the quality before the slash bass.
    const char* slash = std::strchr (root, '/');
    if (slash != nullptr)
        std::snprintf (label, sizeof (label), "%.*s%s%s", int (slash - root), root, suffix, slash);
    else
        std::snprintf (label, sizeof (label), "%s%s", root, suffix);
}
} // namespace

void Sp16Screen::knobs (uint32_t ids[6], char name[SIXTEEN_TAB_NAME_SIZE]) const
{
    const Bank& bank = pages[page].banks[banks[page]];
    std::copy (bank.ids.begin(), bank.ids.end(), ids);
    std::snprintf (name, SIXTEEN_TAB_NAME_SIZE, "%s", bank.strip);
}

bool Sp16Screen::turn (int delta)
{
    if (soundsOpen)
    {
        const int next = (focusedSound + delta % presetCount + presetCount) % presetCount;
        dirty |= next != focusedSound;
        focusedSound = next;
        return delta != 0;
    }
    const int count = pages[page].count;
    const int next = (banks[page] + delta % count + count) % count;
    dirty |= next != banks[page];
    banks[page] = next;
    return delta != 0;
}

int Sp16Screen::push()
{
    dirty = true;
    if (soundsOpen)
    {
        soundsOpen = false;
        return focusedSound;
    }
    page = (page + 1) % Screen::pageCount;
    return -1;
}

Sp16Screen::Touch Sp16Screen::touch (int phase, int x, int y)
{
    Touch result;
    if (phase == SIXTEEN_TOUCH_UP)
    {
        result.used = gesture != Gesture::none;
        result.release = gesture == Gesture::pad;
        gesture = Gesture::none;
        return result;
    }

    if (phase == SIXTEEN_TOUCH_DOWN)
    {
        result.release = gesture == Gesture::pad;
        gesture = Gesture::none;
        if (inside (x, y, soundsX, soundsY, soundsW, soundsH))
        {
            soundsOpen = ! soundsOpen;
            dirty = result.used = true;
            return result;
        }
        if (inside (x, y, left, tabTop, w - 2 * left, tabBottom - tabTop))
        {
            const int target = (x - left) / tabStep;
            if (target < Screen::pageCount && (x - left) % tabStep < tabWidth)
            {
                if (soundsOpen) soundsOpen = false;
                else if (target == page) turn (1);
                page = target;
                dirty = result.used = true;
            }
            return result;
        }
        if (soundsOpen)
        {
            for (int i = 0; i < presetCount; ++i)
                if (inside (x, y, soundX + i % soundCols * soundDX, soundY + i / soundCols * soundDY, soundW, soundH))
                {
                    focusedSound = result.sound = i;
                    soundsOpen = false;
                    dirty = true;
                    break;
                }
            result.used = true;
            return result;
        }
        const Page& current = pages[page];
        for (int i = 0; i < current.count; ++i)
            if (inside (x, y, bankX (current, i), bankY, bankWidth, bankH))
            {
                banks[page] = i;
                dirty = result.used = true;
                return result;
            }
        if (page == Screen::chordsPage && inside (x, y, padX, padY, 3 * padDX + padW, 3 * padDY + padH))
        {
            const int col = (x - padX) / padDX, row = (y - padY) / padDY;
            if ((x - padX) % padDX < padW && (y - padY) % padDY < padH)
            {
                gesture = Gesture::pad;
                result.pad = (3 - row) * 4 + col;
                result.used = true;
            }
        }
        if (page == Screen::motionPage && inside (x, y, globeX, globeY, globeSize, globeSize))
            gesture = Gesture::motion;
    }

    if (gesture == Gesture::motion)
    {
        result.edits.set (motionX, std::clamp ((x - globeCenterX) / float (globeRadius), -1.0f, 1.0f));
        result.edits.set (motionY, std::clamp ((globeCenterY - y) / float (globeRadius), -1.0f, 1.0f));
        result.used = true;
    }
    else if (gesture == Gesture::pad)
        result.used = true;
    return result;
}

bool Sp16Screen::render (const sixteen_screen_surface_t& surface, bool full)
{
    if (surface.pixels == nullptr || surface.width != w || surface.height != h || surface.stride < w)
        return false;
    bool refresh = full || dirty;
    for (int i = 0; i < paramCount; ++i)
    {
        const float v = value (i);
        refresh |= seen[i] != v;
        seen[i] = v;
    }
    const uint32_t sounding = telemetry.sounding.load (std::memory_order_relaxed);
    const uint32_t held = telemetry.padsHeld.load (std::memory_order_relaxed);
    const uint32_t strikes = telemetry.strikes.load (std::memory_order_relaxed);
    const bool moving = ! soundsOpen && (page == Screen::fxPage ? telemetry.peak.load (std::memory_order_relaxed) > 0.003f
                                                                 : sounding != 0);
    const bool changed = sounding != shownSounding || held != shownHeld || strikes != shownStrikes;
    if (! refresh && (soundsOpen || (! changed && moving == wasMoving && (! moving || surface.now_ms - frameAt < 40))))
        return false;

    const Raster r { pixels.data(), w, h };
    if (refresh)
        header (r);
    const Ink* const plates[] = { sp16Art::chords, sp16Art::motion, sp16Art::graphs,
                                  sp16Art::graphs, sp16Art::graphs, sp16Art::fx };
    const Ink* body = soundsOpen ? sp16Art::sounds : plates[page];
    std::copy_n (body, w * (h - bodyTop), r.px + w * bodyTop);
    if (soundsOpen)
        sounds (r);
    else
    {
        switch (page)
        {
            case Screen::chordsPage: chords (r, sounding, held); break;
            case Screen::motionPage: motion (r, sounding); break;
            case Screen::oscPage: oscillators (r); break;
            case Screen::filterPage:
            case Screen::ampPage: contours (r, sounding != 0); break;
            case Screen::fxPage: effects (r); break;
        }
    }

    dirty = false;
    wasMoving = moving;
    frameAt = surface.now_ms;
    shownSounding = sounding;
    shownHeld = held;
    shownStrikes = strikes;
    bool drew = false;
    for (int y = refresh ? 0 : bodyTop; y < h; ++y)
    {
        const uint32_t* row = pixels.data() + y * w;
        uint32_t* out = surface.pixels + y * surface.stride;
        if (full || std::memcmp (row, out, w * sizeof (uint32_t)) != 0)
        {
            std::copy_n (row, w, out);
            drew = true;
        }
    }
    return drew;
}

void Sp16Screen::header (const Raster& r) const
{
    std::copy_n (sp16Art::header, w * bodyTop, r.px);
    const auto& prog = progressionTable[choice (progression)];
    char info[92];
    std::snprintf (info, sizeof (info), "%s   /   %s %s   /   %s", prog.name,
                   keyTonic (choice (key), prog.minor), prog.minor ? "minor" : "major", voicingChoices[choice (voicing)]);
    text<14> (r, 17, 33, info, ink::white);
    sprite (r, soundsX + 7, soundsY + 5, 90, 25, sp16Art::soundButton);
    centered<14> (r, soundsX + soundsW / 2, soundsY + 8, soundsOpen ? "Close" : "Sounds", ink::white);

    if (! soundsOpen)
    {
        const int x = left + page * tabStep;
        for (int row = 0; row < tabBottom - tabTop; ++row)
            std::copy_n (sp16Art::selectedTabs + row * w + x, tabWidth, r.px + (tabTop + row) * w + x);
    }
    for (int i = 0; i < Screen::pageCount; ++i)
        text<14> (r, left + i * tabStep + 43, tabTop + 6, pages[i].tab, ink::text);

    const Page& p = pages[page];
    text<18> (r, 17, tabBottom + 3, soundsOpen ? "Factory sounds" : p.heading, ink::text);
    if (soundsOpen) return;
    for (int i = 0; i < p.count; ++i)
    {
        const int x = bankX (p, i);
        if (banks[page] == i)
            sprite (r, x + 3, bankY + 3, 100, 21, sp16Art::bankFocus);
        centered<14> (r, x + bankWidth / 2, bankY + 4, p.banks[i].name, ink::text);
    }
}

void Sp16Screen::sounds (const Raster& r) const
{
    for (int i = 0; i < presetCount; ++i)
    {
        const int x = soundX + i % soundCols * soundDX, y = soundY + i / soundCols * soundDY;
        if (i == focusedSound)
            sprite (r, x + 1, y + 2, 143, 29, sp16Art::soundFocus);
        centered<14> (r, x + soundW / 2, y + 6, presetTable[i].name, ink::text);
    }
}

void Sp16Screen::chords (const Raster& r, uint32_t sounding, uint32_t held) const
{
    const int prog = choice (progression), tonic = choice (key);
    const bool minor = progressionTable[prog].minor;
    const auto& pp = parsedProgressions()[prog];
    const int last = telemetry.lastSlot.load (std::memory_order_relaxed);
    for (int slot = 0; slot < padSlots; ++slot)
    {
        const int x = padX + (slot % 4) * padDX, y = padY + (3 - slot / 4) * padDY;
        const bool lift = slot >= 8, on = (held & (1u << slot)) != 0;
        if (on) sprite (r, x + 2, y + 2, 112, 30, sp16Art::padDown);
        char name[32], index[4];
        chordText (pp.chords[slot % 8], tonic, minor, name);
        std::snprintf (index, sizeof (index), "%02d", slot + 1);
        centered<14> (r, x + 17, y + 8, index, ink::deep);
        centered<14> (r, x + 17, y + 7, index, ink::white);
        if (textWidth<18> (name) <= 74) text<18> (r, x + 32, y + 4, name, ink::text);
        else if (textWidth<14> (name) <= 77) text<14> (r, x + 32, y + 7, name, ink::text);
        else text<11> (r, x + 31, y + 9, name, ink::text);
        if (slot == last) r.box (x + 40, y + 27, 42, 2, on ? 0xffa8590c : (lift ? ink::teal : ink::blue));
    }

    constexpr int x = 503;
    text<14> (r, x + 15, 128, sounding ? "Sounding notes" : "Last chord", ink::white);
    char title[52] = "Play a chord";
    if (last >= 0 && last < padSlots)
    {
        char name[32];
        chordText (pp.chords[last % 8], tonic, minor, name);
        std::snprintf (title, sizeof (title), "%s%s", name, last >= 8 ? "  /  Lift" : "");
    }
    text<23> (r, x + 16, 150, title, 0xff10476a);
    text<23> (r, x + 15, 149, title, ink::white);
    plots.keyboard (r, x + 11, 190, 242, 31, sounding);
    text<14> (r, x + 15, 235, "01-08  Chords", ink::white);
    text<14> (r, x + 148, 235, "09-16  Lifts", 0xffe3ffa8);
}

void Sp16Screen::motion (const Raster& r, uint32_t sounding) const
{
    const int shape = choice (motionShape);
    plots.motionTrace (r, globeCenterX, globeCenterY, globeRadius, shape, sounding);
    text<14> (r, globeX + 9, globeY + globeSize - 23, motionChoices[shape], ink::white);
    text<14> (r, globeX + globeSize - 37, globeY + globeSize - 23, "X / Y", ink::ice);

    constexpr uint32_t routes[] = { xDest1, xDest2, yDest1, yDest2 };
    constexpr const char* labels[] = { "X1", "X2", "Y1", "Y2" };
    constexpr int x = 178;
    constexpr int railX = x + 310, halfRail = 129;
    for (int i = 0; i < 4; ++i)
    {
        const int y = contentY + 2 + i * 37;
        centered<14> (r, x + 22, y + 8, labels[i], ink::white);
        char dst[44], amt[22];
        formatParam (paramTable[routes[i]], value (routes[i]), dst, sizeof (dst));
        formatParam (paramTable[routes[i] + 1], value (routes[i] + 1), amt, sizeof (amt));
        text<18> (r, x + 62, y + 4, dst, ink::text);
        text<14> (r, x + 239, y + 8, amt, ink::label);
        r.box (railX, y + 15, 2 * halfRail, 3, ink::rule);
        r.box (railX + halfRail, y + 11, 1, 11, ink::guide);
        const int span = int (value (routes[i] + 1) * halfRail);
        const Ink color = i < 2 ? ink::blue : ink::teal;
        r.box (railX + halfRail + std::min (0, span), y + 14, std::abs (span), 5, color);
        r.disc (railX + halfRail + span, y + 16, 4, color);
        r.dot (railX + halfRail + span - 1, y + 15, ink::ice);
    }
}

void Sp16Screen::oscillators (const Raster& r) const
{
    for (int o = 0; o < 2; ++o)
    {
        const int x = 13 + o * 381;
        const char* wave = waveChoices[choice (ParamId::osc (o, 0))];
        section (r, x, contentY, 372, wave);
        plots.scope (r, x + 8, 151, 356, 111, o, false);
    }
}

void Sp16Screen::contours (const Raster& r, bool sounding) const
{
    constexpr int lx = 13, rx = 394, cw = 372, y = 151, ch = 111;
    if (page == Screen::filterPage)
    {
        section (r, lx, contentY, cw, "FILTER RESPONSE");
        section (r, rx, contentY, cw, "FILTER ENVELOPE");
        plots.response (r, lx + 8, y, cw - 16, ch, false);
        plots.envelope (r, rx + 8, y, cw - 16, ch, fltAttack, telemetry.fltEnv.load (std::memory_order_relaxed), ink::lime, sounding, false);
    }
    else
    {
        section (r, lx, contentY, cw, "AMPLITUDE ENVELOPE");
        section (r, rx, contentY, cw, "LFO");
        plots.envelope (r, lx + 8, y, cw - 16, ch, ampAttack, telemetry.ampEnv.load (std::memory_order_relaxed), ink::foam, sounding, false);
        plots.lfoPicture (r, rx + 8, y, cw - 16, ch, false);
    }
}

void Sp16Screen::effects (const Raster& r) const
{
    plots.outputMeter (r, 61, 250);

    constexpr uint32_t mixes[] = { ensMix, dlyMix, revMix };
    constexpr uint32_t details[] = { ensRate, dlyTime, revDecay };
    constexpr const char* labels[] = { "Ensemble", "Tempo delay", "Shimmer reverb" };
    for (int i = 0; i < 3; ++i)
    {
        const int x = 322 + 174 * i, y = 195;
        centered<18> (r, x, 128, labels[i], ink::text);
        char mixText[24], detailText[32];
        const float amount = value (mixes[i]);
        formatParam (paramTable[mixes[i]], amount, mixText, sizeof (mixText));
        formatParam (paramTable[details[i]], value (details[i]), detailText, sizeof (detailText));
        centered<18> (r, x, y - 12, mixText, ink::white);
        centered<14> (r, x, y + 5, "Mix", ink::ice);
        const float angle = (135.0f + 270.0f * amount) * 0.017453293f;
        const int tipX = x + int (std::lround (38.0f * std::cos (angle)));
        const int tipY = y + int (std::lround (38.0f * std::sin (angle)));
        r.disc (tipX, tipY, 3, ink::deep);
        r.disc (tipX, tipY, 2, ink::lime);
        centered<14> (r, x, 241, detailText, ink::text);
    }
}
}
