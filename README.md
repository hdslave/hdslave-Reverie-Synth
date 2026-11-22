# hdslave Reverie

**A chord-progression and X/Y Motion pad synthesizer as a CLAP plug-in for the Akai MPC Sample.**

![Reverie six Frutiger Aero pages running in hdslaveOS](docs/reverie-pages.gif)

Built for the Akai MPC Sample running [Melodee](https://github.com/keremimo/melodee-sample) / [hdslaveOS](https://github.com/hdslave/hdslaveOS) (64-bit ARM Linux, Cortex-A55). The MPC's pads play 80 jazzy, moody, dirty-chord, soulful, R&B, neo-soul, gospel, city-pop, modal, and cinematic chord progressions — voiced with smooth voice-led bass lines across six keyboardist styles and transposed to any key, with progressions and patches chosen independently. Built for huge, lush, evolving pads: dual morphing wavetable oscillators with unison, drift, and air, a per-note X/Y **Motion** path, ensemble chorus, tempo-synced delay, and a big modulated shimmer reverb across six Frutiger Aero screen pages.

Inspired by Korg's M3 workstation (its X/Y MOTION pad, dual-oscillator programs, and chord-trigger pads), Reverie is an original design, not a model of the M3. Korg and M3 are trademarks of their owner; this plug-in is not made or endorsed by Korg.

Hosts list it as **hdslave Reverie** (id `com.hdslave.reverie`, version 0.2.0). Not yet run on the MPC itself.

## Install

Copy the folder `hdslave Reverie` (the `.clap`, this README and `licenses/`) to `Melodee/Plugins/` on the card. Pick
it on a PLUGIN track (SYNTH on Stage). Reverie tells Melodee to play the track's pads as a kit (as drum plug-ins
do): pad A01 is note 36, A16 note 51, whatever the key or the octave, and the Keys page names them Chord 1-8 and
Lift 1-8. Its page needs hdslaveOS; in stock Melodee you get the list of its parameters instead.

## The pads

![The CHORDS page in hdslaveOS, pad 6 just struck](docs/reverie-chords.png)

| Pads | What they play |
|---|---|
| A01-A08 (the bottom two rows) | the progression's eight chords, in order |
| A09-A16 (the top two rows) | the same eight chords **lifted**: one colour tone more and voiced higher, each above its own chord |
| bank B (notes 52-67) | the sixteen again, an octave up; bank C two octaves up; the notes below 36 an octave down per bank |

A chord sounds while its pad is held. A pad struck again while it still holds its chord (Melodee sends on, on, off
when a step repeats) starts the chord fresh and lets the old one ring out. Two strikes of one pad on the same sample
play once, at the louder velocity.

### The chord section

These ten controls belong to the chords, not the sound. **Loading a sound never changes them**, so the progression
and the sound are picked separately: keep a progression and go through the sounds, or keep a sound and go through
the progressions.

| Control | What it does |
|---|---|
| PROG | the progression (80, listed below) |
| VOICING | how the chords are spread (below) |
| KEY | the key, C to B. The progressions are written in C (major or minor) and move to it; the chord names on the page are spelled in it (G# minor, not Ab minor). The bass is voice-led between G1 and C3 in every key |
| OCT | the whole chord up or down, -2 to +2 octaves |
| STRUM | the notes enter one after another, bass first, 0 to 150 ms apart. Let go of the pad before the strum is through and the late notes still play their turn, as long as the pad was held |
| HUMAN | a little velocity and timing jitter per note (up to about 20 ms late) |
| BASS | the bass note's level; at 0 there is no bass note at all |
| VEL | how much velocity sets the level (0: none, 100 %: all of it) |
| LATCH | a struck chord rings on after the pad is up, until the next chord; turning LATCH off lets go of it |
| GLIDE | Chord Glide: a chord struck while another is held (or latched) takes over its notes, and each slides to the new note over this time (0 to 3 s) without striking again; extra notes enter fresh, missing ones let go |

### Voicings

Each chord is a voice-led bass note (the root, or a slash chord's bass, optimized across the 8-chord contour between G1 and C3 so descending, chromatic, and pedal bass lines never jump octaves mid-phrase) and an upper structure: the chord's most telling tones (third and seventh first, then the ninth, eleventh or thirteenth and the fifth) placed in the style's register. Of every way to place them, Reverie takes the one that keeps the low register open (no thirds or seconds crowded down low), has no minor ninths except in b9 chords, keeps the style's spacing, avoids a third or root on top, and moves least from the chord before it. Pads A09–A16 (**Lift**) voice each chord higher and bloom with an extra colour tone (13th on maj9, 11th on m9/m7b5, etc.). The progression is voiced in order twice round, so the eighth chord leads back into the first. Every progression is voiced in every style and key when Reverie is built (a table of 5,760 sets of sixteen pads — 92,160 chords), so picking one, or automating it on the same step as a chord, costs the audio thread nothing.

| Voicing | Sound |
|---|---|
| Neo Soul | bass, then four tones close around middle C (the default) |
| Open | five tones spread wide, often in fourths and fifths (cinematic) |
| Close | four tones packed within an octave |
| Air | the upper structure high above the bass, a wide gap between |
| Quartal | prefers fourths between the tones |
| Rootless | no bass note: five upper tones, for playing over a bass line |

For example, the first progression's chords in Neo Soul are: Fmaj9 = F2 | G3 C4 E4 A4, Em9 = E2 | G3 B3 D4 F#4,
Dm9 = D2 | F3 A3 C4 E4, G13 = G2 | F3 A3 B3 E4.

### The progressions

| # | Name | Key | Chords (in C) |
|---|---|---|---|
| 1 | Velvet Night | major | Fmaj9 Em9 Dm9 G13 Cmaj9 Am9 Dm9 G9sus |
| 2 | Moonlit Two-Five | major | Dm9 G13 Cmaj9 Am9 Dm9 Db9#11 Cmaj9 A7#9 |
| 3 | Sunday Sanctuary | major | Fmaj9 F#dim7 Cmaj9/G A7b13 Dm9 G13sus G13 Cmaj9 |
| 4 | Tidal Hearts | major | Fmaj9 E7#9 Am9 Gm9 C13 Fmaj9 E7alt Am9 |
| 5 | Backdoor Bliss | major | Cmaj9 Fmaj9 Fm9 Bb13 Cmaj9 Am9 Dm9 G7b9 |
| 6 | Aurora | major | Cmaj9#11 Emaj9 Abmaj9 Fm9 Dbmaj9#11 Ebmaj9 Bb13sus Bb13 |
| 7 | Starlight Lydian | major | Cmaj9#11 D9/C Bm9 Em9 Am9 D13 Gmaj9 G9sus |
| 8 | Rose Quartz | major | Cmaj9 Bm7b5 E7b13 Am9 Gm9 C13 Fmaj9 Fm6/9 |
| 9 | Honey Drip | major | Em9 A13 Dm9 G13 Cmaj9 Fmaj9#11 F#m7b5 B7#9 |
| 10 | Neon Dusk | major | Fmaj9 G13 Em9 Am9 Dm11 G13 Cmaj13 C13#11 |
| 11 | Halo Ballad | major | Cmaj9 Gadd9/B Am9 Am9/G Fmaj9 Cmaj9/E Dm9 G13sus |
| 12 | Late Night Drive | major | Dm11 Em9 Fmaj13 E7#9 Am11 Ab13#11 G13sus Cmaj9#11 |
| 13 | Crystal Cave | minor | Cm11 Abmaj9#11 Ebmaj13 Bbadd9/D Cm9 Fm11 G13sus G7b9 |
| 14 | Golden Hour | major | Cmaj9 Gm9 C13 Fmaj9 Fm9 Bb13 Ebmaj9 G7alt |
| 15 | Cotton Clouds | major | Cmaj9 Em9 Fmaj9 Fm6/9 Em7b5 A7b13 Dm9 G13sus |
| 16 | Bossa Moon | major | Cmaj9 C#dim7 Dm9 D#dim7 Em9 A7b13 Dm9 G13 |
| 17 | Celestial Pedal | major | Cmaj9 Dadd9/C Bbmaj9/C Fmaj9/C Abmaj9#11/C Bbmaj9/C Fm6/9/C Cmaj9#11 |
| 18 | Silhouette | major | Fmaj9 Fm9 Em9 Eb9#11 Dm9 Db9#11 Cmaj9 G9sus |
| 19 | Neo Classic | major | Cmaj9 A7#9 Dm9 G7b13 Em9 A13 Dm9 G13sus |
| 20 | Prism | major | Dm11 Em11 Fmaj9#11 G13sus Am13 Fmaj9#11 Ebmaj9#11 G13sus |
| 21 | Ocean Floor | major | Cmaj9 Bbmaj9#11 Abmaj9 Bb13sus Cmaj9#11 Gm9 Fmaj9 Fm6/9 |
| 22 | Sanctuary Steps | major | Cmaj9 C13 Fmaj9 F#dim7 Cmaj9/G A7#9 Dm9 G13b9 |
| 23 | Afterglow | major | Cmaj9 G6/9/B Am9 Fmaj9#11 Cmaj9 Gadd9/B Am11 Fm6/9 |
| 24 | Firefly | major | Cmaj9 E7b13 Am9 C13 Fmaj9 D13 Dm9 G7b9 |
| 25 | Dorian Drift | minor | Cm11 F13 Ebmaj9#11 F13sus Dm11 G13sus Cm13 F13#11 |
| 26 | Glass Garden | major | Fmaj9#11 Em9 Ebmaj9#11 Dm11 Dbmaj9#11 Cmaj13 Gm11 C13#11 |
| 27 | Silver Lining | major | Fmaj13 Em9 A9b13 Dm11 G13sus Cmaj9/E Gm11 C13b9 |
| 28 | Velvet Rope | minor | Cm11 Bmaj9#11 Bbm9 Eb13 Abmaj13 G7#9 Cm13 F9#11 |
| 29 | Heavenly Steps | major | Cmaj9 Ebdim7 Dm9 G13 Em11 A7b13 Dm11 G13sus |
| 30 | Paper Lanterns | major | Cmaj9 Am11 Fmaj9 G13sus Em9 Am11 Dm9 Fm6/9 |
| 31 | Daydream Descent | major | Dm9 Db13 Cmaj9 B7alt Bbmaj9#11 A7#9 Abmaj9 G13 |
| 32 | Amethyst | minor | Cm9 F13 Ebmaj9 Abmaj9#11 Dm7b5 G7b13 Cm11 Bb13sus |
| 33 | Midnight Dorian | minor | Cm11 F13 Bbmaj9 Ebmaj9 Abmaj9#11 Dm7b5 G7#9 G7b9 |
| 34 | Smoke and Velvet | minor | Cm9 Abmaj9#11 Ebmaj13 Bb13sus Fm11 Gm9 Abmaj9 G7alt |
| 35 | Obsidian | minor | Cm9 Dbmaj9#11 Bbm9 Abmaj9 Fm9 Dbmaj9 Dm7b5 G7b9 |
| 36 | Rainy Neo Soul | minor | Cm9 Fm9 Bb13 Ebmaj9 Abmaj9 Fm9 Dm7b5 G7#9 |
| 37 | Line Cliche | minor | Cmadd9 CmMaj9 Cm9 Cm6/9 Abmaj9 Fm9 Dm7b5 G7b9 |
| 38 | Nocturne | minor | Cm9 Abmaj9#11 Fm9 G7b13 Ebmaj9 Abmaj9 Dm7b5 G7#9 |
| 39 | Sapphire | minor | Dm11b5 G9b13 Cm9 C7#9 Fm11 Bb13 Ebmaj13 Abmaj9#11 |
| 40 | Witching Hour | minor | CmMaj9 Abmaj9#11 Fm9 Dbmaj9#11 Bbm9 Ebmaj9 Abmaj9 G7alt |
| 41 | Rose Petal | minor | Cm9 Bb6/9 Abmaj9#11 G9b13 Fm11 Ebmaj9/G Dbmaj9#11 G7alt |
| 42 | Lunar Tide | minor | Cm11 Emaj9#11 Abmaj9#11 G13sus Fm11 Dbmaj9#11 Ebmaj9/G G9b13 |
| 43 | Deep Indigo | minor | Cm9 Bbm9 Abmaj9 Gm9 Fm9 Gm9 Abmaj9 Bb13sus |
| 44 | Phantom Waltz | minor | Cm9 G7b13/B Cm9/Bb Am7b5 Abmaj9#11 Fm9 Dm7b5 G7b9 |
| 45 | Echo Chamber | minor | Cm9 F13 Cm11 F13sus Abmaj9 Gm9 Fm9 Bb13 |
| 46 | Moonstone | minor | Cm11 Dbmaj9#11/C Cm13 Bbm11 Eb13#11 Abmaj13 Dm11b5 G7alt |
| 47 | Cathedral Sun | major | Cmaj13 Abmaj9#11 Emaj9 G13sus Fmaj9#11 Dbmaj9#11 Bbmaj9#11 G13sus |
| 48 | Shibuya Glow | major | Fmaj13 G13/F Em11 A9b13 Dm11 Db13#11 Cmaj13 C13 |
| 49 | Gospel Horizon | major | Cmaj13 E7#9/B Am11 Gm11 C13b9 Fmaj13 F#m11b5 G13sus |
| 50 | Cloud Nine | major | Fmaj9#11 Ebmaj9#11 Dbmaj9#11 Cmaj13 Am11 Ab13#11 G13sus C13#11 |
| 51 | Solaris | major | Cmaj9#11 D6/9/C Em11 Dadd9/F# Gmaj13 Fmaj9#11 Ebmaj9#11 G13sus |
| 52 | Silk Highway | major | Cmaj13 Bm11b5 E9b13 Am11 D13#11 Fmaj13 Fm6/9 G13sus |
| 53 | Amber Glass | major | Cmaj9#11 Eb13#11 Abmaj13 B13#11 Emaj9#11 G13sus Fmaj9#11 G9b13 |
| 54 | Morning Choir | major | Cmaj9 Cmaj9/E Fmaj13 Fm6/9 Em11 A9b13 Dm11 G13sus |
| 55 | Opal Drift | major | Cmaj9#11 Gadd9/B Fmaj9#11/A Cmaj9/G D9#11/F# Fmaj13 Ebmaj9#11 G13sus |
| 56 | Neon Rain | major | Em11 Eb13#11 Dm11 Db13#11 Cmaj13 F#m11b5 B7#9 B7alt |
| 57 | Velvet Eclipse | minor | Cm11 Emaj9#11 Fm11 Dbmaj9#11 Abmaj13 Ebmaj9/G Dm11b5 G7alt |
| 58 | Midnight Garage | minor | Cm11 Ebm11 Fm11 Abm11 Gm11 Bbm11 Abmaj9#11 G9b13 |
| 59 | Abyssal Choir | minor | Cm11 Dbmaj9#11 Eb13sus Dbmaj9#11 Bbm11 Abmaj9#11 Fm11 G7b9 |
| 60 | Satin Noir | minor | Cm13 F13#11 Abmaj13 G9b13 Fm11 Bb13#11 Ebmaj13 G7alt |
| 61 | Astral Minor | minor | Cm11 Abmaj9#11/C Bb6/9/C Fm11/C Dbmaj9#11 Ebmaj13 Fm11 G13sus |
| 62 | Cobalt Pulse | minor | Cm11 Cm9/Bb Abmaj13 G9b13 Fm11 F#dim7 Cm9/G G7alt |
| 63 | Mirage | minor | Cm11 F13#11 Bmaj9#11 E13#11 Abmaj9#11 Db13#11 Dm11b5 G7alt |
| 64 | Starlit Requiem | minor | Cmadd9 Abmaj13/C Ebmaj9/Bb F9#11/A Abmaj9#11 Fm11 Dm11b5 G7b9 |
| 65 | Smoky Backroom | minor | CmMaj11 Am9b5 D7b9b13 G7alt Cm9 F13#9 Dm9b5 G7b9#11 |
| 66 | Velvet Grit | minor | Cm11 B13#9 Bbm11 A7b9#11 Abmaj7#5 G7b9b13 Fm11 G7alt |
| 67 | Midnight Alley | minor | CmMaj9 Fm11 Bb13#9 Ebmaj7#5 Am9b5 D7b9b13 Dm9b5 G7b9#11 |
| 68 | Blue Smoke | minor | Cm13 F13#9 Cm11 Gb13#11 Fm11 B13#11 Dm9b5 G7b9b13 |
| 69 | Ash and Gold | minor | Cm11 Dbmaj7#5 Cm9/Bb Am9b5 Abmaj9#11 G7b9b13 F#m9b5 G7alt |
| 70 | Broken Halo | minor | Cm11 E7b9#11 Fm11 A7b9b13 Dbmaj9#11 G7b9#11 CmMaj11 G7alt |
| 71 | After Hours | minor | Cm13 Eb13#9 Abmaj7#5 G7b9b13 Fm13 Db13#11 Dm9b5 G7alt |
| 72 | Obsidian Rain | minor | Cm11 Db7b9#11/C CmMaj11 B7b9b13/C Bbm11 Eb13#9 Abmaj7#5 G7b9b13 |
| 73 | Crushed Velvet | minor | Cm9 F13#9 Fm9 Bb13#9 Ebmaj7#5 Ab13#11 Dm9b5 G7b9b13 |
| 74 | Subway Nocturne | minor | Cm11 F#m9b5 B7b9b13 Emaj7#5 Ebm11 Ab13#9 Dbmaj9#11 G7b9#11 |
| 75 | Bitter Honey | major | Cmaj13 B7b9b13 Em11 A13#9 Dm11 Db13#11 Cmaj7#5 G7b9#11 |
| 76 | Rust and Silk | major | Fmaj9#11 E7b9b13 Am11 Ab13#9 Gm11 Gb13#11 Fmaj7#5 C13#9 |
| 77 | Dirty Martini | major | Dm11 G13#9 Cmaj13 F#m9b5 B7b9b13 Em11 Eb13#11 A7b9#11 |
| 78 | Neon Noir | minor | CmMaj11 Ebmaj7#5 Abmaj9#11 D7b9#11 G7b9b13 C7#9 Fm11 Db13#11 |
| 79 | Basement Tape | minor | Dm9b5 G7b9b13 Cm11 B13#9 Bbm11 Eb13#9 Abmaj7#5 A7b9#11 |
| 80 | Shadow Gospel | minor | Cm11 C7b9#11/E Fm11 F#dim7 Cm9/G A7b9b13 Dm9b5 G7b9b13 |

## Sound and parameters

![The MOTION page: the path, its centre and a light for every note](docs/reverie-motion.png)

**Oscillators.** Two, each with a WAVE (twelve sources made for pads), a POS (Position: each wave has four frames
and POS morphs across them), OCT, SEMI, FINE, UNISON (1-4 voices), DETUNE (the outer unison voices' spread, up to 50
cents) and LEVEL.

| Wave | Its four frames |
|---|---|
| Velvet Saw | soft, full, bright, and a glowing band |
| Hollow Pulse | square to a narrow pulse |
| Choir | the vowels Oo, Oh, Ah, Eh |
| Strings | a bowed string, the bow at four places, with a body |
| Drawbar | organ registrations: flute, jazz, full, gospel |
| Crystal | sparse high partials (glass) |
| Spectra | a resonant band swept up the harmonics |
| Octaves | octave stacks, then fifths among them |
| Warm | triangle to a soft square |
| Breath | dense, airy, formant-shaped |
| Reed | clarinet to oboe |
| Sine | pure, then gentle organ partials |

The frames share their harmonics' phases, so the morph is a smooth change of the spectrum, never a phasey dip.
Every frame is band-limited for each octave of pitch (no aliasing; images of the interpolation below -45 dB).

**Voice.** WIDTH spreads the unison across the stereo field (the upper notes of a chord also alternate sides; the
bass stays near the middle). AIR adds breath: a band of noise under each note's envelope (A.TONE moves the band).
DRIFT makes each oscillator wander in pitch, slowly, as analog ones do (up to 20 cents).

**Filter.** LP 12, LP 24, BP or HP (a state-variable filter, stereo), CUTOFF, RESO, ENV (the filter envelope's
depth, up to 6 octaves either way), KEYTRK and VEL (soft notes darker). **Envelopes:** amplitude and filter, each
ATTACK, DECAY, SUSTAIN, RELEASE (up to 20 s).

**LFO.** One a note, each starting at its own phase (so a chord shimmers rather than wobbles together): SHAPE (Sine,
Triangle, Drift: a smooth random walk), RATE (0.05-20 Hz), DELAY (fades in over up to 5 s), and its depths to PITCH
(up to 50 cents), CUTOFF and WAVE (Position).

**Motion.** The X / Y pad, as the M3's MOTION records and plays a path on its X / Y pad. PATH picks the path (Orbit,
Figure 8, Pendulum, Spiral, Wander, Tide, Star, Lissajous; Off for none), RATE its speed (0.01-4 Hz; or SYNC to
the song, one turn every DIV), DEPTH its size, X and Y its centre (on the MOTION page K1 and K2: turn them by hand
and Melodee's motion recording keeps your gestures). SPREAD places each note of a chord at its own point of the
path, so the notes move apart and together; RETRIG starts the path again with each chord. The mod wheel moves Y. X
and Y each move two destinations (X>1, X>2, Y>1, Y>2, each with an amount): Wave 1, Wave 2, Waves (both), Osc Mix,
Cutoff, Resonance, Detune, Width, Air, Pitch (up to two semitones), Level, LFO Depth.

**Effects.** ENSEMBLE (three modulated delays a side with a slow and a quick LFO, as string machines did: mix, depth,
rate), DELAY (ping-pong, its time a division of the tempo, its repeats darkening; mix, time, feedback), REVERB (an
eight-line feedback delay network, slowly modulated, with diffusion, a pre-delay and a high-pass ahead of it so the
low end stays clear: mix, size, decay up to 40 s, tone, pre-delay) and SHIMMER (the reverb an octave up, fed back
into it). VOLUME sets the level; a soft limiter keeps the output under 0 dBFS.

## The page

Use Minus and Plus to move through the six pages. [Watch the screen tour in hdslaveOS](docs/reverie-pages.gif). The wheel moves between controls. Push edits one; turn the
wheel to change it (Shift + wheel for fine steps, Minus and Plus for coarse) and Push again when done. On a switch,
Push flips it at once. K1-K3 hold the control picked and the next two. Hold Shift to see the controls.

The skyline shows the progression, voicing and key on every page. A green tab marks the page.
Blue glass marks the selected control and amber marks a control being edited. Each knob has a value marker.
A hollow amber marker shows where a physical knob sits while it waits for pickup.

| Page | What it holds |
|---|---|
| CHORDS | sixteen numbered pads in MPC order, blue for chords and green for lifts, amber while held, with the last strike underlined; PROG, VOICING, KEY, OCT, STRUM, HUMAN, BASS, VEL, LATCH, GLIDE; a keyboard with the notes that sound (the bass in gold), or the last chord's |
| MOTION | a glass globe with the X / Y path, its centre and a light for every sounding note; X, Y, DEPTH (K1-K3), PATH, RATE, SPREAD, SYNC, DIV, RETRIG and the four destinations |
| OSC | both oscillators, each drawn as its Position (and Motion) has it now; their controls; WIDTH, AIR, A.TONE, DRIFT |
| FILTER | the response, with the cutoff where the newest note has it, and the filter envelope |
| AMP | the amplitude envelope and the LFO |
| FX | ensemble, tempo delay and shimmer reverb grouped beside an aquarium with a live output peak meter |

The page draws nothing while nothing changes; while notes sound it moves at 25 frames a second.

![The OSC page: Velvet Saw and Choir at their positions](docs/reverie-osc.png)

## Sounds

Twenty, on Melodee's PRESETS page (and any host's preset browser). A sound sets every control but the chord
section. They are levelled to about -19 dBFS RMS for a held chord (Crystal Harp, a pluck, lower).

Reverie (the default), Velvet Nebula, Glass Cathedral, Choir of Moons, Silk Strings, Aurora Drift, Celestial Keys,
Deep Ocean, Starfield, Lo-Fi Dream, Gospel Glow, Breath of Angels, Velour Swell, Crystal Harp, Dark Matter, Sunrise
Bloom, Ethereal Vox, Mystic Reed, Polar Lights, Satin Keys.

## Cost

Instructions a 128-frame block takes (callgrind, `tools/bench.sh`; a block at 48 kHz is about 4.8 million cycles of
the MPC's 1.8 GHz Cortex-A55):

| Scene | Instructions | Share of one core |
|---|---|---|
| silent | 4,319 | 0.09 % |
| one chord held, default sound | 252,422 | 5.3 % |
| the progression played, releases overlapping | 327,794 | 6.8 % |
| worst: 21 voices (3×7-note Open Lift), unison 4 + 4, LP 24, every effect up | 918,548 | 19.1 % |

The page costs about 75,000 instructions a frame on CHORDS and 114,000 on MOTION while it moves (25 frames a
second, on Melodee's screen thread, not the audio one). The oscillators read each morph as one 128-bit load a sample
(the frame and its difference to the next, interleaved) and mix in stereo pairs with NEON; the filter runs both
channels in one register; air is one noise for every voice; the control work (envelopes, LFOs, Motion, the knobs'
smoothing) runs every 16 samples and only while something moves; silent, Reverie sleeps. Clearing the effects (All
Sound Off, which Melodee sends on all 16 channels at once, the host's reset, falling asleep) does no work on their
lines: a cleared line only forgets what it held, and until it has been written through again the few places each
block reads that were not written since are zeroed first.

## Building and testing

`./build.sh` builds `build/Reverie.clap` and runs the tests in a Linux arm64 container (Melodee's `melodee-builder`
image in the `colima-melodee` Docker context; start it with `colima start melodee`).

- `tests/host.cpp` loads the plug-in as a host does: ports, every parameter and its text, state, the sounds (and
  that they leave the chord section alone), every progression in every voicing and key (92,160 chords: each a sound
  chord, the third there, no stray minor ninths), the pad map Melodee reads, chords from the pads and banks,
  Melodee's on-on-off and same-sample strikes, strum, latch, sustain, Chord Glide, chokes and the channel modes
  (Reset All Controllers lifts the pedal), note-offs and chokes by note id, stolen voices fading without a step
  (at a block's start, ten at once, a chord never taking its own notes), the effects cleared sounding exactly as
  wiped ones (and All Sound Off on 16 channels costing what one does), tuning at 44.1, 48 and 96 kHz, bounded output
  at every extreme, sleeping, and the page (it writes `build/screen-*.png`). It also checks the voicing table
  against the search, set by set.
- `tests/fingerprint.cpp` hashes every output sample over 49 scenes (`tests/fingerprint.txt`), including one in
  blocks of 1, 37, 128 and 4096 frames that must hash the same: the output does not depend on the host's blocks. A
  change meant to keep the sound must keep the hashes; `make fingerprint-update` records a change meant to change it.
- `./build.sh sanitize`: the host tests against an AddressSanitizer and UBSan build.
- `tools/bench.sh`: the cost table above.
- `./build.sh render` then `build/render build/Reverie.clap OUT.wav PLAYLIST`: renders progressions and sounds for
  listening (see `tests/render.cpp` for the playlist's lines).

## Licence

No project licence yet. The page uses Barlow SemiBold (SIL Open Font License, `licenses/OFL-barlow.txt`);
the CLAP headers are MIT (`third_party/clap/LICENSE`); `src/melodee_screen.h` is MIT.
