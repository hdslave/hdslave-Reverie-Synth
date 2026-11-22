// Reverie's chord progressions: eight chords each, written in C (major or minor) as chord symbols, moved to the Key
// parameter when they play. Pads A01-A08 play them in order; A09-A16 play the same chords lifted (chords.h). Each
// loops: the eighth chord leads back to the first. The order is fixed (a project saves the progression by its
// place): new progressions go at the end.
//
// Since 0.3.0 every progression is one of the M3 workstation's factory pad chord sets (its Motion Synth, Pad, SlowSynth
// and Vocal presets), each chord named from the pad's own notes so the name holds exactly its pitch classes, moved so
// pad 1's root is C. tools/m3_progressions.py makes the table from the chord pack; the names are Reverie's own. The
// 0.1-0.2 progressions were retired: a state saved before 0.3.0 starts at the first progression (reverie.cpp).
//
// Symbols: a root (C, Db, F#...), a quality (chords.h: maj9, m11, 7sus, add9, m7(11)...) and an optional bass after a
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
    // --- made by tools/m3_progressions.py: do not edit by hand ---
    // Motion Synth Combis
    { "Ringed Orbit", false, "C D Fadd9 A7sus Eb6 Eb7sus/Db Abm/B Bb9sus" },
    { "Signal Tower", true, "Cm7 Ebm7 Ab Gb B Ab7 F F9sus" },
    { "Watchful Horizon", false, "Cmaj7 Fm7 Gm7(11) Fm11/Ab Am7 Cmaj7/E G7sus/F G7sus" },
    { "Low Cathedral", false, "C Eb Bb F Bb Db Ab Bb" },
    { "Frost Dawn", false, "C Em/B Am Bm7/A C/G Cm/G D7/F# G9sus" },
    { "Tender Embrace", false, "C Cmaj7/G Am A7/E Dm F7 Cadd9/E G7/D" },
    { "Starbound Voyage", true, "Cm Ab/C Bm7 Gm/Bb F/A Ab7b5 G G7/F" },
    { "Spectral Prism", true, "Cm7 Gm7 Dbsus2 Fm7 Eb7sus/Db Abmaj9/C Fm7(11) Ebadd9/G" },
    { "Neon Boulevard", true, "Cm Bb Fm Ab/Eb Db Bbm Fm Eb" },
    { "Hidden Passage", false, "Csus2 Fm7(11) Ebadd9/G Absus2 Dbadd9/F Ebadd11 Dbadd9 Bbm7" },
    { "Kinetic Drift", true, "Cm9 Fm9/Ab Ebmadd9/Gb Ebm7(11) Emaj9#11 Bsus2 Amaj9#11 Gb7sus" },
    { "Prismatic Turn", true, "Cm7 Bbadd9/D Ebadd9 Fsus4 Gm F6 Bbadd11/Eb F7sus" },
    { "Cell Division", true, "Cm Bb Eb Gm7 Abadd9 Bb Eb Bb" },
    { "Sugar Village", true, "Cm Gm Db Eb Ab Bb7/Ab Eb G7/D" },
    { "Horizon Fade", true, "Cm Bb/D Eb Ebadd9/G Abadd9 Ebmaj9/G Dbadd9 Absus2" },
    // Motion Synth Programs
    { "Chrono Shift", true, "Cm11 Ebm11 Bmaj9 Gbadd9/Bb Eadd9/G# Badd11/E Ebm11 Dbm11" },
    { "Tidal Formant", true, "Cm Bbm Abmaj7 Gbsus2 Abadd9 Db6/9 Fm9 Dbadd11/Gb" },
    { "Resonant Pulse", true, "Cm9 Ab6/9 Gm7 Cm9/Eb F7sus G7sus Ab7sus Bb9sus" },
    { "Quiet Machinery", true, "Cmadd11 Dbmaj7 Cm Ebm6 Fmadd9 Gbmaj7 Fm7/Ab Ebadd9/G" },
    { "Folded Space", true, "Cmadd11/Eb G9 Ab Bbm/Db Fm/C Db Bbadd9 Cm/G" },
    { "Mirror Steps", true, "Cm7 Dm7 Fm7 Gm7 Cm7 Dm7 Fm7 Gm7" },
    { "Neon Glyph", false, "C Bbsus2#11 Am7 Fm7 Ebm Ebm7/Db B6 Fsus2/G" },
    { "Temple Gong", true, "Cm Csus2 Gmadd11/C Am7/C Ab/C Abadd#11/C Gmadd11/C D7sus/G" },
    { "Slow Bounce", true, "Cm Cm7/Bb F/A Fm/Ab Fm Dbmaj9 Dbmaj9/C C" },
    { "Endless Ring", false, "Csus2 Absus2 Fsus2 Dbsus2 Bbsus2 F#sus2 Ebsus2 Bsus2" },
    { "Last Departure", true, "Cm Ab Bb Gm Ab Fm Eb Bb" },
    { "Eight Sides", false, "C6 Am Em Dm7 C Bb G9sus A9sus" },
    { "Voice Lattice", true, "Cm Db Gb E6 Dbsus2 Badd9 Bbm Ab7sus" },
    { "Clockwork Lace", false, "Csus2 Ebsus2 Dsus2 Fsus2 D7sus/C F7sus/Eb E7sus/D G7sus/F" },
    { "Dice and Ivory", true, "Cm Fm Eb Gm Ab Eb/G Gb6 F7sus" },
    { "Second Thoughts", false, "Csus#4 C Em/B G/B A9sus Am7 D9sus D" },
    { "Distant Broadcast", true, "Cm F/A Bbm Edim7 Bb/F D/F# Gm7 G" },
    { "Sweeping Choir", false, "Csus2 Fm Bbsus2 Ab F Db Ebadd9 F7sus/Bb" },
    { "Gated Glow", true, "Cm7 F Cm/Eb F Bbm Abmadd11/Db Ebm Emaj7" },
    { "Phosphor Tide", true, "Cm Bb Fm7(11) Gm Cm Eb Bb Fm7(11)" },
    { "Resin Rhythm", true, "Cmadd9 Dm Ebmaj7 F6 Fm6 Gm7/F Em7(11) G7/F" },
    { "Light Steps", true, "Cm Eb Fm7 Bbsus4 Cm Cm7/Eb Fm G" },
    { "Platform Clock", true, "Cm Bbadd9/C Dm7/C Cm Cmadd11/F Bb/F Fm Bb" },
    { "Mischief Broth", true, "Cm Ebmaj7 Fm7(11) G7 F G Abmaj7 Bb7" },
    { "Slow Becoming", true, "Cm C9sus Cm7/Bb Gm7 Dm Eb Dm7 G" },
    { "Water Voices", false, "C Am Bbsus2/C Bb/D Csus2/D Cm/Eb Ab Fm" },
    { "Liquid Ascent", false, "C Em7/G Fmaj9/A Gadd9/B Fadd9/A G7 F Dmadd11/G" },
    { "Shapeshift Cloud", false, "C Em7/G Cadd9/E A7sus C6/9/G C6/9 C6/9/E C6/9/G" },
    { "Timeless Expanse", true, "Cm11 Ebm11 Bbm11 Dbm11 Abm11 Bm11 F#m11 Am11" },
    { "Monarch Drift", true, "Cm Ab Bb/D Eb Abm Gb E Abmaj9/Bb" },
    { "Fairy Lantern", false, "C Eb/Bb Ebsus#4/Bb Ebmadd11/Ab Ebm7/Gb Bb/F Ab G/B" },
    { "Freefall", true, "Cm Ebadd#11 Gm/D Cm/Eb Dbdim/E A/E Dm/F Fdim7" },
    { "Seed Pods", false, "C G/B Am7(11) Em11 Dm11 Am11 Cadd11/F Fadd11/Bb" },
    { "Skyward Glider", true, "Cm Eb6/9 Abmaj13 Gbmaj13 Ebm11 Gbmaj9sus/B Bbm11 Gm11" },
    { "Zephyr Cascade", true, "Cm11 Ebadd11/Ab Fm11 Abadd11/Db Bbm11 Dbadd11/Gb Ebm11 Gbadd11/B" },
    { "Metamorphosis", true, "Cm7(11) F7sus/Eb Abmaj9 Cm7/Bb F7sus Ebadd11/Ab F7sus/Bb Fm7(11)" },
    { "Moth Wings", true, "Cm7 Bbsus4 F/A Bb7sus/Ab Amadd11 Bbsus2#11 Gb7/Bb Bb" },
    { "Circle of Dreams", false, "C7sus F9sus Bb9sus Eb9sus Ab9sus Db9sus F#9sus B9sus" },
    { "Spiral Stair", false, "C7 C7sus Fadd11/A Bb Gmadd9 C6 Fadd#11 Fadd9" },
    { "Night Rotation", true, "Cm Cm7 Dm7/C Gm Gm7 Cm F9sus G7" },
    { "Lunar Capsule", true, "Cm7 Cmadd11/F Gmadd11 D7sus Ebadd9 C9sus Am7 G" },
    { "Mirror Lake", false, "C G Am Bbmaj7 Cm7/Bb Bbadd9/C Eb6 D/F#" },
    { "Hummingbird", true, "Cm7 Abadd9 Gm/Bb Absus2 Ab Eb/G Fm7 Bb9sus" },
    { "Sparkle Veil", true, "Cm6 Abmaj7/Eb Abmaj7#11/D Ebm9/Db Amaj9 Bmaj7#5/A# Abm7 G7" },
    { "Scattered Light", false, "Csus2 Bbm7(11) Bb7sus/Ab Fsus2/G Fm7(11) F7sus/Eb Dbsus2 Cm7(11)" },
    { "Stained Glass", false, "C Bb F/A Ab6 Ebm Bbm/Db Ab G/B" },
    { "Reed Whisper", false, "C F C/G Fmaj7 F Em C G7" },
    { "Delayed Hours", true, "Cm Gm Bbm7 Fm7 Abm Ebm7 Bmaj7 Abm7" },
    { "Old Hymnal", false, "C Csus2#11 Gsus4 G A A/C# G/D D" },
    { "Stutter Choir", false, "Csus4 Bbadd9/D Ebsus2 Fsus2 Gsus2 Fmadd9/Ab Bbsus4 Csus4" },
    { "Torchlight", true, "Cm Ebadd#11/A Bb Bb6 Cm7 F/C Dm G/D" },
    { "Silk Ribbon", false, "Csus4 C Dm7/C Edim/Bb F/A Emadd11/A Gmadd11/Bb Gm/Bb" },
    { "Deep Sea Bloom", true, "Cm7/Bb Bbm7(11) Bmaj7/A# Bsus4 Bm7/A Am7(11) Asus4 Am7/G" },
    { "Satellite Blink", true, "Cm7 Ebmaj7 Abmaj7 Fm7/Eb Dm7b5 Amadd11/D Dbmaj7 Bbm9" },
    { "Countdown", false, "Cadd9 Db7sus E9sus Dbm7(11) Amaj9 B Gmaj9 F#m7(11)" },
    { "Mercury Rain", true, "Cm Ebm Bb/D Dbadd#11 Ab/C Bmaj7 Eb/Bb Bb" },
    { "Grid Shimmer", true, "Cm7 Gm7 Am7 Em7 Amaj7 Am7 Dm7/A G6" },
    { "Carefree Sway", true, "Cm Cm/Eb Fm G/B Fm G Ab6 Bbadd9" },
    { "Golden Avenue", false, "Cmaj9 Dm7(11) G9sus G9 Dm9 Em7 Am7/G Am7/G" },
    // Pads & Vocal Combis
    { "Bow and Gourd", true, "Cm7(11) Ab6/9 F9sus Bbm11/Db Cm9 Ebmaj9sus/Ab Cmadd11/F Gm7(11)" },
    { "High Thermal", true, "Cmadd9 Gm7(11) Ebadd11/Ab Bb Fm7 C9sus Abadd11/Db G7sus" },
    { "Spellbound", false, "Cadd9 G/B Em D A Bm G A" },
    { "Slow Crossfade", false, "C Am6 Abmaj7 F Ebadd9 Dbmaj7 Ab G7" },
    { "Rain on Glass", false, "Csus2 Ebsus2 Absus2 F#sus2 B6 Emaj7 Amaj7#11 Dmaj9#11" },
    { "Adrift", true, "Cmadd11/F Bb/F Fm Eb Dbmaj7 Cm7 Fmadd9 Ebadd9" },
    { "Vowel Moment", false, "C Am/C Cadd9/D F Am Ddim/Ab C/G F#m7b5" },
    { "Cool Meadow", false, "Csus2 Bbsus2 Fadd9 C Abadd9 Bbadd9 G7sus C" },
    { "Closing Credits", true, "Cm Fm/C G7/B Eb/Bb Bbaug Bbm Cm7/Bb Adim" },
    { "Wide Choir", false, "Csus2 Abadd9/C Bbsus2/C Csus4 Bb11/Ab Fm7(11) C7sus/Bb G7sus" },
    { "Moonlit Sleep", true, "Cm7(11) Abmaj13 Fm7(11) Abadd11/Db Gbadd9 Ebm7(11) Badd11/E Db7sus" },
    { "Silicon Lullaby", true, "Cm7(11) Gm7(11) Abadd9 F Bbsus2 G7sus Eb6 Bbadd9/D" },
    { "Orchestral Dawn", false, "Cmaj9 D7sus Dm9/F Bbmadd9/Db Dbsus2/Eb F G9/F Ebmaj7#11/G" },
    { "Warm Front", false, "C Gmadd11/C Fmaj7/C Amadd11/D F6 Em7 Bm7b5/F G" },
    { "Rising Voices", true, "Cm7(11) Gm7(11) Dbadd9/F C/Eb Cm/Eb Bbadd9/D Eb7sus/Db Abadd9/C" },
    { "Velvet Quartet", false, "C13sus F13 Bb13sus A7alt Dm11 Bb13#11 Abmaj13 Db13b9/G" },
    { "Ripple Field", true, "Cmadd11/F Cm7 Dbmaj7/Ab Eb6 Dbmaj7/F Cm7 Dbsus2 Fm11" },
    { "Monastery", false, "Cadd11 Cm11 Fadd11/C Abmaj9/C Dbmaj7/F F9sus Bbm9 Ebmaj7/Bb" },
    { "Unending Bow", false, "Cmaj7#11/B AmMaj7 Am7/G Cadd#11/F# E E7sus/D Cmaj7 Am6" },
    { "Cloud Bells", true, "Cmadd9 Abmaj9 Dm7(11) Dbmaj9 Gbmaj9#11 Dbmaj9/F Ebm7(11) Dbadd9/F" },
    { "Infinite Choir", false, "C Gmaj7/B Em Bm/D Cmaj7 Fmaj7 Bbmaj7 Abmaj7#11" },
    { "Dust and Light", false, "C Am Dm Fadd9 F Em Dm Gsus4" },
    { "Trembling Air", false, "C7 Fm Cm/Eb Dm7 Cm7/Bb Bb Ebsus4 Eb" },
    { "Quiet Aside", true, "Cm F/A Bbadd9 C7/Bb Am A7/C# Dm G/B" },
    // SlowSynth Pad Programs
    { "Vintage Haze", true, "Cm9 Badd9/D# Ebadd9/G Ab7sus/Gb Ebsus2/F Amaj7b13/E Dm/G# Bmaj7#11" },
    { "Open Sky", false, "Cadd11/F Bb6/9/F D9sus G7sus/C E9sus E9 Cadd9/G Gm7" },
    { "Bright Tomorrow", true, "Cm Am7b5 Abmaj7 Fm7 Ebm B Abm7 Bb" },
    { "Digital Hearth", false, "Cadd9/D Dm Amadd11/D Gm/D Ebadd9/G Fsus2/G Gm7(11) Ebadd9/G" },
    { "Old Regrets", true, "Cm7 Fm7 Dbmaj7 Bb7sus Dm Eb Abmaj9 G9sus" },
    { "Mellow Haze", false, "Cadd9 Abmaj7 Bbadd9 Am Dm Cadd9 Abmaj7 G9sus" },
    { "String Garden", false, "C6/9 Ebmaj7#11 Bb6/9 Eb6/9 C7sus/F Fmaj7 Dm7 Fmadd9/Ab" },
    { "Sine Lullaby", false, "Cmaj9 Cm9 Abmaj7 Am7 Eb Dm7 Bbmaj7 Fmaj7/A" },
    { "Suspended Hours", true, "Cmadd11/Eb Bbadd9/D Bbmaj7 Am7 Gm7 Fmaj7 Bbadd11/Eb D7" },
    { "Soft Daydream", true, "Cmadd9 Fm7 Ebmaj7/Bb Abmaj7 Gm7 Ab6/9 Bbadd9/F Gm" },
    { "Glacier Breath", true, "Cm Ebm Bb/D Dm Fm Eb7 Abadd9 Eb" },
    { "Trance State", true, "Cm Fm7 Eb Bb F Bbm Db6 Gm7" },
    { "Fond Memory", true, "Cm Gm F Cm7 Cadd9 Eb6 Abmaj9 G9sus" },
    { "Ember Glow", true, "Cm7 Fm Dm7 G9sus A F Dm G9sus" },
    { "Long Sweep", false, "Cadd9 F6/9 Bb6/9 A7sus Dmadd9 Bbmaj7 Cm7/Bb G9sus" },
    { "Closing Filter", true, "Cm9 Gaddb9/B Cm7/Bb Gm7 Fm Eb/G Db/Ab G" },
    { "Thinning Air", true, "Cm Dm7 Eb F Gm Cm7(11) Dm7(11) Gadd9" },
    { "Hybrid Strings", true, "Cm7(11) Dm7(11) Fsus2 D7sus/G Bbm/Db F/C Eb/Bb Bb" },
    { "Real and Ghost", true, "Cm Abadd9 Bb Cmadd11 Fm Cmadd9 Ab G7" },
    { "Honeyed Pad", true, "Cm7 Ebmaj7 Dm Gm Eb Ab Abadd11/Db Gm7(11)" },
    { "Brass Clouds", false, "C7/Bb Bb6/9 Am7 F/A Dm7 C F6 Gadd9" },
    { "Sea Breeze", true, "Cm Em7 Am9 Bm7 Eadd9 Badd9 Gsus2 B7sus" },
    { "Comet Tails", true, "Cm Gm Bbm7 Fm7 Abm7 Bbm7 Db11/B G7b13" },
    { "Evocation", true, "Cm Bb Ab Fm Bb Cm Bb/D G/B" },
    { "Ghost Voices", true, "Cm7(11) Bbadd9 Ab Gadd9 Amadd9 Gmaj7 Am7 Dadd9" },
    { "Whale Song", false, "C6/9/E Gadd9/D D11/C Gmaj7/B Am11 Gmaj7 Cm13 D7sus" },
    { "Angel Wings", true, "Cm9 Gadd11 Ebadd9 G9sus C Dm7(11) Am G" },
    { "After Midnight", false, "C7sus Bbm7(11) Ebmadd9/Gb Dbadd9/F Bmaj9/D# Dbadd11 Emaj9/G# Bbm7(11)" },
    { "Easy Glow", true, "Cm F Bbm7 Fm7 G9sus A9sus Bb9sus G9sus" },
    { "Hiss and Hum", true, "Cm Fm/C G/B Cm/Eb Fm D7/F# D7sus G/D" },
    { "Molten Iron", true, "Cm Ebm Bbm/F Fm Db/Ab Abadd9 Bmaj7/D# Bbsus4" },
    { "Reed Organ", false, "C Cm Csus4 Abm Fm7 Gm7 Bbm7 Cm7" },
    { "Brass Horizon", true, "Cm Abmaj7/C Eb/Bb Bb Fm7 Bb7sus/Ab Bbsus4 Bb" },
    { "Slow Sine", true, "Cm Dm Gm Ebmaj9 Cm Bbmaj7 Am A7/C#" },
    { "Vector Field", true, "Cm11 Dm11 Gm7 A7/G F/C Gmadd9/Bb Abmaj7 Gm7" },
    { "Ripple Tank", true, "Cm7 Gm7 Dbsus2 Fm7 Abm7(11) Ebm7 Bb7sus C7sus" },
    { "Gentle Bowing", true, "Cm7 Bb6/9/D Fm7(11) Cm7(11) Gm7 Ebadd11/Ab Bbadd9 Bbm11/Db" },
    { "Silent Hours", false, "C7/Bb F/A B7/A E/G# A7/G D/F# G7 C" },
    { "Sealed Letter", true, "Cm Cm7/Bb Fm/Ab Fsus2/G Ab Ddim/Ab Eb Dm7(11)" },
    { "Stillness", true, "Cm Ab Fm Db Bbm Gb Ebm B" },
    { "Old Photographs", true, "Cm7 Eb9sus Fm7(11) G7sus Absus2#11 Dbmaj9sus/Gb Gb9/E Bb9/Ab" },
    { "Simple Sweep", true, "Cm11 Fm11 Dm11 G13b9 Cadd9 Dm7/C Abmaj9 Dbmaj9" },
    { "Wide Vista", true, "Cmadd11/F Bbm9 Ebm11 Absus2 Dbm11 Emaj9sus/A Ab7sus Dbmaj13/Gb" },
    // Vocal & Airy Programs
    { "Scat Cafe", false, "C7/6 B13b9 Bbmaj7 A13b9 Amadd11/D G11 C9sus Bbm6/9/A" },
    { "Baritone Lounge", false, "Cmaj13 Cm11/Eb Am9 D13b9 Gadd9 Bb13 Cm11/Eb D9sus" },
    { "Soprano Smoke", true, "Cm7b5 F13b9 E13sus Amaj13 Ebm7b5 Ab13b9 Gb13sus B13b9" },
    { "Chapel Choir", false, "C Bbadd9/D Ebsus2 D/F# Am7b5/G Gm Eb Bbmaj7/F" },
    { "Circle of Fourths", true, "Cm11 Fm11 Bbm11 Ebm11 Abm11 Dbm11 F#m11 Bm11" },
    { "Breathing Aahs", true, "Cm9/Bb Dmaj7/A Bbmaj9/A Abmaj9#11 Cmaj7/G Cm7b5/Gb Gm9/F Dbmaj7/Ab" },
    { "Soft Aahs", false, "C Gmaj7/B Em Bm/D Cmaj7 Fmaj7 Bbmaj7 Ebmaj7" },
    { "Twin Oohs", false, "Cadd9 Bbmaj13 Bb11/Ab Bbadd9/F Ebmaj7#11 Bbmaj7/D Ebmaj9/Ab Gm11" },
    { "Gentle Oohs", false, "C Em F Am Bb Dm Eb Absus2" },
    { "Choral Mist", true, "Cdim/Eb Ab7 Db6 Abadd9/C Bbm Bbm7/Ab Gb6/9 Dbadd9/F" },
    { "Steppe Voice", true, "Cm7(11) Bbm7(11) Fm7(11) Gm7(11) Gm/D F#m/C# Gm Bm11" },
    { "Birdsong Dawn", false, "C/E Em Bm/D D Dm Am/C Am G" },
    { "Tape Choir", false, "C Cmaj7 F/C Fm/C Dm7 G7/D Dm7 G7" },
    { "Vowel Shift", true, "Cm Bb Ab Eb Ab Bb Bdim7 Cm" },
    { "Highland Voices", true, "Cm9 Abmaj9 Fm9 Bbm9/Db Abmaj7/C Gbadd9/Bb Abadd9 Gbmaj7" },
    { "Alpine Echo", false, "Cmaj7/B AmMaj7 Am7/G Cadd#11/F# E E7sus/D Cmaj7 Am6" },
    { "Dream Chorus", true, "Cm9 Am11 Gm9 Fmaj9 F#m9 Abm9 Gmaj9 Cmaj9" },
    { "Polar Night", true, "Cm Ab/C G/B Gm/Bb F/A Fm/Ab Dm7b5 G7" },
    { "Listening Post", true, "Cm Eb Gm Bdim Ab Fmadd11 Bbadd9/D G7" },
    { "Robot Hymn", true, "Cm7 Gm7 Ab Ebadd9 Ab/C Bb/D Eb Abadd11/Db" },
    { "Machine Psalm", false, "Cmaj7 Dm7 Ebm7 Em7 Fm Eb Dbmaj7 F#13" },
    { "Chrome Ballad", true, "Cm9 B7alt Bbm9 A7alt Abmaj7 Dbmaj9 Gm7b5 C7sus" },
    { "Wired Dusk", true, "Cm C9sus Abmaj7/C C9sus Fm7 Gm7 Abmaj7 Bb" },
    { "Steel Anthem", true, "Cm Ab Bb Gm Ab Fm7 Cm/G G7" },
    { "Synthetic Soul", true, "Cm Ebmaj7/Bb Abmaj7 G7sus Fm9 Dm7b5/Ab Cm/G G7" },
    { "Digital Gospel", false, "Cmaj7 Fm9 Abmaj9 G9sus Fmaj9 Em7 Dm7(11) Cadd11/G" },
    { "Neon Sermon", false, "Cmaj9 Bbmaj9 Am7 Gm9 Fmaj9 Em7(11) Dm7(11) Dsus2/E" },
    { "Static Prayer", true, "Cm Dm7/C Ab Absus2#11 Fm9 Dm/F Ab Bb" },
    { "Carrier Wave", false, "C7sus F/C Abadd9/C C9sus F9sus Fsus4 F7sus Fsus4" },
    { "Modulated Calm", false, "Csus4 Cm Bbsus2 Bb Fm/Ab Ab6 Fsus2/G Gsus4" },
    { "Iron Lullaby", true, "Cm7 Fm9 Gaug C9sus Fm7 Gm7 Ab6 G7b13" },
    // --- end of the made part ---
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
