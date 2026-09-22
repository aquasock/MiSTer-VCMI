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

# Local fixes live in patches/; reset to the pristine clone and reapply all of them in order, every time.
# (Applying only the ones not yet present, via a per-patch reverse-check, breaks once two patches touch
# overlapping lines of the same function: after both are applied, the earlier patch's hunk context has been
# further changed by the later one, so the reverse-check for it can fail and apply_patches tries to apply it
# forward again against an already-patched tree. A full reset-and-reapply is simple and cheap enough
# (a handful of small patches) that there is no reason to keep the fragile per-patch shortcut.)
apply_patches() {
	git -C "$SRC_DIR" checkout -- .
	for p in "$ROOT"/patches/*.patch; do
		[ -e "$p" ] || continue
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
