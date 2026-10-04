#!/bin/sh
# Build static SDL2 (with the MiSTer hybrid drivers), SDL2_mixer and SDL2_net.
# Runs inside the toolchain container (see hybrid/docker.sh).
# usage: build.sh <mister|host> <work directory>
#   libraries and headers end up in <work directory>/<target>/prefix
set -e
TARGET=$1
WORK=$(mkdir -p "$2" && cd "$2" && pwd)
HYBRID=$(cd "$(dirname "$0")/.." && pwd)

SDL=SDL2-2.32.10
MIXER=SDL2_mixer-2.8.2
NET=SDL2_net-2.4.0

fetch() { # <tarball> <url> <sha256>
    [ -f "$WORK/dl/$1" ] || curl -fsSL -o "$WORK/dl/$1" "$2"
    echo "$3  $WORK/dl/$1" | sha256sum -c - > /dev/null
}
mkdir -p "$WORK/dl" "$WORK/src"
fetch $SDL.tar.gz https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/$SDL.tar.gz \
    5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
fetch $MIXER.tar.gz https://github.com/libsdl-org/SDL_mixer/releases/download/release-2.8.2/$MIXER.tar.gz \
    938dff531d00ace2296557a6599abe6f34599e2f34f0a4a08a397e2ccac8b8f7
fetch $NET.tar.gz https://github.com/libsdl-org/SDL_net/releases/download/release-2.4.0/$NET.tar.gz \
    9cbca2527feb3f1a622d48ba65cc7dee9b1e3f2c55ceafb7d7720bb058aafb30
for p in $SDL $MIXER $NET; do
    [ -d "$WORK/src/$p" ] || tar -xzf "$WORK/dl/$p.tar.gz" -C "$WORK/src"
done
# always refreshes the driver sources in the SDL tree
python3 "$HYBRID/sdl2/integrate.py" "$WORK/src/$SDL"

PREFIX=$WORK/$TARGET/prefix
COMMON="-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PREFIX -DCMAKE_PREFIX_PATH=$PREFIX -DCMAKE_INSTALL_LIBDIR=lib -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=OFF"
if [ "$TARGET" = mister ]; then
    COMMON="$COMMON -DCMAKE_TOOLCHAIN_FILE=$HYBRID/toolchain/mister.cmake -DCMAKE_FIND_ROOT_PATH=$PREFIX"
fi
JOBS=$(nproc)
mkdir -p "$WORK/$TARGET"

# Only what a hybrid core can use: the MiSTer drivers, the dummy and disk
# drivers for running headless, the software renderer
cmake -S "$WORK/src/$SDL" -B "$WORK/$TARGET/$SDL" $COMMON \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DSDL_MISTER=ON \
    -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF -DSDL_RPI=OFF -DSDL_VIVANTE=OFF -DSDL_DIRECTFB=OFF \
    -DSDL_OFFSCREEN=OFF -DSDL_OPENGL=OFF -DSDL_OPENGLES=OFF -DSDL_VULKAN=OFF \
    -DSDL_ALSA=OFF -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF -DSDL_JACK=OFF -DSDL_OSS=OFF -DSDL_SNDIO=OFF \
    -DSDL_ESD=OFF -DSDL_ARTS=OFF -DSDL_NAS=OFF -DSDL_FUSIONSOUND=OFF -DSDL_LIBSAMPLERATE=OFF \
    -DSDL_DBUS=OFF -DSDL_IBUS=OFF -DSDL_FCITX=OFF -DSDL_LIBUDEV=OFF \
    -DSDL_HIDAPI=ON -DSDL_HIDAPI_JOYSTICK=OFF -DSDL_HIDAPI_LIBUSB=OFF -DSDL_VIRTUAL_JOYSTICK=ON \
    -DSDL_HAPTIC=OFF -DSDL_SENSOR=OFF > "$WORK/$TARGET/$SDL.log"
cmake --build "$WORK/$TARGET/$SDL" -j"$JOBS" >> "$WORK/$TARGET/$SDL.log"
cmake --install "$WORK/$TARGET/$SDL" >> "$WORK/$TARGET/$SDL.log"

# WAV, Ogg Vorbis (stb), FLAC (dr_flac) and MP3 (minimp3) decoders are built in
cmake -S "$WORK/src/$MIXER" -B "$WORK/$TARGET/$MIXER" $COMMON \
    -DSDL2MIXER_SAMPLES=OFF -DSDL2MIXER_DEPS_SHARED=OFF -DSDL2MIXER_VENDORED=OFF \
    -DSDL2MIXER_OPUS=OFF -DSDL2MIXER_MOD=OFF -DSDL2MIXER_MIDI=OFF -DSDL2MIXER_WAVPACK=OFF -DSDL2MIXER_GME=OFF \
    -DSDL2MIXER_FLAC=ON -DSDL2MIXER_FLAC_LIBFLAC=OFF -DSDL2MIXER_FLAC_DRFLAC=ON \
    -DSDL2MIXER_MP3=ON -DSDL2MIXER_MP3_MINIMP3=ON -DSDL2MIXER_MP3_MPG123=OFF \
    -DSDL2MIXER_VORBIS=STB > "$WORK/$TARGET/$MIXER.log"
cmake --build "$WORK/$TARGET/$MIXER" -j"$JOBS" >> "$WORK/$TARGET/$MIXER.log"
cmake --install "$WORK/$TARGET/$MIXER" >> "$WORK/$TARGET/$MIXER.log"

cmake -S "$WORK/src/$NET" -B "$WORK/$TARGET/$NET" $COMMON \
    -DSDL2NET_SAMPLES=OFF > "$WORK/$TARGET/$NET.log"
cmake --build "$WORK/$TARGET/$NET" -j"$JOBS" >> "$WORK/$TARGET/$NET.log"
cmake --install "$WORK/$TARGET/$NET" >> "$WORK/$TARGET/$NET.log"

echo "SDL2 for $TARGET installed in $PREFIX"
