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
constexpr int headerH = 80, bodyTop = 114, paneW = w / 2;
constexpr int patchX = 509, patchW = 262, arrowW = 36, patchH = 42;
constexpr int viewsY = 43;
constexpr int padX = 8, padY = 118, padW = 90, padH = 30, padDX = 94, padDY = 33;
constexpr int globeX = 397, globeY = 121, globeSize = 150;
constexpr int globeCenterX = globeX + 75, globeCenterY = globeY + 67, globeRadius = 56;
constexpr int soundCols = 5, soundX = 13, soundY = 123, soundDX = 152, soundDY = 31, soundW = 146, soundH = 30;
static_assert (soundY + (presetCount - 1) / soundCols * soundDY + soundH <= h, "Factory sound tiles must fit the touchscreen");

struct Bank
{
    const char* name;
    const char* strip;
    std::array<uint32_t, 6> ids;
};

constexpr auto none = SIXTEEN_INVALID_PARAM_ID;
constexpr Bank chordBanks[] = {
    { "Progression", "CHORDS / PROG", { progression, voicing, key, octave, strum, latch } },
    { "Feel", "CHORDS / FEEL", { humanize, bass, velocity, glide, none, none } },
    { "Mood", "CHORDS / MOOD", { mood, moodShift, none, none, none, none } },
};
constexpr Bank motionBanks[] = {
    { "Path", "MOTION / PATH", { motionX, motionY, motionDepth, motionShape, motionRate, motionSpread } },
    { "Clock", "MOTION / CLOCK", { motionSync, motionDiv, motionRetrig, none, none, none } },
    { "X routes", "MOTION / X ROUTES", { xDest1, xAmt1, xDest2, xAmt2, none, none } },
    { "Y routes", "MOTION / Y ROUTES", { yDest1, yAmt1, yDest2, yAmt2, none, none } },
};
constexpr Bank oscBanks[] = {
    { "Morph", "OSC / MORPH", { osc1Wave, osc1Pos, osc1Level, osc2Wave, osc2Pos, osc2Level } },
    { "OSC 1", "OSC / OSC 1", { osc1Octave, osc1Semi, osc1Fine, osc1Unison, osc1Detune, none } },
    { "OSC 2", "OSC / OSC 2", { osc2Octave, osc2Semi, osc2Fine, osc2Unison, osc2Detune, none } },
    { "Voice", "OSC / VOICE", { width, air, airTone, drift, none, none } },
};
constexpr Bank filterBanks[] = {
    { "Response", "FILTER / RESPONSE", { filterType, cutoff, resonance, filterEnv, keyTrack, velCutoff } },
    { "Envelope", "FILTER / ENVELOPE", { fltAttack, fltDecay, fltSustain, fltRelease, none, none } },
};
constexpr Bank ampBanks[] = {
    { "Envelope", "AMP / ENVELOPE", { ampAttack, ampDecay, ampSustain, ampRelease, volume, none } },
    { "LFO", "AMP / LFO", { lfoShape, lfoRate, lfoDelay, lfoPitch, lfoCutoff, lfoWave } },
};
constexpr Bank fxBanks[] = {
    { "Ens / Delay", "FX / ENS + DELAY", { ensMix, ensDepth, ensRate, dlyMix, dlyTime, dlyFeedback } },
    { "Reverb", "FX / REVERB", { revMix, revSize, revDecay, revDamp, revPredelay, shimmer } },
    { "Phaser", "FX / PHASER", { phaserMix, phaserRate, phaserFeedback, phaserEnv, none, none } },
};

struct Page
{
    const char* heading;
    const Bank* banks;
    int count;
};

constexpr Page pages[] = {
    { "Chord pads", chordBanks, std::size (chordBanks) },
    { "Motion", motionBanks, std::size (motionBanks) },
    { "Oscillators", oscBanks, std::size (oscBanks) },
    { "Filter & envelope", filterBanks, std::size (filterBanks) },
    { "Amplitude & LFO", ampBanks, std::size (ampBanks) },
    { "Effects & output", fxBanks, std::size (fxBanks) },
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

template<int Size> struct Font;
template<> struct Font<11> { static constexpr auto widths = sp16Art::width11, mask = sp16Art::mask11;
                             static constexpr auto offsets = sp16Art::offset11; };
template<> struct Font<14> { static constexpr auto widths = sp16Art::width14, mask = sp16Art::mask14;
                             static constexpr auto offsets = sp16Art::offset14; };
template<> struct Font<18> { static constexpr auto widths = sp16Art::width18, mask = sp16Art::mask18;
                             static constexpr auto offsets = sp16Art::offset18; };

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

int bankWidth (const Bank& bank) { return std::max (40, textWidth<14> (bank.name) + 14); }

int bankX (int section, int bank)
{
    int x = (section % 2 + 1) * paneW - 4;
    for (int i = pages[section].count - 1; i >= bank; --i)
        x -= bankWidth (pages[section].banks[i]);
    return x;
}

template<size_t N>
void picture (const Raster& r, int x, int y, int width, const Ink (&art)[N])
{
    for (size_t pos = 0; pos < N; pos += width, ++y)
        std::copy_n (art + pos, width, r.px + y * r.w + x);
}

void button (const Raster& r, int x, int y, int width, int height, bool active)
{
    if (active) r.gel (x, y, width, height, 0xff488f75, 0xfff4ffd9, 0xffabd879, 0xffd1efa1);
    else r.gel (x, y, width, height, 0xff1b7898, 0xffa3eafb, 0xff12698c, 0xff2794ae);
}

void title (const Raster& r, int x, int y, const char* label)
{
    text<14> (r, x, y + 1, label, ink::deep);
    text<14> (r, x, y, label, ink::white);
}

void row (const Raster& r, int x, int y, int width, const char* label, const char* value)
{
    r.box (x, y, width, 1, 0xffe0f8f8);
    r.box (x, y + 19, width, 1, ink::rule);
    text<14> (r, x + 4, y, label, ink::label);
    text<14> (r, x + width - 4 - textWidth<14> (value), y, value, ink::text);
}

void section (const Raster& r, int x, int y, int width, const char* label)
{
    text<14> (r, x + 2, y, label, ink::label);
    const int offset = textWidth<14> (label) + 10;
    r.box (x + offset, y + 10, width - offset, 1, ink::rule);
}
} // namespace

void Sp16Screen::knobs (uint32_t ids[6], char name[SIXTEEN_TAB_NAME_SIZE]) const
{
    const Bank& bank = pages[page].banks[banks[page]];
    std::transform (bank.ids.begin(), bank.ids.end(), ids, [] (uint32_t row) { return row == none ? none : paramTable[row].id; });
    std::snprintf (name, SIXTEEN_TAB_NAME_SIZE, "%s", bank.strip);
}

void Sp16Screen::soundLoaded (int index)
{
    lastFactory = index;
    focusedSound = std::max (0, index);
    dirty = true;
}

int Sp16Screen::matchingSound() const
{
    // Match only the sound parameters. Key, voicing and progression stay
    // independent, including when an old project restores its parameter blob.
    static const auto factory = [] {
        std::array<std::array<float, paramCount>, presetCount> result {};
        for (int i = 0; i < presetCount; ++i)
            presetValuesOf (presetTable[i], result[i].data());
        return result;
    }();
    std::array<float, paramCount> live {};
    for (int i = chordSection; i < paramCount; ++i)
        if (isSoundRow ((uint32_t) i)) live[i] = value (i);
    for (int i = 0; i < presetCount; ++i)
        if (std::equal (live.begin() + chordSection, live.end(), factory[i].begin() + chordSection,
                        [] (float a, float b) { return std::abs (a - b) < 1.0e-6f; }))
            return i;
    return -1;
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
        if (inside (x, y, patchX, 0, patchW, patchH))
        {
            const int match = matchingSound();
            const int anchor = match >= 0 ? match : lastFactory;
            if (x < patchX + arrowW || x >= patchX + patchW - arrowW)
            {
                const int step = x < patchX + arrowW ? -1 : 1;
                result.sound = anchor < 0 ? (step < 0 ? presetCount - 1 : 0) : (anchor + step + presetCount) % presetCount;
                soundsOpen = false;
            }
            else
            {
                soundsOpen = ! soundsOpen;
                if (soundsOpen) focusedSound = std::max (0, anchor);
            }
            dirty = result.used = true;
            return result;
        }
        if (inside (x, y, patchX, viewsY, patchW, headerH - viewsY))
        {
            const int view = (x - patchX) * 3 / patchW;
            if (page / 2 != view) page = view * 2;
            soundsOpen = false;
            dirty = result.used = true;
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
        if (inside (x, y, 0, headerH, w, bodyTop - headerH))
        {
            page = (page / 2) * 2 + x / paneW;
            for (int i = 0; i < pages[page].count; ++i)
                if (x >= bankX (page, i) && x < bankX (page, i) + bankWidth (pages[page].banks[i]))
                {
                    banks[page] = i;
                    break;
                }
            dirty = result.used = true;
            return result;
        }
        if (page / 2 == 0 && inside (x, y, padX, padY, 3 * padDX + padW, 3 * padDY + padH))
        {
            const int col = (x - padX) / padDX, row = (y - padY) / padDY;
            if ((x - padX) % padDX < padW && (y - padY) % padDY < padH)
            {
                gesture = Gesture::pad;
                result.pad = (3 - row) * 4 + col;
                result.used = true;
            }
        }
        if (page / 2 == 0 && inside (x, y, globeX, globeY, globeSize, globeSize))
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
    const bool moving = ! soundsOpen && (sounding != 0 || (page / 2 == 2 && telemetry.peak.load (std::memory_order_relaxed) > 0.003f));
    const bool changed = sounding != shownSounding || held != shownHeld || strikes != shownStrikes;
    if (! refresh && (soundsOpen || (! changed && moving == wasMoving && (! moving || surface.now_ms - frameAt < 40))))
        return false;

    const Raster r { pixels.data(), w, h };
    const bool refreshHeader = refresh || (! soundsOpen && page / 2 == 0 && strikes != shownStrikes);
    if (refreshHeader)
    {
        const int match = matchingSound();
        if (match >= 0) lastFactory = match;
        header (r, match);
    }
    std::copy_n (sp16Art::water, w * (h - bodyTop), r.px + w * bodyTop);
    if (soundsOpen) sounds (r);
    else
    {
        r.box (paneW - 1, bodyTop, 1, h - bodyTop, ink::rule);
        switch (page / 2)
        {
            case 0: performance (r, sounding, held); break;
            case 1: synthesis (r, sounding != 0); break;
            case 2: output (r, sounding != 0); break;
        }
    }

    dirty = false;
    wasMoving = moving;
    frameAt = surface.now_ms;
    shownSounding = sounding;
    shownHeld = held;
    shownStrikes = strikes;
    bool drew = false;
    for (int y = refreshHeader ? 0 : bodyTop; y < h; ++y)
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

void Sp16Screen::header (const Raster& r, int sound) const
{
    static_assert (std::size (sp16Art::header) == w * headerH && std::size (sp16Art::water) == w * (h - bodyTop));
    std::copy_n (sp16Art::header, w * headerH, r.px);
    const auto& prog = progressionTable[choice (progression)];
    char mode[16];
    const int set = choice (mood);
    if (set) std::snprintf (mode, sizeof (mode), "Mood %+d", choice (moodShift));
    else std::snprintf (mode, sizeof (mode), "%s %s", keyTonic (choice (key), prog.minor), prog.minor ? "minor" : "major");
    const int badgeW = textWidth<14> (mode) + 14;
    r.gel (15, 54, badgeW, 21, 0xff1f728b, 0xfff4ffff, 0xff8adcdf, 0xffc3eff4);
    centered<14> (r, 15 + badgeW / 2, 55, mode, ink::deep);
    title (r, 23 + badgeW, 55, set ? moodNames[set] : prog.name);

    button (r, patchX, 7, arrowW - 2, 29, false);
    button (r, patchX + arrowW, 7, patchW - 2 * arrowW, 29, soundsOpen);
    button (r, patchX + patchW - arrowW + 2, 7, arrowW - 2, 29, false);
    for (int dir : { -1, 1 })
    {
        const int cx = dir < 0 ? patchX + arrowW / 2 : patchX + patchW - arrowW / 2;
        r.line (cx - 3 * dir, 17, cx + 2 * dir, 21, ink::white);
        r.line (cx + 2 * dir, 21, cx - 3 * dir, 25, ink::white);
    }
    char patch[64] = "Custom sound";
    const int origin = sound >= 0 ? sound : lastFactory;
    if (origin >= 0)
        std::snprintf (patch, sizeof (patch), "%02d/%02d  %s%s", origin + 1, presetCount,
                       presetTable[origin].name, sound >= 0 ? "" : " *");
    const Ink patchInk = soundsOpen ? ink::deep : ink::white;
    if (textWidth<18> (patch) <= patchW - 2 * arrowW - 10)
        centered<18> (r, patchX + patchW / 2, 9, patch, patchInk);
    else
        centered<14> (r, patchX + patchW / 2, 12, patch, patchInk);
    constexpr const char* viewNames[] = { "Perform", "Synth", "Output" };
    for (int i = 0; i < 3; ++i)
    {
        const int x = patchX + (i * patchW + 2) / 3, end = patchX + ((i + 1) * patchW + 2) / 3;
        const bool active = ! soundsOpen && page / 2 == i;
        button (r, x, 47, end - x - 2, 23, active);
        centered<14> (r, (x + end) / 2 - 1, 49, viewNames[i], active ? ink::deep : ink::white);
    }

    r.gradient (0, headerH, w, 15, 0xff82cbd9, 0xff277e93);
    r.gradient (0, headerH + 15, w, bodyTop - headerH - 15, 0xff176a79, 0xff43949b);
    r.box (0, headerH, w, 1, ink::ice);
    if (soundsOpen)
    {
        title (r, 13, 87, "Factory sounds");
        return;
    }
    for (int side = 0; side < 2; ++side)
    {
        const int s = page / 2 * 2 + side, x = side * paneW;
        const bool focused = s == page;
        const auto& lens = focused ? sp16Art::sectionLit : sp16Art::sectionIdle;
        constexpr int lensW = 12, lensH = 26;
        static_assert (std::size (sp16Art::sectionIdle) == lensW * lensH && std::size (sp16Art::sectionLit) == lensW * lensH);
        for (int y = 0; y < lensH; ++y)
            for (int col = 0; col < lensW; ++col)
            {
                const Ink pixel = lens[y * lensW + col];
                const unsigned alpha = pixel >> 24;
                if (alpha) r.blend (x + 2 + col, headerH + 4 + y, pixel, alpha + (alpha == 255));
            }
        title (r, x + 18, 87, pages[s].heading);
        if (s == Screen::chordsPage)
        {
            const int slot = telemetry.lastSlot.load (std::memory_order_relaxed);
            if (slot >= 0 && slot < padSlots)
            {
                char name[32], caption[48];
                if (set)
                {
                    const auto& pad = moodPads[set - 1][slot];
                    std::snprintf (name, sizeof (name), "%s%s%s%s", moodPitchName (pad.root, choice (moodShift)),
                                   pad.quality, *pad.slashBass ? "/" : "", moodPitchName (pad.slashBass, choice (moodShift)));
                }
                else chordText (parsedProgressions()[choice (progression)].chords[slot % 8], choice (key), prog.minor, name, sizeof (name));
                std::snprintf (caption, sizeof (caption), "%s%s", name, ! set && slot >= 8 ? " / Lift" : "");
                text<14> (r, 99, 87, caption, ink::ice);
            }
        }
        for (int b = 0; b < pages[s].count; ++b)
        {
            const int bx = bankX (s, b), width = bankWidth (pages[s].banks[b]);
            const bool active = focused && b == banks[s];
            if (active) button (r, bx + 1, 86, width - 2, 22, true);
            centered<14> (r, bx + width / 2, 88, pages[s].banks[b].name, active ? ink::deep : ink::white);
        }
    }
}

void Sp16Screen::sounds (const Raster& r) const
{
    for (int i = 0; i < presetCount; ++i)
    {
        const int x = soundX + i % soundCols * soundDX, y = soundY + i / soundCols * soundDY;
        if (i == focusedSound) button (r, x + 1, y + 2, soundW - 2, soundH - 4, true);
        else r.box (x + 4, y + soundH - 1, soundW - 8, 1, ink::rule);
        centered<14> (r, x + soundW / 2, y + 6, presetTable[i].name, ink::text);
    }
}

void Sp16Screen::field (const Raster& r, int x, int y, int width, const char* label, uint32_t id) const
{
    char formatted[64];
    formatParam (paramTable[id], value (id), formatted, sizeof (formatted));
    row (r, x, y, width, label, formatted);
}

void Sp16Screen::performance (const Raster& r, uint32_t sounding, uint32_t held) const
{
    const int prog = choice (progression), tonic = choice (key), set = choice (mood);
    const auto& sequence = parsedProgressions()[prog];
    const int last = telemetry.lastSlot.load (std::memory_order_relaxed);
    for (int slot = 0; slot < padSlots; ++slot)
    {
        const int x = padX + (slot % 4) * padDX, y = padY + (3 - slot / 4) * padDY;
        const bool lift = ! set && slot >= 8, on = (held & (1u << slot)) != 0;
        if (on) r.gel (x, y, padW, padH, 0xffae7c34, 0xfffff3bb, 0xffffd77c, 0xffffc262);
        else if (lift) r.gel (x, y, padW, padH, 0xff70a385, 0xffeaffdf, 0xffb4e3be, 0xffa7d8be);
        else r.gel (x, y, padW, padH, 0xff6b9fb7, 0xffe6fbff, 0xffb4e3f1, 0xffa0d4ec);
        const auto& chord = sequence.chords[slot % 8];
        char root[16], index[4];
        const char* quality = chordQualityText (chord);
        if (set)
        {
            const auto& pad = moodPads[set - 1][slot];
            moodRootText (pad, choice (moodShift), root, sizeof (root));
            quality = pad.quality;
        }
        else chordRootText (chord, tonic, progressionTable[prog].minor, root, sizeof (root));
        std::snprintf (index, sizeof (index), "%02d", slot + 1);
        text<14> (r, x + 6, y, root, ink::text);
        if (textWidth<14> (quality) <= padW - 10) text<14> (r, x + 6, y + 12, quality, ink::text);
        else text<11> (r, x + 6, y + 14, quality, ink::text);
        text<11> (r, x + padW - 19, y + 2, index, ink::label);
        if (slot == last) r.box (x + padW - 22, y + padH - 6, 14, 2, on ? 0xffa8590c : ink::teal);
    }
    plots.keyboard (r, 8, 256, 373, 22, sounding);
    picture (r, globeX, globeY, globeSize, sp16Art::lens);
    const int shape = choice (motionShape);
    plots.motionTrace (r, globeCenterX, globeCenterY, globeRadius, shape, sounding);
    text<14> (r, globeX + 7, globeY + 127, motionChoices[shape], ink::white);
    text<14> (r, globeX + 112, globeY + 127, "X / Y", ink::ice);

    constexpr int x = 553, col = 106, top = 118, dy = 23;
    field (r, x, top, col, "X", motionX);
    field (r, x + col + 3, top, col, "Y", motionY);
    field (r, x, top + dy, col, "Depth", motionDepth);
    field (r, x + col + 3, top + dy, col, "Rate", motionRate);
    field (r, x, top + dy * 2, col, "Sync", motionSync);
    field (r, x + col + 3, top + dy * 2, col, "Div", motionDiv);
    constexpr uint32_t routes[] = { xDest1, xDest2, yDest1, yDest2 };
    constexpr const char* labels[] = { "X to 1", "X to 2", "Y to 1", "Y to 2" };
    for (int i = 0; i < 4; ++i)
    {
        const int y = top + (3 + i) * dy;
        field (r, x, y, 128, labels[i], routes[i]);
        field (r, x + 131, y, 84, "Amt", routes[i] + 1);
    }
}

void Sp16Screen::synthesis (const Raster& r, bool sounding) const
{
    constexpr int column = 184;
    for (int o = 0; o < 2; ++o)
    {
        const int x = 8 + o * 189;
        plots.scope (r, x, 119, column, 65, o);
        field (r, x, 189, column, "Wave", ParamId::osc (o, 0));
        field (r, x, 212, column, "Position", ParamId::osc (o, 1));
        field (r, x, 235, column, "Level", ParamId::osc (o, 7));
        field (r, x, 258, column, "Detune", ParamId::osc (o, 6));
    }
    plots.response (r, 397, 119, 168, 76);
    plots.envelope (r, 397, 201, 168, 76, fltAttack, telemetry.fltEnv.load (std::memory_order_relaxed), ink::lime, sounding);
    section (r, 571, 117, 200, "Filter response");
    field (r, 571, 138, 200, "Type", filterType);
    field (r, 571, 160, 200, "Cutoff", cutoff);
    field (r, 571, 182, 200, "Resonance", resonance);
    section (r, 571, 205, 200, "Filter envelope");
    field (r, 571, 228, 98, "A", fltAttack);
    field (r, 672, 228, 99, "D", fltDecay);
    field (r, 571, 253, 98, "S", fltSustain);
    field (r, 672, 253, 99, "R", fltRelease);
}

void Sp16Screen::output (const Raster& r, bool sounding) const
{
    plots.envelope (r, 8, 119, 164, 76, ampAttack, telemetry.ampEnv.load (std::memory_order_relaxed), ink::foam, sounding);
    plots.lfoPicture (r, 8, 201, 164, 76);
    section (r, 178, 117, 203, "Amplitude envelope");
    field (r, 178, 139, 99, "A", ampAttack);
    field (r, 280, 139, 101, "D", ampDecay);
    field (r, 178, 162, 99, "S", ampSustain);
    field (r, 280, 162, 101, "R", ampRelease);
    section (r, 178, 195, 203, "LFO");
    field (r, 178, 218, 203, "Shape", lfoShape);
    field (r, 178, 241, 203, "Rate", lfoRate);

    picture (r, 397, 119, 148, sp16Art::tank);
    r.box (397, 245, 148, 13, ink::deep);
    plots.outputMeter (r, 397, 245);
    field (r, 397, 260, 148, "Volume", volume);
    if (fxBanks[banks[Screen::fxPage]].ids[0] == phaserMix)
    {
        section (r, 551, 117, 220, "Stereo phaser");
        field (r, 551, 139, 220, "Mix", phaserMix);
        field (r, 551, 168, 220, "Rate", phaserRate);
        field (r, 551, 197, 220, "Feedback", phaserFeedback);
        field (r, 551, 226, 220, "Filter env", phaserEnv);
        return;
    }
    constexpr const char* labels[] = { "Ensemble", "Tempo delay", "Shimmer reverb" };
    constexpr const char* detail[] = { "Depth", "Time", "Decay" };
    constexpr uint32_t mixes[] = { ensMix, dlyMix, revMix }, ids[] = { ensDepth, dlyTime, revDecay };
    for (int i = 0; i < 3; ++i)
    {
        const int y = 117 + 56 * i;
        section (r, 551, y, 220, labels[i]);
        field (r, 551, y + 22, 101, "Mix", mixes[i]);
        field (r, 655, y + 22, 116, detail[i], ids[i]);
    }
}
}
