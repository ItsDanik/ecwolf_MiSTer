# ECWolf for MiSTer: notes

How the port works and how it is built. For playing it, see the [README](README.md).

## Frame pacing

Wolfenstein 3D moves 70 times per second (70 "tics"), the refresh rate of the VGA mode it was written for. A 15kHz picture a TV accepts has about 60 fields per second: the core shows 59.64 (262 lines of 400 pixels at 6.25MHz). One field is therefore 1.17 tics. Drawing the state of the last tic for every field makes every sixth picture jump two tics ahead, which shows as a regular stutter; ECWolf has the same on any 60Hz monitor.

This build draws exactly one frame per field and puts each frame at the right point in time:

- The game's clock is the field counter of the core, not the system clock, so every frame is 1.17 tics after the one before.
- The game still runs whole tics, 70 per second, with the same rules and speed. The picture is drawn part of the way into the last tic: the player, the other actors, the doors and the pushwalls are put between where they were before that tic and where they are after it. What you see is one tic (14ms) behind the game.
- The mouse is the exception: it is read once per frame and what it turns the player is on the screen at once.
- The launcher runs the game at a raised priority (`nice -n -20`), so the other programs on the MiSTer cannot make it miss a field.

The game holds the full rate of one frame per field (59.64 per second, "60fps") at every resolution: a frame of 640x240, the largest, takes at most 12.6ms of the 16.8ms of a field.

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
- `MISTER=root@<address> tools/test_on_mister.sh WL` tries the build on a MiSTer next to what is installed there, without installing it: from `/tmp`, with the installed game's folder mirrored there (what the game writes stays in `/tmp`) and its own OSD settings. `SOD`, `SD2`, `SD3` for the other games, `list` and `nodata` for the core started by itself and without data, `log` for the game's log, `stop` to remove all of it again.
- `touch /tmp/ecwolf_nolaunch` on the MiSTer keeps the core loaded without starting the game, so you can start a development binary by hand (set `MISTER_HYBRID_CORE=ECWolf`, and `MISTER_HYBRID_SET` for a game's set name, e.g. `ECWolf_SOD`). `/tmp/danik_hybrid_ports.log` shows what the launcher daemon did.
- The games: `MiSTer_PickIWad()` in `ecwolf/src/mister/mister.cpp` matches the games ECWolf found (by their `Autoname` in `iwadinfo.txt`) with the menu entries of `package/games.txt` and lets `MH_PickGame()` choose. 240 lines: `MiSTer_VirtualTop()` and `VirtualToRealCoords()` in `v_draw.cpp` put the 200 virtual lines in the middle and the status bar (`SbarGraphic()` in `g_wolf/wolf_sbar.cpp`) at the HUD margin; `SetViewSize()` in `wl_main.cpp` gives the view all lines; `R_RenderView()` draws the bar over every frame. Cheats: `MiSTerCheats()` in `wl_debug.cpp`.
