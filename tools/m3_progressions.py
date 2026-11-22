#!/usr/bin/env python3
"""Reverie's progressions, made from the M3 factory pad chord sets.

Reads the chord pack's all-progressions.json (every factory Pad 1-8 chord set of the M3 XPanded Motion Synth, Pad,
SlowSynth and Vocal presets, with the exact MIDI notes of each pad) and writes Reverie's progression table:

  * one progression per distinct set (sets that are the same chords in another key count once);
  * a set is taken when every pad is a chord: three pitch classes or more, or a third (a fifth is then implied);
    sets with power chords, octaves, single notes or clusters no chord name fits are left out and listed;
  * each pad is named from its own notes with a quality of src/chords.h whose tones are exactly the pad's pitch
    classes (the fifth may be missing), its lowest note the bass (a slash chord when that is not the root);
  * the set moves so pad 1's root is C, minor when pad 1 is a minor chord; KEY then moves it to any key.

  tools/m3_progressions.py PACK/all-progressions.json             report what it takes and leaves
  tools/m3_progressions.py PACK/all-progressions.json --write     also rewrite src/progressions.h and README.md

The names are Reverie's own (the factory preset names are not used). Run from the repository.
"""
import collections
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# Reverie's name for each set, by the preset it was first found in (bank, slot).
NAMES = {
    ("COMBIA", 7): "Ringed Orbit", ("COMBIA", 23): "Signal Tower", ("COMBIA", 42): "Watchful Horizon",
    ("COMBIA", 58): "Low Cathedral", ("COMBIA", 74): "Frost Dawn", ("COMBIA", 90): "Tender Embrace",
    ("COMBIB", 23): "Starbound Voyage", ("COMBIB", 42): "Spectral Prism", ("COMBIB", 74): "Neon Boulevard",
    ("COMBIB", 82): "Hidden Passage", ("COMBIB", 122): "Kinetic Drift", ("COMBIC", 23): "Prismatic Turn",
    ("COMBIC", 42): "Cell Division", ("COMBIC", 58): "Sugar Village", ("COMBIC", 90): "Horizon Fade",
    ("PROGIA", 17): "Chrono Shift", ("PROGIF", 67): "Tidal Formant", ("PROGIF", 68): "Resonant Pulse",
    ("PROGIF", 69): "Quiet Machinery", ("PROGIF", 72): "Folded Space", ("PROGIF", 73): "Mirror Steps",
    ("PROGIF", 76): "Neon Glyph", ("PROGIF", 78): "Temple Gong", ("PROGIF", 79): "Slow Bounce",
    ("PROGIF", 81): "Endless Ring", ("PROGIF", 83): "Last Departure", ("PROGIF", 85): "Eight Sides",
    ("PROGIF", 86): "Voice Lattice", ("PROGIF", 87): "Clockwork Lace", ("PROGIF", 88): "Dice and Ivory",
    ("PROGIF", 90): "Second Thoughts", ("PROGIF", 91): "Distant Broadcast", ("PROGIF", 92): "Sweeping Choir",
    ("PROGIF", 93): "Gated Glow", ("PROGIF", 94): "Phosphor Tide", ("PROGIF", 96): "Resin Rhythm",
    ("PROGIF", 98): "Light Steps", ("PROGIF", 99): "Platform Clock", ("PROGIF", 101): "Mischief Broth",
    ("PROGUC", 12): "Slow Becoming", ("PROGUC", 13): "Water Voices", ("PROGUC", 14): "Liquid Ascent",
    ("PROGUC", 15): "Shapeshift Cloud", ("PROGUC", 16): "Timeless Expanse", ("PROGUC", 17): "Monarch Drift",
    ("PROGUC", 18): "Fairy Lantern", ("PROGUC", 19): "Freefall", ("PROGUC", 20): "Seed Pods",
    ("PROGUC", 21): "Skyward Glider", ("PROGUC", 22): "Zephyr Cascade", ("PROGUC", 23): "Metamorphosis",
    ("PROGUC", 25): "Moth Wings", ("PROGUC", 26): "Circle of Dreams", ("PROGUC", 27): "Spiral Stair",
    ("PROGUC", 28): "Night Rotation", ("PROGUC", 29): "Lunar Capsule", ("PROGUC", 30): "Mirror Lake",
    ("PROGUC", 31): "Hummingbird", ("PROGUC", 32): "Sparkle Veil", ("PROGUC", 33): "Scattered Light",
    ("PROGUC", 37): "Stained Glass", ("PROGUC", 38): "Reed Whisper", ("PROGUC", 40): "Delayed Hours",
    ("PROGUC", 41): "Old Hymnal", ("PROGUC", 42): "Stutter Choir", ("PROGUC", 44): "Torchlight",
    ("PROGUC", 45): "Silk Ribbon", ("PROGUC", 46): "Deep Sea Bloom", ("PROGUC", 48): "Satellite Blink",
    ("PROGUC", 49): "Countdown", ("PROGUC", 51): "Mercury Rain", ("PROGUC", 55): "Grid Shimmer",
    ("PROGUC", 58): "Carefree Sway", ("PROGUC", 60): "Golden Avenue", ("COMBIA", 12): "Bow and Gourd",
    ("COMBIA", 28): "High Thermal", ("COMBIA", 43): "Spellbound", ("COMBIA", 82): "Slow Crossfade",
    ("COMBIA", 91): "Rain on Glass", ("COMBIA", 107): "Adrift", ("COMBIA", 123): "Vowel Moment",
    ("COMBIB", 12): "Cool Meadow", ("COMBIB", 28): "Closing Credits", ("COMBIB", 43): "Wide Choir",
    ("COMBIB", 59): "Moonlit Sleep", ("COMBIB", 61): "Silicon Lullaby", ("COMBIB", 75): "Orchestral Dawn",
    ("COMBIB", 91): "Warm Front", ("COMBIB", 107): "Rising Voices", ("COMBIC", 12): "Velvet Quartet",
    ("COMBIC", 28): "Ripple Field", ("COMBIC", 45): "Monastery", ("COMBIC", 59): "Unending Bow",
    ("COMBIC", 73): "Cloud Bells", ("COMBIC", 75): "Infinite Choir", ("COMBIC", 76): "Dust and Light",
    ("COMBIC", 77): "Trembling Air", ("COMBIC", 107): "Quiet Aside", ("PROGIA", 14): "Vintage Haze",
    ("PROGIF", 19): "Open Sky", ("PROGIF", 20): "Bright Tomorrow", ("PROGIF", 21): "Digital Hearth",
    ("PROGIF", 22): "Old Regrets", ("PROGIF", 23): "Mellow Haze", ("PROGIF", 24): "String Garden",
    ("PROGIF", 25): "Sine Lullaby", ("PROGIF", 26): "Suspended Hours", ("PROGIF", 27): "Soft Daydream",
    ("PROGIF", 28): "Glacier Breath", ("PROGIF", 29): "Trance State", ("PROGIF", 30): "Fond Memory",
    ("PROGIF", 31): "Ember Glow", ("PROGIF", 33): "Long Sweep", ("PROGIF", 34): "Closing Filter",
    ("PROGIF", 35): "Thinning Air", ("PROGUA", 72): "Hybrid Strings", ("PROGUA", 73): "Real and Ghost",
    ("PROGUA", 75): "Honeyed Pad", ("PROGUA", 76): "Brass Clouds", ("PROGUA", 77): "Sea Breeze",
    ("PROGUA", 78): "Comet Tails", ("PROGUA", 79): "Evocation", ("PROGUA", 80): "Ghost Voices",
    ("PROGUA", 81): "Whale Song", ("PROGUA", 82): "Angel Wings", ("PROGUA", 83): "After Midnight",
    ("PROGUA", 84): "Easy Glow", ("PROGUA", 85): "Hiss and Hum", ("PROGUA", 86): "Molten Iron",
    ("PROGUA", 87): "Reed Organ", ("PROGUA", 97): "Brass Horizon", ("PROGUA", 98): "Slow Sine",
    ("PROGUA", 101): "Vector Field", ("PROGUA", 103): "Ripple Tank", ("PROGUA", 105): "Gentle Bowing",
    ("PROGUA", 107): "Silent Hours", ("PROGUA", 108): "Sealed Letter", ("PROGUA", 113): "Stillness",
    ("PROGUA", 114): "Old Photographs", ("PROGUA", 116): "Simple Sweep", ("PROGUA", 121): "Wide Vista",
    ("PROGIC", 102): "Scat Cafe", ("PROGIC", 103): "Baritone Lounge", ("PROGIC", 104): "Soprano Smoke",
    ("PROGIC", 105): "Chapel Choir", ("PROGIC", 106): "Circle of Fourths", ("PROGIC", 107): "Breathing Aahs",
    ("PROGIC", 108): "Soft Aahs", ("PROGIC", 109): "Twin Oohs", ("PROGIC", 110): "Gentle Oohs",
    ("PROGIC", 112): "Choral Mist", ("PROGIC", 117): "Steppe Voice", ("PROGIC", 120): "Birdsong Dawn",
    ("PROGIC", 121): "Tape Choir", ("PROGIC", 124): "Vowel Shift", ("PROGIC", 126): "Highland Voices",
    ("PROGID", 1): "Alpine Echo", ("PROGID", 2): "Dream Chorus", ("PROGID", 3): "Polar Night",
    ("PROGIF", 124): "Listening Post", ("PROGIF", 125): "Robot Hymn", ("PROGUD", 32): "Machine Psalm",
    ("PROGUD", 33): "Chrome Ballad", ("PROGUD", 35): "Wired Dusk", ("PROGUD", 37): "Steel Anthem",
    ("PROGUD", 39): "Synthetic Soul", ("PROGUD", 41): "Digital Gospel", ("PROGUD", 43): "Neon Sermon",
    ("PROGUD", 49): "Static Prayer", ("PROGUD", 55): "Carrier Wave", ("PROGUD", 59): "Modulated Calm",
    ("PROGUD", 63): "Iron Lullaby",
}

PC_NAME = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
LETTERS = "CDEFGAB"
NATURAL = [0, 2, 4, 5, 7, 9, 11]


def read_qualities():
    """The qualities of src/chords.h: suffix -> (tones, full pitch-class set)."""
    text = open(os.path.join(ROOT, "src", "chords.h")).read()
    body = text[text.index("inline constexpr Quality qualities[]"):text.index("constexpr int qualityCount")]
    out = []
    for suffix, tones, count, _flat9, _lift in re.findall(
            r'\{ "([^"]*)", \{ ([-\d, ]+) \}, (\d+), (true|false), \{ ([-\d, ]+) \} \}', body):
        t = [int(x) for x in tones.split(",")][: int(count)]
        out.append((suffix, t, set(t) | {0}))
    if not out:
        sys.exit("no qualities found in src/chords.h")
    return out


# How plain a name reads: where two names fit the same notes, the plainer wins (D7/F# rather than Csus2#11/F#,
# Bm7/A rather than D6/A).
PLAIN = {"", "m", "7", "m7", "maj7", "sus2", "sus4", "7sus", "add9", "madd9", "m7b5", "dim", "dim7", "aug"}
FAIRLY_PLAIN = {"6", "m6", "9", "m9", "maj9", "6/9", "m6/9", "9sus", "11", "m11", "13", "m13", "maj13", "add11",
                "madd11", "m7(11)", "13sus", "7b9", "7#9", "7b13", "maj7#11"}


def rarity(suffix):
    return 0 if suffix in PLAIN else 1 if suffix in FAIRLY_PLAIN else 3


def chord_names(notes, qualities):
    """Every exact name of a pad (cost, root pc, suffix, bass interval), best first."""
    bass = min(notes) % 12
    pcs = {n % 12 for n in notes}
    found = []
    for root in sorted(pcs):
        rel = {(x - root) % 12 for x in pcs}
        relb = (bass - root) % 12
        for qi, (suffix, _tones, full) in enumerate(qualities):
            upper = rel if relb in full else rel - {relb}
            if upper <= full and (full - upper) <= {7}:
                missing = bool(full - upper)
                cost = (0 if relb == 0 else 10) + (0 if relb in full else 6) + len(full) * 0.5 + rarity(suffix) + qi * 0.001
                if missing:  # a fifth left out: fine in a big chord, but a whole triad's inversion reads better
                    cost += 12 if len(upper) <= 3 else 0.2
                found.append((cost, root, suffix, relb))
    found.sort()
    return found


def is_chord(pad):
    pcs = sorted({n % 12 for n in pad})
    if not pcs:
        return False
    if len(pcs) >= 3:
        return True
    return len(pcs) == 2 and (pcs[1] - pcs[0]) % 12 in (3, 4, 8, 9)


def minor_quality(suffix):
    return (suffix.startswith("m") and not suffix.startswith("maj")) or suffix.startswith("dim")


def root_spelling(pc, minor, suffix):
    if pc == 6:
        dim = suffix.startswith("dim") or "b5" in suffix
        return "Gb" if minor and not minor_quality(suffix) and not dim else "F#"
    return PC_NAME[pc]


def bass_spelling(root_name, relb, suffix, bass_pc):
    """The slash bass spelled from the chord's root (D7/F#, Ab/C), else the plain way."""
    step = {0: 0, 1: 1, 2: 1, 3: 2, 4: 2, 5: 3, 6: 3, 7: 4, 8: 5, 9: 5, 10: 6, 11: 6}[relb]
    if relb == 6 and ("b5" in suffix or suffix.startswith("dim")):
        step = 4
    if relb == 8 and ("#5" in suffix or suffix == "aug"):
        step = 4
    letter = (LETTERS.index(root_name[0]) + step) % 7
    acc = ((bass_pc - NATURAL[letter] + 6) % 12) - 6
    name = LETTERS[letter] + ("#" if acc == 1 else "b" if acc == -1 else "")
    if abs(acc) > 1 or name in ("Cb", "Fb", "E#", "B#"):
        return PC_NAME[bass_pc]
    return name


def signature(item):
    pads = item["pads_midi"]
    b0 = min(pads[0]) if pads[0] else 0
    return tuple(((min(p) - b0) % 12, tuple(sorted({(n - b0) % 12 for n in p}))) if p else () for p in pads)


def build(pack_path):
    qualities = read_qualities()
    full_of = {s: f for s, _t, f in qualities}
    items = json.load(open(pack_path))
    seen = {}
    taken, left = [], []
    for it in items:
        sig = signature(it)
        if sig in seen:
            seen[sig]["also"].append(it)
            continue
        entry = {"item": it, "also": []}
        seen[sig] = entry
        pads = it["pads_midi"]
        why = None
        if not all(is_chord(p) for p in pads):
            why = "power chords, octaves or single notes"
        elif not all(chord_names(p, qualities) for p in pads):
            why = "a cluster no chord name fits"
        if why:
            left.append((it, why))
            continue
        named = [chord_names(p, qualities)[0] for p in pads]
        _c, root0, suffix0, _b = named[0]
        minor = minor_quality(suffix0)
        shift = (-root0) % 12
        chords = []
        for (_cost, root, suffix, relb), pad in zip(named, pads):
            # the check: the name's tones are the pad's pitch classes (the fifth may be missing), its bass the bass
            pcs = {(n - root) % 12 for n in pad}
            full = full_of[suffix]
            upper = pcs if relb in full else pcs - {relb}
            assert upper <= full and (full - upper) <= {7} and (min(pad) - root) % 12 == relb, (it["name"], suffix)
            rp = (root + shift) % 12
            name = root_spelling(rp, minor, suffix)
            symbol = name + suffix
            if relb:
                symbol += "/" + bass_spelling(name, relb, suffix, (rp + relb) % 12)
            chords.append(symbol)
        key = (it["bank"], it["slot"])
        if key not in NAMES:
            sys.exit(f"no Reverie name for {key} ({it['name']})")
        entry.update(name=NAMES[key], minor=minor, chords=chords, group=it["group"])
        taken.append(entry)
    names = [e["name"] for e in taken]
    assert len(set(names)) == len(names), "names repeat"
    assert all(len(n) <= 18 for n in names), "a name is longer than 18 characters"
    return taken, left


def header_table(taken):
    lines = []
    group = None
    for e in taken:
        if e["group"] != group:
            group = e["group"]
            lines.append(f"    // {group.split(' - ', 1)[1]}")
        lines.append(f'    {{ "{e["name"]}", {"true" if e["minor"] else "false"}, "{" ".join(e["chords"])}" }},')
    return "\n".join(lines)


def readme_table(taken, left):
    out = ["| # | Progression | Key | Pads A01-A08 (in C) |", "|---|---|---|---|"]
    for i, e in enumerate(taken, 1):
        out.append(f"| {i} | {e['name']} | {'minor' if e['minor'] else 'major'} | {' '.join(e['chords'])} |")
    out.append("")
    reasons = collections.Counter(why for _it, why in left)
    out.append(f"Left out ({len(left)} sets): " + "; ".join(f"{n} with {why}" for why, n in sorted(reasons.items())) + ".")
    return "\n".join(out)


def replace_between(path, start, end, body):
    text = open(path).read()
    a = text.index(start) + len(start)
    b = text.index(end)
    open(path, "w").write(text[:a] + "\n" + body + "\n" + text[b:])


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    taken, left = build(sys.argv[1])
    pads = 8 * len(taken)
    print(f"{len(taken)} progressions ({pads} pads, every one named exactly from its notes); {len(left)} sets left out")
    for it, why in left:
        print(f"  left out: {it['bank']} {it['slot']:3d}  {why}")
    if "--write" in sys.argv[2:]:
        replace_between(os.path.join(ROOT, "src", "progressions.h"),
                        "// --- made by tools/m3_progressions.py: do not edit by hand ---",
                        "    // --- end of the made part ---", header_table(taken))
        replace_between(os.path.join(ROOT, "README.md"), "<!-- progressions: made by tools/m3_progressions.py -->",
                        "<!-- end of progressions -->", readme_table(taken, left))
        print("wrote src/progressions.h and README.md")


if __name__ == "__main__":
    main()
