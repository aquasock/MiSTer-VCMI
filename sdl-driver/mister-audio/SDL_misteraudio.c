#include "../../SDL_internal.h"

#ifdef SDL_AUDIO_DRIVER_MISTER

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "SDL_audio.h"
#include "SDL_cpuinfo.h"
#include "SDL_timer.h"
#include "../SDL_audio_c.h"
#include "SDL_misteraudio.h"

#define MRAUDIO_DEVICE "/dev/MrAudio"
#define MRAUDIO_RATE   48000
#define MRAUDIO_FRAME  4 /* 16-bit stereo */

#define DBG(...)                                        \
    do {                                                \
        if (SDL_getenv("SDL_MISTER_DEBUG")) {           \
            fprintf(stderr, "[mister audio] " __VA_ARGS__); \
            fputc('\n', stderr);                        \
        }                                               \
    } while (0)

/* Bytes waiting in the ring buffer, or -1 if the status line cannot be read. Each open() makes the kernel
   fetch the read pointer from the FPGA, which is exactly what we want. */
static int MISTERAUDIO_Queued(void)
{
    char line[160];
    const char *p;
    ssize_t n;
    int fd = open(MRAUDIO_DEVICE, O_RDONLY | O_CLOEXEC);

    if (fd < 0) {
        return -1;
    }
    n = read(fd, line, sizeof(line) - 1);
    close(fd);
    if (n <= 0) {
        return -1;
    }
    line[n] = '\0';
    p = SDL_strstr(line, "len:");
    return p ? SDL_atoi(p + 4) : -1;
}

/* Wait until the queue has drained to the target depth, so the next period is written just in time.
   Every status read costs an SPI transaction in the kernel, so read it once per period: sleep for the
   excess and write straight away instead of polling until it reaches the target exactly. */
static void MISTERAUDIO_WaitDevice(_THIS)
{
    struct SDL_PrivateAudioData *h = _this->hidden;
    int queued = MISTERAUDIO_Queued();
    int excess;
    Uint32 ms;

    if (queued < 0) {
        SDL_Delay(h->period_ms); /* no status: fall back to one period per period */
        return;
    }
    excess = queued - h->target_bytes;
    if (excess <= 0) {
        return; /* at or below the target (or an underrun): write now */
    }
    ms = (Uint32)((int64_t)excess * 1000 / (MRAUDIO_RATE * MRAUDIO_FRAME));
    if (ms > 100) {
        ms = 100;
    }
    if (ms > 0) {
        SDL_Delay(ms);
    }
}

static void MISTERAUDIO_PlayDevice(_THIS)
{
    struct SDL_PrivateAudioData *h = _this->hidden;
    const Uint8 *p = h->mixbuf;
    int left = (int)_this->spec.size;

    while (left > 0) {
        ssize_t n = write(h->fd, p, (size_t)left);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            SDL_OpenedAudioDeviceDisconnected(_this);
            return;
        }
        p += n;
        left -= (int)n;
    }
}

static Uint8 *MISTERAUDIO_GetDeviceBuf(_THIS)
{
    return _this->hidden->mixbuf;
}

static void MISTERAUDIO_CloseDevice(_THIS)
{
    struct SDL_PrivateAudioData *h = _this->hidden;

    if (h) {
        if (h->fd >= 0) {
            close(h->fd);
        }
        SDL_SIMDFree(h->mixbuf);
        SDL_free(h);
        _this->hidden = NULL;
    }
}

static int MISTERAUDIO_OpenDevice(_THIS, const char *devname)
{
    struct SDL_PrivateAudioData *h;
    const char *ms_env = SDL_getenv("SDL_MISTER_AUDIO_MS");
    int ms = ms_env ? SDL_atoi(ms_env) : 70;

    if (_this->iscapture) {
        return SDL_SetError("MiSTer audio: capture is not supported");
    }
    h = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*h));
    if (!h) {
        return SDL_OutOfMemory();
    }
    _this->hidden = h;
    h->fd = -1;

    /* The FPGA only plays 48 kHz stereo S16; SDL converts whatever the application asked for. */
    _this->spec.format = AUDIO_S16LSB;
    _this->spec.channels = 2;
    _this->spec.freq = MRAUDIO_RATE;
    SDL_CalculateAudioSpec(&_this->spec);

    h->fd = open(MRAUDIO_DEVICE, O_WRONLY | O_CLOEXEC);
    if (h->fd < 0) {
        return SDL_SetError("MiSTer audio: cannot open %s: %s", MRAUDIO_DEVICE, strerror(errno));
    }
    if (ms < 20) {
        ms = 20;
    } else if (ms > 1500) {
        ms = 1500;
    }
    h->target_bytes = (ms * (MRAUDIO_RATE / 1000)) * MRAUDIO_FRAME;
    h->period_ms = (Uint32)(_this->spec.samples * 1000 / MRAUDIO_RATE);
    if (h->period_ms < 1) {
        h->period_ms = 1;
    }
    h->mixbuf = (Uint8 *)SDL_SIMDAlloc(_this->spec.size);
    if (!h->mixbuf) {
        return SDL_OutOfMemory();
    }
    SDL_memset(h->mixbuf, _this->spec.silence, _this->spec.size);
    DBG("opened %s: %d frames per period, queue target %d ms (%d bytes)", MRAUDIO_DEVICE, _this->spec.samples, ms, h->target_bytes);
    return 0;
}

static SDL_bool MISTERAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    if (access(MRAUDIO_DEVICE, W_OK) != 0) {
        return SDL_FALSE; /* not a MiSTer: let SDL try the next driver */
    }
    impl->OpenDevice = MISTERAUDIO_OpenDevice;
    impl->WaitDevice = MISTERAUDIO_WaitDevice;
    impl->PlayDevice = MISTERAUDIO_PlayDevice;
    impl->GetDeviceBuf = MISTERAUDIO_GetDeviceBuf;
    impl->CloseDevice = MISTERAUDIO_CloseDevice;
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    impl->SupportsNonPow2Samples = SDL_TRUE;
    return SDL_TRUE;
}

AudioBootStrap MISTERAUDIO_bootstrap = {
    "mister", "MiSTer audio (/dev/MrAudio)", MISTERAUDIO_Init, SDL_FALSE
};

#endif /* SDL_AUDIO_DRIVER_MISTER */
