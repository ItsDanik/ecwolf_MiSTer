ECWolf for MiSTer - Wolfenstein 3D as a hybrid core
====================================================

The game (ECWolf, a source port of the Wolfenstein 3D engine) runs on the
MiSTer's ARM CPU; the ECWolf FPGA core provides native 320x200 15kHz video
(CRT, VGA and HDMI), audio and input.

Requirements
  - danik_hybrid_cores, the launcher that comes with this release
    (Scripts/danik_hybrid_cores.sh): copy it to /media/fat/Scripts/ and run
    it ONCE from the MiSTer's Scripts menu. It starts the game whenever
    the core is loaded, keeps running after a reboot, and serves all our
    hybrid cores. Without it the core only shows colour bars.
  - Game data. It is not included: use the shareware episode or your own
    copy of Wolfenstein 3D, Spear of Destiny or Super 3-D Noah's Ark.

Install
  1. Copy _Other/ECWolf_*.rbf to /media/fat/_Other/
  2. Copy games/ECWolf/ to /media/fat/games/ECWolf/
  3. Copy the data files of your games to /media/fat/games/ECWolf/
       Wolfenstein 3D              *.WL6
       Wolfenstein 3D shareware    *.WL1
       Spear of Destiny            *.SOD (mission packs: *.SD2, *.SD3)
       Spear of Destiny demo       *.SDM
       Super 3-D Noah's Ark        *.N3D
  4. Run danik_hybrid_cores from the Scripts menu, if you have not done so
     before (see Requirements).
  5. Load ECWolf from the Other menu.

With more than one game installed a list comes up to pick from; quitting a
game returns to it. With one game, quitting returns to the MiSTer menu.

OSD options
  Aspect ratio, Scale, Scandoubler Fx, Stereo Mix as in other cores.
  Mouse Sensitivity
                how fast the mouse turns, 25% to 400%.
  Stick Sensitivity
                how fast the gamepad's stick turns, 25% to 300%. Both
                multiply the sensitivity set in the game's own menu and
                apply at once.
  Menu OK, Menu Back
                the gamepad button that confirms / goes back in the game's
                menus, whatever it does in the game. MiSTer (default) uses
                the OK/Back buttons of your MiSTer menu.

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
  /media/fat/logs/ECWolf/ecwolf.log   the game's log
