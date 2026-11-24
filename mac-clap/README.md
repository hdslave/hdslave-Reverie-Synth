# hdslave Reverie — macOS (and Windows) Desktop CLAP Plug-in

Separate desktop CLAP build of **hdslave Reverie** for **FL Studio** (2024+) and any other CLAP host on macOS (Apple Silicon / Intel) and Windows (x64).

![Reverie’s desktop panel with its chord pads, motion lens and live output meter](assets/reverie-desktop.png)

Desktop version **0.1.6** opens at its smallest size, **960 × 648**, and keeps the panel’s 40:27 proportions when resized. The glass panorama holds the sound preset selector, MIDI Learn and the live parameter readout. The six sections below it give direct access to all 80 parameters. The sixteen pads audition the chord bank from A01 at the lower left through A16 at the upper right.

Drag or scroll a value to edit it. Shift-drag or right-drag for fine adjustments. Click a choice to advance it, double-click to restore its default, or right-click for its menu. Click MIDI Learn, select a parameter and move a MIDI controller to bind it.

- **Standalone from the MPC Sample build:** this directory is completely separate from the MPC Sample build in the repository root (`../src`, `../build.sh`). GitHub releases remain MPC Sample-only for now while the desktop builds are being finalized.

## Build & Install (macOS)

Requires Xcode Command Line Tools and CMake 3.20+ (Ninja optional):

```bash
cd mac-clap
./build-macos.sh
./install-macos.sh
```

`install-macos.sh` builds, ad-hoc signs, tests, and installs `Reverie.clap` to `/Library/Audio/Plug-Ins/CLAP/` (pass `--user` to install to `~/Library/Audio/Plug-Ins/CLAP/`). Quit and reopen FL Studio after reinstalling so macOS reloads the updated bundle.

## Build (Windows x64)

```powershell
cd mac-clap
.\build-windows.ps1
```

Copy `build\windows\clap\Release\Reverie.clap` to `C:\Program Files\Common Files\CLAP\`.
