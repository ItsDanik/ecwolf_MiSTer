# ECWolf for MiSTer

Wolfenstein 3D on [MiSTer FPGA](https://github.com/MiSTer-devel/Main_MiSTer/wiki) as a hybrid core.

The game itself is [ECWolf](https://maniacsvault.net/ecwolf/), the source port of the Wolfenstein 3D engine. It runs on the MiSTer's ARM CPU. The ECWolf FPGA core provides native 320x200 15kHz video (CRT, VGA and HDMI), 44.1kHz audio and keyboard, mouse and gamepad input. The two halves talk through shared DDR3 memory.

> **Not released yet.** First development version.

## Requirements

- **danik_hybrid_cores**, the launcher that comes in the release zip (`Scripts/danik_hybrid_cores.sh`): run it **once** from the MiSTer's `Scripts` menu. It starts the game whenever the core is loaded, keeps running after a reboot, and serves all our hybrid cores. Without it the core only shows colour bars.
- **Game data**, which is not included: the shareware episode or your own copy of Wolfenstein 3D, Spear of Destiny or Super 3-D Noah's Ark.

## Installation

1. Extract `ECWolf_YYYYMMDD.zip` to the root of your SD card (`/media/fat`). That gives you:
   - `_Other/ECWolf_YYYYMMDD.rbf`, the FPGA core
   - `games/ECWolf/`, the game binary and its launcher
   - `Scripts/danik_hybrid_cores.sh`, the launcher
2. Copy the data files of your games to `/media/fat/games/ECWolf/`:

   | Game | Files |
   |---|---|
   | Wolfenstein 3D | `*.WL6` |
   | Wolfenstein 3D shareware | `*.WL1` |
   | Spear of Destiny | `*.SOD` (mission packs: `*.SD2`, `*.SD3`) |
   | Spear of Destiny demo | `*.SDM` |
   | Super 3-D Noah's Ark | `*.N3D` |
3. Run **danik_hybrid_cores** from the `Scripts` menu, if you have not done so before (see Requirements).
4. Load **ECWolf** from the `Other` menu.

With more than one game installed a list comes up to pick from; quitting a game returns to it. With one game, quitting returns to the MiSTer menu. The game's log is in `/media/fat/logs/ECWolf/ecwolf.log`, its settings in `games/ECWolf/ecwolf.cfg`, saved games in `games/ECWolf/saves`.

## OSD options

| Option | |
|---|---|
| Aspect ratio, Scale, Scandoubler Fx, Stereo Mix | as in other cores |
| Mouse Sensitivity | how fast the mouse turns, 25% to 400% |
| Stick Sensitivity | how fast the gamepad's stick turns, 25% to 300%. Both multiply the sensitivity set in the game's own menu and apply at once |
| Menu OK, Menu Back | the gamepad button that confirms / goes back in the game's menus, whatever it does in the game. **MiSTer** (default) uses the OK/Back buttons of your MiSTer menu |

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
| `hybrid/` | What all our hybrid cores share: the FPGA host module, the ARM side library, SDL2 drivers, toolchain and conventions. See [hybrid/README.md](hybrid/README.md). |
| `core/` | FPGA core, based on [Template_MiSTer](https://github.com/MiSTer-devel/Template_MiSTer): `ECWolf.sv` (OSD and button names) around `hybrid/rtl/hybrid_host.sv` |
| `ecwolf/` | submodule: [ItsDanik/ecwolf](https://github.com/ItsDanik/ecwolf) branch `mister`, [ECWolf](https://bitbucket.org/ecwolf/ecwolf) with the MiSTer changes: `src/mister/` and a few `#ifdef MISTER_HYBRID` |
| `package/` | files shipped in the release next to the binary (`danik_hybrid_launch.sh`, README) |

### Development notes

- `./build.sh host` builds the same game for the PC. It runs against `hybrid/tools/fakecore`, a stand-in for the FPGA core that takes scripted input and saves screenshots and audio:
  ```sh
  gcc -O2 -o build/host/fakecore hybrid/tools/fakecore.c
  build/host/fakecore /tmp/shm script.txt audio.raw &
  cd <data folder> && MISTER_HYBRID_SHM=/tmp/shm ~/MiSTer-ecwolf/build/host/ecwolf
  ```
- `hybrid/sim/run.sh` runs the testbench of the FPGA host module.
- `touch /tmp/ecwolf_nolaunch` on the MiSTer keeps the core loaded without starting the game, so you can start a development binary by hand (set `MISTER_HYBRID_CORE=ECWolf`). `/tmp/danik_hybrid_cores.log` shows what the launcher daemon did.

## Credits

- **[ECWolf](https://maniacsvault.net/ecwolf/)** by Braden "Blzut3" Obrzut and contributors, based on Wolf4SDL and the Wolfenstein 3D source by id Software.
- **[SDL](https://libsdl.org)** by Sam Lantinga and contributors.
- **[MiSTer](https://github.com/MiSTer-devel)** by Sorgelig and the MiSTer-devel contributors: the framework and Template_MiSTer.

This project and its maintainers are in no way associated with or endorsed by id Software, Apogee, FormGen or Wisdom Tree. It does not include any game data.

## License

The top-level scripts, tools and documentation are licensed under the [GPL-3.0](LICENSE). The components keep their own licenses: ECWolf is GPL-2.0-or-later as built here (`ecwolf/docs/copyright`), SDL and the SDL drivers in `hybrid/sdl2` are zlib, the FPGA core and the MiSTer framework are GPL-2.0 (`core/LICENSE`, with the core's own sources GPL-2.0-or-later).
