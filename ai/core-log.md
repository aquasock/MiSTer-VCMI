## 001 COMMIT Unreleased c7fffbe 2026-09-21T17:34:00-07:00

#### Coming From:

Unreleased e098077

#### Purpose:

Record a baseline of the project state at the adoption of the core.md development process.

#### Outcome:

This entry is a baseline reconstructed from the git history and the README and v0.1.0 release notes, not from build or test logs, so nothing in it was re-verified. The project runs VCMI 1.7.5 on the ARM (HPS) side of a MiSTer as an ordinary Linux program, using a PC cross-build for the Cortex-A9, custom SDL2 `mister` video and audio drivers under `sdl-driver`, three VCMI patches under `patches`, build and release scripts under `scripts`, and diagnostic tools under `tools`. Release `v0.1.0` was tagged at `2eadc9d` on 2026-09-20 and documented in `docs/release-notes/0.1.0.md`; it reports the adventure map at about 55 to 59 fps at rest and about 43 to 52 fps during hero movement at 800x600, with menus at about 30 fps and no video playback. Two commits followed the tag: `ca96b8f` removed FPGA project references from the README, and `e098077` changed the SDL driver to cache input device classification instead of reopening every node on each rescan. The `ai` project control folder was added in `c7fffbe`. No hardware validation of `e098077` is recorded anywhere in the history, so both Status boxes are left unchecked.

#### Next Steps:

Obtain the next objective from the user and follow the Standard Workflow by proposing a plan as a `???` entry before touching source. Two mismatches between core.md and the repository should be raised for the user to decide, since core.md is restricted and is not to be edited without an explicit request: the Build Environment section names Quartus Prime and a QMTech MiSTer while the project is now ARM-side only, and it directs work onto `master` while the repository branch is `main`. The `CHANGELOG.md` that the Releasing section expects does not exist yet and will be needed before the next release. The hardware validation state of `e098077` should also be established with the user.

#### Files Modified:

None.

#### Status:

- [ ] Built
- [ ] Passed

---

## 002 COMMIT Unreleased ??? 2026-09-21T17:44:08-07:00

#### Coming From:

Unreleased c7fffbe

#### Purpose:

Get the Horn of the Abyss (HotA) mod loading and playable on the MiSTer by dropping its files into a mods directory, with no VCMI launcher involved.

#### Outcome:

Proposed and approved by the user on 2026-09-21; no work is done yet. The user reports that release `v0.1.0` passes on their hardware, while `e098077` remains untested by the user's decision and is not part of what they run. Research so far found that the HotA port on the `vcmi-1.7` branch of `vcmi-mods/horn-of-the-abyss` is version 1.8.005 with a minimum VCMI version of 1.7.3, so it fits the pinned VCMI 1.7.5, and that it is licensed CC BY-SA 4.0. A summary of its manifest states that no original HotA game files are required, but that is unconfirmed and is verified in the first step. The `ModManager` source in `work/vcmi-src` at `lib/modding/ModManager.cpp` enables any mod that has no entry in `config/modSettings.json` unless its `mod.json` sets `keepDisabled`, so placing the mod folder under `data/Mods` should activate it without the launcher; this has not been run.

#### Next Steps:

First fetch the HotA release matching VCMI 1.7.5 on the PC and record its size, dependencies, submod list, `keepDisabled` submods and directory layout, and confirm whether original HotA files are needed. Second add a deterministic script under `tools` that fetches HotA at a pinned tag, verifies a checksum and lays out a drop-in `Mods` folder; the mod files are neither committed nor bundled into releases because of size and licence. Third the user copies the folder to `/media/fat/vcmi/data/Mods` and tests on hardware, recording load time, resident memory against the roughly 492 MiB available, the VCMI error log and the mod list in the client. Fourth the user plays a scenario using the Cove or Factory town and checks computer-player turn times against the current 4 to 8 seconds, frame rates, and save, load and clean exit. Last fix whatever the test exposes, then document mod installation in the README and install text and record the HotA attribution. If any step shows a finding that changes this plan, work stops until the user approves a revised plan.

#### Files Modified:

- tools/fetch-hota.sh
- tools/README.md
- README.md
- ATTRIBUTIONS.md

#### Status:

- [ ] Built
- [ ] Passed

---
