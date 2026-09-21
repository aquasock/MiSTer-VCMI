/* Exercises the SDL "mister" video driver: colour bars, moving box, software cursor,
   input event log, frame rate. Usage: sdl-test [--size WxH] [--seconds N] [--nopresent] */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    int w = 800, h = 600, seconds = 15, present = 1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &w, &h);
        else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--nopresent")) present = 0;
    }
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    printf("video driver: %s\n", SDL_GetCurrentVideoDriver());
    SDL_DisplayMode dm;
    SDL_GetDesktopDisplayMode(0, &dm);
    printf("desktop mode: %dx%d fmt=%s\n", dm.w, dm.h, SDL_GetPixelFormatName(dm.format));
    for (int i = 0; i < SDL_GetNumDisplayModes(0); i++) {
        SDL_GetDisplayMode(0, i, &dm);
        printf("  mode %d: %dx%d\n", i, dm.w, dm.h);
    }

    SDL_Window *win = SDL_CreateWindow("sdl-test", 0, 0, w, h, SDL_WINDOW_FULLSCREEN);
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    SDL_Surface *s = SDL_GetWindowSurface(win);
    if (!s) { fprintf(stderr, "SDL_GetWindowSurface: %s\n", SDL_GetError()); return 1; }
    printf("window %dx%d, surface %dx%d fmt=%s\n", w, h, s->w, s->h, SDL_GetPixelFormatName(s->format->format));
    SDL_ShowCursor(SDL_DISABLE);

    int mx = s->w / 2, my = s->h / 2, quit = 0, frames = 0, events = 0;
    Uint32 start = SDL_GetTicks(), last = start;
    while (!quit && SDL_GetTicks() - start < (Uint32)seconds * 1000) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            events++;
            switch (e.type) {
            case SDL_KEYDOWN:
                printf("key down: %s\n", SDL_GetKeyName(e.key.keysym.sym));
                if (e.key.keysym.sym == SDLK_ESCAPE) quit = 1;
                break;
            case SDL_TEXTINPUT: printf("text: %s\n", e.text.text); break;
            case SDL_MOUSEMOTION: mx = e.motion.x; my = e.motion.y; break;
            case SDL_MOUSEBUTTONDOWN: printf("button %d at %d,%d\n", e.button.button, e.button.x, e.button.y); break;
            case SDL_MOUSEWHEEL: printf("wheel %d\n", e.wheel.y); break;
            case SDL_QUIT: quit = 1; break;
            }
        }
        /* orientation: black square top-left, then RED GREEN BLUE WHITE bars */
        SDL_FillRect(s, NULL, SDL_MapRGB(s->format, 24, 24, 48));
        Uint32 cols[4] = { SDL_MapRGB(s->format, 255, 0, 0), SDL_MapRGB(s->format, 0, 255, 0),
                           SDL_MapRGB(s->format, 0, 0, 255), SDL_MapRGB(s->format, 255, 255, 255) };
        for (int i = 0; i < 4; i++) {
            SDL_Rect r = { i * s->w / 4, s->h / 4, s->w / 4, s->h / 2 };
            SDL_FillRect(s, &r, cols[i]);
        }
        SDL_Rect corner = { 0, 0, 40, 40 };
        SDL_FillRect(s, &corner, SDL_MapRGB(s->format, 0, 0, 0));
        int bx = (int)((SDL_GetTicks() - start) / 4 % (Uint32)(s->w - 60));
        SDL_Rect box = { bx, s->h - 80, 60, 60 };
        SDL_FillRect(s, &box, SDL_MapRGB(s->format, 255, 200, 0));
        SDL_Rect cur = { mx - 3, my - 3, 7, 7 }, curv = { mx - 1, my - 10, 3, 21 };
        SDL_FillRect(s, &cur, SDL_MapRGB(s->format, 255, 255, 255));
        SDL_FillRect(s, &curv, SDL_MapRGB(s->format, 255, 255, 255));
        if (present) SDL_UpdateWindowSurface(win);
        frames++;
        Uint32 now = SDL_GetTicks();
        if (now - last >= 5000) { printf("%.1f fps, %d events so far\n", frames * 1000.0 / (now - start), events); last = now; }
    }
    Uint32 total = SDL_GetTicks() - start;
    printf("done: %d frames in %.1f s = %.1f fps, %d events\n", frames, total / 1000.0, frames * 1000.0 / total, events);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
