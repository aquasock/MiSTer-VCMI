# MiSTer-VCMI

[VCMI](https://github.com/vcmi/vcmi), the open-source Heroes of Might and Magic III
engine, running on the ARM (HPS) side of a [MiSTer](https://github.com/MiSTer-devel)
(DE10-Nano, dual Cortex-A9). The game runs as an ordinary Linux program on the
stock MiSTer system: nothing is installed outside `/media/fat`, and the FPGA only
shows the picture and plays the sound. This project supplies the missing pieces: a
cross-build for the MiSTer's old userland, SDL2 video and audio drivers for the
MiSTer's framebuffer and audio device, and a launcher that fills the screen. It is
software, not an FPGA core, and sits beside the FPGA projects
[MiSTer-Raster](https://github.com/aquasock/MiSTer-Raster) and
[MiSTer-Phosphor](https://github.com/aquasock/MiSTer-Phosphor).

You supply your own copy of the game data. This project does not include it.

## What it does

- **VCMI 1.7.5, cross-built for the Cortex-A9** — client and server with the
  software renderer, built on a PC with the Ubuntu ARM cross toolchain. It ships
  its own glibc and loader, so it runs on the MiSTer's much older root filesystem
  without changing it.
- **SDL2 `mister` video driver** — draws through `/dev/fb0` (the `MiSTer_fb` frame
  reader). A presenter thread waits for vertical sync and copies frames, so the
  game thread never blocks on the display; the driver requests the resolution from
  Main_MiSTer and recovers if the framebuffer mode changes underneath it.
- **SDL2 `mister` audio driver** — plays 48 kHz stereo through `/dev/MrAudio`, the
  kernel ring buffer the FPGA plays out, keeping about 70 ms queued.
- **Mouse and keyboard** — read from the Linux input devices, with USB hotplug
  (the device list is rescanned every 1.5 seconds).
- **A launcher for the OSD Scripts menu** — switches the HDMI output to 800x600 so
  your display scales the picture to fill the screen, runs the game, and restores
  your mode afterwards, even after a crash.
- **Patches for VCMI** — two spectator-mode crash fixes and a change to its
  software renderer that removes a full-frame copy and a redundant clear, guarded
  by the interface lock so the minimap and resource counters never draw half-updated.
- **Diagnostics** — on-device frame-time statistics, a per-thread CPU profiler, and
  tools that drive the game with a virtual mouse and capture the framebuffer; see
  [tools/README.md](tools/README.md).

## Performance

Measured on a DE10-Nano MiSTer at 800x600. The figures depend on the save and the
scene, so treat them as a guide.

| Situation | Result |
| --- | --- |
| Menus | about 30 fps (the menu art is expensive to draw) |
| Adventure map at rest | about 55–59 fps |
| Hero movement and panning | about 43–52 fps, with occasional short hitches |
| Computer players' turns | about 4–8 seconds for three AIs early in a game |
| Memory | roughly 100–115 MB resident |
| Sound | gapless in 90 seconds of play: the audio queue never fell below about 64 ms |

Hero movement is the slowest case. Both Cortex-A9 cores are busy, and every
full-frame copy costs about 4 ms of the frame budget.

## Requirements

- A MiSTer with a DE10-Nano (Cyclone V SoC), running the Menu core, with a MiSTer
  Linux image that has the `MiSTer_fb` and `MrAudio` devices. Tested on the Buildroot
  image with Linux 6.18.38.
- A USB mouse and keyboard, and an HDMI display that accepts 800x600 at 60 Hz.
- Heroes of Might and Magic III data. The GOG "Heroes of Might and Magic 3 -
  Complete" release was used.
- To build: a Linux PC (Ubuntu 26.04 was used), about 3 GB of free disk space (plus
  about 1 GB for the extracted game data), and a network connection for fetching
  sources.

## Installation

1. Build the bundle (see [Building](#building)) or use a bundle you have already
   built.
2. Copy it to the MiSTer over SSH. `scripts/deploy.sh` logs in as `root` with the
   stock MiSTer password; edit `scripts/deploy.sh` if yours differs.

   ```sh
   MISTER_HOST=<your MiSTer's IP> scripts/deploy.sh code
   ```

   This installs the game to `/media/fat/vcmi` and the launcher to
   `/media/fat/Scripts/vcmi.sh`.
3. Extract the GOG installer and copy the game data across. The extraction needs
   `innoextract`; the video archive `VIDEO.VID` is skipped because video playback
   is not built.

   ```sh
   innoextract -d work/h3-extract "setup_heroes_of_might_and_magic_3_complete_....exe"
   MISTER_HOST=<your MiSTer's IP> scripts/deploy.sh data
   ```

4. On the MiSTer, open the OSD (F12), choose **Scripts**, and run **vcmi**. The
   screen blinks as the output switches to 800x600, and the game starts. Quit from
   the game's own menu; the launcher switches your display back.

To remove it, delete `/media/fat/vcmi` and `/media/fat/Scripts/vcmi.sh`.

## Configuration

Local settings go in `/media/fat/vcmi/env.sh`, which the launcher reads if it
exists. Everything works without it.

| Setting | Meaning |
| --- | --- |
| `MISTER_OUTPUT_MODE=800x600` | The default: HDMI output and VCMI resolution 800x600 |
| `MISTER_OUTPUT_MODE=1024x600` | Widescreen. About 28% more map width, about 44% more CPU per frame |
| `MISTER_OUTPUT_MODE=off` | Do not change the HDMI mode (and leave VCMI's resolution alone) |
| `MISTER_RESTORE_MODE="<modeline>"` | Mode to switch back to afterwards. Default is 1080p60 |

The modelines are `hact,hfp,hs,hbp,vact,vfp,vs,vbp,pixel-clock-kHz,hsync,vsync`,
the format Main_MiSTer's `video_mode` command takes.

VCMI's own settings are in `/media/fat/vcmi/save/settings.json`. The launcher
creates it on first run with the software renderer and cursor, exclusive
fullscreen, and error-only logging, and keeps the resolution in step with the
output mode.

The drivers read these environment variables. In `env.sh` they need `export`, for
example `export SDL_MISTER_VSYNC=0`:

| Variable | Effect |
| --- | --- |
| `SDL_MISTER_VSYNC=0` | Do not wait for vertical sync before each copy |
| `SDL_MISTER_ASYNC=0` | Copy from the game thread instead of a presenter thread |
| `SDL_MISTER_ZEROCOPY` | Hand the renderer's buffers to the presenter without copying (the launcher sets 1; only valid for programs that redraw the whole window each frame) |
| `SDL_MISTER_FORMAT=xrgb` | Window pixel format (default `argb`) |
| `SDL_MISTER_FB=WxH` | Force the framebuffer size |
| `SDL_MISTER_NOMODE=1` | Never ask Main_MiSTer to resize the framebuffer |
| `SDL_MISTER_AUDIO_MS` | Audio queue depth in milliseconds (default 70) |
| `SDL_MISTER_STATS=<file>` | Log frame timing every 3 seconds; `/tmp/mister_vsync` and `/tmp/mister_async` then toggle vsync and async live |
| `SDL_MISTER_PROFILE=<file>` | Sample where each thread spends CPU; resolve it with `tools/profsym.py` |
| `SDL_MISTER_DEBUG=1` | Log what the drivers do |

## Current limitations

- **800x600 is the supported size.** VCMI needs at least 800x600, and the FPGA
  scaler only multiplies the framebuffer by whole numbers, so the launcher
  changes the HDMI mode and lets your display do the scaling. Larger sizes cost
  CPU roughly in proportion to their pixels; 1024x600 is the cheapest widescreen
  option.
- **Hero movement is not perfectly smooth.** See [Performance](#performance).
- **No video playback.** VCMI's FFmpeg support is off, so intro and campaign
  videos are skipped.
- **Not built:** VCMI's launcher, map editor, lobby server, ERM and Lua scripting,
  and the machine-learning AI. SDL_mixer is built without MIDI, tracker-module,
  Opus and WavPack support.
- **Only the GOG Heroes III Complete data has been tested.** Other editions,
  mods, campaigns in depth and networked multiplayer have not.
- **Your original HDMI mode is not saved.** The MiSTer here has no `MiSTer.ini`, so
  the launcher restores 1080p60. Set `MISTER_RESTORE_MODE` if your display uses
  something else.
- **VCMI 1.7.5 bugs that remain:** `--headless` crashes on a null pointer, and
  AI-only spectator mode (`--testmap`, `--onlyAI`) still has one unfixed crash.
  Normal play is unaffected.
- **VCMI ignores `SIGTERM`.** Quit from the game menu, or use `kill -KILL`; the
  launcher restores the display either way.
- VCMI writes about 200 log lines at startup before it reads its settings. After
  that, only errors are logged.

## Building

Install the prerequisites on an Ubuntu (or Debian) PC:

```sh
sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
    libc6-dev-armhf-cross cmake ninja-build pkg-config patchelf patch \
    qemu-user-binfmt git curl python3 dpkg innoextract
```

Then, from the repository root:

```sh
scripts/build-deps.sh   # Boost, zlib, minizip-ng, TBB, libsquish, SDL2 and its libraries
scripts/build-vcmi.sh   # fetches VCMI 1.7.5, applies patches/, builds client and server
scripts/bundle.sh       # assembles work/bundle: binaries, libraries, glibc, launcher
scripts/deploy.sh all   # copies the bundle and the game data to the MiSTer
```

Everything is created under `work/`, which is not tracked, and the pinned versions
are in `scripts/env.sh`. Each dependency step leaves a stamp, so a rerun skips
finished work; changing the SDL drivers or patches rebuilds SDL automatically.
Downloads come from GitHub, Debian, and Ubuntu's ports archive.

`scripts/deploy.sh tools` copies the on-device test tools to the MiSTer's `/tmp`,
which a reboot wipes.

## Source layout

- `scripts/` — `env.sh` (versions and paths), `toolchain.cmake`, `build-deps.sh`,
  `build-vcmi.sh`, `bundle.sh` (also generates the launcher) and `deploy.sh`.
- `sdl-driver/mister/` — the SDL2 video driver; `sdl-driver/mister-audio/` — the SDL2
  audio driver.
- `sdl-driver/*.patch` — the changes to SDL that register the drivers and speed up
  its audio resampler.
- `sdl-driver/*.c` — small test programs: `sdl_test.c`, `sdl_bench.c` (the present
  pipeline), `sdl_audiotest.c` and `membench.c` (framebuffer copy speed).
- `patches/` — the changes to VCMI, applied in order by `build-vcmi.sh`.
- `tools/` — on-device test and profiling tools; see [tools/README.md](tools/README.md).
- `work/` — created by the scripts: downloads, sources, build trees, the install
  prefix and the bundle. Not tracked.

## Documentation

- [Tools](tools/README.md)
- [Source attributions](ATTRIBUTIONS.md)
- [Upstream VCMI documentation](https://github.com/vcmi/vcmi/tree/develop/docs)

## License

Original project code is licensed GPL-2.0-or-later (see `LICENSE.txt`), the same
baseline as MiSTer-Raster and MiSTer-Phosphor. The complete bundle contains GPL-3.0
components, so it is distributed under GPL-3.0-or-later (`COPYING`). VCMI, SDL and
the other libraries keep their own licenses; see [ATTRIBUTIONS.md](ATTRIBUTIONS.md)
for the full inventory, the third-party license texts to ship with binaries, and
the redistribution checklist.
