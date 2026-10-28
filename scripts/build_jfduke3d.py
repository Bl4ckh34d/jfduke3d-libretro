"""Build the pinned JFDuke3D native ARMv7 libretro adapter."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import subprocess
import os

ROOT = Path(__file__).resolve().parents[1]
SOURCES = Path(os.environ.get("PORT_SOURCES_DIR", str(ROOT / "sources"))).resolve()
NDK_HOME = Path(os.environ.get("ANDROID_NDK_HOME", "C:/tools/android/android-ndk-r27d"))
SRC = SOURCES / 'jfduke3d'
COMMON = SOURCES / 'libretro-common'
ADL = SOURCES / 'libadlmidi'
LLVM = NDK_HOME / 'toolchains/llvm/prebuilt/windows-x86_64/bin'
BUILD = ROOT / 'build/jfduke3d'
BUILD.mkdir(parents=True, exist_ok=True)
for source, revision in [(SRC, '55c5f9592d2a78e427a2bbdb13ce3c4c9ccf6f04'),
                         (SRC / 'jfbuild', '40a98be571e359a70fc60476899e4650e285dbca'),
                         (SRC / 'jfaudiolib', '68be97f6d33d7be2421a6bea437f21c60d70f8bb'),
                         (SRC / 'jfmact', '731c0cc4aabd3cc0f510c9322825f1b61b2e47b3'),
                         (ADL, '3e9ddb13ad39e2de0ca7eb003e622d4741aa50b7'),
                         (COMMON, '879c8d507b0b52e77e27d759239c2b5df1e26dfd')]:
    actual = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != revision: raise SystemExit(f'Unexpected source revision: {source}: {actual}')
for source, filename in [(SRC, 'jfduke3d-libretro-hooks.patch'),
                          (SRC / 'jfbuild', 'jfbuild-libretro-path.patch'),
                          (SRC / 'jfaudiolib', 'jfaudiolib-libretro-driver.patch')]:
    patch = ROOT / 'patches' / filename
    if subprocess.run(['git', '-C', str(source), 'apply', '--reverse', '--check', str(patch)], capture_output=True).returncode:
        subprocess.run(['git', '-C', str(source), 'apply', str(patch)], check=True)

groups = {
    'src': 'game actors gamedef global menues player premap sector rts config osdfuncs osdcmds grpscan sounds version',
    'jfmact': 'util_lib file_lib control keyboard mouse mathutil scriplib animlib',
    'jfbuild/src': 'a-c asmprot baselayer cache1d compat crc32 defs engine kplib mmulti_null osd pragmas scriptfile textfont talltextfont smalltextfont version',
    'jfaudiolib/src': 'drivers fx_man cd multivoc mix mixst pitch vorbis music midi asssys',
}
sources = [SRC / folder / (name + '.c') for folder, names in groups.items() for name in names.split()]
sources += [COMMON / 'libco/libco.c', *sorted((ROOT / 'ports/jfduke3d').glob('*.c'))]
includes = [SRC / x for x in ['src', 'jfbuild/include', 'jfbuild/src', 'jfmact', 'jfaudiolib/include', 'jfaudiolib/src']]
includes += [COMMON / 'include', ADL / 'include']
flags = ['--target=armv7a-linux-androideabi21', '-O2', '-g', '-fPIC', '-fvisibility=hidden',
         '-fno-strict-aliasing', '-ffunction-sections', '-fdata-sections', '-DLIBRETRO', '-DNDEBUG',
         '-DUSE_POLYMOST=0', '-DUSE_OPENGL=0', '-DUSE_ASM=0', '-std=gnu99']
flags += ['-I' + str(path) for path in includes]
header_hash = hashlib.sha256()
for base in includes:
    for header in sorted(base.glob('*.h')): header_hash.update(header.read_bytes())
header_signature = header_hash.digest()

def compile_source(source):
    obj = BUILD / (hashlib.sha256(str(source).encode()).hexdigest()[:16] + '.o')
    extra = ['-Dexit=Duke_Exit', '-Dfopen=Duke_fopen'] if source.is_relative_to(SRC) else []
    command = [str(LLVM / 'clang.exe'), *flags, *extra, '-c', str(source), '-o', str(obj)]
    signature = hashlib.sha256(' '.join(command).encode() + source.read_bytes() + header_signature).hexdigest()
    stamp = obj.with_suffix('.sha256')
    if not obj.exists() or not stamp.exists() or stamp.read_text() != signature:
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode: raise RuntimeError(f'{source}\n{result.stderr}')
        stamp.write_text(signature)
    return obj

with ThreadPoolExecutor(max_workers=4) as pool:
    objects = list(pool.map(compile_source, sources))
# The synthesizer is identical to the Dune build; avoid duplicate object storage.
names = ['adlmidi.cpp', 'adlmidi_load.cpp', 'adlmidi_midiplay.cpp', 'adlmidi_opl3.cpp',
         'adlmidi_private.cpp', 'inst_db.cpp', 'wopl/wopl_file.c',
         'chips/dosbox_opl2.cpp', 'chips/dosbox_opl3.cpp', 'chips/dosbox/dbopl.cpp']
for source in [*[ADL / 'src' / name for name in names], *sorted((ADL / 'src/models').glob('*.c'))]:
    obj = ROOT / 'build/opendune' / (hashlib.sha256(str(source).encode()).hexdigest()[:16] + '.o')
    if not obj.exists(): raise SystemExit('Build the shared synthesizer with scripts/build_duke_synth.py first')
    objects.append(obj)
exports = BUILD / 'exports.map'; exports.write_text('{ global: retro_*; local: *; };\n')
response = BUILD / 'objects.rsp'
response.write_text('\n'.join('"' + str(obj).replace('\\', '/') + '"' for obj in objects))
out = ROOT / 'release/duke3d_native_libretro_android.so'
out.parent.mkdir(exist_ok=True)
subprocess.run([str(LLVM / 'clang++.exe'), '--target=armv7a-linux-androideabi21', '-shared',
                '-static-libstdc++', '@' + str(response), '-Wl,--no-undefined', '-Wl,--gc-sections',
                '-Wl,--version-script=' + str(exports), '-lm', '-o', str(out)], check=True)
subprocess.run([str(LLVM / 'llvm-strip.exe'), '--strip-unneeded', str(out)], check=True)
print(out, out.stat().st_size, hashlib.sha256(out.read_bytes()).hexdigest())
