/* Times the per-frame present pipeline VCMI uses (streaming texture update, clear, copy, present) so
   driver options can be compared without playing. Usage: sdl-bench [--size WxH] [--frames N] [--noclear] [--direct] */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double ms(Uint64 ticks) { return 1000.0 * (double)ticks / (double)SDL_GetPerformanceFrequency(); }

int main(int argc, char **argv)
{
    int w = 800, h = 600, frames = 300, noclear = 0, direct = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &w, &h);
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--noclear")) noclear = 1;
        else if (!strcmp(argv[i], "--direct")) direct = 1;
    }
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    SDL_Window *win = SDL_CreateWindow("bench", 0, 0, w, h, SDL_WINDOW_FULLSCREEN);
    SDL_Renderer *r = SDL_CreateRenderer(win, -1, 0);
    if (!win || !r) { fprintf(stderr, "window/renderer: %s\n", SDL_GetError()); return 1; }
    SDL_RendererInfo ri; SDL_GetRendererInfo(r, &ri);
    SDL_RenderSetLogicalSize(r, w, h);
    SDL_Surface *screen = SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    SDL_SetSurfaceBlendMode(screen, SDL_BLENDMODE_NONE);
    SDL_Texture *tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (direct) {
        /* software renderer only: a streaming texture is plain memory, so draw into it directly */
        void *px; int pitch;
        SDL_LockTexture(tex, NULL, &px, &pitch);
        SDL_UnlockTexture(tex);
        SDL_FreeSurface(screen);
        screen = SDL_CreateRGBSurfaceFrom(px, w, h, 32, pitch, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
        SDL_SetSurfaceBlendMode(screen, SDL_BLENDMODE_NONE);
    }
    printf("driver=%s renderer=%s window=%dx%d clear=%s direct=%s\n", SDL_GetCurrentVideoDriver(), ri.name, w, h, noclear ? "no" : "yes", direct ? "yes" : "no");

    Uint64 t_draw = 0, t_upd = 0, t_copy = 0, t_pres = 0, start = SDL_GetPerformanceCounter();
    for (int f = 0; f < frames; f++) {
        SDL_Event e; while (SDL_PollEvent(&e)) {}
        Uint64 a = SDL_GetPerformanceCounter();
        SDL_Rect rc = { (f * 7) % (w - 100), (f * 5) % (h - 100), 100, 100 };
        SDL_FillRect(screen, &rc, SDL_MapRGB(screen->format, f & 255, 128, 255 - (f & 255)));
        Uint64 b = SDL_GetPerformanceCounter();
        if (!direct) SDL_UpdateTexture(tex, NULL, screen->pixels, screen->pitch);
        Uint64 c = SDL_GetPerformanceCounter();
        if (!noclear) SDL_RenderClear(r);
        SDL_RenderCopy(r, tex, NULL, NULL);
        Uint64 d = SDL_GetPerformanceCounter();
        SDL_RenderPresent(r);
        Uint64 g = SDL_GetPerformanceCounter();
        t_draw += b - a; t_upd += c - b; t_copy += d - c; t_pres += g - d;
    }
    Uint64 total = SDL_GetPerformanceCounter() - start;
    double n = frames;
    printf("per frame: update texture %.2f ms | clear+copy %.2f ms | present %.2f ms | total pipeline %.2f ms  (%.1f fps)\n",
           ms(t_upd) / n, ms(t_copy) / n, ms(t_pres) / n, ms(t_upd + t_copy + t_pres) / n, n / (ms(total) / 1000.0));
    SDL_Quit();
    return 0;
}
