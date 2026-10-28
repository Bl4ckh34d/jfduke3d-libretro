# JFDuke3D libretro for Android TV

A native libretro adapter for JFDuke3D, targeting 32-bit ARM Android TV. Original Duke Nukem 3D data is required.

## Scope

640x400 software rendering; 60 Hz output; 48 kHz PCM/MIDI audio; analog movement/look; gamepad actions; native saves; engine cleanup across reloads. Game Quit sends one libretro shutdown request after cleanup, returning to RetroArch when Load Dummy on Core Shutdown is enabled.

This repository contains the integration sources, patches and build/profile files. Upstream engines and dependencies are fetched at the exact revisions in `dependencies.json`; original commercial game data is not included.

## Build

The verified build host is Windows with Python 3.10+, Git for Windows at `C:/Program Files/Git` (short shell path `C:/Progra~1/Git/bin/sh.exe`) and Android NDK r27d. Use a checkout path without spaces. Set `ANDROID_NDK_HOME` if the NDK is not at `C:/tools/android/android-ndk-r27d`.

```powershell
python scripts/fetch_sources.py
python scripts/build_duke_synth.py
python scripts/build_jfduke3d.py
```

Output: `release/duke3d_native_libretro_android.so` (ARMv7, outer adapter targets Android API 23; tested on Android 11). `PORT_SOURCES_DIR` optionally points to an existing dependency directory to share downloads between builds. Fetch refuses unexpected existing revisions/origins instead of resetting them. Remove only disposable `build/` output when a clean rebuild is needed.

## Install and configure

Install the core through RetroArch's Android core installation interface. The tested package is `com.retroarch.ra32`; its `com.retroarch.browser.debug.CoreSideloadActivity` accepts a `LIBRETRO` string pointing to a readable core file. Other RetroArch packages may differ.

Keep original game data on USB. Keep saves in a writable directory. `config/` contains the tested content-specific override/remap/options examples; adjust Android paths and content filenames to your installation. See [setup details](docs/setup.md). Do not replace the global RetroArch configuration. Xbox labels here refer to the tested Android Xbox mapping; other controller profiles can differ.

## Verification and limits

Hollywood Holocaust and Red Light District movement, shooting, native save/load and resume tested. Street sample: about 60 presented FPS over 21.9 seconds, not a whole-campaign benchmark. Quit regression reproduces the old freeze and verifies shutdown/reload; live in-game Quit returned to RetroArch.

RetroArch save states are not implemented. L3 saves native slot 9 as TV Quicksave; R3 loads it, including from the menu after Reset. Saving requires a living single player outside demo playback. Reset unloads the embedded engine and opens a fresh instance while retaining disk saves. The loader uses an anonymous memory file and verifies unloading; no RetroArch rebuild is required. The Build engine retains its separate license; this repository does not distribute a prebuilt combined binary.

Physical Xbox controls: left stick move, right stick look, A jump, B crouch, X activate, Y use selected inventory, R2 fire, LB/RB previous/next weapon, View map, Start menu. D-pad up performs Quick Kick, down toggles the jetpack, and left/right select inventory. The example remap accounts for Xbox X/Y labels. L2 is reserved for the frontend's fast-forward shortcut; disable frontend save-state buttons for this native-save core as shown in the example override.

These are bounded historical gameplay checks on one Android 11 ARMv7/Mali-G52 TV. Physical controller feel and speaker quality were not independently measured. See `docs/verification.md` for publication-time build checks.

## Licensing and credits

New integration code, build scripts and documentation in this repository are provided under GPL-2.0-or-later. Patches retain the applicable upstream file licenses. See `LICENSE` and `licenses/` for upstream notices. Dependencies retain their own licenses; review them before distributing a linked binary. Game names identify compatibility and do not imply endorsement.

```text
// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
```
