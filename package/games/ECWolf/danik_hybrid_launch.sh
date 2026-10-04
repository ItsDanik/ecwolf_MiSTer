#!/bin/bash
#
# ECWolf (Wolfenstein 3D) launcher. danik_hybrid_cores (Scripts/danik_hybrid_cores.sh)
# runs it when the ECWolf core is loaded and stops it, with SIGTERM to the
# process group, when another core is loaded.

CORE="ECWolf"
GAMEDIR="/media/fat/games/$CORE"
LOGDIR="/media/fat/logs/$CORE"
LOG="$LOGDIR/ecwolf.log"

cd "$GAMEDIR" || exit 1
mkdir -p "$LOGDIR" config saves

# Only once
exec 9> /tmp/ecwolf.lock
flock -n 9 || exit 0

# Development: keep the core loaded without starting the game
[ -f /tmp/ecwolf_nolaunch ] && exit 0

# FPGA settle after the core was just loaded
sleep 1

mv -f "$LOG" "$LOGDIR/ecwolf.prev.log" 2>/dev/null

# For the hybrid core drivers in the binary: which core to attach to and its
# default button mapping ("jn" in core/ECWolf.sv)
export MISTER_HYBRID_CORE="$CORE"
export MISTER_HYBRID_JN="R,B,A,Y,X,L,Select,Start"
# Everything the game writes stays in its folder
export HOME="$GAMEDIR"
export XDG_CONFIG_HOME="$GAMEDIR/config"
export XDG_DATA_HOME="$GAMEDIR/config"

# Both CPUs: the game takes CPU0 (better DDR3 bandwidth, Main_MiSTer lives on
# CPU1) and runs audio and the frame copy on CPU1.
# Exit code 42: the player quit a game picked from the list, show the list again.
GAME=
# The game must not outlive us: on SIGTERM it saves its settings and quits
trap '[ -n "$GAME" ] && kill "$GAME" 2>/dev/null; exit 0' TERM INT
while :; do
    taskset 0x03 ./ECWolf --config "$GAMEDIR/ecwolf.cfg" --savedir "$GAMEDIR/saves" --audiobuffer 1024 >> "$LOG" 2>&1 &
    GAME=$!
    wait "$GAME"
    [ $? -eq 42 ] || break
done
GAME=

# Back to the MiSTer menu, unless another core was loaded meanwhile
if grep -q "^$CORE" /tmp/CORENAME 2>/dev/null; then
    echo "load_core /media/fat/menu.rbf" > /dev/MiSTer_cmd
fi
