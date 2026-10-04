#!/bin/sh
# Assemble the MiSTer release in dist/ECWolf_<date>/ from the build outputs
set -e
cd "$(dirname "$0")"
DATE=$(date +%Y%m%d)
OUT=dist/ECWolf_$DATE
rm -rf "$OUT"
mkdir -p "$OUT/_Other" "$OUT/games/ECWolf" "$OUT/Scripts"
cp hybrid/launcher/danik_hybrid_cores.sh "$OUT/Scripts/danik_hybrid_cores.sh"
cp core/output_files/ECWolf.rbf "$OUT/_Other/ECWolf_$DATE.rbf"
cp -r package/games/ECWolf/. "$OUT/games/ECWolf/"
cp build/mister/ecwolf "$OUT/games/ECWolf/ECWolf"
cp build/mister/ecwolf.pk3 "$OUT/games/ECWolf/ecwolf.pk3"
hybrid/docker.sh arm-linux-gnueabihf-strip "$OUT/games/ECWolf/ECWolf"
chmod +x "$OUT/games/ECWolf/ECWolf" "$OUT"/games/ECWolf/*.sh "$OUT/Scripts/danik_hybrid_cores.sh"
cp ecwolf/docs/copyright "$OUT/games/ECWolf/LICENSE-ecwolf.txt"
cp ecwolf/docs/license-gpl.txt "$OUT/games/ECWolf/LICENSE-gpl.txt"
cp build/sdl/src/SDL2-*/LICENSE.txt "$OUT/games/ECWolf/LICENSE-sdl.txt"
rm -f "dist/ECWolf_$DATE.zip"
(cd "$OUT" && python3 -m zipfile -c "../ECWolf_$DATE.zip" _Other games Scripts)
find "$OUT" -type f | sort
ls -la "dist/ECWolf_$DATE.zip"
