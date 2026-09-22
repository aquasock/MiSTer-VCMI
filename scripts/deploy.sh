#!/usr/bin/env bash
# Copies the bundle (and, once, the game data) to the MiSTer over ssh.
# Usage: scripts/deploy.sh [code|data|mods|tools|all]   (default: code)
#   mods copies the mods staged by tools/fetch-hota.sh into data/Mods; code leaves any other mods in data/Mods alone.
#   MISTER_HOST (default 10.10.0.22), DEVICE_DIR (default /media/fat/vcmi), H3_DATA (extracted GOG dir)
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"

HOST="${MISTER_HOST:-10.10.0.22}"
DEVICE_DIR="${DEVICE_DIR:-/media/fat/vcmi}"
H3_DATA="${H3_DATA:-$WORK/h3-extract}"
BUNDLE="$WORK/bundle"

# The MiSTer's stock login is root/1; feed it to ssh without sshpass.
ASKPASS="$WORK/askpass.sh"
printf '#!/bin/sh\necho 1\n' > "$ASKPASS"; chmod +x "$ASKPASS"
export SSH_ASKPASS="$ASKPASS" SSH_ASKPASS_REQUIRE=force
SSH=(ssh -o StrictHostKeyChecking=accept-new -o PubkeyAuthentication=no "root@$HOST")

code() {
	"${SSH[@]}" "mkdir -p $DEVICE_DIR && rm -rf $DEVICE_DIR/bin $DEVICE_DIR/libs $DEVICE_DIR/data/config $DEVICE_DIR/data/Mods/vcmi"
	tar -C "$BUNDLE" -cf - --exclude=./Scripts . | "${SSH[@]}" "tar --no-same-owner -C $DEVICE_DIR -xf -"
	for f in "$BUNDLE"/Scripts/*.sh; do
		"${SSH[@]}" "mkdir -p /media/fat/Scripts && cat > /media/fat/Scripts/$(basename "$f") && chmod +x /media/fat/Scripts/$(basename "$f")" < "$f"
	done
}

data() {
	# VIDEO.VID (600 MB) is only useful with ffmpeg video support, which is off.
	tar --exclude=Data/VIDEO.VID -C "$H3_DATA" -cf - Data Maps Mp3 | "${SSH[@]}" "mkdir -p $DEVICE_DIR/data && tar --no-same-owner -C $DEVICE_DIR/data -xf -"
}

mods() {
	[ -d "$WORK/hota-mods/Mods" ] || { echo "run tools/fetch-hota.sh first" >&2; exit 1; }
	tar -C "$WORK/hota-mods/Mods" -cf - . | "${SSH[@]}" "mkdir -p $DEVICE_DIR/data/Mods && tar --no-same-owner -C $DEVICE_DIR/data/Mods -xf -"
}

# The on-device test tools live in /tmp, which a reboot wipes.
tools() {
	for f in "$ROOT"/tools/*.py; do "${SSH[@]}" "cat > /tmp/$(basename "$f")" < "$f"; done
}

case "${1:-code}" in
	tools) tools ;;
	code) code ;;
	data) data ;;
	mods) mods ;;
	all) code; data ;;
esac
