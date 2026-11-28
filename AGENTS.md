# Notes for coding agents

**hdslave Reverie** is a pad and chord synth whose pads play chord progressions: a CLAP plug-in for the Akai MPC Sample. It is made to run in hdslaveOS, the hdslave fork of Kerem Kiliç's Melodee. Its CLAP id is `com.hdslave.reverie`, and it builds to `build/Reverie.clap`.

## Read the workspace guide first

On the author's machine, this repository sits in one folder beside the other hdslave projects. That folder's `../AGENTS.md` and `../Workflow/` hold the shared workflow:
- building in the Linux VM;
- what hdslaveOS does to plug-ins;
- screen pages;
- reviews;
- publishing, the hub repository and staging for the SD card.

Read `../AGENTS.md`, then `../Workflow/plugins.md` and `../Workflow/host-facts.md`, before you change anything. The rules below hold either way.

## Build and test

- `./build.sh` builds the plug-in in a Linux arm64 container (image `melodee-builder`, Colima VM `melodee`). It runs `make all test`: the host tests in `tests/host.cpp` and the sound's fingerprint. Pass make targets to run others, for example `./build.sh sanitize` and `./build.sh render` (WAVs for listening).
- `tools/bench.sh` counts the instructions of a 128-frame block with callgrind. A block is about 4.8 M cycles on one of the MPC's Cortex-A55 cores.
- Check the build's own exit code: `./build.sh | grep …` hides a failure.

## Rules

- **Fixed for saved projects.** Parameter ids, preset keys and choice orders never change once released, because saved projects and presets refer to them.
  - Add new ones at the end.
  - Ranges stay as they are too: the host stores bindings normalized to the range.
- **The sound is held to the bit** by `tests/fingerprint.txt`.
  - A refactor or speed-up must leave every hash unchanged. With `-ffp-contract=fast`, even reshaping a loop can change the last bit, so check after every edit.
  - Only a change meant to change the sound runs `make fingerprint-update`, and its commit says which hashes moved and why.
- **Real-time and determinism.** The output is the same at every block size. No allocation, locks or file I/O on the audio thread.
- **Notes** must survive how hdslaveOS sends them:
  - every MIDI event of a block arrives at frame 0;
  - a re-pressed key arrives as on, on, off;
  - CC 120 and 123 arrive at 0 on every channel when the host silences.
- **Commits** are made as `hdslave <111106843+hdslave@users.noreply.github.com>`, with Conventional Commits. Never push, publish or stage for the card without the owner's word.
- **Other agents' work.** Others may be working here. Check `git status` and `git worktree list`, and never edit, commit or reset files you did not change.
- **Names and art.** No trademarked names in the plug-in's names, presets, UI text or art. Art is original; fonts are OFL, with their licences in `licenses/`.
- **Read the README** before you change the sound. It lists the sources, the inferred values and the deliberate deviations. Keep it current: version, costs, and "not yet run on the MPC" until it has been.

## This plug-in

- **The chord section** is ids 0-9: PROG, KEY, VOICING, OCT, STRUM, HUMAN, BASS, LATCH, GLIDE and VEL. Sound presets never touch them.
- **Progressions** are a choice parameter whose order and count are fixed once released. Since 0.3.0 they are the M3 factory pad chord sets (1-172), and since 0.4.0 the TORAIZ SQUID's 18 chord sets follow as 36 progressions (173-208, each set's pads 1-8 then 9-16). `tools/make_progressions.py` makes them from two private packs outside the repo: `MPC CFW/Korg M3 Pad & Motion Synth Chord Pack/all-progressions.json` and `MPC CFW/Toraiz SQUID Chord Sets/chord-sets.json` (read from the SQUID firmware by that folder's `extract_chord_sets.py`). Never commit either pack: they are the makers' factory data. The 0.1-0.2 set was retired; a version-1 state resets PROG. Once released (staged for the card or pinned in the hub) the 208 are fixed: hdslaveOS keeps PROG's motion and bindings as a share of its range, so a progression added at the end (raising `max`) moves every recorded one, and re-running the tool must give the same table. More progressions go in a new parameter. Each chord's quality must hold exactly its notes: the tool names chords from `qualities[]` in `src/chords.h`, and pads 1-8 play only those tones; Lift's colour tone comes from `Quality::lift`.
- **Voicings** are baked at build time. `tools/voicings.cpp` writes `build/voicings.inc`, with the plug-in's own flags.
- **Drum map.** The `.clap` embeds one, so hdslaveOS plays the pads as notes 36-51.
- **The page.** It is `src/screen.cpp` with `src/ui/screen_art.h`, baked by `tools/bake_screen_art.py` from `assets/`, whose prompts are in `assets/README.md`.

## Release packaging

Every release ZIP includes the full repository `LICENSE.txt` beside the third-party notices, outside signed plug-in bundles. Preserve both when packaging the plug-in for another project.
