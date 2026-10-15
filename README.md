# ECWolf for MiSTer

Wolfenstein 3D and Spear of Destiny on [MiSTer FPGA](https://github.com/MiSTer-devel/Main_MiSTer/wiki) as a hybrid port.

The game itself is [ECWolf](https://maniacsvault.net/ecwolf/), the source port of the Wolfenstein 3D engine. It runs on the MiSTer's ARM CPU. The ECWolf FPGA core provides native 15kHz video (CRT, VGA and HDMI) at 320x200, 640x200, 320x240 or 640x240, 44.1kHz audio and keyboard, mouse and gamepad input. The two halves talk through shared DDR3 memory.

> **Beta.** Expect rough edges and please report problems in the issues.

Disclaimer: AI is being used to speed up development of this project.

## Requirements

- **danik_hybrid_ports**, the launcher (`Scripts/danik_hybrid_ports.sh`): run it **once** from the MiSTer's `Scripts` menu. It starts the game whenever its menu entry is loaded, keeps running after a reboot, and serves all our hybrid ports; its screen picks which ports and games to have, and can keep them up to date through `update_all`. It replaces `danik_hybrid_cores`, the launcher before it, and moves ports installed the old way (`games/ECWolf/`) into the new folders by itself. Without it the core says "Game not running".
- **Game data**, which is not included: your own copy of Wolfenstein 3D or Spear of Destiny (with its mission packs), or the shareware episode or demo.

## Installation

With `update_all` (Downloader 2.4 or newer, which update_all keeps up to date):

1. Copy [`downloader_danik_hybrid_ports.ini`](https://raw.githubusercontent.com/ItsDanik/Hybrid_MiSTer/main/launcher/downloader_danik_hybrid_ports.ini) to the root of your SD card (`/media/fat`).
2. Run `update_all`. That installs the launcher, **danik_hybrid_ports**, in `Scripts`.
3. Run **danik_hybrid_ports** from the `Scripts` menu, tick the games you want and press *X: Apply*. They are installed, and update_all keeps them up to date.

By hand:

1. Download the newest `ECWolf_YYYYMMDD.zip` from [releases](releases/) and extract it to the root of your SD card (`/media/fat`). That gives you:
   - `_Other/dnk-Wolfenstein 3D.mgl`, `_Other/dnk-Spear of Destiny.mgl`, `_Other/dnk-SoD Return to Danger.mgl` and `_Other/dnk-SoD Ultimate Challenge.mgl`, the games' entries in the `Other` menu
   - `_Other/dnk/dnk-ECWolf_YYYYMMDD.rbf`, the FPGA core
   - `games/dnk/ECWolf/`, the game binary and its launch script
   - `Scripts/danik_hybrid_ports.sh`, the launcher (from [Hybrid_MiSTer](https://github.com/ItsDanik/Hybrid_MiSTer))
   - `downloader_danik_hybrid_ports.ini`, with which `update_all` keeps the ports up to date from then on
2. Copy the data files of your games to `/media/fat/games/dnk/ECWolf/`:

   | Game | Files |
   |---|---|
   | Wolfenstein 3D | `*.WL6` (or `*.WL3`, the first three episodes) |
   | Wolfenstein 3D shareware | `*.WL1` |
   | Spear of Destiny | `*.SOD` |
   | SoD: Return to Danger, SoD: Ultimate Challenge | `*.SD2`, `*.SD3`, next to those of Spear of Destiny |
   | Spear of Destiny demo | `*.SDM` |
3. Run **danik_hybrid_ports** from the `Scripts` menu, if you have not done so before (see Requirements).
4. Load the game from the `Other` menu, e.g. **dnk-Wolfenstein 3D**.

The menu entry of Wolfenstein 3D starts the shareware when the full game is not there, that of Spear of Destiny the demo. When a game's files are missing, the core says which files to copy where. Each game keeps its own OSD settings and button mappings. The core file itself (`_Other/dnk/dnk-ECWolf_*.rbf`, or from *Recent*) lists the games that are there to pick from; quitting a game then returns to the list. The game's log is in `/media/fat/logs/dnk/ECWolf/ecwolf.log`, its settings in `games/dnk/ECWolf/ecwolf.cfg`, saved games in `games/dnk/ECWolf/saves`.

## OSD options

| Option | |
|---|---|
| Aspect ratio, Scale, Scandoubler Fx, Stereo Mix | as in other cores |
| FPS Counter | shows the pictures per second in the top left corner |
| HDMI Only | as in the other hybrid ports, for resolutions of 640x400 and more: ECWolf has none, so nothing changes here |
| CRT Options | for a 15kHz screen: *Horizontal Size*, *Horizontal Pos* and *Vertical Pos* fit the picture to it. The pixels stay as they are; HDMI is not affected. The size is for the game's picture: the core's own, with its messages, keeps its width. Not there with `forced_scandoubler=1`. *HUD (Bottom)*: in the 240-line resolutions, how many lines the status bar sits above the bottom edge of the screen, 0 to 24 (10 by default), so that a CRT's overscan does not hide it; greyed out at 200 lines |
| Resolution | what the game renders and the core puts out, always at 15kHz: **320x200** (default, as the original), **640x200**, the same 200 lines with twice the detail across: every pixel of the original becomes two, so the picture has the same size and shape on the screen, and **320x240** and **640x240**, the same with 240 lines: the game's view fills all of them, from the top edge to the bottom one, with the status bar drawn over it at the *HUD (Bottom)* margin, and the weapon on top of the bar. The menus, title screens and the intermission keep their 200 lines in the middle of the screen, not stretched. Applies at once in the game and its menus, on the title screens with the next page |
| FM Music | where the AdLib music and AdLib sound effects are made: **FPGA** (default), on a YM3812 chip in the core, which the game only sends its register writes to, or **Software**, the game working the chip out on the ARM as before. Applies at once, so the two can be compared while the music plays. With an older core file the game uses Software by itself |
| Mouse Sensitivity | how fast the mouse turns, 25% to 400% |
| Stick Sensitivity | how fast the gamepad's stick turns, 25% to 300%. Both multiply the sensitivity set in the game's own menu and apply at once |
| Cheats | **God Mode**: nothing hurts you while it is on. **Give Weapons/Ammo/Keys**: every weapon, full ammo and both keys, once each time it is selected. **Next Level**: the level counts as completed, as if you had reached the exit |
| Menu OK, Menu Back | the gamepad button that confirms / goes back in the game's menus, whatever it does in the game. **MiSTer** (default) uses the OK/Back buttons of your MiSTer menu |

## Frame pacing

Wolfenstein 3D moves 70 times per second (70 "tics"), the refresh rate of the VGA mode it was written for. A 15kHz picture a TV accepts has about 60 fields per second: the core shows 59.64 (262 lines of 400 pixels at 6.25MHz). One field is therefore 1.17 tics. Drawing the state of the last tic for every field makes every sixth picture jump two tics ahead, which shows as a regular stutter; ECWolf has the same on any 60Hz monitor.

This build draws exactly one frame per field and puts each frame at the right point in time:

- The game's clock is the field counter of the core, not the system clock, so every frame is 1.17 tics after the one before.
- The game still runs whole tics, 70 per second, with the same rules and speed. The picture is drawn part of the way into the last tic: the player, the other actors, the doors and the pushwalls are put between where they were before that tic and where they are after it. What you see is one tic (14ms) behind the game.
- The mouse is the exception: it is read once per frame and what it turns the player is on the screen at once.
- The launcher runs the game at a raised priority (`nice -n -20`), so the other programs on the MiSTer cannot make it miss a field.

The game holds the full rate of one frame per field (59.64 per second, "60fps") at every resolution: a frame of 640x240, the largest, takes at most 12.6ms of the 16.8ms of a field.

## Controls

- **Keyboard and mouse:**

| Key | Action |
|---|---|
| W, S (or up, down) | Walk forward, back |
| A, D | Strafe left, right |
| Mouse left/right (or left, right) | Turn |
| Left mouse button (or Ctrl) | Fire |
| Space | Open doors, push walls |
| Left Shift | Run |
| R | Next weapon (1-4 select one) |
| Tab | Map |
| Esc | Menu |

- **Gamepad** (default mapping):

| Button | Action |
|---|---|
| Left stick | Walk and strafe |
| Right stick left/right | Turn |
| D-pad | Walk and turn |
| R (RB / R1) | Fire |
| B (Xbox A / PlayStation Cross) | Open doors, push walls |
| A (Xbox B / PlayStation Circle) | Run |
| Y (Xbox X / PlayStation Square) | Next weapon |
| X (Xbox Y / PlayStation Triangle) | Previous weapon |
| L (LB / L1) | Strafe: hold to sidestep with the d-pad |
| Select | Map |
| Start | Menu |

The game's own *Control* menu changes keys, mouse and stick assignments; they are saved in `ecwolf.cfg`. A release that changes the defaults above replaces saved controls with the new defaults once.

Change the buttons in the OSD under *Define ECWolf buttons*. *Menu OK* and *Menu Back* there are only needed for a button without a game function; MiSTer doesn't let you assign a button twice, so to confirm with a button that also fires, pick it in the OSD's Menu OK/Menu Back options instead.

Naming a saved game without a keyboard: up/down changes the letter, left/right moves the cursor, Menu OK confirms.

## Building

Everything builds in Docker on a Linux PC.

```sh
git clone --recursive https://github.com/ItsDanik/ecwolf_MiSTer.git
cd ecwolf_MiSTer
./build.sh              # ARM game binary  -> build/mister/ecwolf, ecwolf.pk3
./core/build_core.sh    # FPGA core (Quartus Lite 17.0.2) -> core/output_files/ECWolf.rbf
./package.sh            # release zip      -> dist/ECWolf_YYYYMMDD.zip
```

The toolchain image (Debian bullseye, glibc 2.31 to match the MiSTer) is built from `hybrid/toolchain/` on first use. The first build also downloads and builds SDL2, SDL2_mixer and SDL2_net.

### Repository layout

| Path | |
|---|---|
| `hybrid/` | submodule: [ItsDanik/Hybrid_MiSTer](https://github.com/ItsDanik/Hybrid_MiSTer), what all our hybrid ports share: the FPGA host module, the ARM side library, SDL2 drivers, the launcher (`danik_hybrid_ports.sh`), toolchain and conventions. See its README. |
| `core/` | FPGA core, based on [Template_MiSTer](https://github.com/MiSTer-devel/Template_MiSTer): `ECWolf.sv` (OSD and button names) around `hybrid/rtl/hybrid_host.sv` |
| `ecwolf/` | submodule: [ItsDanik/ecwolf](https://github.com/ItsDanik/ecwolf) branch `mister`, [ECWolf](https://bitbucket.org/ecwolf/ecwolf) with the MiSTer changes: `src/mister/` and a few `#ifdef MISTER_HYBRID` |
| `package/` | files shipped in the release next to the binary (`danik_hybrid_launch.sh`, README), and `games.txt`: the games and their menu entries |
| `releases/` | release packages |

### Development notes

- `./build.sh host` builds the same game for the PC. It runs against `hybrid/tools/fakecore`, a stand-in for the FPGA core that takes scripted input and saves screenshots and audio:
  ```sh
  gcc -O2 -o build/host/fakecore hybrid/tools/fakecore.c
  build/host/fakecore /tmp/shm script.txt audio.raw &
  cd <data folder> && MISTER_HYBRID_SHM=/tmp/shm ~/MiSTer-ecwolf/build/host/ecwolf
  ```
- `hybrid/sim/run.sh` runs the testbench of the FPGA host module.
- Frame pacing (see above) in the source: `MiSTer_FrameTics()` in `ecwolf/src/mister/mister.cpp` waits for the next field and turns fields into tics and a fraction (`r_ticfrac`); `CalcTics()` in `wl_play.cpp` uses it. `PlayLoop()` stores where the actors are before every tic (`R_StoreActorPositions()`); `ThreeDRefresh()` in `wl_draw.cpp` moves them to their place between the tics for the time of drawing and back. Doors and pushwalls tell the renderer how far they moved in their tic (`R_InterpolateMapValue()`, `R_InterpolatePushwall()`, called from `lnspec.cpp`). Game logic never sees the interpolated values. Without the core (headless) the original timing is used.
- Floors and ceilings of one colour all over the map, as in every level of Wolfenstein 3D, are filled row by row instead of textured pixel by pixel (`SolidPlaneColor()` in `wl_floorceiling.cpp`, `GameMap::GetUniformFlats()`). The picture is the same; maps with textured or mixed floors take the original path.
- `MISTER=root@<address> tools/test_on_mister.sh WL` tries the build on a MiSTer next to what is installed there, without installing it: from `/tmp`, with a copy of the installed game's folder and its own OSD settings. `SOD`, `SD2`, `SD3` for the other games, `list` and `nodata` for the core started by itself and without data, `log` for the game's log, `stop` to remove all of it again.
- `touch /tmp/ecwolf_nolaunch` on the MiSTer keeps the core loaded without starting the game, so you can start a development binary by hand (set `MISTER_HYBRID_CORE=ECWolf`, and `MISTER_HYBRID_SET` for a game's set name, e.g. `ECWolf_SOD`). `/tmp/danik_hybrid_ports.log` shows what the launcher daemon did.
- The games: `MiSTer_PickIWad()` in `ecwolf/src/mister/mister.cpp` matches the games ECWolf found (by their `Autoname` in `iwadinfo.txt`) with the menu entries of `package/games.txt` and lets `MH_PickGame()` choose. 240 lines: `MiSTer_VirtualTop()` and `VirtualToRealCoords()` in `v_draw.cpp` put the 200 virtual lines in the middle and the status bar (`SbarGraphic()` in `g_wolf/wolf_sbar.cpp`) at the HUD margin; `SetViewSize()` in `wl_main.cpp` gives the view all lines; `R_RenderView()` draws the bar over every frame. Cheats: `MiSTerCheats()` in `wl_debug.cpp`.

## Support

If you enjoy this project, you can support my work on [Patreon](https://www.patreon.com/itsdanik).

## Credits

- **[ECWolf](https://maniacsvault.net/ecwolf/)** by Braden "Blzut3" Obrzut and contributors, based on Wolf4SDL and the Wolfenstein 3D source by id Software.
- **[SDL](https://libsdl.org)** by Sam Lantinga and contributors.
- **[MiSTer](https://github.com/MiSTer-devel)** by Sorgelig and the MiSTer-devel contributors: the framework and Template_MiSTer.
- **[MiSTer Frontier](https://github.com/MiSTerOrganize/MiSTer_Frontier)** by MiSTer Organize: thank you for the inspiration. Hybrid cores on the MiSTer, and the way their game is launched (a daemon that watches the loaded core and runs a script from its games folder), come from MiSTer Frontier. Our launcher is a separate implementation and does not need MiSTer Frontier installed.

This project and its maintainers are in no way associated with or endorsed by id Software, Apogee or FormGen. It does not include any game data.

## License

The top-level scripts, tools and documentation are licensed under the [GPL-3.0](LICENSE). The components keep their own licenses: the shared framework in `hybrid/` is GPL-3.0 except where its files say otherwise, ECWolf is GPL-2.0-or-later as built here (`ecwolf/docs/copyright`), SDL and the SDL drivers in `hybrid/sdl2` are zlib, the FPGA core and the MiSTer framework are GPL-2.0 (`core/LICENSE`, with the core's own sources GPL-2.0-or-later).
