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

## 002 COMMIT Unreleased 8047fc9 2026-09-21T17:44:08-07:00

#### Coming From:

Unreleased c7fffbe

#### Purpose:

Get the Horn of the Abyss (HotA) mod loading and playable on the MiSTer by dropping its files into a mods directory, with no VCMI launcher involved.

#### Outcome:

Proposed and approved by the user on 2026-09-21; no work is done yet. The user reports that release `v0.1.0` passes on their hardware, while `e098077` remains untested by the user's decision and is not part of what they run. Research so far found that the HotA port on the `vcmi-1.7` branch of `vcmi-mods/horn-of-the-abyss` is version 1.8.005 with a minimum VCMI version of 1.7.3, so it fits the pinned VCMI 1.7.5, and that it is licensed CC BY-SA 4.0. A summary of its manifest states that no original HotA game files are required, but that is unconfirmed and is verified in the first step. The `ModManager` source in `work/vcmi-src` at `lib/modding/ModManager.cpp` enables any mod that has no entry in `config/modSettings.json` unless its `mod.json` sets `keepDisabled`, so placing the mod folder under `data/Mods` was expected to activate it without the launcher. A test on the PC on 2026-09-21 showed that is wrong for root mods: `vcmiserver --dummy-run` under qemu with the staged mods detected the HotA submods but loaded only `vcmi` and `core`, because root mods must be listed in the active preset of `save/modSettings.json`. With `hota` and `vcmi-extras` added to that preset the same run loaded 72 mods and reported all game content loaded with no errors; the emulated timing says nothing about MiSTer speed or memory. The HotA 1.7 release is 442 MB, has a submod that depends on `vcmi-extras`, and needs no original HotA game files. The user approved a revised plan that adds an automatic enable step to the launcher script. That plan is now built and committed at `8047fc9`. `tools/fetch-hota.sh` fetches HotA and `vcmi-extras` from the official VCMI 1.7 mod repository listing, checks pinned SHA-256 sums, and stages `work/hota-mods/Mods`, dropping 13 of 79 submods whose dependencies are unmet (translation buttons, voice packs, and `vcmi-extras.chroniclesicon`). The `run.sh` template in `scripts/bundle.sh` gained an `enable_mods` step that lists every mod folder under `data/Mods` into the active preset of `save/modSettings.json` on each start; `scripts/deploy.sh` gained a `mods` action and a `code` deploy now only clears `data/Mods/vcmi`, not the whole directory. Verified on the PC against the actual bundled ARM `vcmiserver` under qemu: `enable_mods` run from the real `run.sh` correctly wrote the preset, was idempotent on a second run, and preserved a manually-set submod override; a `--dummy-run` with the staged mods enabled loaded 72 mods and reported all game content loaded with no errors, only the same unrelated map-format warnings seen without the mods. None of this is hardware validation; the emulated run says nothing about MiSTer load time, memory or gameplay.

#### Next Steps:

The user runs `tools/fetch-hota.sh` then `scripts/deploy.sh mods` (or copies `work/hota-mods/Mods` by hand) onto a MiSTer already running the code from this commit, launches vcmi from the OSD, and tests on hardware: load time, resident memory against the roughly 492 MiB available, the VCMI log, and the mod list the client reports. Then a scenario using the Cove or Factory town, checking computer-player turn times against the current 4 to 8 seconds, frame rates, and a clean save, load and exit. The next entry records those results; if they show a finding that changes this plan, work stops until the user approves a revision, per the Standard Workflow.

#### Files Modified:

- tools/fetch-hota.sh
- tools/README.md
- scripts/bundle.sh
- scripts/deploy.sh
- README.md
- ATTRIBUTIONS.md

#### Status:

- [x] Built
- [ ] Passed

---

## 003 COMMIT Unreleased 8047fc9 2026-09-21T19:17:17-07:00

#### Coming From:

Unreleased 8047fc9

#### Purpose:

Record hardware validation of the Horn of the Abyss mod support from `8047fc9`.

#### Outcome:

The agent deployed `8047fc9` directly to the user's MiSTer with `scripts/deploy.sh code` then `scripts/deploy.sh mods`, and confirmed on the device before handing off that `hota`, `vcmi-extras` and `vcmi` were all present under `data/Mods`, that `run.sh` carried the new `enable_mods` step, and that game data and existing saves were untouched, with 452 GB of SD card and 454 MB of RAM free. The agent did not launch the game itself, since that would switch the user's live HDMI output. The user then played on hardware and reported "it works great, everything passes," a blanket pass with no numeric detail volunteered, so no frame-time, memory or load-time figures beyond the pre-launch check above are recorded here.

#### Next Steps:

Obtain the next objective from the user. Hardware-validated HotA support is a new capability, which the Versioning section of core.md ties to a MINOR bump (0.1.0 to 0.2.0); this should be raised with the user as a release candidate. A release would need `CHANGELOG.md`, flagged as missing since entry 001 and still absent, plus a full regression pass per the Releasing section before any tag.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 004 COMMIT Unreleased c300672 2026-09-21T19:24:53-07:00

#### Coming From:

Unreleased 8047fc9

#### Purpose:

Split the single mod-enabling launcher into a `vcmi` script that always plays the base game and a `vcmi-hota` script that plays HotA when it is installed, per the user's direct instruction.

#### Outcome:

`select_preset` replaces the prior `enable_mods` in the `run.sh` template generated by `scripts/bundle.sh`. It takes the wanted preset from `VCMI_PRESET` (set by the calling Scripts entry), always forces the `vcmi` preset's mod list to exactly `vcmi` and `core` regardless of what is sitting in `data/Mods`, and builds a `hota` preset from `vcmi`, `core` and every mod folder found only when `data/Mods/hota` is present, falling back silently to the `vcmi` preset otherwise. Per-submod settings inside each preset are left untouched, since VCMI keeps no in-client mod manager (that UI is only in the separate Launcher app this project does not build) and the only way to disable one is to edit `modSettings.json` directly. `scripts/bundle.sh` now writes both `Scripts/vcmi.sh` (`VCMI_PRESET=vcmi`) and `Scripts/vcmi-hota.sh` (`VCMI_PRESET=hota`); `scripts/deploy.sh` needed no change, since its `code` target already copies every `Scripts/*.sh`. Verified on the PC with the real bundled `run.sh` and the ARM `vcmiserver` under qemu: the base preset loads only `core` and `vcmi` with no HotA content, the HotA preset loads HotA and `vcmi-extras`, removing `data/Mods/hota` makes `VCMI_PRESET=hota` fall back to the base preset, the base preset is idempotent, and a submod disabled by hand in the `hota` preset stays disabled (and correctly cascades to submods that depend on it) across reruns. The agent then committed `c300672`, deployed it directly to the user's MiSTer with `scripts/deploy.sh code`, and confirmed on the device (without launching the game) that both `Scripts/vcmi.sh` and `Scripts/vcmi-hota.sh` are present with the right content and that `run.sh` contains `select_preset`. The device's existing `modSettings.json`, from the prior cycle's hardware-validated `default` preset, has a `settings` block for `hota` and `vcmi-extras` that is not a deliberate user customization: every disabled entry in it exactly matches a submod whose `mod.json` sets `keepDisabled`, which is what `ModManager.cpp` writes automatically the first time a preset resolves those mods. The new `hota` preset has no `settings` block yet, but VCMI computes the same defaults from `keepDisabled` the first time it is used, so the first `vcmi-hota` launch is expected to reproduce that already-validated content exactly.

#### Next Steps:

Have the user run **vcmi** and **vcmi-hota** from the OSD to confirm the base game loads with no HotA content and HotA loads as it did under the old single-preset launcher, and to catch anything the "expected to reproduce" claim above got wrong.

#### Files Modified:

- scripts/bundle.sh
- README.md
- tools/README.md

#### Status:

- [x] Built
- [ ] Passed

---

---
