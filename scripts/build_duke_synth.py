"""Build the unmodified pinned MIDI dependency used by the Duke adapter."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import hashlib
import subprocess
import os

ROOT = Path(__file__).resolve().parents[1]
SOURCES = Path(os.environ.get("PORT_SOURCES_DIR", str(ROOT / "sources"))).resolve()
NDK_HOME = Path(os.environ.get("ANDROID_NDK_HOME", "C:/tools/android/android-ndk-r27d"))
ADL = SOURCES / 'libadlmidi'
BUILD = ROOT / 'build/opendune'
LLVM = NDK_HOME / 'toolchains/llvm/prebuilt/windows-x86_64/bin'
assert subprocess.check_output(['git', '-C', str(ADL), 'rev-parse', 'HEAD'], text=True).strip() == '3e9ddb13ad39e2de0ca7eb003e622d4741aa50b7'
BUILD.mkdir(parents=True, exist_ok=True)
names = ['adlmidi.cpp', 'adlmidi_load.cpp', 'adlmidi_midiplay.cpp', 'adlmidi_opl3.cpp',
         'adlmidi_private.cpp', 'inst_db.cpp', 'wopl/wopl_file.c',
         'chips/dosbox_opl2.cpp', 'chips/dosbox_opl3.cpp', 'chips/dosbox/dbopl.cpp']
sources = [ADL / 'src' / name for name in names] + sorted((ADL / 'src/models').glob('*.c'))
flags = ['--target=armv7a-linux-androideabi21', '-O2', '-g', '-fPIC', '-fvisibility=hidden',
         '-ffunction-sections', '-fdata-sections', '-DNDEBUG', '-I' + str(ADL / 'include'),
         '-DADLMIDI_DISABLE_MIDI_SEQUENCER']
flags += ['-DADLMIDI_DISABLE_' + name + '_EMULATOR' for name in ['NUKED', 'OPAL', 'JAVA', 'ESFMU', 'MAME_OPL2', 'YMFM']]

def compile_source(source):
    obj = BUILD / (hashlib.sha256(str(source).encode()).hexdigest()[:16] + '.o')
    compiler = LLVM / ('clang++.exe' if source.suffix == '.cpp' else 'clang.exe')
    subprocess.run([str(compiler), *flags, '-std=c++11' if source.suffix == '.cpp' else '-std=gnu99',
                    '-c', str(source), '-o', str(obj)], check=True, capture_output=True)
    return obj

with ThreadPoolExecutor(max_workers=4) as pool:
    objects = list(pool.map(compile_source, sources))
print('Built', len(objects), 'shared synthesizer objects')
