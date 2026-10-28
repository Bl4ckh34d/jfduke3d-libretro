"""Exercise the real native PCM/MIDI driver on the ARMv7 TV."""
from pathlib import Path
import hashlib
import subprocess
import os

ROOT = Path(__file__).resolve().parents[1]
SOURCES = Path(os.environ.get("PORT_SOURCES_DIR", str(ROOT / "sources"))).resolve()
NDK_HOME = Path(os.environ.get("ANDROID_NDK_HOME", "C:/tools/android/android-ndk-r27d"))
LLVM = NDK_HOME / 'toolchains/llvm/prebuilt/windows-x86_64/bin'
ADL = SOURCES / 'libadlmidi'
BUILD = ROOT / 'build/duke-audio-test'
BUILD.mkdir(exist_ok=True)
objects = []
for source in [ROOT / 'ports/jfduke3d/audio.c', ROOT / 'tests/duke_audio_test.c']:
    obj = BUILD / (source.stem + '.o')
    subprocess.run([str(LLVM / 'clang.exe'), '--target=armv7a-linux-androideabi21', '-O2', '-std=gnu99',
                    '-I' + str(ADL / 'include'), '-I' + str(SOURCES / 'jfduke3d/jfaudiolib/src'),
                    '-c', str(source), '-o', str(obj)], check=True)
    objects.append(obj)
# Reuse the identical, pinned synthesizer objects already built for Dune.
names = ['adlmidi.cpp', 'adlmidi_load.cpp', 'adlmidi_midiplay.cpp', 'adlmidi_opl3.cpp',
         'adlmidi_private.cpp', 'inst_db.cpp', 'wopl/wopl_file.c',
         'chips/dosbox_opl2.cpp', 'chips/dosbox_opl3.cpp', 'chips/dosbox/dbopl.cpp']
for source in [*[ADL / 'src' / name for name in names], *sorted((ADL / 'src/models').glob('*.c'))]:
    objects.append(ROOT / 'build/opendune' / (hashlib.sha256(str(source).encode()).hexdigest()[:16] + '.o'))
binary = BUILD / 'duke-audio-test'
subprocess.run([str(LLVM / 'clang++.exe'), '--target=armv7a-linux-androideabi21', '-static-libstdc++',
                *map(str, objects), '-lm', '-o', str(binary)], check=True)
adb = [os.environ.get('ADB', 'adb')] + (['-s', os.environ['ANDROID_SERIAL']] if os.environ.get('ANDROID_SERIAL') else [])
subprocess.run(adb + ['push', str(binary), '/data/local/tmp/duke-audio-test'], check=True)
subprocess.run(adb + ['shell', 'chmod 700 /data/local/tmp/duke-audio-test'], check=True)
subprocess.run(adb + ['shell', '/data/local/tmp/duke-audio-test'], check=True)
