# Shared settings for the MiSTer-VCMI cross build. Source this file.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$ROOT/work"
DL="$WORK/dl"
SRC="$WORK/src"
BUILD="$WORK/build"
LOGS="$WORK/logs"
PREFIX="$WORK/prefix"          # armhf install prefix for all dependencies
SYSROOT=/usr/arm-linux-gnueabihf
TOOLCHAIN="$ROOT/scripts/toolchain.cmake"
JOBS="${JOBS:-$(nproc)}"

VCMI_TAG=1.7.5
BOOST_VER=1.90.0
ZLIB_VER=1.3.1
MINIZIP_VER=4.0.10
TBB_VER=2022.3.0
SDL2_VER=2.32.10
SDL2_IMAGE_VER=2.8.12
SDL2_MIXER_VER=2.8.1
SDL2_TTF_VER=2.24.0
SQUISH_VER=1.15

export VCMI_PREFIX="$PREFIX"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$PREFIX/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR=""
mkdir -p "$DL" "$SRC" "$BUILD" "$LOGS" "$PREFIX"
