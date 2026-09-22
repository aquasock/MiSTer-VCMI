#!/usr/bin/env bash
# Builds the user-facing release archive from the bundle:  scripts/release.sh 0.1.0
#
#   work/dist/MiSTer-VCMI-v<version>.zip   unpack onto the root of the MiSTer's SD card
#   work/dist/SHA256SUMS
#   work/dist/RELEASE_NOTES-v<version>.md  (only if docs/release-notes/<version>.md exists)
#
# The archive holds vcmi/ (game, libraries, launcher, license texts, source list) and the Scripts/*.sh launchers
# (vcmi, and one per supported mod; a mod's own launcher falls back to the base game until its files are fetched
# separately with tools/fetch-*.sh, see README.md#mods). It leaves out the test programs, the unlicensed
# fbterm-toggle helper, any Heroes III data, and mod files themselves.
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"

VERSION="${1:?usage: scripts/release.sh <version, e.g. 0.1.0>}"
NAME="MiSTer-VCMI-v$VERSION"
DIST="$WORK/dist"
STAGE="$DIST/stage/$NAME"
SRCTREE="$WORK/src"

# 1. Fresh bundle.
"$ROOT/scripts/bundle.sh" >/dev/null
rm -rf "$DIST/stage" "$DIST/$NAME.zip"
mkdir -p "$STAGE/vcmi" "$STAGE/Scripts"

# 2. The game: everything in the bundle except test tools and the unlicensed helper.
cp -a "$WORK/bundle/libs" "$WORK/bundle/data" "$WORK/bundle/run.sh" "$STAGE/vcmi/"
mkdir -p "$STAGE/vcmi/bin"
cp -a "$WORK/bundle/bin/vcmiclient" "$WORK/bundle/bin/vcmiserver" "$STAGE/vcmi/bin/"
cp -a "$WORK/bundle/Scripts/"*.sh "$STAGE/Scripts/"
echo "$VERSION" > "$STAGE/vcmi/VERSION"

# 3. License texts of everything that ships, plus an index.
L="$STAGE/vcmi/LICENSES"; mkdir -p "$L"
cp "$ROOT/COPYING" "$L/GPL-3.0.txt"
cp "$ROOT/LICENSE.txt" "$L/GPL-2.0.txt"
cp "$ROOT/COPYING.ZLIB" "$L/SDL2-zlib.txt"
cp "$SRCTREE/SDL2_image-2.8.12/LICENSE.txt" "$L/SDL2_image-zlib.txt"
cp "$SRCTREE/SDL2_mixer-2.8.1/LICENSE.txt" "$L/SDL2_mixer-zlib.txt"
cp "$SRCTREE/SDL2_ttf-2.24.0/LICENSE.txt" "$L/SDL2_ttf-zlib.txt"
cp "$SRCTREE/SDL2_ttf-2.24.0/external/freetype/docs/FTL.TXT" "$L/FreeType-FTL.txt"
cp "$SRCTREE/SDL2_mixer-2.8.1/external/minimp3/LICENSE" "$L/minimp3-CC0-1.0.txt"
cp "$SRCTREE/boost-1.90.0/LICENSE_1_0.txt" "$L/Boost-1.0.txt"
cp "$SRCTREE/oneTBB-2022.3.0/LICENSE.txt" "$L/oneTBB-Apache-2.0.txt"
cp "$SRCTREE/zlib-1.3.1/LICENSE" "$L/zlib.txt"
cp "$SRCTREE/minizip-ng-4.0.10/LICENSE" "$L/minizip-ng.txt"
cp "$SRCTREE/squish-1.15/LICENSE.txt" "$L/libsquish-MIT.txt"
cp "$WORK/vcmi-src/license.txt" "$L/VCMI-GPL-2.0.txt"
cp /usr/share/common-licenses/LGPL-2.1 "$L/glibc-LGPL-2.1.txt"
cp /usr/share/doc/libc6-armhf-cross/copyright "$L/glibc-copyright.txt"
cp /usr/share/doc/libstdc++6/copyright "$L/GCC-runtime-libstdc++-copyright.txt"   # includes the GCC Runtime Library Exception
cat > "$L/VCMI-assets-CC-BY-SA-4.0.txt" <<'EOF'
The game content installed under data/Mods/vcmi and data/config is VCMI project material. The VCMI project
licenses its assets under Creative Commons Attribution-ShareAlike 4.0 International:
    https://creativecommons.org/licenses/by-sa/4.0/legalcode
Asset sources and the list of contributors: https://github.com/vcmi/vcmi-assets
VCMI source code is GPL-2.0-or-later (VCMI-GPL-2.0.txt). Copyright (C) 2007-2025 VCMI Team; see the AUTHORS file
in https://github.com/vcmi/vcmi for the contributors.
EOF
{
	printf 'Copyright 2022 The Noto Project Authors (https://github.com/notofonts/latin-greek-cyrillic)\n\n'
	printf 'This applies to the Noto Sans and Noto Serif fonts in data/Mods/vcmi/Content/Data.\n\n'
	# The OFL text as reproduced in Debian's copyright file: indented one space, blank lines written as " ."
	sed -n '/^ *SIL OPEN FONT LICENSE Version 1.1/,/^ *DEALINGS IN THE FONT SOFTWARE/p' /usr/share/doc/fonts-liberation/copyright |
		sed -e 's/^ \.$//' -e 's/^ //'
} > "$L/Noto-OFL-1.1.txt"
grep -q "DEALINGS IN THE FONT SOFTWARE" "$L/Noto-OFL-1.1.txt" || { echo "could not extract the OFL text" >&2; exit 1; }
cat > "$L/README.txt" <<'EOF'
Licenses of the software in this package. See ATTRIBUTIONS.md for what each component is used for.

  GPL-2.0.txt, GPL-3.0.txt              MiSTer-VCMI's own code (GPL-2.0-or-later); the bundle as a whole is GPL-3.0-or-later
  VCMI-GPL-2.0.txt                      VCMI (game engine)
  VCMI-assets-CC-BY-SA-4.0.txt          VCMI's own game content (attribution and license link)
  Noto-OFL-1.1.txt                      Noto Sans / Noto Serif fonts inside the VCMI mod
  SDL2-zlib.txt, SDL2_image-zlib.txt,   SDL2 and its image, mixer and font libraries (with the patches applied here)
  SDL2_mixer-zlib.txt, SDL2_ttf-zlib.txt
  FreeType-FTL.txt                      FreeType, built into libSDL2_ttf
  minimp3-CC0-1.0.txt                   minimp3, built into libSDL2_mixer (dr_flac, stb_vorbis and stb_image are public domain)
  Boost-1.0.txt                         Boost, linked statically
  zlib.txt, minizip-ng.txt              zlib and minizip-ng, linked statically
  libsquish-MIT.txt                     libsquish, linked statically
  oneTBB-Apache-2.0.txt                 oneTBB (libtbb.so.12)
  glibc-LGPL-2.1.txt, glibc-copyright.txt   the C library shipped in libs/ (ld-linux-armhf.so.3, libc.so.6, libm.so.6, libs/gconv)
  GCC-runtime-libstdc++-copyright.txt   libstdc++ and libgcc_s in libs/ (GPL-3.0 with the GCC Runtime Library Exception)

FuzzyLite (linked into the Nullkiller AI library) is GPL-3.0; see GPL-3.0.txt.
Heroes of Might and Magic III game data is not included.
EOF
cp "$ROOT/README.md" "$ROOT/ATTRIBUTIONS.md" "$STAGE/vcmi/"
cp "$ROOT/COPYING" "$ROOT/LICENSE.txt" "$ROOT/COPYING.ZLIB" "$STAGE/vcmi/"

# 4. Where the corresponding source comes from, with checksums of the exact archives used.
{
	echo "MiSTer-VCMI v$VERSION: corresponding source"
	echo
	echo "This package's own source: https://github.com/aquasock/MiSTer-VCMI"
	if git -C "$ROOT" rev-parse HEAD >/dev/null 2>&1; then
		echo "  commit $(git -C "$ROOT" rev-parse HEAD)$(git -C "$ROOT" diff --quiet HEAD -- 2>/dev/null || echo ' (built with uncommitted changes)')"
	fi
	echo "  scripts/env.sh pins every version below; scripts/build-deps.sh and build-vcmi.sh fetch and build them."
	echo
	echo "VCMI $VCMI_TAG: https://github.com/vcmi/vcmi (tag $VCMI_TAG, with submodules)"
	echo "  commit $(git -C "$WORK/vcmi-src" rev-parse HEAD 2>/dev/null || echo unknown)"
	echo "  plus the patches in patches/ of this project"
	echo
	echo "Libraries (sha256 of the archives that were built):"
	for f in "$DL"/*.tar.* "$DL"/*.deb; do
		printf '  %s  %s\n' "$(sha256sum "$f" | cut -d' ' -f1)" "$(basename "$f")"
	done
	echo "  (SDL2, SDL2_image, SDL2_mixer, SDL2_ttf: https://github.com/libsdl-org; with sdl-driver/*.patch and sdl-driver/mister*/ applied)"
	echo "  (boost: https://github.com/boostorg/boost; oneTBB: https://github.com/uxlfoundation/oneTBB;"
	echo "   zlib: https://github.com/madler/zlib; minizip-ng: https://github.com/zlib-ng/minizip-ng;"
	echo "   libsquish: https://deb.debian.org/debian/pool/main/libs/libsquish/)"
	echo "minimp3 (SDL_mixer): https://github.com/lieff/minimp3, commit $(git -C "$SRCTREE/SDL2_mixer-2.8.1/external/minimp3" rev-parse HEAD 2>/dev/null || echo unknown)"
	echo
	echo "C runtime in libs/: glibc $(ls "$DL" | sed -n 's/^libc6_\(.*\)_armhf.deb/\1/p'), Ubuntu."
	echo "  Source: 'apt source glibc' on Ubuntu, or https://packages.ubuntu.com (source package glibc)."
	echo "  libc.so.6, ld-linux-armhf.so.3 and libm.so.6 come from Ubuntu's libc6-armhf-cross (source package cross-toolchain-base)."
	echo "libstdc++ and libgcc_s in libs/: GCC 15 (Ubuntu source package gcc-15-cross)."
	echo
	echo "LGPL/GPL note: the C and C++ runtime libraries are dynamically linked and are replaceable: the programs start"
	echo "through libs/ld-linux-armhf.so.3 with libs/ on their library path. On request, the source for any GPL or LGPL"
	echo "component listed here can also be obtained from the project page above."
} > "$STAGE/vcmi/SOURCES.txt"

# 5. Instructions for people who only download the zip.
cat > "$STAGE/vcmi/INSTALL.txt" <<EOF
MiSTer-VCMI v$VERSION: installing

1. Copy the two folders from this zip onto the root of your MiSTer's SD card, merging with what is there:
       vcmi/     ->  /media/fat/vcmi
       Scripts/  ->  /media/fat/Scripts
   The location matters: the programs look for their libraries in /media/fat/vcmi.

2. Add your own Heroes of Might and Magic III data. Copy the folders Data, Maps and Mp3 from an installed copy
   into /media/fat/vcmi/data. (GOG: unpack the installer with innoextract, https://constexpr.org/innoextract/ ,
   and copy those three folders. Data/VIDEO.VID is not needed.)
   Only the GOG "Complete" release has been tested.

3. On the MiSTer press F12, choose Scripts, and run "vcmi" for the base game. This zip also includes a Scripts
   entry per supported mod (currently "vcmi-hota" and "vcmi-wog"); each plays the base game until you fetch that
   mod's files yourself on a PC with the project's tools/fetch-hota.sh or tools/fetch-wog.sh and copy them to
   /media/fat/vcmi/data/Mods (see README.md#mods). Whichever entry you run, the screen blinks as the HDMI output
   switches to 800x600 (your display scales it to fill the screen); quit from the game's own menu and the display
   switches back to 1080p60.

Needs: a MiSTer with a DE10-Nano, a USB mouse and keyboard, and an HDMI display that accepts 800x600 at 60 Hz.
Settings, saves and logs are kept in /media/fat/vcmi/save and /media/fat/vcmi/cache.

To remove: delete /media/fat/vcmi and every /media/fat/Scripts/vcmi*.sh.
More: README.md, and the license texts in LICENSES/ (see ATTRIBUTIONS.md).
EOF

# 6. Pack it, keeping the layout at the top level and recording modes.
python3 - "$STAGE" "$DIST/$NAME.zip" <<'PY'
import os, sys, zipfile
stage, out = sys.argv[1:3]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for root, dirs, files in os.walk(stage):
        dirs.sort()
        for f in sorted(files):
            full = os.path.join(root, f)
            arc = os.path.relpath(full, stage)
            info = zipfile.ZipInfo.from_file(full, arc)
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(full, "rb") as fh:
                z.writestr(info, fh.read(), zipfile.ZIP_DEFLATED, 9)
PY
( cd "$DIST" && sha256sum "$NAME.zip" > SHA256SUMS )
[ -f "$ROOT/docs/release-notes/$VERSION.md" ] && cp "$ROOT/docs/release-notes/$VERSION.md" "$DIST/RELEASE_NOTES-v$VERSION.md"
echo "built $DIST/$NAME.zip ($(du -h "$DIST/$NAME.zip" | cut -f1))"
