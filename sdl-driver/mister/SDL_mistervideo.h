/*
  MiSTer framebuffer video driver for SDL2.

  Presents the window surface through /dev/fb0 (the MiSTer_fb frame reader that
  Main_MiSTer scans out via the FPGA scaler) and reads input through SDL's evdev
  code. Resolution changes go through Main_MiSTer's fb_cmd1 on /dev/MiSTer_cmd.

  Environment (all optional):
    SDL_MISTER_FBDEV   framebuffer device (default /dev/fb0)
    SDL_MISTER_VSYNC   0 to skip waiting for vertical sync before each present
    SDL_MISTER_FB      WxH to force the framebuffer size instead of following the window
    SDL_MISTER_NOMODE  1 to never send fb_cmd1; keep whatever size fb0 already has
    SDL_MISTER_NOVT    1 to leave the virtual terminal in text mode
    SDL_MISTER_DEBUG   1 to log what the driver does to stderr
    SDL_MISTER_ASYNC   0 to present from the game thread (default: a presenter thread waits for vsync and copies)
    SDL_MISTER_ZEROCOPY 1 to hand the window surface's own buffers to the presenter (renderer apps only)
    SDL_MISTER_FORMAT  argb (default) or xrgb: window surface pixel format; the scaler ignores alpha
    SDL_MISTER_STATS   1 or a file path: log presents/s, timings; /tmp/mister_vsync and /tmp/mister_async toggle live
*/
#ifndef SDL_mistervideo_h_
#define SDL_mistervideo_h_

#include "../SDL_sysvideo.h"

extern VideoBootStrap MISTER_bootstrap;

#endif /* SDL_mistervideo_h_ */
