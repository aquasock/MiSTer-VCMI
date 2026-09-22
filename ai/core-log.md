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

## 005 COMMIT Unreleased c300672 2026-09-21T19:28:58-07:00

#### Coming From:

Unreleased c300672

#### Purpose:

Record hardware validation of the `vcmi`/`vcmi-hota` launcher split from `c300672`.

#### Outcome:

The user reported "it works great, the build passes" after running both Scripts entries on hardware: a blanket pass with no numeric or per-script detail volunteered, so this only confirms that `vcmi` and `vcmi-hota` both work as intended, not that the "expected to reproduce" content claim in entry 004 was checked in detail.

#### Next Steps:

Obtain the next objective from the user. Hardware-validated HotA support, now split into its own launcher entry, remains a release candidate under the Versioning section of core.md; `CHANGELOG.md` is still missing and still needed before any tag.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 006 COMMIT Unreleased 6ce86c2 2026-09-21T20:01:12-07:00

#### Coming From:

Unreleased c300672

#### Purpose:

Investigate the user's long-standing report that the adventure map stutters badly during hero movement but nowhere else, and fix what the investigation found.

#### Outcome:

Diagnosed live on the user's hardware using the on-device tools plus the driver's own `SDL_MISTER_STATS`/`SDL_MISTER_PROFILE` instrumentation (the first attempt used relative paths, which the driver silently drops for `SDL_MISTER_STATS` and which also failed for `SDL_MISTER_PROFILE`, most likely because VCMI changes its working directory at startup; absolute paths fixed it). Idle screens ran about 45 to 58 fps with a 4 to 13 ms per-frame game-thread cost and 50 to 90 ms worst-case gaps; hero movement ran about 20 to 50 fps, jittery, with a 13 to 45 ms per-frame cost and 100 to 900 ms worst-case gaps; ending a turn produced separate multi-second stalls where the game thread was mostly blocked rather than computing, which is a different, expected phenomenon the user was not describing. A CPU profile during movement found the dominant costs in SDL2's scalar software blit routines, particularly the colorkey blitter `BlitNtoNKey`, and confirmed in VCMI's own source (`client/mapView/MapViewCache.cpp`) that camera movement disables the renderer's lazy-update path, forcing a full-viewport recomposite every frame, which idle screens and battle do not do; this is architectural to VCMI's renderer, not a bug introduced by this project. The user pointed at the sibling MiSTer-GemRB project (same author, same Cortex-A9 target), which had already solved the same class of problem: its `neon-blit-opaque-dst.patch` fixes a real correctness bug in SDL's stock ARM NEON alpha blitter, which leaves destination alpha unblended where the C blitter blends it, by restricting the accelerated path to destinations without an alpha channel; its `blendfillrect-neon.patch` adds a NEON translucent-rectangle fill that is bit-identical to the C code; and its `enable-asm.cmake` project-include works around SDL's NEON detection needing a runnable test binary, which `try_run` cannot reliably give under cross-compilation even with this project's own `qemu-arm` emulator wired in. Both patches were ported unchanged into `sdl-driver/`, wired into `scripts/build-deps.sh`'s `step_sdl2` with `-DSDL_ARMNEON=ON -DARMNEON_FOUND=1`, and confirmed by disassembly to be compiled in (63 inlined NEON vector instructions in the alpha-fill path; the NEON alpha-blit dispatch and its external pixman assembly symbol both present). Neither patch touches `BlitNtoNKey`, the colorkey blitter that dominated the profile; stock SDL2 has no NEON acceleration for it. Deployed directly to the user's MiSTer and validated with a clean before/after `SDL_MISTER_STATS` comparison: the user saw no visual glitches, and the data shows the movement-phase average per-frame cost fell from about 16.8 to 15.7 ms (roughly 6 to 7 percent) and idle-screen smoothness improved more clearly, worst-case gaps falling from 50 to 90 ms to a tight 20 to 24 ms and per-frame cost from about 4.2 to 3.7 ms; the large 600 to 900 ms stutter spikes during movement are unchanged, as expected, since they come from the untouched colorkey path. The user asked to keep the fix.

#### Next Steps:

Obtain the next objective from the user. A larger fix for the movement stutter itself would mean patching VCMI's own `MapViewCache` to avoid recompositing the full viewport every scrolled frame, which is a bigger and riskier change to upstream game logic than anything done here and was not undertaken in this cycle; it remains available as a future option if the user wants to pursue it. `CHANGELOG.md` is still missing and still needed before any release.

#### Files Modified:

- scripts/build-deps.sh
- sdl-driver/sdl2-neon-blit-opaque-dst.patch
- sdl-driver/sdl2-blendfillrect-neon.patch
- scripts/enable-asm.cmake
- ATTRIBUTIONS.md

#### Status:

- [x] Built
- [x] Passed

---

## 007 COMMIT Unreleased c09a006 2026-09-21T20:22:15-07:00

#### Coming From:

Unreleased 6ce86c2

#### Purpose:

Reduce the movement-stutter cost that `6ce86c2`'s NEON blitters did not touch, by cutting the per-frame blit call count in VCMI's own adventure-map compositing.

#### Outcome:

Traced the exact mechanism in VCMI's source: the per-tile content refresh (`MapRenderer::renderTile`, the expensive `BlitNtoNKey`-heavy sprite compositing) is driven by an animation timer (`animationTime / 180ms` in `MapRendererContext.cpp`) and runs at roughly the same amortized rate whether the camera is moving or not, so it is not the movement-specific cost. What is movement-specific is `MapViewCache::render`'s final compositing step: it disables its own skip-unchanged-tiles optimization whenever the camera moves, forcing every visible tile (several hundred) to be blitted individually into the target every frame. Confirmed that consecutive visible tiles are also contiguous in VCMI's tile cache except at one wraparound seam per row (the cache is a ring buffer sized to the visible tile count), so `patches/0004-mapview-batch-tile-blits.patch` merges runs of adjacent tiles that all need drawing into one wider blit instead of one call per tile; this changes call count only; the merged blit copies exactly the pixels the per-tile blits already did, so the output is provably unchanged. Chose this over the bigger "shift the previous frame and redraw only the exposed edge" design (discussed with the user as option 2) as a lower-risk first step, since it cannot introduce a visual difference from the unpatched behavior. Verified by compiling successfully, applying cleanly to a pristine checkout, and a clean `vcmiserver --dummy-run` regression pass on the PC; a headless client smoke test was attempted but abandoned as not worth the setup cost, since VCMI's own `--testsave`/`--testmap`/`--headless` paths are unreliable for this outside real hardware (an unrelated pre-existing crash on a nonexistent test save was hit and traced away from the patch). Deployed directly to the user's MiSTer; the user reported no visual glitches after walking around. A clean stats comparison against the prior (NEON-only) baseline, restricted to the steady walking portion of the capture past load and cache-warmup, showed the average per-frame game-thread cost fall from about 15.7 to 13.7 ms, a further roughly 13 percent on top of the NEON fix and about 18 percent from the original baseline; the worst-case 500 to 900 ms stutter spikes during movement were unchanged, as expected, since this patch addresses steady per-frame overhead, not the periodic large stalls. The user asked to keep this patch and proceed to the bigger frame-shift design (option 2) to address those remaining spikes.

#### Next Steps:

Design and implement option 2: keep a persisted snapshot of the previously composited map view, blit it shifted by the pan delta into the target in one call, then redraw only the newly exposed edge plus whichever tiles `updateTile` actually flagged as changed this frame, falling back to the current full-redraw behavior on zoom change, view transitions, or a camera jump larger than one screen. This needs a genuinely separate snapshot buffer (not a same-surface self-blit, which SDL2 does not guarantee is safe for overlapping regions) and careful shift-direction and edge-region math; it is materially riskier than this entry's change and needs the same build/deploy/user-visual-check/stats-comparison cycle before being kept.

#### Files Modified:

- patches/0004-mapview-batch-tile-blits.patch
- ATTRIBUTIONS.md

#### Status:

- [x] Built
- [x] Passed

---

## 008 COMMIT Unreleased d2965cf 2026-09-21T20:31:54-07:00

#### Coming From:

Unreleased c09a006

#### Purpose:

Implement option 2 (the frame-shift design) to address the worst-case movement stutter spikes that `c09a006` left unchanged, per the user's approval.

#### Outcome:

`patches/0005-mapview-fast-pan.patch`, on top of `0004`, adds a `previousFrame` snapshot to `MapViewCache`: on a plain camera pan (nothing else changed), it blits that snapshot shifted by the pan delta in one call, then redraws only the newly exposed edge and whichever tiles `updateTile` actually flagged as changed this frame, reusing `0004`'s run-merging for those. The snapshot is taken right after the terrain-compositing loop, before overlays or a view-transition blend, so a later fast-pan frame shifts terrain only and overlays keep redrawing themselves fresh every frame as before. `canFastPan` is deliberately narrow: it requires no overlay-visibility change, no full-redraw request, no view transition in progress, a valid snapshot whose recorded tile size still matches the current one, a snapshot pixel size matching the current viewport, and a shift smaller than the viewport. Two correctness bugs were found and fixed during design, not left for hardware testing to catch: first, `MapViewCache::update()` (called every frame before `render()`) overwrites `cachedSize` with the *current* tile size before `render()` can compare it, so `cachedSize` cannot detect a zoom change the way `cachedPosition` detects a pan; a new `previousFrameTileSize`, set only when the snapshot itself is taken, is used instead. Second, a same-surface self-blit (shifting `target` onto itself) is not guaranteed safe by SDL2 for overlapping regions, so the snapshot is a genuinely separate `Canvas`/`SDL_Surface`, never blitted into itself. A third, unrelated issue surfaced only when actually building: `scripts/build-vcmi.sh`'s `apply_patches` used a per-patch reverse-apply check to decide what still needed applying, which breaks once two patches (`0004` and `0005`) touch overlapping lines of the same function, since after both are applied the earlier patch's hunk context has been further changed by the later one and its reverse-check can fail; `apply_patches` now resets the source tree to the pristine clone and reapplies every patch in order every time, which is simple, cheap at this patch count, and correct regardless of how patches overlap. Verified compiling cleanly, applying cleanly in the full five-patch sequence from a pristine checkout, idempotent on a second `build-vcmi.sh build` run (the exact scenario that exposed the `apply_patches` bug), and a clean `vcmiserver --dummy-run` regression pass on the PC. Deployed to the user's MiSTer; no game was running at deploy time. Not yet visually validated on hardware: this is a genuine rendering-logic change, not a provably-identical transformation like `0004`, so it needs real testing, specifically including a zoom (mouse wheel) while panning, which exercises a guard that PC-side testing cannot reach.

#### Next Steps:

Have the user walk around, watching specifically for ghosting or tearing at the screen edges during panning, any stale content when panning resumes after being idle, and any corruption when zooming while moving. If clean, capture a before/after `SDL_MISTER_STATS` comparison focused on the worst-case gap figures (the 500 to 900 ms spikes `0004` left unchanged), since that is what this change is meant to fix and the earlier two rounds did not touch.

#### Files Modified:

- patches/0005-mapview-fast-pan.patch
- scripts/build-vcmi.sh
- ATTRIBUTIONS.md

#### Status:

- [x] Built
- [ ] Passed

---

## 009 COMMIT Unreleased d2965cf 2026-09-21T20:41:39-07:00

#### Coming From:

Unreleased d2965cf

#### Purpose:

Record hardware validation of the fast-pan patch (`d2965cf`) and close out the hero-movement stutter investigation.

#### Outcome:

The user reported no visual glitches, including after zooming while moving, so the design's correctness guards held on real hardware. A `SDL_MISTER_STATS` capture of a walk taken right after ending a turn (the user was out of movement points) showed two clearly different phases: about 60 seconds of the turn's own processing stall followed by settling-in movement, with worst-case gaps still ranging 40 to 1431 ms, matching the pre-`d2965cf` pattern; then a sustained 30-second stretch of continued walking with worst-case gaps tightly between 37 and 57 ms and a steady ~14 ms per-frame cost, the smoothest and most consistent result of this entire investigation, well beyond what patches `c09a006` or `6ce86c2` achieved alone. The user confirmed this matched their own long, uninterrupted stretch of walking. The rough phase is concentrated at the start of essentially every walk, since the user always takes a turn immediately before walking, so it likely dominates what gets noticed even though the sustained portion afterward is a real, large improvement; the turn-processing stall itself remains unaddressed by any of this investigation's patches, confirmed as a separate phenomenon since entry 003. The user judged the result "not perfect, but almost" and asked to leave it as is rather than continue.

#### Next Steps:

Obtain the next objective from the user. Two candidate follow-ups are now on record if the user wants to return to this later: the turn-processing stall itself (multi-second, blocked-not-computing, likely AI or local client-server round-trip cost, never investigated) and the rough settling-in period immediately after a turn ends and movement resumes (not isolated from the turn stall itself in this investigation). `CHANGELOG.md` is still missing and still needed before any release; three real, hardware-validated performance fixes (NEON blitters, batched tile blits, fast-pan) are now release candidates alongside HotA support.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 010 COMMIT Unreleased e098077 2026-09-21T20:44:19-07:00

#### Coming From:

Unreleased d2965cf

#### Purpose:

Record hardware validation of `e098077`, open and untested since entry 001.

#### Outcome:

`e098077` (SDL driver: cache input device classification instead of reopening every node on each rescan) has, without anyone deliberately arranging it, been running on the user's MiSTer for the entire stutter investigation: every SDL2 rebuild since `6ce86c2` picks up whatever is currently in `sdl-driver/mister/`, which has included `e098077`'s change since before this session began, and the deployed `libSDL2-2.0.so.0` was confirmed byte-identical to the local build. Hours of ordinary play across the NEON, batched-blit and fast-pan test cycles turned up no input problems, which is de facto evidence for basic functionality but does not specifically exercise the hotplug path the commit changed. The user has separately tested that path directly: unplugging and replugging a USB mouse or keyboard while the game runs continues to work. Both Status boxes, left unchecked since entry 001 for lack of any hardware test, are now checked.

#### Next Steps:

Obtain the next objective from the user. With this closed, the only items left on the standing list are the turn-processing stall (never investigated), the missing `CHANGELOG.md`, and the accumulated release candidate (HotA support plus the NEON, batched-blit and fast-pan performance fixes) still waiting on a version decision.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 011 COMMIT Unreleased e098077 2026-09-21T20:46:06-07:00

#### Coming From:

Unreleased e098077

#### Purpose:

Record the user's judgment on the turn-processing stall raised in entry 009's Next Steps, without a dedicated investigation.

#### Outcome:

Asked whether to investigate the multi-second, mostly-blocked-not-computing gaps observed around ending a turn in earlier stats captures. The user reports the turn takes a normal amount of time from their own observations, matching the AI thinking time the project's README has documented since before this engagement began (about 4 to 8 seconds for three AIs early in a game). No dedicated investigation was done; this closes the item on the user's own assessment rather than on measured evidence.

#### Next Steps:

Obtain the next objective from the user. The remaining standing items are the missing `CHANGELOG.md` and the accumulated release candidate (HotA support plus the NEON, batched-blit and fast-pan performance fixes) waiting on a version decision.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 012 COMMIT Unreleased ??? 2026-09-21T21:02:46-07:00

#### Coming From:

Unreleased e098077

#### Purpose:

Add In The Wake of Gods (WoG) as a second mod, following the same drop-in pattern as HotA, per the user's direct instruction.

#### Outcome:

Researched WoG the same way as HotA in entry 002: release `1.7` of `vcmi-mods/wake-of-gods`, version 9.1.82, about 99 MB, `mod.json` declares no license (same as `vcmi-extras`). Unlike HotA it needs no sibling mod; its only external references are optional `mithril` compatibility submods for other terrain mods (including HotA's), which are pruned when those mods are not also staged. `tools/fetch-wog.sh` mirrors `tools/fetch-hota.sh`. Building it exposed two problems in the existing HotA tooling that a single-mod design had not needed to handle. First, the shared staging directory (renamed `work/hota-mods` to `work/mods` since it now holds more than one mod) was being `rm -rf`'d wholesale before each install and its dependency-pruning pass scanned every top-level folder under it, so fetching one mod could delete or prune another's files; both scripts now touch only their own subfolder and only prune within it, while still checking dependencies against everything staged. Second, and more subtly: WoG's own submods cross-reference each other using its upstream folder name (`wake-of-gods.creatures`, for example), and VCMI derives a mod's id from its folder name, so the initial attempt to shorten the staged folder to `wog` (matching the intended `vcmi-wog` launcher name) silently broke those internal references, causing `tools/fetch-wog.sh`'s own pruning pass to wrongly delete submods whose dependencies were, in fact, present (`wog.stackexperience` and `wog.creaturebanks` among others). Caught by inspecting the prune list rather than by a build or hardware failure. Fixed by keeping the staged folder as `wake-of-gods` and mapping the short `wog` preset name to it in `scripts/bundle.sh`'s `select_preset`, which was also generalized while fixing this: it previously hardcoded a single "hota" preset that blindly included every top-level mod folder found, which would have loaded WoG and HotA together the moment both were staged; it now uses a small table of preset name to (root mod folder, declared sibling mods), so each Scripts entry loads only its own mod, confirmed with both mods staged simultaneously. Verified: both fetch scripts run correctly together (regression-tested that the fix did not change HotA's kept/removed submod counts), all three presets (`vcmi`, `hota`, `wog`) build correctly under a PC test harness including cross-contamination checks in both directions, the `wog`-not-installed fallback works, and a real `vcmiserver --dummy-run` loads WoG's content cleanly (53 mods, "All game content loaded", no errors) as well as a HotA regression re-check (72 mods, unchanged from before). Deployed directly to the user's MiSTer (no game was running); `Scripts/vcmi-wog.sh`, `data/Mods/wake-of-gods` (170 MB) and the updated `run.sh` all confirmed present. Not yet visually validated on hardware.

#### Next Steps:

Have the user run **vcmi-wog** from the OSD and check for visual glitches, confirm the base game (**vcmi**) still shows no mod content, and if both HotA and WoG are staged, confirm each Scripts entry only loads its own mod. `CHANGELOG.md` is still missing and still needed before any release; the release candidate now includes two mods and three performance fixes.

#### Files Modified:

- tools/fetch-wog.sh
- tools/fetch-hota.sh
- scripts/bundle.sh
- scripts/deploy.sh
- README.md
- tools/README.md
- ATTRIBUTIONS.md

#### Status:

- [ ] Built
- [ ] Passed

---
