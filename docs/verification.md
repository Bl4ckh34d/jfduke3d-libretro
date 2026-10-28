# Follow-up verification — 2026-10-29

The repository's build script completed on Windows with NDK r27d using the pinned shared sources. Python scripts parse, and source patches pass both a forward check against a temporary pristine upstream index and a reverse check against the working source. Original game data, saves, screenshots and binaries are excluded from publication.

Build: `duke3d_native_libretro_android.so`, 1,284,180 bytes, SHA-256 `bc0a694741093d5ac452e05b3691e6058cbb99869034efb706b410775cecb7ce`.

The publication build passed the real ARM quit/reload regression twice (one shutdown notification per session). The reset adapter passed ten fresh-engine resets with callbacks and controller device retained. Gameplay save/load, reset/load, both vertical look directions and Close Content were checked on the source-equivalent installed build. Its different artifact hash reflects build-specific metadata; the publication artifact itself is not claimed to have been installed.

These are bounded checks on Android 11 ARMv7. They do not establish whole-campaign performance or subjective physical-controller feel. Earlier records below describe their own historical builds.

---

# Publication verification � 2026-10-28

The checked-in build recipe passed on Windows with Python/Git and NDK r27d, targeting ARMv7 API 21. Python scripts parse successfully. All patches pass forward checks against the pinned upstream index and reverse checks against the patched working tree.

Build: `duke3d_native_libretro_android.so`, 1261188 bytes, SHA256 `4f529656429c7a024b94de892f8813aa13e70b6bc6625819ee6feebf5c76d555`. This identifies this verification build; paths and timestamps can affect rebuild hashes. Original game files, player saves and generated output are not included in Git.

Actual ARM tests passed: PCM ring and MIDI timing/synthesis/shutdown; early unload; E1L1 gameplay followed by one shutdown at frame 1261; second load shutdown at frame 782; three extra calls did not duplicate shutdown. The second run can exit from retained menu state. Production core unchanged.

Fresh source fetch and repeated fetch both passed with explicit pinned JFBuild/JFAudioLib/JFMACT checkouts. Required JFBuild modification notice is included in the patch.
