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
