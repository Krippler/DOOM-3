#!/bin/sh
# Starts the DOOM 3 engine installed in /usr/lib/doom3 and points it at the
# game data. The first directory with a base/ folder containing .pk4 files wins:
#   $DOOM3_DATA
#   ~/.local/share/doom3          ( copy your base/ and d3xp/ folders here )
#   ~/.doom3
#   the Steam copy of DOOM 3
#   /usr/share/doom3, /usr/share/games/doom3
# Pass +set fs_basepath yourself to override.

ENGINE=/usr/lib/doom3/doom3

has_data() {
	[ -n "$1" ] || return 1
	for pak in "$1"/base/*.pk4; do
		[ -f "$pak" ] && return 0
	done
	return 1
}

for dir in \
	"$DOOM3_DATA" \
	"${XDG_DATA_HOME:-$HOME/.local/share}/doom3" \
	"$HOME/.doom3" \
	"$HOME/.local/share/Steam/steamapps/common/Doom 3" \
	"$HOME/.steam/steam/steamapps/common/Doom 3" \
	"$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common/Doom 3" \
	/usr/share/doom3 \
	/usr/share/games/doom3
do
	if has_data "$dir"; then
		exec "$ENGINE" +set fs_basepath "$dir" "$@"
	fi
done

echo "doom3: no DOOM 3 game data found." >&2
echo "Copy the base/ folder ( pak000.pk4 ... pak008.pk4 ) of your copy of DOOM 3 to" >&2
echo "${XDG_DATA_HOME:-$HOME/.local/share}/doom3/base, or set DOOM3_DATA to the directory that contains base/." >&2
exec "$ENGINE" "$@"
