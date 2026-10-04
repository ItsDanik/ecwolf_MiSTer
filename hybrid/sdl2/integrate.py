#!/usr/bin/env python3
"""Add the MiSTer hybrid core drivers to an SDL2 source tree (2.32.x).

usage: integrate.py <SDL2 source directory>

Copies the drivers and the HPS library (hybrid/hps) into the tree and
registers them. Configure SDL with -DSDL_MISTER=ON afterwards; the "mister"
video and audio drivers are then tried first and only work with a hybrid core
loaded. mister_hybrid.h is installed next to the SDL headers.
"""
import shutil
import sys
from pathlib import Path

here = Path(__file__).resolve().parent
sdl = Path(sys.argv[1])


def patch(path, anchor, replacement):
    text = (sdl / path).read_text()
    if replacement in text:
        return
    if text.count(anchor) != 1:
        sys.exit(f"integrate.py: {path}: anchor not found exactly once: {anchor!r}")
    (sdl / path).write_text(text.replace(anchor, replacement))


def copy(src, dst):
    (sdl / dst).parent.mkdir(parents=True, exist_ok=True)
    # unchanged files keep their time stamp, so SDL only rebuilds what changed
    if not (sdl / dst).exists() or (sdl / dst).read_bytes() != Path(src).read_bytes():
        shutil.copyfile(src, sdl / dst)


copy(here / "SDL_mistervideo.c", "src/video/mister/SDL_mistervideo.c")
copy(here / "SDL_misteraudio.c", "src/audio/mister/SDL_misteraudio.c")
copy(here.parent / "hps/mister_hybrid.c", "src/core/mister/mister_hybrid.c")
copy(here.parent / "hps/mister_joymap.c", "src/core/mister/mister_joymap.c")
copy(here.parent / "hps/mister_ui.c", "src/core/mister/mister_ui.c")
copy(here.parent / "hps/mister_font.h", "src/core/mister/mister_font.h")
copy(here.parent / "hps/mister_hybrid.h", "include/mister_hybrid.h")

patch("src/video/SDL_video.c",
      "static VideoBootStrap *bootstrap[] = {\n",
      "static VideoBootStrap *bootstrap[] = {\n#ifdef SDL_VIDEO_DRIVER_MISTER\n    &MISTER_bootstrap,\n#endif\n")
patch("src/video/SDL_sysvideo.h",
      "extern VideoBootStrap DUMMY_bootstrap;\n",
      "extern VideoBootStrap MISTER_bootstrap;\nextern VideoBootStrap DUMMY_bootstrap;\n")
patch("src/audio/SDL_audio.c",
      "static const AudioBootStrap *const bootstrap[] = {\n",
      "static const AudioBootStrap *const bootstrap[] = {\n#ifdef SDL_AUDIO_DRIVER_MISTER\n    &MISTERAUDIO_bootstrap,\n#endif\n")
patch("src/audio/SDL_sysaudio.h",
      "extern AudioBootStrap DUMMYAUDIO_bootstrap;\n",
      "extern AudioBootStrap MISTERAUDIO_bootstrap;\nextern AudioBootStrap DUMMYAUDIO_bootstrap;\n")

patch("CMakeLists.txt",
      "if(SDL_AUDIO)\n  # CheckDummyAudio/CheckDiskAudio - valid for all platforms\n",
      """set_option(SDL_MISTER "MiSTer hybrid core video, audio and input drivers" OFF)
if(SDL_MISTER)
  file(GLOB MISTER_SOURCES
    ${SDL2_SOURCE_DIR}/src/video/mister/*.c
    ${SDL2_SOURCE_DIR}/src/audio/mister/*.c
    ${SDL2_SOURCE_DIR}/src/core/mister/*.c)
  list(APPEND SOURCE_FILES ${MISTER_SOURCES})
  add_definitions(-DSDL_VIDEO_DRIVER_MISTER=1 -DSDL_AUDIO_DRIVER_MISTER=1)
endif()

if(SDL_AUDIO)
  # CheckDummyAudio/CheckDiskAudio - valid for all platforms
""")
# The core hands us the joysticks Main_MiSTer has already mapped: SDL must not
# also open the raw devices
patch("CMakeLists.txt",
      "if((LINUX OR FREEBSD) AND HAVE_LINUX_INPUT_H AND NOT ANDROID)\n      set(SDL_JOYSTICK_LINUX 1)",
      "if((LINUX OR FREEBSD) AND HAVE_LINUX_INPUT_H AND NOT ANDROID AND NOT SDL_MISTER)\n      set(SDL_JOYSTICK_LINUX 1)")
print(f"MiSTer hybrid drivers added to {sdl}")
