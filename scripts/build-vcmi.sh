#!/usr/bin/env bash
# Cross-builds VCMI (client + server only) against work/prefix and installs to work/vcmi-install.
# Usage: scripts/build-vcmi.sh [configure|build]   (default: both)
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"

SRC_DIR="$WORK/vcmi-src"
B="$BUILD/vcmi"
INSTALL="$WORK/vcmi-install"

# Fetch the pinned VCMI release (with the submodules the build needs) if it is not there yet.
fetch_vcmi() {
	[ -d "$SRC_DIR/.git" ] && return 0
	git clone --depth 1 --branch "$VCMI_TAG" --recurse-submodules --shallow-submodules \
		https://github.com/vcmi/vcmi.git "$SRC_DIR"
}

# Local fixes live in patches/; apply any that are not already in the tree.
apply_patches() {
	for p in "$ROOT"/patches/*.patch; do
		[ -e "$p" ] || continue
		if git -C "$SRC_DIR" apply --reverse --check "$p" 2>/dev/null; then continue; fi
		git -C "$SRC_DIR" apply "$p"
	done
}

configure() {
	fetch_vcmi
	apply_patches
	cmake -S "$SRC_DIR" -B "$B" -G Ninja \
		-DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX="$INSTALL" \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		-DENABLE_CLIENT=ON -DENABLE_SERVER=ON \
		-DENABLE_LAUNCHER=OFF -DENABLE_EDITOR=OFF -DENABLE_LOBBY=OFF -DENABLE_TEST=OFF \
		-DENABLE_VIDEO=OFF -DENABLE_TRANSLATIONS=OFF -DENABLE_MMAI=OFF -DENABLE_DISCORD=OFF \
		-DENABLE_INNOEXTRACT=OFF -DENABLE_GITVERSION=OFF -DENABLE_PCH=OFF \
		-DENABLE_LUA=OFF -DENABLE_ERM=OFF \
		-DENABLE_MONOLITHIC_INSTALL=ON \
		-DVCMI_PORTMASTER=ON \
		-DLIBSQUISH_INCLUDE_DIR="$PREFIX/include" -DLIBSQUISH_LIBRARY="$PREFIX/lib/libsquish.a"
}

build() {
	fetch_vcmi
	apply_patches
	cmake --build "$B" -j"$JOBS"
	cmake --install "$B"
}

case "${1:-all}" in
	configure) configure ;;
	build) build ;;
	all) configure; build ;;
esac
