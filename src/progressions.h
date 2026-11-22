// Reverie's chord progressions: eight chords each, written in C (major or minor) as chord symbols, moved to the Key
// parameter when they play. Pads A01-A08 play them in order; A09-A16 play the same chords lifted (chords.h). Each
// loops: the eighth chord leads back to the first. The order is fixed (a project saves the progression by its
// place): new progressions go at the end.
//
// Symbols: a root (C, Db, F#...), a quality (chords.h: maj9, m11, 13sus, 7#9, 6/9...) and an optional bass after a
// slash (Cmaj9/G). The tests parse every one.
#pragma once

namespace reverie
{
struct ProgressionText
{
    const char* name;
    bool minor;          // in C minor (the key's minor), else C major
    const char* chords;  // eight symbols
};

inline constexpr ProgressionText progressionTable[] = {
    // Major keys
    { "Velvet Night", false, "Fmaj9 Em9 Dm9 G13 Cmaj9 Am9 Dm9 G9sus" },
    { "Moonlit Two-Five", false, "Dm9 G13 Cmaj9 Am9 Dm9 Db9#11 Cmaj9 A7#9" },
    { "Sunday Sanctuary", false, "Fmaj9 F#dim7 Cmaj9/G A7b13 Dm9 G13sus G13 Cmaj9" },
    { "Tidal Hearts", false, "Fmaj9 E7#9 Am9 Gm9 C13 Fmaj9 E7alt Am9" },
    { "Backdoor Bliss", false, "Cmaj9 Fmaj9 Fm9 Bb13 Cmaj9 Am9 Dm9 G7b9" },
    { "Aurora", false, "Cmaj9#11 Emaj9 Abmaj9 Fm9 Dbmaj9#11 Ebmaj9 Bb13sus Bb13" },
    { "Starlight Lydian", false, "Cmaj9#11 D9/C Bm9 Em9 Am9 D13 Gmaj9 G9sus" },
    { "Rose Quartz", false, "Cmaj9 Bm7b5 E7b13 Am9 Gm9 C13 Fmaj9 Fm6/9" },
    { "Honey Drip", false, "Em9 A13 Dm9 G13 Cmaj9 Fmaj9#11 F#m7b5 B7#9" },
    { "Neon Dusk", false, "Fmaj9 G13 Em9 Am9 Dm11 G13 Cmaj13 C13#11" },
    { "Halo Ballad", false, "Cmaj9 Gadd9/B Am9 Am9/G Fmaj9 Cmaj9/E Dm9 G13sus" },
    { "Late Night Drive", false, "Dm11 Em9 Fmaj13 E7#9 Am11 Ab13#11 G13sus Cmaj9#11" },
    { "Crystal Cave", true, "Cm11 Abmaj9#11 Ebmaj13 Bbadd9/D Cm9 Fm11 G13sus G7b9" },
    { "Golden Hour", false, "Cmaj9 Gm9 C13 Fmaj9 Fm9 Bb13 Ebmaj9 G7alt" },
    { "Cotton Clouds", false, "Cmaj9 Em9 Fmaj9 Fm6/9 Em7b5 A7b13 Dm9 G13sus" },
    { "Bossa Moon", false, "Cmaj9 C#dim7 Dm9 D#dim7 Em9 A7b13 Dm9 G13" },
    { "Celestial Pedal", false, "Cmaj9 Dadd9/C Bbmaj9/C Fmaj9/C Abmaj9#11/C Bbmaj9/C Fm6/9/C Cmaj9#11" },
    { "Silhouette", false, "Fmaj9 Fm9 Em9 Eb9#11 Dm9 Db9#11 Cmaj9 G9sus" },
    { "Neo Classic", false, "Cmaj9 A7#9 Dm9 G7b13 Em9 A13 Dm9 G13sus" },
    { "Prism", false, "Dm11 Em11 Fmaj9#11 G13sus Am13 Fmaj9#11 Ebmaj9#11 G13sus" },
    { "Ocean Floor", false, "Cmaj9 Bbmaj9#11 Abmaj9 Bb13sus Cmaj9#11 Gm9 Fmaj9 Fm6/9" },
    { "Sanctuary Steps", false, "Cmaj9 C13 Fmaj9 F#dim7 Cmaj9/G A7#9 Dm9 G13b9" },
    { "Afterglow", false, "Cmaj9 G6/9/B Am9 Fmaj9#11 Cmaj9 Gadd9/B Am11 Fm6/9" },
    { "Firefly", false, "Cmaj9 E7b13 Am9 C13 Fmaj9 D13 Dm9 G7b9" },
    { "Dorian Drift", true, "Cm11 F13 Ebmaj9#11 F13sus Dm11 G13sus Cm13 F13#11" },
    { "Glass Garden", false, "Fmaj9#11 Em9 Ebmaj9#11 Dm11 Dbmaj9#11 Cmaj13 Gm11 C13#11" },
    { "Silver Lining", false, "Fmaj13 Em9 A9b13 Dm11 G13sus Cmaj9/E Gm11 C13b9" },
    { "Velvet Rope", true, "Cm11 Bmaj9#11 Bbm9 Eb13 Abmaj13 G7#9 Cm13 F9#11" },
    { "Heavenly Steps", false, "Cmaj9 Ebdim7 Dm9 G13 Em11 A7b13 Dm11 G13sus" },
    { "Paper Lanterns", false, "Cmaj9 Am11 Fmaj9 G13sus Em9 Am11 Dm9 Fm6/9" },
    { "Daydream Descent", false, "Dm9 Db13 Cmaj9 B7alt Bbmaj9#11 A7#9 Abmaj9 G13" },
    // Minor keys
    { "Amethyst", true, "Cm9 F13 Ebmaj9 Abmaj9#11 Dm7b5 G7b13 Cm11 Bb13sus" },
    { "Midnight Dorian", true, "Cm11 F13 Bbmaj9 Ebmaj9 Abmaj9#11 Dm7b5 G7#9 G7b9" },
    { "Smoke and Velvet", true, "Cm9 Abmaj9#11 Ebmaj13 Bb13sus Fm11 Gm9 Abmaj9 G7alt" },
    { "Obsidian", true, "Cm9 Dbmaj9#11 Bbm9 Abmaj9 Fm9 Dbmaj9 Dm7b5 G7b9" },
    { "Rainy Neo Soul", true, "Cm9 Fm9 Bb13 Ebmaj9 Abmaj9 Fm9 Dm7b5 G7#9" },
    { "Line Cliche", true, "Cmadd9 CmMaj9 Cm9 Cm6/9 Abmaj9 Fm9 Dm7b5 G7b9" },
    { "Nocturne", true, "Cm9 Abmaj9#11 Fm9 G7b13 Ebmaj9 Abmaj9 Dm7b5 G7#9" },
    { "Sapphire", true, "Dm11b5 G9b13 Cm9 C7#9 Fm11 Bb13 Ebmaj13 Abmaj9#11" },
    { "Witching Hour", true, "CmMaj9 Abmaj9#11 Fm9 Dbmaj9#11 Bbm9 Ebmaj9 Abmaj9 G7alt" },
    { "Rose Petal", true, "Cm9 Bb6/9 Abmaj9#11 G9b13 Fm11 Ebmaj9/G Dbmaj9#11 G7alt" },
    { "Lunar Tide", true, "Cm11 Emaj9#11 Abmaj9#11 G13sus Fm11 Dbmaj9#11 Ebmaj9/G G9b13" },
    { "Deep Indigo", true, "Cm9 Bbm9 Abmaj9 Gm9 Fm9 Gm9 Abmaj9 Bb13sus" },
    { "Phantom Waltz", true, "Cm9 G7b13/B Cm9/Bb Am7b5 Abmaj9#11 Fm9 Dm7b5 G7b9" },
    { "Echo Chamber", true, "Cm9 F13 Cm11 F13sus Abmaj9 Gm9 Fm9 Bb13" },
    { "Moonstone", true, "Cm11 Dbmaj9#11/C Cm13 Bbm11 Eb13#11 Abmaj13 Dm11b5 G7alt" },
    // Extended bank: cinematic, neo-soul, gospel, city-pop, modal, dark ambient & UKG
    { "Cathedral Sun", false, "Cmaj13 Abmaj9#11 Emaj9 G13sus Fmaj9#11 Dbmaj9#11 Bbmaj9#11 G13sus" },
    { "Shibuya Glow", false, "Fmaj13 G13/F Em11 A9b13 Dm11 Db13#11 Cmaj13 C13" },
    { "Gospel Horizon", false, "Cmaj13 E7#9/B Am11 Gm11 C13b9 Fmaj13 F#m11b5 G13sus" },
    { "Cloud Nine", false, "Fmaj9#11 Ebmaj9#11 Dbmaj9#11 Cmaj13 Am11 Ab13#11 G13sus C13#11" },
    { "Solaris", false, "Cmaj9#11 D6/9/C Em11 Dadd9/F# Gmaj13 Fmaj9#11 Ebmaj9#11 G13sus" },
    { "Silk Highway", false, "Cmaj13 Bm11b5 E9b13 Am11 D13#11 Fmaj13 Fm6/9 G13sus" },
    { "Amber Glass", false, "Cmaj9#11 Eb13#11 Abmaj13 B13#11 Emaj9#11 G13sus Fmaj9#11 G9b13" },
    { "Morning Choir", false, "Cmaj9 Cmaj9/E Fmaj13 Fm6/9 Em11 A9b13 Dm11 G13sus" },
    { "Opal Drift", false, "Cmaj9#11 Gadd9/B Fmaj9#11/A Cmaj9/G D9#11/F# Fmaj13 Ebmaj9#11 G13sus" },
    { "Neon Rain", false, "Em11 Eb13#11 Dm11 Db13#11 Cmaj13 F#m11b5 B7#9 B7alt" },
    { "Velvet Eclipse", true, "Cm11 Emaj9#11 Fm11 Dbmaj9#11 Abmaj13 Ebmaj9/G Dm11b5 G7alt" },
    { "Midnight Garage", true, "Cm11 Ebm11 Fm11 Abm11 Gm11 Bbm11 Abmaj9#11 G9b13" },
    { "Abyssal Choir", true, "Cm11 Dbmaj9#11 Eb13sus Dbmaj9#11 Bbm11 Abmaj9#11 Fm11 G7b9" },
    { "Satin Noir", true, "Cm13 F13#11 Abmaj13 G9b13 Fm11 Bb13#11 Ebmaj13 G7alt" },
    { "Astral Minor", true, "Cm11 Abmaj9#11/C Bb6/9/C Fm11/C Dbmaj9#11 Ebmaj13 Fm11 G13sus" },
    { "Cobalt Pulse", true, "Cm11 Cm9/Bb Abmaj13 G9b13 Fm11 F#dim7 Cm9/G G7alt" },
    { "Mirage", true, "Cm11 F13#11 Bmaj9#11 E13#11 Abmaj9#11 Db13#11 Dm11b5 G7alt" },
    { "Starlit Requiem", true, "Cmadd9 Abmaj13/C Ebmaj9/Bb F9#11/A Abmaj9#11 Fm11 Dm11b5 G7b9" },
};
constexpr int progressionCount = (int) (sizeof (progressionTable) / sizeof (progressionTable[0]));
constexpr int chordsPerProgression = 8;

struct ProgressionNames
{
    const char* list[progressionCount];
    constexpr ProgressionNames() : list {}
    {
        for (int i = 0; i < progressionCount; ++i) list[i] = progressionTable[i].name;
    }
};
inline constexpr ProgressionNames progressionNameList {};
inline const char* const* const progressionNames = progressionNameList.list;
} // namespace reverie
