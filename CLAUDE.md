# ECWolf hybrid core for MiSTer (Wolfenstein 3D)

@hybrid/CLAUDE.md

## This core

- Name everywhere: `ECWolf` (core, folders, release), `ecwolf` (game submodule, binary in `build/`, log, lock).
- The game is the `ecwolf/` submodule (`ItsDanik/ecwolf`, branch `mister`). Its MiSTer code is `src/mister/` and `#ifdef MISTER_HYBRID` elsewhere.
- Build: `./build.sh` (game), `./core/build_core.sh` (FPGA), `./package.sh` (zip in `dist/`), `MISTER=root@<address> tools/deploy.sh`.
- Release: the zip in `releases/`. Players get it once it is published to the database, only when Danik asks: `hybrid/launcher/distribution.py publish ~/dnk_Distribution_MiSTer releases/ECWolf_YYYYMMDD.zip` (the rule in `hybrid/CLAUDE.md`).
- Try a build on the MiSTer without installing it: `MISTER=root@<address> tools/test_on_mister.sh WL|SOD|SD2|SD3|list|nodata|log|stop` (the rule in `hybrid/CLAUDE.md`).
- ECWolf is the core `hybrid/template/` was cut from. A change here to a file the template also has (`core/` except the `CONF_STR`, `package.sh`, `tools/deploy.sh`, the launch script, the README sections) goes into the template too.

## Beyond the base

These are ECWolf's own features, not something other cores start with:

- 640x200 video mode next to 320x200, and 320x240 and 640x240, with the OSD option *Resolution*. The 200-line modes stay (Danik, 2026-10-16: "Some games draw in 200 lines as well! ECWolf should offer all four resolutions").
- Frame interpolation between the game's 70Hz tics (README, "Frame pacing").
- Solid colour floors and ceilings filled row by row.
- Cheats page of the OSD (status bit 35 God Mode, triggers T[56] give weapons/ammo/keys and T[57] next level): `MiSTerCheats()` in `src/wl_debug.cpp`, once a frame while the game is played.
- AdLib music and sound effects on the core's FM chip (`MiSTer_FM*` in `src/mister/mister.cpp`, hooks in `id_sd.cpp`), with the OSD option *FM Music* (status bit 34) to fall back to DBOPL on the ARM. The game's volume settings become operator levels there.

## Games

- One core, a menu entry per game (`package/games.txt`: WL, SOD, SD2, SD3). `MiSTer_PickIWad()` matches the games ECWolf finds by their iwadinfo `Autoname` (`Wolf.Wolf3D.Registered` WL, `.Shareware` WL1 shareware of WL, `Wolf.Spear.Commercial` SOD, `.Demo` SDM demo of SOD, `.Danger` SD2, `.Challenge` SD3) and hands `MH_PickGame()` a table of those, plus one entry with a file pattern and message for each menu entry's game that was not found. The 3-episode Wolfenstein (`*.WL3`) is `Wolf.Wolf3D.Registered` as well.
- Not in the catalogue, no menu entry (Danik, 2026-10-16): Super 3-D Noah's Ark, the Mac Wolfenstein encounters, Rise of the Triad, Planet Strike. A game ECWolf finds beyond the four (Noah's Ark, `*.N3D`) is still in the list of the core started by itself, under ECWolf's name for it.
- dnkLauncher screenshots, `package/screens/<id>.png` (2026-10-25): taken on the PC with `fakecore` at 320x200, `MISTER_HYBRID_FPS=0`, `MISTER_HYBRID_SET=ECWolf_<id>` and `--nowait --tedlevel MAPnn` (`--normal` for WL and SD2, `--hard` for SOD and SD3, whose maps have their guards near the start only at that skill), a scripted walk or turn with a `shot` every few fields (keys are ignored until *Get Psyched!* is over, about field 230), the PPM made a PNG and nothing else. WL: E1M3 (MAP03), firing at a guard in the grey hall; SOD: Tunnels 1, two guards among the pillars behind the start; SD2: Tunnels 1, a dog in the blue cave; SD3: Tunnels 3, an SS in the hall past the first door. Not E1M1, the README's.

## 240 lines

- The 200 virtual lines of the 2D graphics are centred (`VirtualToRealCoords()` in `src/v_draw.cpp` with `MiSTer_VirtualTop()`), the status bar's (`SbarGraphic()` in `src/g_wolf/wolf_sbar.cpp`, which sets `MiSTer_HudDrawing`) the *HUD (Bottom)* margin above the bottom edge. Menus already used whole-number factors and are unchanged.
- `SetViewSize()` (`src/wl_main.cpp`): `statusbary2` is where the bar starts; view size 20 gives the view all lines, smaller sizes fit above the bar. `R_RenderView()` (`src/wl_draw.cpp`) draws the bar over every frame where it overlaps the view; `R_DrawPlayerSprite()` (`src/r_sprites.cpp`) puts the weapon on top of the bar.
- The margin is read by `MiSTer_UpdateResolution()` every frame (play loop, menus, title pages); a change makes the layout again (`NewViewSize()`) and the screen is drawn again. `MH_SetMenuMask(1)` greys both HUD lines in 200-line modes (`D0`); *HUD (Top)* is always greyed out (`HUD_EDGES` 2'b10, `D7`), Wolfenstein has nothing at the top. Both margins are 0 by default since host version 16 (entry n is n pixels).

## Host version 17 (2026-10-23)

- Built against Hybrid_MiSTer 1c48a06 (the pointer 1aa15c4: only documents changed after it): each game's logo at its start (`package/logos/<id>.png`, Danik's, 440x200), *Save Diagnostic Report* and the info texts, the HUD lines of host version 16, `MIDI_SYNTH` 0 (Danik, 2026-10-23: the games' music is AdLib, no synthesizer, no SoundFont in the zip; no *Music* / *SoundFont* / MT32-pi lines).
- `MiSTer_PickIWad()` gives each game ECWolf found its files in the game's folder (`./VSWAP.WL6` and the rest, `InGameFolder()`), so `MH_PickGame()` shows "Checking Game Data...", "Game Data OK" and "Loading <title>..." under the logo; a game found only in `$PROGDIR` has none to look for and gets "Loading <title>..." alone. It no longer shows "Loading <title>..." itself. A game beyond the four (Noah's Ark) has no logo: "Loading <title>..." under the MiSTer logo.
- Core: 31% of the logic, 25% of the RAM blocks, 71% of the DSP blocks, worst slack +0.223 ns. Tried on the MiSTer with `tools/test_on_mister.sh` (2026-10-23): WL, SOD, SD2, SD3 each with its logo and the three lines; `list` (under the MiSTer logo), Wolfenstein 3D picked and quit, the list back; `nodata`; SD3's menu entry without data (`TEST_ENV="MISTER_HYBRID_SET=ECWolf_SD3"` with `nodata`: its logo, "Checking Game Data...", the message); `TEST_USB=1` SOD and `nodata`. *Save Diagnostic Report* not yet tried on the MiSTer: `hybrid/tools/uinput_kbd.py`'s F12 did not open the OSD.

## Host version 18 (2026-10-28)

- Built against Hybrid_MiSTer 96e3fde (the pointer 1498830: only documents and the template's `mister_port.c`, which ECWolf does not use, changed after it): the core's own picture (purple screen, logo, fades) moves at every field of 480i; the game's reset, the USER button or the OSD's *Reset* (`"R0,Reset;"` before *Save Diagnostic Report*): the library ends the game with exit code 44 and the launch script (the template's loop, `MISTER_HYBRID_RESET`, `mh_message --wait-reset`) starts the same game again. `FLICKER` 0: ECWolf's modes are 320 and 640 x 200 and 240, none interlaced, so the *Flicker Filter* line is hidden; `INTRO` 1. README: the *HDMI Only* row says the core's own screen is progressive with it too, and the *Reset* row. `core/build_core.sh` takes a Quartus slot (the template's).
- Core: 31% of the logic, 25% of the RAM blocks, 71% of the DSP blocks, worst slack +0.226 ns. Checked on the PC with fakecore: WL from its set name to its first frame (logo and lines), the script's `reset` ends it with exit code 44, and started again with `MISTER_HYBRID_RESET=1` (the pick file naming WL) WL comes again without the list. Not yet tried on the MiSTer.

## Frame pacing (host version 13, measured 2026-10-16)

- ECWolf paces itself (`MiSTer_FrameTics()`, interpolation between tics) and does not draw ahead (`MH_PresentAtField()` is not used). Measured on the MiSTer with `MISTER_HYBRID_PACE=1`, in play on E1M1, turning: 320x240 at most 8.3ms of the 16.8ms field, 640x240 at most 12.6ms; after the level start 300 of 300 frames reach the core in the next field (a few frames at the start of a level). With no frames missed or shown twice there is nothing for drawing ahead to take down, so it stays off, for the lower latency.
- In the menus about 50 of 300 frames reach the core in the same field as the one before (the menu code draws without waiting for a field); the menus are still pictures, it does not show.
