# Setup details

Load DUKE3D.GRP from the original data directory, with companion game files beside it. The supplied DUKE3D.rmp maps physical Xbox X to Open/Use and Y to inventory use. Left/right sticks move/look; A jumps, RT fires, Start opens the game menu. Keep RetroArch load_dummy_on_core_shutdown enabled to return to the menu on game quit.

The libretro startup selects toggle mouse aiming with aiming enabled, so right-stick Y looks vertically without holding another button. In JFDuke3D, `ud.mouseaiming = 1` means momentary/hold mode; it does not mean always-on aiming. Both look directions and aiming after Reset/native load were verified on ARM, followed by both directions in actual RetroArch. Existing production axis assignments remain unchanged.

After building, set ADB (or put adb on PATH), ANDROID_SERIAL and DUKE_GRP to the device-side original GRP path. Run scripts/test_duke_audio.py. Push the core to /data/local/tmp/duke-fixed.so, then run scripts/test_duke_quit.py /data/local/tmp/duke-fixed.so. The quit harness checks early unload, a gameplay quit, one shutdown notification and reload safety. Its second load can exit from retained menu state; it is not a second full gameplay run. Temporary saves are under /data/local/tmp/duke-quit-saves. Remove test files afterwards.
