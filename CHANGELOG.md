# Changelog

All notable changes to this project are documented in this file, in the
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) style. This project
does not strictly follow Semantic Versioning for its MiSTer binary naming (see
the Versioning section of `ai/core.md`), but its own version numbers do.

## [Unreleased]

## [0.2.0] - 2026-09-21

### Added

- Horn of the Abyss (HotA) mod support: `tools/fetch-hota.sh` stages the mod and
  its `vcmi-extras` dependency, and a new `vcmi-hota` Scripts entry plays it,
  falling back to the base game if it was never fetched.
- In The Wake of Gods (WoG) mod support: `tools/fetch-wog.sh` stages the mod, and
  a new `vcmi-wog` Scripts entry plays it, falling back to the base game if it
  was never fetched. Each mod's Scripts entry loads only its own mod, even if
  more than one is staged at once.

### Changed

- SDL's software blitter, the dominant CPU cost during adventure-map scrolling,
  is faster: SDL's ARM NEON blitters are now built in (ported from the sibling
  MiSTer-GemRB project, including its fix for a real correctness bug in SDL's
  stock NEON alpha blitter), and VCMI's own `MapViewCache::render` merges
  adjacent tile blits into fewer, wider ones and, on a plain camera pan, reuses
  the previous frame (shifted) instead of recompositing the whole viewport every
  frame. Together these measurably reduce the stutter during hero movement,
  though the largest stutter spikes, which come from a different cause, remain.

### Fixed

- The OSD's "press ENTER to continue" prompt after quitting no longer shows a
  black screen; the console is now reliably put back into text mode regardless
  of how the game exited.

## [0.1.0] - 2026-09-20

First public preview.

### Added

- VCMI 1.7.5 cross-built for the MiSTer's Cortex-A9, with its own glibc and
  loader so it runs on the stock MiSTer root filesystem unmodified.
- SDL2 `mister` video driver, drawing through `/dev/fb0` with a vsync-paced
  presenter thread, and `mister` audio driver, playing through `/dev/MrAudio`.
- Mouse and keyboard support with USB hotplug.
- A Scripts-menu launcher that switches the HDMI output to 800x600 (or
  1024x600 widescreen) to fill the screen, and restores the previous mode
  afterwards.
- Two spectator-mode crash fixes and a software-renderer patch to VCMI.
- On-device diagnostics: frame-time statistics, a per-thread CPU profiler, and
  tools that drive the game with a virtual mouse and capture the framebuffer.
