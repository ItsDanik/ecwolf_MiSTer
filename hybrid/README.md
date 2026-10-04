# MiSTer hybrid core framework

What every hybrid core shares. A hybrid core runs an open source game on the MiSTer's ARM CPU (the HPS) and uses a small FPGA core for what the ARM side has no hardware for: native 15kHz video, audio and the MiSTer's input devices. The two halves talk through shared DDR3 memory.

This directory is meant to be the same in every hybrid core repository (a git submodule once it has its own repository). A game repository adds the game, its `CONF_STR` and its launcher.

| Path | |
|---|---|
| `rtl/hybrid_host.sv` | The FPGA side: 320x200 scanout at 15.6kHz / 59.6Hz from DDR3 (8bpp paletted or RGB565, triple buffered), 44.1kHz audio ring, keyboard, mouse, joysticks and OSD status published every vblank. Its header documents the shared memory layout. `sim/run.sh` runs its testbench. |
| `hps/` | The ARM side as a small C library (`mister_hybrid.h`): attach to the core, present frames, palette, input, audio, the OSD's Menu OK/Back resolution, and the shared screens (game list, error message). |
| `sdl2/` | SDL2 video, audio and input drivers on top of `hps/`, and the script that builds a static SDL2, SDL2_mixer and SDL2_net with them. An SDL2 game needs little more than a recompile. |
| `launcher/danik_hybrid_cores.sh` | What starts the games on the MiSTer: a daemon that runs `games/<core name>/danik_hybrid_launch.sh` while that core is loaded. The user runs it once from the Scripts menu; it registers itself in `user-startup.sh`. Every core ships the same file as `Scripts/danik_hybrid_cores.sh`. Its header documents the contract for a `danik_hybrid_launch.sh`. |
| `toolchain/` | Docker images: ARM cross compiler matching the MiSTer's glibc, HDL simulation tools. `docker.sh` runs a command in the toolchain. |
| `tools/fakecore.c` | Stand-in for the FPGA core on the PC: runs the game against a file instead of the DDR3 window, feeds scripted input, takes screenshots and records audio. |
| `tools/*.py` | For tests on the MiSTer over ssh: `uinput_kbd.py` and `uinput_pad.py` press keys and gamepad buttons through Main_MiSTer, `status.py` prints what the core publishes. |

## Porting a game

**An SDL2 game:** build it against the SDL2 from `sdl2/build.sh` (`CMAKE_PREFIX_PATH=<work>/mister/prefix`). The "mister" drivers are picked when the core is loaded.

- The window is the screen: 320x200. Other sizes are cropped or centred.
- Games that draw 8-bit: set the hint `SDL_MISTER_VIDEO_FORMAT=INDEX8`, draw to `SDL_GetWindowSurface()` and set its palette. The FPGA does the palette lookup, so palette fades and flashes cost nothing. Everything else gets RGB565, including the 2D render API (software renderer).
- Audio is converted to 44.1kHz stereo by SDL; the core's sample clock paces the audio thread.
- Keyboard and mouse arrive as SDL events. Joysticks 1 and 2 are SDL joysticks with the d-pad as hat 0, the sticks as axes 0-3 and the buttons of the core's `J1` list as buttons 0.. in that order. Buttons 28 and 29 are Menu OK and Menu Back (see below).
- `SDL_QUIT` arrives when another core is loaded.
- `#include <mister_hybrid.h>` for the rest: OSD options (`MH_OSDStatus()`), the shared screens (`MH_UI_Menu()`, `MH_UI_Message()`).

**A game without SDL:** use `hps/` directly, as the SDL drivers do (`sdl2/SDL_mistervideo.c` and `SDL_misteraudio.c` are the reference).

**The FPGA core:** copy a `core/` directory of an existing hybrid core (Template_MiSTer's `sys/`, a 50MHz PLL, `hybrid_host`, `video_mixer`, `video_freak`) and change the name and the `CONF_STR`.

**Testing without a MiSTer:** `gcc -o fakecore tools/fakecore.c`, start `fakecore <file> [script]`, then the PC build of the game with `MISTER_HYBRID_SHM=<file>`.

## Conventions

These make the cores look and work alike. Where the framework can enforce one, it does.

**Naming and layout on the SD card**

- One name everywhere: core name in `CONF_STR`, `_Other/<Name>_YYYYMMDD.rbf`, `games/<Name>/`, `logs/<Name>/`, binary `games/<Name>/<Name>`.
- `games/<Name>/danik_hybrid_launch.sh` is the launcher `danik_hybrid_cores` runs. Everything the game writes (settings, saves) stays in `games/<Name>/`; the log goes to `/media/fat/logs/<Name>/`.
- Game data is never shipped. The README names the files to copy and where.
- Release: `<Name>_YYYYMMDD.zip` to extract at the root of the SD card, containing `_Other/`, `games/<Name>/` with `README.txt` and the licenses, and `Scripts/danik_hybrid_cores.sh`. Nothing else has to be installed; the README lists running `danik_hybrid_cores` once from the Scripts menu as a requirement.

**OSD** (`CONF_STR`), in this order:

```
"<Name>;;",
"-;",
"O[122:121],Aspect ratio,Original,Full Screen,[ARC1],[ARC2];",
"O[125:123],Scale,Normal,V-Integer,Narrower HV-Integer,Wider HV-Integer,HV-Integer;",
"O[4:2],Scandoubler Fx,None,HQ2x,CRT 25%,CRT 50%,CRT 75%;",
"O[6:5],Stereo Mix,None,25%,50%,100%;",
"-;",
"O[10:7],Sound Volume,100%,90%,...,0%;",      only if the game mixes sound and music separately
"O[14:11],Music Volume,100%,90%,...,0%;",
<game options, status bits 24..63>
"-;",
"O[19:16],Menu OK,MiSTer,A,B,X,Y,L,R,Select,Start;",
"O[23:20],Menu Back,MiSTer,A,B,X,Y,L,R,Select,Start;",
"-;",
"J1,<8 game actions>,Menu OK,Menu Back;",
"jn,<the 8 buttons A,B,X,Y,L,R,Select,Start in the order of the actions>;",
"V,v",`BUILD_DATE
```

- Status bits 0..23 mean the same in every core (`MH_OSD_*` in `mister_hybrid.h`), bits 24..63 belong to the game, bits 64 and up never reach the game and are for the core's video options.
- The first entry of every option is its default, so a fresh install needs no settings.
- An option the game cannot honour is left out, not shown greyed or ignored.

**Controls**

- Keyboard and mouse work as in the original game.
- The `J1` list has the game's eight most important actions and maps all eight buttons of MiSTer's default pad (`jn`), so every button does something and the layout shows up in "Define buttons". The last action is the one that opens the game's menu, on Start.
- In menus the d-pad and left stick move, and confirm/back are *Menu OK* / *Menu Back* from the OSD: by default the same buttons as in the MiSTer menu, whatever they do in the game. `MH_MenuButtons()` resolves them (the SDL driver delivers them as buttons 28 and 29).
- Nothing requires a keyboard: lists, confirmations and name entry work with the pad.

**Behaviour**

- Loading the core starts the game; there is no file to pick in the OSD.
- Until the game shows its first frame the core shows colour bars, so a core without its game is recognisable.
- A game with several data sets (or nothing but a choice to make before it starts) asks with `MH_UI_Menu()`. Problems the player can fix (missing data) are shown with `MH_UI_Message()` in plain words that say which files go where, never left in the log only.
- Quitting from the game's menu returns to the MiSTer menu (or to the game list if the player came from it). The launcher loads the menu core only if `/tmp/CORENAME` still names this core.
- The game leaves when another core is loaded (`MH_CheckAlive()`, `SDL_QUIT`) and never touches the shared memory afterwards.
- The launcher runs the game with `taskset 0x03`; the game thread takes CPU0, audio and the frame copy run on CPU1.
- `touch /tmp/<name>_nolaunch` keeps the core loaded without starting the game, for development.

**Documentation**

- `README.md` in the repository and `README.txt` in the package have the same sections in the same order: Requirements, Installation, OSD options, Controls (keyboard, then a table of the default gamepad mapping with the MiSTer, Xbox and PlayStation names), Building, Credits, License.
