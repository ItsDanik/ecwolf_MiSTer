ECWolf for MiSTer
=================

Wolfenstein 3D and Spear of Destiny on MiSTer FPGA as a hybrid port.

Everything about it (options, controls, help):
  https://github.com/ItsDanik/ecwolf_MiSTer

Game files
  Copy everything in your games' folders into this folder,
  /media/fat/games/dnk/ECWolf/, all the files as they are:
    Wolfenstein 3D              *.WL6 (or *.WL3)
    Wolfenstein 3D shareware    *.WL1
    Spear of Destiny            *.SOD
    SoD: Return to Danger       *.SD2, next to the *.SOD files
    SoD: Ultimate Challenge     *.SD3, next to the *.SOD files
    Spear of Destiny demo       *.SDM
  They can go on a USB drive instead, in /media/usb0/games/dnk/ECWolf/.

Playing
  1. Run danik_hybrid_ports once from the MiSTer's Scripts menu. It starts
     the game whenever its menu entry is loaded.
  2. Load the game from the Other menu, e.g. dnk-Wolfenstein 3D.
  Buttons are changed in the OSD under "Define ECWolf buttons". A button
  left without a name there does not reach the game at all: name every
  button, or leave the definition as it is.

If something is wrong
  Open the OSD while it happens and pick Save Diagnostic Report, the last
  line. Send the file it saves in games/dnk/diagnostics/ with a few words
  on what went wrong: https://github.com/ItsDanik/ecwolf_MiSTer/issues

Credits and licenses
  ECWolf by Braden "Blzut3" Obrzut and contributors: GPL-2.0-or-later
  (LICENSE-ecwolf.txt, LICENSE-gpl.txt). SDL by Sam Lantinga and
  contributors: zlib (LICENSE-sdl.txt). MiSTer by Sorgelig and the
  MiSTer-devel contributors. Inspired by MiSTer Frontier by MiSTer
  Organize. The launcher: GPL-3.0 (LICENSE-gpl3.txt); the FPGA core:
  GPL-2.0. Source: https://github.com/ItsDanik/ecwolf_MiSTer

Support the project: https://www.patreon.com/itsdanik
