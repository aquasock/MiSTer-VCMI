/* Plays a tone through SDL audio to check the MiSTer audio driver. Usage: sdl-audiotest [--freq HZ] [--seconds N] [--rate R] */
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double phase, step; static int channels = 2; static SDL_AudioFormat fmt;

static void cb(void *ud, Uint8 *out, int len)
{
    (void)ud;
    if (fmt == AUDIO_S16SYS) {
        Sint16 *o = (Sint16 *)out; int frames = len / (2 * channels);
        for (int i = 0; i < frames; i++) { Sint16 v = (Sint16)(9000 * sin(phase)); phase += step; for (int c = 0; c < channels; c++) *o++ = v; }
    } else {
        memset(out, 0, len);
    }
}

int main(int argc, char **argv)
{
    double freq = 440; int seconds = 4, rate = 44100;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--freq") && i + 1 < argc) freq = atof(argv[++i]);
        else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rate") && i + 1 < argc) rate = atoi(argv[++i]);
    }
    if (SDL_Init(SDL_INIT_AUDIO) < 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    printf("audio driver: %s\n", SDL_GetCurrentAudioDriver());
    SDL_AudioSpec want, have;
    SDL_zero(want); want.freq = rate; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024; want.callback = cb;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (!dev) { fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError()); return 1; }
    printf("asked %d Hz, got %d Hz, %d ch, %d frames per period\n", want.freq, have.freq, have.channels, have.samples);
    fmt = have.format; channels = have.channels; step = 2 * M_PI * freq / have.freq;
    SDL_PauseAudioDevice(dev, 0);
    SDL_Delay(seconds * 1000);
    SDL_CloseAudioDevice(dev);
    SDL_Quit();
    return 0;
}
