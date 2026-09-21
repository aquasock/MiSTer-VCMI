#!/usr/bin/env bash
# Cross-builds every VCMI dependency for the MiSTer into work/prefix.
# Usage: scripts/build-deps.sh [step...]   (default: all steps, skipping finished ones)
# Steps: fetch zlib minizip boost tbb squish sdl2 sdl2_image sdl2_mixer sdl2_ttf
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"

STAMPS="$WORK/stamps"; mkdir -p "$STAMPS"

say() { printf '\n=== %s ===\n' "$*"; }

fetch() {
	local url=$1 out=${2:-$(basename "$1")}
	[ -s "$DL/$out" ] || curl -fsSL --retry 3 -o "$DL/$out" "$url"
}

extract() {  # extract <archive> <expected top dir>
	[ -d "$SRC/$2" ] || tar xf "$DL/$1" -C "$SRC"
}

# cm <name> <source dir> [cmake args...]: configure, build, install with a log.
cm() {
	local name=$1 src=$2; shift 2
	[ -e "$STAMPS/$name" ] && { echo "$name: already built"; return; }
	say "$name"
	(
		cmake -S "$src" -B "$BUILD/$name" -G Ninja \
			-DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
			-DCMAKE_INSTALL_PREFIX="$PREFIX" \
			-DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
			-DBUILD_SHARED_LIBS=OFF \
			"$@"
		cmake --build "$BUILD/$name" -j"$JOBS"
		cmake --install "$BUILD/$name"
	) >"$LOGS/$name.log" 2>&1 || { echo "$name FAILED, see $LOGS/$name.log"; tail -25 "$LOGS/$name.log"; exit 1; }
	touch "$STAMPS/$name"
}

step_fetch() {
	say fetch
	fetch "https://github.com/boostorg/boost/releases/download/boost-$BOOST_VER/boost-$BOOST_VER-cmake.tar.xz"
	fetch "https://github.com/madler/zlib/releases/download/v$ZLIB_VER/zlib-$ZLIB_VER.tar.gz"
	fetch "https://github.com/zlib-ng/minizip-ng/archive/refs/tags/$MINIZIP_VER.tar.gz" "minizip-ng-$MINIZIP_VER.tar.gz"
	fetch "https://github.com/uxlfoundation/oneTBB/archive/refs/tags/v$TBB_VER.tar.gz" "oneTBB-$TBB_VER.tar.gz"
	fetch "https://deb.debian.org/debian/pool/main/libs/libsquish/libsquish_$SQUISH_VER.orig.tar.gz"
	fetch "https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VER/SDL2-$SDL2_VER.tar.gz"
	fetch "https://github.com/libsdl-org/SDL_image/releases/download/release-$SDL2_IMAGE_VER/SDL2_image-$SDL2_IMAGE_VER.tar.gz"
	fetch "https://github.com/libsdl-org/SDL_mixer/releases/download/release-$SDL2_MIXER_VER/SDL2_mixer-$SDL2_MIXER_VER.tar.gz"
	fetch "https://github.com/libsdl-org/SDL_ttf/releases/download/release-$SDL2_TTF_VER/SDL2_ttf-$SDL2_TTF_VER.tar.gz"
}

step_zlib() {
	extract "zlib-$ZLIB_VER.tar.gz" "zlib-$ZLIB_VER"
	cm zlib "$SRC/zlib-$ZLIB_VER"
	# zlib installs a shared copy too; drop it so everything links the static one.
	rm -f "$PREFIX"/lib/libz.so*
}

step_minizip() {
	extract "minizip-ng-$MINIZIP_VER.tar.gz" "minizip-ng-$MINIZIP_VER"
	cm minizip "$SRC/minizip-ng-$MINIZIP_VER" \
		-DMZ_BZIP2=OFF -DMZ_LZMA=OFF -DMZ_ZSTD=OFF -DMZ_OPENSSL=OFF -DMZ_ICONV=OFF \
		-DMZ_LIBBSD=OFF -DMZ_PKCRYPT=OFF -DMZ_WZAES=OFF -DMZ_SIGNING=OFF \
		-DMZ_FETCH_LIBS=OFF -DMZ_FORCE_FETCH_LIBS=OFF \
		-DZLIB_ROOT="$PREFIX"
}

step_boost() {
	extract "boost-$BOOST_VER-cmake.tar.xz" "boost-$BOOST_VER"
	cm boost "$SRC/boost-$BOOST_VER" \
		-DBOOST_LOCALE_ENABLE_ICU=OFF -DBOOST_LOCALE_ENABLE_ICONV=ON \
		-DBOOST_ENABLE_MPI=OFF -DBOOST_ENABLE_PYTHON=OFF -DBUILD_TESTING=OFF \
		-DZLIB_ROOT="$PREFIX"
}

step_tbb() {
	extract "oneTBB-$TBB_VER.tar.gz" "oneTBB-$TBB_VER"
	# TBB is only supported as a shared library.
	cm tbb "$SRC/oneTBB-$TBB_VER" \
		-DBUILD_SHARED_LIBS=ON -DTBB_TEST=OFF -DTBB_STRICT=OFF -DTBB_EXAMPLES=OFF \
		-DTBBMALLOC_BUILD=OFF -DTBB4PY_BUILD=OFF
}

step_squish() {
	[ -e "$STAMPS/squish" ] && { echo "squish: already built"; return; }
	say squish
	local d="$SRC/squish-$SQUISH_VER"
	mkdir -p "$d"; tar xzf "$DL/libsquish_$SQUISH_VER.orig.tar.gz" -C "$d"
	(
		cd "$d"
		for f in *.cpp; do
			arm-linux-gnueabihf-g++ -O2 -fPIC -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -c "$f" -o "${f%.cpp}.o"
		done
		arm-linux-gnueabihf-ar rcs libsquish.a *.o
		install -D -m644 libsquish.a "$PREFIX/lib/libsquish.a"
		install -D -m644 squish.h "$PREFIX/include/squish.h"
	) >"$LOGS/squish.log" 2>&1 || { echo "squish FAILED, see $LOGS/squish.log"; tail -25 "$LOGS/squish.log"; exit 1; }
	touch "$STAMPS/squish"
}

step_sdl2() {
	extract "SDL2-$SDL2_VER.tar.gz" "SDL2-$SDL2_VER"
	local d="$SRC/SDL2-$SDL2_VER"
	# MiSTer video + audio drivers: register them in SDL (start from a clean tree when the hooks changed), then drop the sources in.
	if ! grep -q SDL_MISTERAUDIO "$d/CMakeLists.txt" 2>/dev/null; then
		rm -rf "$d"; extract "SDL2-$SDL2_VER.tar.gz" "SDL2-$SDL2_VER"
		patch -s -d "$d" -p1 <"$ROOT/sdl-driver/sdl2-mister-hooks.patch"
	fi
	grep -q "division-free resampler" "$d/src/audio/SDL_audiocvt.c" || patch -s -d "$d" -p1 <"$ROOT/sdl-driver/sdl2-resampler.patch"
	mkdir -p "$d/src/video/mister" "$d/src/audio/mister"
	cp "$ROOT"/sdl-driver/mister/* "$d/src/video/mister/"
	cp "$ROOT"/sdl-driver/mister-audio/* "$d/src/audio/mister/"
	local h; h=$(cat "$ROOT"/sdl-driver/mister/* "$ROOT"/sdl-driver/mister-audio/* "$ROOT/sdl-driver/sdl2-mister-hooks.patch" "$ROOT/sdl-driver/sdl2-resampler.patch" | md5sum | cut -d' ' -f1)
	[ "$(cat "$STAMPS/sdl2.driver" 2>/dev/null)" = "$h" ] || { rm -f "$STAMPS/sdl2"; echo "$h" >"$STAMPS/sdl2.driver"; }
	# Shared, so the drivers can be updated without relinking VCMI. Other audio backends stay off.
	cm sdl2 "$d" -DSDL_MISTER=ON -DSDL_MISTERAUDIO=ON \
		-DBUILD_SHARED_LIBS=ON -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF -DSDL_TEST=OFF \
		-DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF -DSDL_VULKAN=OFF -DSDL_OPENGL=OFF \
		-DSDL_OPENGLES=OFF -DSDL_RPI=OFF -DSDL_VIVANTE=OFF -DSDL_OFFSCREEN=ON \
		-DSDL_ALSA=OFF -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF -DSDL_JACK=OFF -DSDL_SNDIO=OFF \
		-DSDL_OSS=OFF -DSDL_ESD=OFF -DSDL_ARTS=OFF -DSDL_NAS=OFF \
		-DSDL_DBUS=OFF -DSDL_IBUS=OFF -DSDL_LIBUDEV=OFF -DSDL_HIDAPI=OFF -DSDL_HIDAPI_LIBUSB=OFF \
		-DSDL_LIBSAMPLERATE=OFF -DSDL_CCACHE=OFF
}

sdl_addon_args=(-DBUILD_SHARED_LIBS=ON -DSDL2_DIR="$PREFIX/lib/cmake/SDL2")

step_sdl2_image() {
	extract "SDL2_image-$SDL2_IMAGE_VER.tar.gz" "SDL2_image-$SDL2_IMAGE_VER"
	cm sdl2_image "$SRC/SDL2_image-$SDL2_IMAGE_VER" "${sdl_addon_args[@]}" \
		-DSDL2IMAGE_VENDORED=OFF -DSDL2IMAGE_DEPS_SHARED=OFF -DSDL2IMAGE_BACKEND_STB=ON \
		-DSDL2IMAGE_AVIF=OFF -DSDL2IMAGE_JXL=OFF -DSDL2IMAGE_TIF=OFF -DSDL2IMAGE_WEBP=OFF \
		-DSDL2IMAGE_SAMPLES=OFF -DSDL2IMAGE_TESTS=OFF
}

step_sdl2_mixer() {
	extract "SDL2_mixer-$SDL2_MIXER_VER.tar.gz" "SDL2_mixer-$SDL2_MIXER_VER"
	local mm="$SRC/SDL2_mixer-$SDL2_MIXER_VER/external/minimp3"
	[ -e "$mm/minimp3.h" ] || { rm -rf "$mm"; git clone --depth 1 https://github.com/lieff/minimp3.git "$mm" >"$LOGS/minimp3.log" 2>&1; }
	cm sdl2_mixer "$SRC/SDL2_mixer-$SDL2_MIXER_VER" "${sdl_addon_args[@]}" \
		-DSDL2MIXER_VENDORED=OFF -DSDL2MIXER_DEPS_SHARED=OFF \
		-DSDL2MIXER_MP3=ON -DSDL2MIXER_MP3_MINIMP3=ON -DSDL2MIXER_MP3_MPG123=OFF \
		-DSDL2MIXER_VORBIS=STB -DSDL2MIXER_FLAC=ON -DSDL2MIXER_FLAC_DRFLAC=ON \
		-DSDL2MIXER_MOD=OFF -DSDL2MIXER_MIDI=OFF -DSDL2MIXER_OPUS=OFF -DSDL2MIXER_WAVPACK=OFF \
		-DSDL2MIXER_SAMPLES=OFF -DSDL2MIXER_CMD=OFF
}

step_sdl2_ttf() {
	extract "SDL2_ttf-$SDL2_TTF_VER.tar.gz" "SDL2_ttf-$SDL2_TTF_VER"
	cm sdl2_ttf "$SRC/SDL2_ttf-$SDL2_TTF_VER" "${sdl_addon_args[@]}" \
		-DSDL2TTF_VENDORED=ON -DSDL2TTF_HARFBUZZ=OFF -DSDL2TTF_SAMPLES=OFF
}

ALL=(fetch zlib minizip boost tbb squish sdl2 sdl2_image sdl2_mixer sdl2_ttf)
STEPS=("${@:-${ALL[@]}}")
for s in "${STEPS[@]}"; do "step_$s"; done
say "done: $PREFIX"
