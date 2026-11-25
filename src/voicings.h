// The chords the pads play, read from a table: every progression's sixteen pads in every voicing and key, voiced
// when Reverie is built (tools/voicings.cpp runs chords.h's search with the plug-in's flags and writes
// build/voicings.inc). Nothing searches while Reverie plays or paints: a chord is a lookup, so a Progression, Voicing
// or Key change takes effect on its own sample at no cost to the audio thread.
#pragma once

#include "chords.h"

#include <algorithm>
#include <cstddef>

namespace reverie
{
namespace table
{
#include "voicings.inc"
} // namespace table

namespace originalVoicings
{
// The first MPC release used these pitches for all six styles, including its Lift pads.
#include "../mac-clap/src/voicings.inc"
} // namespace originalVoicings

constexpr size_t chordBytes = 8;
constexpr size_t progressionBytes = chordBytes * padSlots * 12 * Voicing::count;
static_assert (sizeof (originalVoicings::voicingBytes) == progressionBytes * progressionCount);
static_assert (sizeof (table::voicingBytes) == progressionBytes * factoryBank::progressionCount,
               "build/voicings.inc does not match the progressions: make writes it again");

// Pad `slot`'s chord in a progression, voicing and key (out of range: clamped, the key taken modulo 12).
inline ChordNotes padChord (int progression, int voicing, int key, int slot)
{
    progression = std::clamp (progression, 0, allProgressionCount - 1);
    const unsigned char* bytes = table::voicingBytes;
    if (progression >= factoryBank::progressionCount)
    {
        progression -= factoryBank::progressionCount;
        bytes = originalVoicings::voicingBytes;
    }
    voicing = std::clamp (voicing, 0, Voicing::count - 1);
    key = detail::mod12 (key);
    slot = std::clamp (slot, 0, padSlots - 1);
    const size_t at = (((size_t) progression * Voicing::count + (size_t) voicing) * 12 + (size_t) key) * padSlots + (size_t) slot;
    const unsigned char* b = bytes + at * chordBytes;
    ChordNotes c;
    c.count = (uint8_t) std::min<int> (b[0] & 0x7f, maxChordNotes); // a chord holds at most its notes, whatever the byte says
    c.hasBass = (b[0] & 0x80) != 0;
    for (int i = 0; i < c.count; ++i) c.note[i] = (int8_t) b[1 + i];
    return c;
}

inline void padChords (int progression, int voicing, int key, VoicingSet& out)
{
    for (int s = 0; s < padSlots; ++s) out.slot[s] = padChord (progression, voicing, key, s);
}
} // namespace reverie

#include "mood.h"
