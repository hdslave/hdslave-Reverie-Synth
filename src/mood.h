// Included by voicings.h after the edition's ChordNotes and progression lookup.
#pragma once

namespace reverie
{
inline ChordNotes performanceChord (int progression, int voicing, int key, int slot, int mood, int shift)
{
    if (mood <= 0 || mood > moodCount) return padChord (progression, voicing, key, slot);
    const auto& pad = moodPads[mood - 1][std::clamp (slot, 0, padSlots - 1)];
    ChordNotes result {};
    result.count = pad.count;
    result.hasBass = true;
    result.note[0] = (int8_t) (pad.bassNote + shift);
    for (int i = 1; i < result.count; ++i)
        result.note[i] = (int8_t) (result.note[0] + pad.upper[i - 1]);
    return result;
}

inline const char* moodPitchName (const char* name, int shift)
{
    if (shift % 12 == 0 || ! *name) return name;
    constexpr int pc[] = { 9, 11, 0, 2, 4, 5, 7 };
    const int accidental = (name[1] == '#') - (name[1] == 'b');
    return keyChoices[detail::mod12 (pc[name[0] - 'A'] + accidental + shift)];
}

inline void moodRootText (const MoodPad& pad, int shift, char* out, size_t size)
{
    std::snprintf (out, size, "%s%s%s", moodPitchName (pad.root, shift), *pad.slashBass ? "/" : "",
                   moodPitchName (pad.slashBass, shift));
}
} // namespace reverie
