# ECWolf hybrid core for MiSTer (Wolfenstein 3D)

@hybrid/CLAUDE.md

## This core

- Name everywhere: `ECWolf` (core, folders, release), `ecwolf` (game submodule, binary in `build/`, log, lock).
- The game is the `ecwolf/` submodule (`ItsDanik/ecwolf`, branch `mister`). Its MiSTer code is `src/mister/` and `#ifdef MISTER_HYBRID` elsewhere.
- Build: `./build.sh` (game), `./core/build_core.sh` (FPGA), `./package.sh` (zip in `dist/`), `MISTER=root@<address> tools/deploy.sh`.
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

## 240 lines

- The 200 virtual lines of the 2D graphics are centred (`VirtualToRealCoords()` in `src/v_draw.cpp` with `MiSTer_VirtualTop()`), the status bar's (`SbarGraphic()` in `src/g_wolf/wolf_sbar.cpp`, which sets `MiSTer_HudDrawing`) the *HUD (Bottom)* margin above the bottom edge. Menus already used whole-number factors and are unchanged.
- `SetViewSize()` (`src/wl_main.cpp`): `statusbary2` is where the bar starts; view size 20 gives the view all lines, smaller sizes fit above the bar. `R_RenderView()` (`src/wl_draw.cpp`) draws the bar over every frame where it overlaps the view; `R_DrawPlayerSprite()` (`src/r_sprites.cpp`) puts the weapon on top of the bar.
- The margin is read by `MiSTer_UpdateResolution()` every frame (play loop, menus, title pages); a change makes the layout again (`NewViewSize()`) and the screen is drawn again. `MH_SetMenuMask(1)` greys the HUD line in 200-line modes.

## Frame pacing (host version 13, measured 2026-10-16)

- ECWolf paces itself (`MiSTer_FrameTics()`, interpolation between tics) and does not draw ahead (`MH_PresentAtField()` is not used). Measured on the MiSTer with `MISTER_HYBRID_PACE=1`, in play on E1M1, turning: 320x240 at most 8.3ms of the 16.8ms field, 640x240 at most 12.6ms; after the level start 300 of 300 frames reach the core in the next field (a few frames at the start of a level). With no frames missed or shown twice there is nothing for drawing ahead to take down, so it stays off, for the lower latency.
- In the menus about 50 of 300 frames reach the core in the same field as the one before (the menu code draws without waiting for a field); the menus are still pictures, it does not show.
