#!/bin/sh
# Build ECWolf for the MiSTer in the toolchain container: static SDL2 with the
# hybrid core drivers first (once), then the game.
# usage: ./build.sh [mister|host] [Release|RelWithDebInfo|Debug]
#   mister  ARM binary for the MiSTer     -> build/mister/ecwolf + ecwolf.pk3
#   host    same code for this PC, to run against hybrid/tools/fakecore
set -e
cd "$(dirname "$0")"
TARGET=${1:-mister}
TYPE=${2:-Release}
ROOT=$PWD
PREFIX=$ROOT/build/sdl/$TARGET/prefix

hybrid/docker.sh hybrid/sdl2/build.sh "$TARGET" build/sdl

CROSS=
if [ "$TARGET" = mister ]; then
    # ECWolf's build tools (zipdir for ecwolf.pk3) run on the PC
    hybrid/docker.sh sh -c "
      cmake -S ecwolf -B build/tools -DCMAKE_BUILD_TYPE=Release -DTOOLS_ONLY=ON \
        -DINTERNAL_ZLIB=ON -DINTERNAL_BZIP2=ON > build/tools.log &&
      cmake --build build/tools -j\$(nproc) >> build/tools.log"
    CROSS="-DCMAKE_TOOLCHAIN_FILE=$ROOT/hybrid/toolchain/mister.cmake -DCMAKE_FIND_ROOT_PATH=$PREFIX"
    CROSS="$CROSS -DIMPORT_EXECUTABLES=$ROOT/build/tools/ImportExecutables.cmake"
    # the C++ runtime is linked in, the binary only needs the MiSTer's glibc
    CROSS="$CROSS -DCMAKE_EXE_LINKER_FLAGS='-no-pie -static-libstdc++ -static-libgcc'"
    # gdtoa finds out about the floating point format by running a program on
    # the target. For ARMv7 hard-float (little endian IEEE) the answers are:
    mkdir -p build/mister/deps/gdtoa
    printf '#define IEEE_8087\n#define Arith_Kind_ASL 1\n' > build/mister/deps/gdtoa/arith.h
    printf '#define f_QNAN 0x7fc00000\n#define d_QNAN0 0x0\n#define d_QNAN1 0x7ff80000\n' > build/mister/deps/gdtoa/gd_qnan.h
fi

hybrid/docker.sh sh -c "
  cmake -S ecwolf -B build/$TARGET -DCMAKE_BUILD_TYPE=$TYPE $CROSS \
    -DCMAKE_PREFIX_PATH=$PREFIX -DMISTER_HYBRID=ON -DNO_GTK=ON \
    -DINTERNAL_ZLIB=ON -DINTERNAL_BZIP2=ON -DINTERNAL_JPEG=ON &&
  cmake --build build/$TARGET -j\$(nproc)"
ls -la "build/$TARGET/ecwolf" "build/$TARGET/ecwolf.pk3"
