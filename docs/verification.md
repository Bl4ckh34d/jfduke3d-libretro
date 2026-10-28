# Publication verification — 2026-10-28

The checked-in build recipe passed on Windows with Python/Git and NDK r27d, targeting ARMv7 API 21. Python scripts parse successfully. All patches pass forward checks against the pinned upstream index and reverse checks against the patched working tree.

Build: `duke3d_native_libretro_android.so`, 1261188 bytes, SHA256 `4f529656429c7a024b94de892f8813aa13e70b6bc6625819ee6feebf5c76d555`. This identifies this verification build; paths and timestamps can affect rebuild hashes. Original game files, player saves and generated output are not included in Git.

Actual ARM tests passed: PCM ring and MIDI timing/synthesis/shutdown; early unload; E1L1 gameplay followed by one shutdown at frame 1261; second load shutdown at frame 782; three extra calls did not duplicate shutdown. The second run can exit from retained menu state. Production core unchanged.

Fresh source fetch and repeated fetch both passed with explicit pinned JFBuild/JFAudioLib/JFMACT checkouts. Required JFBuild modification notice is included in the patch.
