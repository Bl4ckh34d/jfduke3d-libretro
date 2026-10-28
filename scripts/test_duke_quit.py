"""Run the real ARM quit/reload regression on the connected TV."""
from pathlib import Path
import subprocess
import shlex
import os
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCES = Path(os.environ.get("PORT_SOURCES_DIR", str(ROOT / "sources"))).resolve()
NDK_HOME = Path(os.environ.get("ANDROID_NDK_HOME", "C:/tools/android/android-ndk-r27d"))
ADB = [os.environ.get('ADB', 'adb')] + (['-s', os.environ['ANDROID_SERIAL']] if os.environ.get('ANDROID_SERIAL') else [])
LLVM = NDK_HOME / 'toolchains/llvm/prebuilt/windows-x86_64/bin'
binary = ROOT / 'build/duke_quit_test'
binary.parent.mkdir(exist_ok=True)
subprocess.run([str(LLVM / 'clang.exe'), '--target=armv7a-linux-androideabi21', '-O1', '-g',
                '-I' + str(SOURCES / 'libretro-common/include'),
                str(ROOT / 'tests/duke_quit_test.c'), '-ldl', '-o', str(binary)], check=True)
subprocess.run([*ADB, 'push', str(binary), '/data/local/tmp/duke_quit_test'], check=True, capture_output=True)
subprocess.run([*ADB, 'shell', 'chmod 700 /data/local/tmp/duke_quit_test'], check=True)
core = sys.argv[1] if len(sys.argv) > 1 else '/data/local/tmp/duke-fixed.so'
result = subprocess.run([*ADB, 'shell', shlex.join(['/data/local/tmp/duke_quit_test', core, os.environ['DUKE_GRP']])], capture_output=True, text=True, timeout=60)
print(result.stdout[-1800:])
print(result.stderr[-1200:])
sys.exit(result.returncode)
