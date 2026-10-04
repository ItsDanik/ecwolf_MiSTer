# ECWolf hybrid core for MiSTer (Wolfenstein 3D)

@hybrid/CLAUDE.md

## This core

- Name everywhere: `ECWolf` (core, folders, release), `ecwolf` (game submodule, binary in `build/`, log, lock).
- The game is the `ecwolf/` submodule (`ItsDanik/ecwolf`, branch `mister`). Its MiSTer code is `src/mister/` and `#ifdef MISTER_HYBRID` elsewhere.
- Build: `./build.sh` (game), `./core/build_core.sh` (FPGA), `./package.sh` (zip in `dist/`), `MISTER=root@<address> tools/deploy.sh`.
- ECWolf is the core `hybrid/template/` was cut from. A change here to a file the template also has (`core/` except the `CONF_STR`, `package.sh`, `tools/deploy.sh`, the launch script, the README sections) goes into the template too.

## Beyond the base

These are ECWolf's own features, not something other cores start with:

- 640x200 video mode with the OSD option *Resolution*.
- Frame interpolation between the game's 70Hz tics (README, "Frame pacing").
- Solid colour floors and ceilings filled row by row.
