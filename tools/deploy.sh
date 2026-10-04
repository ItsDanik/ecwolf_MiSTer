#!/bin/sh
# Package the build and copy it to a MiSTer over ssh.
# usage: MISTER=root@<address> tools/deploy.sh
# The core is not (re)loaded: load ECWolf from the menu afterwards. Run
# /media/fat/Scripts/danik_hybrid_cores.sh on the MiSTer once to set up the launcher.
set -e
cd "$(dirname "$0")/.."
HOST=${MISTER:?set MISTER=root@<address of the MiSTer>}
./package.sh > /dev/null
OUT=dist/ECWolf_$(date +%Y%m%d)
ssh "$HOST" "mkdir -p /media/fat/games/ECWolf && rm -f /media/fat/_Other/ECWolf_*.rbf"
# a running game keeps its binary busy: copy next to it, then swap
scp -q "$OUT/games/ECWolf/ECWolf" "$HOST:/media/fat/games/ECWolf/ECWolf.new"
ssh "$HOST" "mv -f /media/fat/games/ECWolf/ECWolf.new /media/fat/games/ECWolf/ECWolf"
scp -q "$OUT"/games/ECWolf/ecwolf.pk3 "$OUT"/games/ECWolf/*.sh "$OUT"/games/ECWolf/*.txt "$HOST:/media/fat/games/ECWolf/"
scp -q "$OUT/Scripts/danik_hybrid_cores.sh" "$HOST:/media/fat/Scripts/"
scp -q "$OUT"/_Other/*.rbf "$HOST:/media/fat/_Other/"
ssh "$HOST" "chmod +x /media/fat/games/ECWolf/ECWolf /media/fat/games/ECWolf/*.sh /media/fat/Scripts/danik_hybrid_cores.sh; ls -la /media/fat/games/ECWolf /media/fat/_Other/ECWolf_*.rbf"
