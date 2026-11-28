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

- `main` serves MPC Sample and desktop. Build macOS and Windows CLAP/VST3 from `mac-clap/` as described in [the desktop guide](mac-clap/README.md).
- `sp16-screen` serves freesp16. Verify shared engine changes there against an ARMv7 baseline. The MPC fingerprint file is an AArch64 reference.
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

- **Chord controls.** Sound presets leave IDs 0–9 and 97–100 alone. Mood (97) and Mood Transpose (98) keep their sixteen-pad override in both banks.
- **Prog banks.** Original Prog (99) has the 46 progressions from the first MPC release. Both the symbols and baked MIDI notes are shared with desktop Reverie in `mac-clap/src/progressions.h` and `mac-clap/src/voicings.inc`. Prog Bank (100) defaults to Original. Keep the Chords panel and the host’s first remote control bound to the active bank’s parameter.
- **Factory compatibility.** ID 0 retains the released order and range of the 208 Factory progressions. The first 172 are M3 chord sets and the last 36 are paired eight-pad versions of the SQUID sets. `tools/make_progressions.py` generates the table from the two private chord packs outside the repo. Keep the packs private. The Factory voicing search runs at build time in `tools/voicings.cpp`. State version 2 restores Factory. Version 1 restores a recognized Original slot.
- **Drum map.** The `.clap` embeds one, so hdslaveOS plays the pads as notes 36-51.
- **The page.** It is `src/screen.cpp` with `src/ui/screen_art.h`, baked by `tools/bake_screen_art.py` from `assets/`, whose prompts are in `assets/README.md`.

## Release packaging

Every release ZIP includes the full repository `LICENSE.txt` beside the third-party notices, outside signed plug-in bundles. Preserve both when packaging the plug-in for another project.
