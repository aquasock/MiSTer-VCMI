#!/usr/bin/env bash
# Assembles work/bundle: VCMI + its runtime libraries + a private glibc, laid out for PORTMASTER_HOME.
#   bin/     vcmiclient vcmiserver   (interpreter patched to the bundled loader)
#   libs/    libvcmi.so, AI/, SDL2 + add-ons, TBB, glibc, libstdc++, gconv/
#   data/    config, Mods (from the VCMI install); game data is added by deploy.sh
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"

INSTALL="$WORK/vcmi-install"
OUT="$WORK/bundle"
DEVICE_DIR="${DEVICE_DIR:-/media/fat/vcmi}"      # where the bundle lives on the MiSTer
STRIP=arm-linux-gnueabihf-strip
CXX_LIB=$(dirname "$(arm-linux-gnueabihf-g++ -print-file-name=libstdc++.so.6)")

rm -rf "$OUT"; mkdir -p "$OUT/bin" "$OUT/libs/AI" "$OUT/data"

# Boost.Locale converts text with glibc iconv, which loads charset modules (CP1252, ...) from disk.
# The cross sysroot lacks them, so take them from the matching armhf libc6 package.
GLIBC_DEB="libc6_${GLIBC_DEB_VER:-2.43-2ubuntu2}_armhf.deb"
[ -s "$DL/$GLIBC_DEB" ] || curl -fsSL -o "$DL/$GLIBC_DEB" "http://ports.ubuntu.com/ubuntu-ports/pool/main/g/glibc/$GLIBC_DEB"
rm -rf "$WORK/libc6-x"; mkdir -p "$WORK/libc6-x"; dpkg-deb -x "$DL/$GLIBC_DEB" "$WORK/libc6-x"
cp -r "$WORK"/libc6-x/usr/lib/arm-linux-gnueabihf/gconv "$OUT/libs/gconv"

cp "$INSTALL/vcmiclient" "$INSTALL/vcmiserver" "$OUT/bin/"
cp "$INSTALL/libvcmi.so" "$OUT/libs/"
cp "$INSTALL"/AI/*.so "$OUT/libs/AI/"
cp -r "$INSTALL/config" "$INSTALL/Mods" "$OUT/data/"

for l in libSDL2-2.0.so.0 libSDL2_image-2.0.so.0 libSDL2_mixer-2.0.so.0 libSDL2_ttf-2.0.so.0 libtbb.so.12; do
	cp -L "$PREFIX/lib/$l" "$OUT/libs/"
done
for l in ld-linux-armhf.so.3 libc.so.6 libm.so.6; do cp -L "$SYSROOT/lib/$l" "$OUT/libs/"; done
cp -L "$CXX_LIB/libstdc++.so.6" "$CXX_LIB/libgcc_s.so.1" "$OUT/libs/"

# Test tools: an SDL program for the MiSTer video driver, and the F9 toggle that switches the
# scaler over to the Linux framebuffer (borrowed from the MiSTer-Pet project's source).
CC=arm-linux-gnueabihf-gcc
CFLAGS="-O2 -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard"
$CC $CFLAGS "$ROOT/sdl-driver/sdl_bench.c" -o "$OUT/bin/sdl-bench" -I"$PREFIX/include/SDL2" -L"$PREFIX/lib" -lSDL2
$CC $CFLAGS "$ROOT/sdl-driver/sdl_audiotest.c" -o "$OUT/bin/sdl-audiotest" -I"$PREFIX/include/SDL2" -L"$PREFIX/lib" -lSDL2 -lm
$CC $CFLAGS "$ROOT/sdl-driver/sdl_test.c" -o "$OUT/bin/sdl-test" -I"$PREFIX/include/SDL2" -L"$PREFIX/lib" -lSDL2
TOGGLE_SRC="${FBTERM_TOGGLE_SRC:-$ROOT/../MiSTer-Pet/src/fbterm_toggle.c}"
if [ -f "$TOGGLE_SRC" ]; then $CC $CFLAGS "$TOGGLE_SRC" -o "$OUT/bin/fbterm-toggle"; else echo "note: $TOGGLE_SRC not found, skipping fbterm-toggle"; fi

$STRIP --strip-unneeded "$OUT"/bin/* "$OUT"/libs/*.so* "$OUT"/libs/AI/*.so 2>/dev/null || true

# Executables start through the bundled loader, so exec'd children (the server) work directly.
for b in "$OUT"/bin/*; do
	patchelf --set-interpreter "$DEVICE_DIR/libs/ld-linux-armhf.so.3" --set-rpath '$ORIGIN/../libs' "$b"
done
for l in "$OUT"/libs/libvcmi.so "$OUT"/libs/libSDL2*.so* "$OUT"/libs/libtbb.so*; do
	patchelf --set-rpath '$ORIGIN' "$l"
done
for l in "$OUT"/libs/AI/*.so; do patchelf --set-rpath '$ORIGIN/..' "$l"; done

cat > "$OUT/run.sh" <<'RUN'
#!/bin/sh
# Usage: run.sh [vcmiclient args...]   (SDL_VIDEODRIVER / SDL_AUDIODRIVER can be set in the environment)
# Local settings go in env.sh next to this file, for example:
#   MISTER_OUTPUT_MODE=800x600|1024x600|off   HDMI output mode (and VCMI resolution) while playing; off leaves it alone.
#                                        Default 800x600. 1024x600 is widescreen (about 44% more CPU per frame).
#                                        A mode given on the command line beats env.sh.
#   MISTER_RESTORE_MODE="<modeline>"     mode to switch back to afterwards (default: 1080p60)
#   SDL_MISTER_STATS=/media/fat/vcmi/cache/present-stats.log
cd "$(dirname "$0")"

# The video driver switches this console to graphics mode while the game runs and back afterwards. If the game
# crashes it cannot, and the console then stays invisible (a black screen where the OSD's "press ENTER to
# continue" prompt should be, after quitting) until something puts it back in text mode. So do that now, in case
# a previous run left it that way, and again when the game ends however it ended.
reset_console() { python3 -c 'import fcntl; fcntl.ioctl(0, 0x4B3A, 0)' 2>/dev/null; }
reset_console

export PORTMASTER_HOME="$PWD"
export GCONV_PATH="$PWD/libs/gconv"
mkdir -p save cache
caller_mode="$MISTER_OUTPUT_MODE"   # a mode given on the command line beats env.sh
[ -f ./env.sh ] && . ./env.sh
[ -n "$caller_mode" ] && MISTER_OUTPUT_MODE="$caller_mode"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-mister}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-mister}"
# VCMI redraws the whole window every frame, so the presenter can take the renderer's buffers without copying.
export SDL_MISTER_ZEROCOPY="${SDL_MISTER_ZEROCOPY:-1}"
# First run: defaults that suit the MiSTer (exclusive 800x600, software renderer and cursor).
# VCMI logs everything at trace level to a file by default; on the SD card that costs ~5 ms per line,
# which made AI turns take seconds longer, so log errors only (file and console).
[ -e save/settings.json ] || cat > save/settings.json <<'JSON'
{
	"video" : {
		"fullscreen" : true,
		"realFullscreen" : true,
		"driver" : "software",
		"cursor" : "software",
		"vsync" : false,
		"resolution" : { "width" : 800, "height" : 600, "scaling" : 100 }
	},
	"logging" : {
		"console" : { "threshold" : "error" },
		"loggers" : [ { "domain" : "global", "level" : "error" } ]
	}
}
JSON

# The scaler in Main_MiSTer only scales the framebuffer by whole numbers, and 800x600 does not fit 1080p twice.
# So switch the HDMI output itself to the game's resolution while it runs: the picture is 1:1 from the MiSTer and
# the display does the scaling to fill the screen. The previous mode is restored when the game exits.
MODE_800X600="800,40,128,88,600,1,4,23,40000,+hsync,+vsync"        # VESA 800x600 @ 60 Hz
MODE_1024X600="1024,40,104,144,600,3,10,11,49000,-hsync,+vsync"    # CVT 1024x600 @ 59.85 Hz (widescreen, 17:10)
MODE_1080P60="1920,88,44,148,1080,4,5,36,148500,+hsync,+vsync"     # CEA 1080p @ 60 Hz

# VCMI reads its resolution from save/settings.json, so keep it in step with the output mode.
set_resolution() {
	command -v python3 >/dev/null 2>&1 || return 0
	python3 - "$1" "$2" <<'PY'
import json, sys
p = "save/settings.json"
w, h = int(sys.argv[1]), int(sys.argv[2])
try:
    d = json.load(open(p))
except Exception:
    d = {}
r = d.setdefault("video", {}).setdefault("resolution", {})
if r.get("width") != w or r.get("height") != h:
    r["width"], r["height"] = w, h
    json.dump(d, open(p, "w"), indent="\t")
PY
}

# Mods are dropped into data/Mods as folders (each with a mod.json) instead of being installed by VCMI's launcher, and
# VCMI only loads root mods listed in the active preset of save/modSettings.json. Scripts/vcmi.sh sets VCMI_PRESET=vcmi
# for a clean base game; each other Scripts/vcmi-<name>.sh sets VCMI_PRESET=<name> for one mod, falling back to the base
# game if that mod was never fetched. Per-submod settings inside a preset are never touched here, so a submod you
# disabled by hand by editing modSettings.json directly (there is no in-client mod manager; that UI lives only in
# VCMI's separate Launcher app, which this project does not build) stays disabled.
select_preset() {
	command -v python3 >/dev/null 2>&1 || return 0
	python3 - "${VCMI_PRESET:-vcmi}" <<'PY'
import json, os, sys

p = "save/modSettings.json"
mods_dir = "data/Mods"
wanted = sys.argv[1]
found = sorted(d.lower() for d in os.listdir(mods_dir) if os.path.isfile(os.path.join(mods_dir, d, "mod.json"))) if os.path.isdir(mods_dir) else []

# Each preset other than "vcmi" (the base game) is one mod, named for its own Scripts/vcmi-<name>.sh: the launcher
# name is not always the mod's own folder name (VCMI derives a mod's id from its folder name, and some mods, like
# WoG, cross-reference their own submods by their upstream folder name, so tools/fetch-wog.sh cannot shorten it the
# way tools/fetch-hota.sh's folder already happens to match). "extra" lists sibling mod folders the preset needs
# that aren't part of the mod's own folder, e.g. HotA's dependency on vcmi-extras; only included if actually present.
PRESETS = {
	"hota": {"root": "hota", "extra": ["vcmi-extras"]},
	"wog": {"root": "wake-of-gods", "extra": []},
}

if wanted != "vcmi" and wanted not in PRESETS:
	wanted = "vcmi"
elif wanted != "vcmi" and PRESETS[wanted]["root"] not in found:
	print("vcmi-%s: mod not installed (see tools/fetch-%s.sh); starting the base game instead." % (wanted, wanted))
	wanted = "vcmi"

try:
	cfg = json.load(open(p)) if os.path.exists(p) else {}
except Exception:
	cfg = {}
presets = cfg.setdefault("presets", {})
presets.setdefault("vcmi", {})["mods"] = ["vcmi", "core"]
for name, info in PRESETS.items():
	if info["root"] in found:
		extra = [m for m in info["extra"] if m in found]
		presets.setdefault(name, {})["mods"] = ["vcmi", "core", info["root"]] + extra
cfg["activePreset"] = wanted
json.dump(cfg, open(p, "w"), indent="\t")
print("vcmi preset: %s" % wanted)
PY
}
select_preset

switched=0
restore_mode() {
	[ "$switched" = 1 ] || return 0
	switched=0
	echo "video_mode ${MISTER_RESTORE_MODE:-$MODE_1080P60}" > /dev/MiSTer_cmd
	sleep 1
}
modeline=""
case "${MISTER_OUTPUT_MODE:-800x600}" in
	800x600)  modeline="$MODE_800X600";  set_resolution 800 600 ;;
	1024x600) modeline="$MODE_1024X600"; set_resolution 1024 600 ;;
esac
if [ -n "$modeline" ] && [ -p /dev/MiSTer_cmd ]; then
	echo "video_mode $modeline" > /dev/MiSTer_cmd
	switched=1
	sleep 2.5   # let the display re-sync; Main resizes the framebuffer to match
fi

./bin/vcmiclient "$@" &
child=$!
trap 'kill -TERM $child 2>/dev/null' TERM HUP INT
wait $child
rc=$?
wait $child 2>/dev/null   # a trapped signal ends the first wait early
reset_console
restore_mode
exit $rc
RUN
chmod +x "$OUT/run.sh"

# Entry for the OSD Scripts menu (F12 > Scripts). Scripts run on tty2 with the Linux framebuffer
# switched in, which is what the driver expects, and typed text works because tty2 is a real terminal.
# The output mode is run.sh's default (800x600); set MISTER_OUTPUT_MODE in env.sh to change it.
mkdir -p "$OUT/Scripts"
cat > "$OUT/Scripts/vcmi.sh" <<LAUNCH
#!/bin/bash
VCMI_PRESET=vcmi exec $DEVICE_DIR/run.sh "\$@"
LAUNCH
cat > "$OUT/Scripts/vcmi-hota.sh" <<LAUNCH
#!/bin/bash
# Horn of the Abyss, if tools/fetch-hota.sh has staged it (see README.md#mods); otherwise this falls back to the base game.
VCMI_PRESET=hota exec $DEVICE_DIR/run.sh "\$@"
LAUNCH
cat > "$OUT/Scripts/vcmi-wog.sh" <<LAUNCH
#!/bin/bash
# In The Wake of Gods, if tools/fetch-wog.sh has staged it (see README.md#mods); otherwise this falls back to the base game.
VCMI_PRESET=wog exec $DEVICE_DIR/run.sh "\$@"
LAUNCH
chmod +x "$OUT/Scripts/vcmi.sh" "$OUT/Scripts/vcmi-hota.sh" "$OUT/Scripts/vcmi-wog.sh"

du -sh "$OUT"; du -sh "$OUT"/*
