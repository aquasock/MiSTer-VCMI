/*
  MiSTer audio driver for SDL2.

  Writes raw 48 kHz stereo 16-bit PCM to /dev/MrAudio, the kernel's ring buffer that the FPGA plays out (see
  sound/drivers/MiSTer-audio-spi.c). Reading the same device returns "rptr: N, wptr: N, len: N, comp: N";
  len is the number of bytes waiting to be played, which is what this driver paces itself against: it keeps
  about SDL_MISTER_AUDIO_MS (default 70) of audio queued. SDL converts from whatever format the app asked for.

  Environment: SDL_MISTER_AUDIO_MS   queue depth in milliseconds (bigger = safer against stalls, more latency)
               SDL_MISTER_DEBUG      1 to log what the driver does
*/
#ifndef SDL_misteraudio_h_
#define SDL_misteraudio_h_

#include "../SDL_sysaudio.h"

/* Hidden "this" pointer for the audio functions */
#define _THIS SDL_AudioDevice *_this

struct SDL_PrivateAudioData
{
    int fd;              /* /dev/MrAudio, opened for writing */
    Uint8 *mixbuf;       /* one period of audio, filled by the SDL callback */
    int target_bytes;    /* queue depth to keep in the ring buffer */
    Uint32 period_ms;    /* fallback pacing if the status line cannot be read */
};

#endif /* SDL_misteraudio_h_ */
