ECWolf for MiSTer - Wolfenstein 3D and Spear of Destiny as a hybrid port
========================================================================

The game (ECWolf, a source port of the Wolfenstein 3D engine) runs on the
MiSTer's ARM CPU; the ECWolf FPGA core provides native 15kHz video (CRT, VGA
and HDMI) at 320x200, 640x200, 320x240 or 640x240, audio and input.

Disclaimer: AI is being used to speed up development of this project.

Requirements
  - danik_hybrid_ports, the launcher that comes with this release
    (Scripts/danik_hybrid_ports.sh): run it ONCE from the MiSTer's Scripts
    menu. It starts the game whenever its menu entry is loaded, keeps
    running after a reboot, and serves all our hybrid ports; its screen
    picks which ports and games to have, and can keep them up to date
    through update_all. It replaces danik_hybrid_cores, the launcher before
    it, and moves ports installed the old way (games/ECWolf/) into the new
    folders by itself. Without it the core says "Game not running".
  - Game data. It is not included: your own copy of Wolfenstein 3D or
    Spear of Destiny (with its mission packs), or the shareware episode or
    demo.

Install
  With update_all (Downloader 2.4 or newer): copy
  downloader_danik_hybrid_ports.ini to /media/fat/, run update_all, run
  danik_hybrid_ports from the Scripts menu, tick the games, X: Apply.
  By hand:
  1. Extract the zip to the root of the SD card (/media/fat): _Other/,
     games/dnk/ECWolf/, Scripts/ and downloader_danik_hybrid_ports.ini
     (with which update_all keeps the ports up to date from then on).
  2. Copy the data files of your games to /media/fat/games/dnk/ECWolf/
       Wolfenstein 3D              *.WL6 (or *.WL3, episodes 1 to 3)
       Wolfenstein 3D shareware    *.WL1
       Spear of Destiny            *.SOD
       SoD: Return to Danger       *.SD2, next to the *.SOD files
       SoD: Ultimate Challenge     *.SD3, next to the *.SOD files
       Spear of Destiny demo       *.SDM
  3. Run danik_hybrid_ports from the Scripts menu, if you have not done so
     before (see Requirements).
  4. Load the game from the Other menu, e.g. dnk-Wolfenstein 3D.

The menu entry of Wolfenstein 3D starts the shareware when the full game is
not there, that of Spear of Destiny the demo. When a game's files are
missing, the core says which files to copy where. Each game keeps its own
OSD settings and button mappings. The core file itself
(_Other/dnk/dnk-ECWolf_*.rbf, or from Recent) lists the games that are
there to pick from; quitting a game then returns to the list.

OSD options
  Aspect ratio, Scale, Scandoubler Fx, Stereo Mix as in other cores.
  FPS Counter   shows the pictures per second in the top left corner.
  HDMI Only     as in the other hybrid ports, for resolutions of 640x400
                and more: ECWolf has none, so nothing changes here.
  CRT Options   for a 15kHz screen: Horizontal Size, Horizontal Pos and
                Vertical Pos fit the picture to it. The pixels stay as they
                are; HDMI is not affected. The size is for the game's
                picture: the core's own, with its messages, keeps its
                width. Not there with forced_scandoubler=1. HUD (Bottom):
                in the 240-line resolutions, how many lines the status bar
                sits above the bottom edge of the screen, 0 to 24 (10 by
                default), so that a CRT's overscan does not hide it; greyed
                out at 200 lines.
  Resolution    what the game renders and the core puts out, always at
                15kHz: 320x200 (default, as the original), 640x200, the
                same 200 lines with twice the detail across: every pixel
                of the original becomes two, so the picture has the same
                size and shape on the screen, and 320x240 and 640x240,
                the same with 240 lines: the game's view fills all of them,
                from the top edge to the bottom one, with the status bar
                drawn over it at the HUD (Bottom) margin and the weapon on
                top of the bar. The menus, title screens and the
                intermission keep their 200 lines in the middle of the
                screen, not stretched. Applies at once in the game and its
                menus, on the title screens with the next page.
  FM Music      where the AdLib music and sound effects are made: FPGA
                (default), on a YM3812 chip in the core, or Software, on
                the ARM. Applies at once. With an older core file the game
                uses Software by itself.
  Mouse Sensitivity
                how fast the mouse turns, 25% to 400%.
  Stick Sensitivity
                how fast the gamepad's stick turns, 25% to 300%. Both
                multiply the sensitivity set in the game's own menu and
                apply at once.
  Cheats        God Mode: nothing hurts you while it is on. Give
                Weapons/Ammo/Keys: every weapon, full ammo and both keys,
                once each time it is selected. Next Level: the level counts
                as completed, as if you had reached the exit.
  Menu OK, Menu Back
                the gamepad button that confirms / goes back in the game's
                menus, whatever it does in the game. MiSTer (default) uses
                the OK/Back buttons of your MiSTer menu.

Frame pacing
  Wolfenstein 3D moves 70 times per second, the refresh rate of the VGA
  mode it was written for; the core shows 59.64 fields per second, as a
  15kHz screen needs. To keep movement even, the game draws exactly one
  frame per field and places the player, enemies, doors and pushwalls
  between two steps of the game for it. The game itself runs at its
  original speed. It holds the full rate of one frame per field ("60fps")
  at every resolution.

Controls
  Keyboard and mouse:
    W, S (or up, down)                  Walk forward, back
    A, D                                Strafe left, right
    Mouse left/right (or left, right)   Turn
    Left mouse button (or Ctrl)         Fire
    Space                               Open doors, push walls
    Left Shift                          Run
    R                                   Next weapon (1-4 select one)
    Tab                                 Map
    Esc                                 Menu
  Gamepad (default mapping):
    Left stick                          Walk and strafe
    Right stick left/right              Turn
    D-pad                               Walk and turn
    R (RB / R1)                         Fire
    B (Xbox A / PlayStation Cross)      Open doors, push walls
    A (Xbox B / PlayStation Circle)     Run
    Y (Xbox X / PlayStation Square)     Next weapon
    X (Xbox Y / PlayStation Triangle)   Previous weapon
    L (LB / L1)                         Strafe: hold to sidestep with the
                                        d-pad
    Select                              Map
    Start                               Menu
  The game's own Control menu changes keys, mouse and stick assignments;
  they are saved in ecwolf.cfg. A release that changes the defaults above
  replaces saved controls with the new defaults once.
  Change the buttons in the OSD under "Define ECWolf buttons". "Menu OK" and
  "Menu Back" there are only needed for a button without a game function;
  MiSTer doesn't let you assign a button twice, so to confirm with a button
  that also fires, pick it in the OSD's Menu OK/Menu Back options instead.
  Naming a saved game without a keyboard: up/down changes the letter,
  left/right moves the cursor, Menu OK confirms.

Files
  ecwolf.cfg    the game's settings, written on exit
  saves/        saved games
  /media/fat/logs/dnk/ECWolf/ecwolf.log   the game's log

Credits
  ECWolf by Braden "Blzut3" Obrzut and contributors, based on Wolf4SDL and
  the Wolfenstein 3D source by id Software.
  SDL by Sam Lantinga and contributors.
  MiSTer by Sorgelig and the MiSTer-devel contributors.
  MiSTer Frontier by MiSTer Organize
  (https://github.com/MiSTerOrganize/MiSTer_Frontier): thank you for the
  inspiration. Hybrid cores on the MiSTer, and the way their game is
  launched, come from MiSTer Frontier. Our launcher is a separate
  implementation and does not need MiSTer Frontier installed.

License
  ECWolf: GPL-2.0-or-later (LICENSE-ecwolf.txt, LICENSE-gpl.txt)
  SDL: zlib (LICENSE-sdl.txt)
  The launcher scripts: GPL-3.0 (LICENSE-gpl3.txt)
  The FPGA core: GPL-2.0
  Source: https://github.com/ItsDanik/ecwolf_MiSTer
          https://github.com/ItsDanik/Hybrid_MiSTer (launcher, framework)
