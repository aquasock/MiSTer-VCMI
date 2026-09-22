On-device test tools (copy to /tmp on the MiSTer and run with its python3). They drive and observe the game
without anyone at the controls; the display must be switched to the Linux framebuffer (F9, or an OSD script) so
Main_MiSTer releases the input devices.

- vin.py       virtual mouse + keyboard fed through /tmp/vin.cmd (`goto X Y`, `click`, `key ESC`, ...)
- fbcap.py     `raw OUT` dumps the framebuffer; `burst N` / `bbox N` report which screen regions change between frames
- fbpan.py     samples sparse framebuffer rows ~every ms and reports on-screen frame intervals + static-UI changes
- thrsample.py per-thread run/sleep state (and the syscall a sleeper is blocked in) of vcmiclient every 3 s
- profsym.py   runs on the PC: resolves an `SDL_MISTER_PROFILE` capture (copy `profile.txt`, `profile.txt.maps` and
               `profile.txt.threads` from the device's `cache/` directory) into hot functions per module and per thread;
               `--tid N` limits it to one thread

`scripts/deploy.sh tools` copies the `.py` files to the MiSTer's `/tmp` (a reboot wipes them). To drive a menu,
start `vin.py` first, then write commands such as `goto 640 187` and `click` to `/tmp/vin.cmd`.

## PC-side helper scripts

Unlike the tools above, these run on the build PC, not the device.

- `fetch-hota.sh` and `fetch-wog.sh` each download one mod (HotA also fetches its `vcmi-extras` dependency) and
  stage it as a drop-in `Mods` directory at `work/mods/Mods`, shared between them, with no VCMI launcher involved.
  Each checks its download against a pinned SHA-256 sum and drops submods whose dependencies are not present,
  touching only the mod it just fetched even though the directory is shared. Copy the staged directory's contents
  into `/media/fat/vcmi/data/Mods` on the MiSTer, or run `scripts/deploy.sh mods`. Mod files are not committed or
  bundled into releases (HotA is CC BY-SA 4.0 and about 0.5 GB; WoG's `mod.json` declares no license and is about
  0.15 GB); see [ATTRIBUTIONS.md](../ATTRIBUTIONS.md#mods). The **vcmi-hota** and **vcmi-wog** Scripts entries each
  load only what their own fetch script staged, even if both are present; **vcmi** never loads either. See
  [README.md#mods](../README.md#mods).
