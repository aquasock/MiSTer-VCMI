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
