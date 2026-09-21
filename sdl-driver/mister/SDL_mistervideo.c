#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_MISTER

#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>
#include <ucontext.h>
#include <sys/mman.h>
#include <unistd.h>

#include "SDL_hints.h"
#include "SDL_timer.h"
#include "SDL_thread.h"
#include "SDL_mutex.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../core/linux/SDL_evdev.h"
#include "../../core/linux/SDL_evdev_capabilities.h"

#include "SDL_mistervideo.h"

#define MISTER_DRIVER_NAME "mister"
#define MISTER_CMD_FIFO    "/dev/MiSTer_cmd"
#define MISTER_MODE_PARAM  "/sys/module/MiSTer_fb/parameters/mode"
#define MISTER_SURFACE     "_SDL_MiSTerSurface"

#define DBG(...)                                        \
    do {                                                \
        if (SDL_getenv("SDL_MISTER_DEBUG")) {           \
            fprintf(stderr, "[mister] " __VA_ARGS__);   \
            fputc('\n', stderr);                        \
        }                                               \
    } while (0)

typedef struct
{
    int fd;
    Uint8 *map;
    size_t maplen;
    int fb_w, fb_h, stride; /* current framebuffer geometry */
    int native_w, native_h; /* geometry when the driver started; restored on quit */
    int vt_fd;
    long vt_prev_mode;
    SDL_bool vsync;
    SDL_bool send_modes;
    int forced_w, forced_h;
    int req_w, req_h; /* last size asked of Main_MiSTer, so repeats are skipped */
    int want_w, want_h; /* size of the window the game asked for */
    SDL_bool async;   /* SDL_MISTER_ASYNC=0 to present from the game thread instead of the presenter thread */
    SDL_bool zerocopy; /* SDL_MISTER_ZEROCOPY=1: hand the window surface's own buffer to the presenter (no snapshot copy).
                          Only valid for renderer-style apps that redraw the whole window every frame. */
    Uint32 format;    /* SDL_MISTER_FORMAT=argb|xrgb: pixel format of the window surface (the scaler ignores alpha) */
    /* presenter thread: the game snapshots each finished frame into one of three buffers and
       moves on; the presenter waits for vsync and copies the newest snapshot into the framebuffer */
    SDL_Thread *thread;
    SDL_mutex *lock;
    SDL_cond *cond;
    SDL_bool quit;
    Uint8 *bufs[3];
    int buf_w, buf_h, buf_pitch;
    int work, ready, busy; /* buffer roles: being filled, waiting to be shown, being shown (-1 = none) */
    /* input hotplug: /dev/input is rescanned every second or so and SDL's evdev reopened if the device list changed */
    SDL_bool input_auto;
    char input_list[2048];
    Uint64 input_t;
    /* SDL_MISTER_STATS: log timing every few seconds (to stderr, or appended to that path if it starts with '/') */
    SDL_bool stats;
    const char *stats_path;
    Uint64 ctl_t, prev_end, prev_cpu_end, t0, max_gap;
    Uint64 st_game, st_game_cpu, st_snap, st_calls;         /* game thread */
    Uint64 st_wait, st_copy, st_shown;                      /* presenter (updated under lock) */
} MISTER_Data;

static void MISTER_Flush(MISTER_Data *d);
static void MISTER_StopPresenter(MISTER_Data *d);

/* ------------------------------------------------------------------ fb --- */

static int MISTER_ReadFb(MISTER_Data *d)
{
    struct fb_var_screeninfo v;
    struct fb_fix_screeninfo f;

    if (ioctl(d->fd, FBIOGET_VSCREENINFO, &v) || ioctl(d->fd, FBIOGET_FSCREENINFO, &f)) {
        return SDL_SetError("MiSTer: FBIOGET_*SCREENINFO failed: %s", strerror(errno));
    }
    if (v.bits_per_pixel != 32) {
        return SDL_SetError("MiSTer: framebuffer is %u bpp, need 32", v.bits_per_pixel);
    }

    if (d->map) {
        munmap(d->map, d->maplen);
        d->map = NULL;
    }
    d->fb_w = (int)v.xres;
    d->fb_h = (int)v.yres;
    d->stride = (int)f.line_length;
    d->maplen = f.smem_len ? f.smem_len : (size_t)d->stride * d->fb_h;
    d->map = (Uint8 *)mmap(NULL, d->maplen, PROT_READ | PROT_WRITE, MAP_SHARED, d->fd, 0);
    if (d->map == MAP_FAILED) {
        d->map = NULL;
        return SDL_SetError("MiSTer: mmap of framebuffer failed: %s", strerror(errno));
    }
    DBG("fb %dx%d stride=%d maplen=%zu", d->fb_w, d->fb_h, d->stride, d->maplen);
    return 0;
}

static int MISTER_SendCmd(const char *cmd)
{
    int fd = open(MISTER_CMD_FIFO, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    ssize_t n;
    if (fd < 0) {
        DBG("open %s: %s", MISTER_CMD_FIFO, strerror(errno));
        return -1;
    }
    n = write(fd, cmd, strlen(cmd));
    close(fd);
    DBG("sent: %s", cmd);
    return n == (ssize_t)strlen(cmd) ? 0 : -1;
}

/* Ask Main_MiSTer for a w x h buffer and wait for MiSTer_fb to pick it up. Main only
   honours fb_cmd1 while the Linux framebuffer is switched in; otherwise the size stays
   as it is and the window is centred in whatever fb0 currently is. */
static void MISTER_SetFbSize(MISTER_Data *d, int w, int h)
{
    char cmd[64];
    int i;

    MISTER_Flush(d); /* nothing may still be writing to the old mapping */
    if (d->forced_w > 0) {
        w = d->forced_w;
        h = d->forced_h;
    }
    if (d->send_modes && w > 0 && h > 0 && (w != d->fb_w || h != d->fb_h) && (w != d->req_w || h != d->req_h)) {
        d->req_w = w;
        d->req_h = h;
        SDL_snprintf(cmd, sizeof(cmd), "fb_cmd1 8888 1 %d %d\n", w, h);
        if (MISTER_SendCmd(cmd) == 0) {
            for (i = 0; i < 50; i++) { /* up to ~1 s */
                unsigned fmt = 0, rb = 0, mw = 0, mh = 0, ms = 0;
                FILE *fp = fopen(MISTER_MODE_PARAM, "r");
                if (fp) {
                    if (fscanf(fp, "%u %u %u %u %u", &fmt, &rb, &mw, &mh, &ms) == 5 &&
                        mw > 0 && ((int)mw != d->fb_w || (int)mh != d->fb_h)) {
                        fclose(fp);
                        DBG("MiSTer_fb switched to %ux%u after %d ms", mw, mh, i * 20);
                        break;
                    }
                    fclose(fp);
                }
                usleep(20 * 1000);
            }
            usleep(60 * 1000);
        }
    }
    /* A reopen is the reliable way to pick up the new geometry. */
    MISTER_ReadFb(d);
    if (d->fb_w == w && d->fb_h == h) {
        d->req_w = d->req_h = 0;
    }
}

/* ------------------------------------------------------------------ vt ---- */

static void MISTER_VTEnter(MISTER_Data *d)
{
    struct vt_stat st;
    char path[32];
    int fd0;

    d->vt_fd = -1;
    if (SDL_getenv("SDL_MISTER_NOVT")) {
        return;
    }
    fd0 = open("/dev/tty0", O_RDWR | O_CLOEXEC);
    if (fd0 < 0) {
        return;
    }
    if (ioctl(fd0, VT_GETSTATE, &st) == 0) {
        SDL_snprintf(path, sizeof(path), "/dev/tty%d", st.v_active);
        d->vt_fd = open(path, O_RDWR | O_CLOEXEC);
        if (d->vt_fd >= 0) {
            ioctl(d->vt_fd, KDGETMODE, &d->vt_prev_mode);
            if (ioctl(d->vt_fd, KDSETMODE, KD_GRAPHICS)) {
                DBG("KDSETMODE: %s", strerror(errno));
            } else {
                DBG("%s set to KD_GRAPHICS", path);
            }
        }
    }
    close(fd0);
}

static void MISTER_VTLeave(MISTER_Data *d)
{
    if (d->vt_fd >= 0) {
        ioctl(d->vt_fd, KDSETMODE, d->vt_prev_mode);
        close(d->vt_fd);
        d->vt_fd = -1;
    }
}

/* ---------------------------------------------------------------- input --- */

/* Without udev SDL_EVDEV only opens the devices named in SDL_EVDEV_DEVICES ("class:path,..."),
   so scan /dev/input ourselves and classify each device by its capability bits. */
static void MISTER_BuildInputList(char *list, size_t size, SDL_bool log)
{
    size_t len = 0;
    int i;

    list[0] = '\0';
    for (i = 0; i < 32; i++) {
        unsigned long ev[NBITS(EV_MAX)] = { 0 }, abs_[NBITS(ABS_MAX)] = { 0 };
        unsigned long key[NBITS(KEY_MAX)] = { 0 }, rel[NBITS(REL_MAX)] = { 0 };
        char path[40], name[128] = "";
        int fd, cls, n;

        SDL_snprintf(path, sizeof(path), "/dev/input/event%d", i);
        fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        ioctl(fd, EVIOCGBIT(0, sizeof(ev)), ev);
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_)), abs_);
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key)), key);
        ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel)), rel);
        close(fd);

        cls = SDL_EVDEV_GuessDeviceClass(ev, abs_, key, rel) &
              (SDL_UDEV_DEVICE_MOUSE | SDL_UDEV_DEVICE_KEYBOARD | SDL_UDEV_DEVICE_TOUCHSCREEN | SDL_UDEV_DEVICE_TOUCHPAD);
        /* Main_MiSTer's own uinput device would echo its input back to us. */
        if (!cls || SDL_strstr(name, "MiSTer virtual input")) {
            if (log) {
                DBG("input: skip %s (%s)", path, name);
            }
            continue;
        }
        n = SDL_snprintf(list + len, size - len, "%s%d:%s", len ? "," : "", cls, path);
        if (n < 0 || (size_t)n >= size - len) {
            break;
        }
        len += (size_t)n;
        if (log) {
            DBG("input: %s class=0x%x (%s)", path, cls, name);
        }
    }
}

/* Called from PumpEvents: if a device appeared or went away, hand SDL the new list. */
static void MISTER_RescanInput(MISTER_Data *d)
{
    char list[sizeof(d->input_list)];

    MISTER_BuildInputList(list, sizeof(list), SDL_FALSE);
    if (SDL_strcmp(list, d->input_list) == 0) {
        return;
    }
    DBG("input devices changed: %s", list);
    SDL_strlcpy(d->input_list, list, sizeof(d->input_list));
    SDL_setenv("SDL_EVDEV_DEVICES", list, 1);
    SDL_EVDEV_Quit();
    SDL_EVDEV_Init();
    SDL_ResetKeyboard(); /* a key held while its device went away would otherwise stay down */
}

/* -------------------------------------------------------------- cursor ---- */
/* The scaler has no hardware cursor, so games must draw their own. Provide inert
   cursor objects so SDL_CreateColorCursor and friends succeed. */

static SDL_Cursor *MISTER_CreateCursor(SDL_Surface *surface, int hot_x, int hot_y)
{
    SDL_Cursor *cursor = (SDL_Cursor *)SDL_calloc(1, sizeof(*cursor));
    if (!cursor) {
        SDL_OutOfMemory();
    }
    return cursor;
}

static SDL_Cursor *MISTER_CreateSystemCursor(SDL_SystemCursor id)
{
    return MISTER_CreateCursor(NULL, 0, 0);
}

static int MISTER_ShowCursor(SDL_Cursor *cursor)
{
    return 0;
}

static void MISTER_FreeCursor(SDL_Cursor *cursor)
{
    SDL_free(cursor);
}

/* ------------------------------------------------------------- profiler --- */
/* SDL_MISTER_PROFILE=/path: sample where the process spends CPU. A 1 kHz SIGPROF timer records the
   interrupted thread's pc and lr; every few seconds the game thread appends them to /path (lines of
   "tid pc lr") and rewrites /path.maps so the addresses can be resolved offline. */
#define PROF_MAX 65536
#define PROF_SIG (SIGRTMIN + 3) /* SDL blocks SIGPROF (and friends) in the threads it creates */
static struct { unsigned tid, pc, lr; } prof_buf[PROF_MAX];
static volatile unsigned prof_n;
static const char *prof_path;
static Uint64 prof_last;

static void MISTER_ProfHandler(int sig, siginfo_t *si, void *ctx)
{
    ucontext_t *uc = (ucontext_t *)ctx;
    unsigned n = prof_n;
    (void)sig;
    (void)si;
    if (n < PROF_MAX) {
        prof_buf[n].tid = (unsigned)syscall(SYS_gettid);
        prof_buf[n].pc = (unsigned)uc->uc_mcontext.arm_pc;
        prof_buf[n].lr = (unsigned)uc->uc_mcontext.arm_lr;
        prof_n = n + 1;
    }
}

/* One CPU-time timer per thread, so a sample is the pc of a thread that is really running.
   (A process-wide ITIMER_PROF would deliver every tick to the main thread, whatever it is doing.) */
#define PROF_MAX_THREADS 64
static struct { pid_t tid; timer_t timer; } prof_timers[PROF_MAX_THREADS];
static int prof_ntimers;

static void MISTER_ProfSync(void)
{
    DIR *dir = opendir("/proc/self/task");
    struct dirent *e;

    if (!dir) {
        return;
    }
    while ((e = readdir(dir)) != NULL) {
        pid_t tid = (pid_t)SDL_atoi(e->d_name);
        int i, known = 0;
        struct sigevent sev;
        struct itimerspec its;
        clockid_t clk;

        if (tid <= 0) {
            continue;
        }
        for (i = 0; i < prof_ntimers; i++) {
            if (prof_timers[i].tid == tid) {
                known = 1;
            }
        }
        if (known || prof_ntimers >= PROF_MAX_THREADS) {
            continue;
        }
        clk = (clockid_t)(((~(unsigned)tid) << 3) | 6); /* MAKE_THREAD_CPUCLOCK(tid, CPUCLOCK_SCHED) */
        SDL_zero(sev);
        sev.sigev_notify = SIGEV_THREAD_ID;
        sev.sigev_signo = PROF_SIG;
        sev._sigev_un._tid = tid;
        if (timer_create(clk, &sev, &prof_timers[prof_ntimers].timer) == 0) {
            its.it_interval.tv_sec = 0;
            its.it_interval.tv_nsec = 1000000;
            its.it_value = its.it_interval;
            timer_settime(prof_timers[prof_ntimers].timer, 0, &its, NULL);
            prof_timers[prof_ntimers].tid = tid;
            prof_ntimers++;
        }
    }
    closedir(dir);
}

static void MISTER_ProfStart(const char *path)
{
    struct sigaction sa;

    prof_path = path;
    SDL_zero(sa);
    sa.sa_sigaction = MISTER_ProfHandler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(PROF_SIG, &sa, NULL);
    MISTER_ProfSync();
}

static void MISTER_ProfDump(void)
{
    unsigned n = prof_n, i;
    FILE *out, *maps;
    char line[512], mpath[300];

    if (!prof_path) {
        return;
    }
    MISTER_ProfSync();
    if (!n) {
        return;
    }
    prof_n = 0; /* samples arriving while we write are simply lost */
    out = fopen(prof_path, "a");
    if (out) {
        for (i = 0; i < n && i < PROF_MAX; i++) {
            fprintf(out, "%u %x %x\n", prof_buf[i].tid, prof_buf[i].pc, prof_buf[i].lr);
        }
        fclose(out);
    }
    SDL_snprintf(mpath, sizeof(mpath), "%s.maps", prof_path);
    maps = fopen("/proc/self/maps", "r");
    out = fopen(mpath, "w");
    if (maps && out) {
        while (fgets(line, sizeof(line), maps)) {
            fputs(line, out);
        }
    }
    if (maps) fclose(maps);
    if (out) fclose(out);
    /* thread names, so a profile can say which thread is which */
    SDL_snprintf(mpath, sizeof(mpath), "%s.threads", prof_path);
    out = fopen(mpath, "w");
    if (out) {
        DIR *dir = opendir("/proc/self/task");
        struct dirent *e;
        while (dir && (e = readdir(dir)) != NULL) {
            char cp[64], name[64] = "";
            FILE *cf;
            SDL_snprintf(cp, sizeof(cp), "/proc/self/task/%s/comm", e->d_name);
            cf = fopen(cp, "r");
            if (cf) {
                if (fgets(name, sizeof(name), cf)) {
                    name[strcspn(name, "\n")] = '\0';
                    fprintf(out, "%s %s\n", e->d_name, name);
                }
                fclose(cf);
            }
        }
        if (dir) closedir(dir);
        fclose(out);
    }
}

/* ---------------------------------------------------------- video device --- */

static void MISTER_GetDisplayModes(_THIS, SDL_VideoDisplay *display)
{
    static const int sizes[][2] = {
        { 320, 240 }, { 640, 360 }, { 640, 480 }, { 800, 600 }, { 960, 540 },
        { 1024, 600 }, { 1024, 768 }, { 1280, 720 }, { 1280, 960 }, { 1440, 1080 }, { 1920, 1080 }
    };
    MISTER_Data *d = (MISTER_Data *)_this->driverdata;
    SDL_DisplayMode mode;
    size_t i;

    for (i = 0; i < SDL_arraysize(sizes); i++) {
        if (sizes[i][0] > d->native_w || sizes[i][1] > d->native_h) {
            continue;
        }
        SDL_zero(mode);
        mode.format = d->format;
        mode.w = sizes[i][0];
        mode.h = sizes[i][1];
        mode.refresh_rate = 60;
        SDL_AddDisplayMode(display, &mode);
    }
}


static int MISTER_VideoInit(_THIS)
{
    MISTER_Data *d = (MISTER_Data *)_this->driverdata;
    const char *dev = SDL_getenv("SDL_MISTER_FBDEV");
    const char *forced = SDL_getenv("SDL_MISTER_FB");
    struct fb_fix_screeninfo f;
    SDL_DisplayMode mode;
    SDL_Mouse *mouse;

    if (!dev) {
        dev = "/dev/fb0";
    }
    d->fd = open(dev, O_RDWR | O_CLOEXEC);
    if (d->fd < 0) {
        return SDL_SetError("MiSTer: cannot open %s: %s", dev, strerror(errno));
    }
    if (ioctl(d->fd, FBIOGET_FSCREENINFO, &f) || SDL_strncmp(f.id, "MiSTer_fb", 9) != 0) {
        return SDL_SetError("MiSTer: %s is not the MiSTer_fb framebuffer", dev);
    }
    if (MISTER_ReadFb(d) < 0) {
        return -1;
    }
    d->native_w = d->fb_w;
    d->native_h = d->fb_h;
    d->vsync = SDL_getenv("SDL_MISTER_VSYNC") ? (SDL_atoi(SDL_getenv("SDL_MISTER_VSYNC")) != 0) : SDL_TRUE;
    d->send_modes = SDL_getenv("SDL_MISTER_NOMODE") ? SDL_FALSE : SDL_TRUE;
    d->zerocopy = SDL_getenv("SDL_MISTER_ZEROCOPY") ? (SDL_atoi(SDL_getenv("SDL_MISTER_ZEROCOPY")) != 0) : SDL_FALSE;
    d->format = (SDL_getenv("SDL_MISTER_FORMAT") && SDL_strcasecmp(SDL_getenv("SDL_MISTER_FORMAT"), "xrgb") == 0) ? SDL_PIXELFORMAT_XRGB8888 : SDL_PIXELFORMAT_ARGB8888;
    d->async = SDL_getenv("SDL_MISTER_ASYNC") ? (SDL_atoi(SDL_getenv("SDL_MISTER_ASYNC")) != 0) : SDL_TRUE;
    d->stats = SDL_getenv("SDL_MISTER_STATS") ? SDL_TRUE : SDL_FALSE;
    if (SDL_getenv("SDL_MISTER_PROFILE")) {
        MISTER_ProfStart(SDL_getenv("SDL_MISTER_PROFILE"));
    }
    d->stats_path = (d->stats && SDL_getenv("SDL_MISTER_STATS")[0] == '/') ? SDL_getenv("SDL_MISTER_STATS") : NULL;
    if (forced && SDL_sscanf(forced, "%dx%d", &d->forced_w, &d->forced_h) != 2) {
        d->forced_w = d->forced_h = 0;
    }

    SDL_zero(mode);
    mode.format = d->format;
    mode.w = d->native_w;
    mode.h = d->native_h;
    mode.refresh_rate = 60;
    if (SDL_AddBasicVideoDisplay(&mode) < 0) {
        return -1;
    }
    SDL_AddDisplayMode(&_this->displays[0], &mode);
    /* SDL only asks for the mode list when it is empty, so add the standard sizes now. */
    MISTER_GetDisplayModes(_this, &_this->displays[0]);

    MISTER_VTEnter(d);

    d->input_auto = SDL_getenv("SDL_EVDEV_DEVICES") ? SDL_FALSE : SDL_TRUE;
    if (d->input_auto) {
        MISTER_BuildInputList(d->input_list, sizeof(d->input_list), SDL_TRUE);
        if (d->input_list[0]) {
            SDL_setenv("SDL_EVDEV_DEVICES", d->input_list, 1);
        }
    }
    if (SDL_EVDEV_Init() < 0) {
        return SDL_SetError("MiSTer: SDL_EVDEV_Init failed");
    }
    mouse = SDL_GetMouse();
    mouse->CreateCursor = MISTER_CreateCursor;
    mouse->CreateSystemCursor = MISTER_CreateSystemCursor;
    mouse->ShowCursor = MISTER_ShowCursor;
    mouse->FreeCursor = MISTER_FreeCursor;
    SDL_SetDefaultCursor(MISTER_CreateCursor(NULL, 0, 0));
    return 0;
}

static void MISTER_VideoQuit(_THIS)
{
    MISTER_Data *d = (MISTER_Data *)_this->driverdata;

    SDL_EVDEV_Quit();
    if (d) {
        MISTER_StopPresenter(d);
        if (d->send_modes && (d->fb_w != d->native_w || d->fb_h != d->native_h)) {
            MISTER_SetFbSize(d, d->native_w, d->native_h);
        }
        MISTER_VTLeave(d);
        if (d->map) {
            munmap(d->map, d->maplen);
        }
        if (d->fd >= 0) {
            close(d->fd);
        }
    }
}

static int MISTER_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    MISTER_SetFbSize((MISTER_Data *)_this->driverdata, mode->w, mode->h);
    return 0;
}

/* Main_MiSTer resets MiSTer_fb to the full output size whenever the Linux framebuffer is switched in (F9 or an OSD
   script), which can happen after the game already asked for its size. Notice that and ask again. */
static void MISTER_CheckFbMode(MISTER_Data *d)
{
    unsigned fmt = 0, rb = 0, mw = 0, mh = 0, ms = 0;
    FILE *fp;

    if (!d->send_modes || d->want_w <= 0) {
        return;
    }
    fp = fopen(MISTER_MODE_PARAM, "r");
    if (!fp) {
        return;
    }
    if (fscanf(fp, "%u %u %u %u %u", &fmt, &rb, &mw, &mh, &ms) == 5 && mw > 0 && ((int)mw != d->fb_w || (int)mh != d->fb_h)) {
        fclose(fp);
        DBG("MiSTer_fb is %ux%u behind our back; asking for %dx%d again", mw, mh, d->want_w, d->want_h);
        MISTER_Flush(d);
        MISTER_ReadFb(d);
        d->req_w = d->req_h = 0;
        MISTER_SetFbSize(d, d->want_w, d->want_h);
        return;
    }
    fclose(fp);
}

static void MISTER_PumpEvents(_THIS)
{
    MISTER_Data *d = (MISTER_Data *)_this->driverdata;

    if (prof_path) {
        Uint64 pnow = SDL_GetPerformanceCounter();
        if (pnow - prof_last >= 4 * SDL_GetPerformanceFrequency()) {
            prof_last = pnow;
            MISTER_ProfDump();
        }
    }
    if (d->input_auto) {
        Uint64 now = SDL_GetPerformanceCounter();
        if (now - d->input_t >= SDL_GetPerformanceFrequency() * 3 / 2) {
            d->input_t = now;
            MISTER_RescanInput(d);
            MISTER_CheckFbMode(d);
        }
    }
    SDL_EVDEV_Poll();
}

/* -------------------------------------------------------------- windows --- */

static int MISTER_CreateWindow(_THIS, SDL_Window *window)
{
    MISTER_Data *d = (MISTER_Data *)_this->driverdata;
    d->want_w = window->w;
    d->want_h = window->h;
    MISTER_SetFbSize(d, window->w, window->h);
    if (d->map) {
        SDL_memset(d->map, 0, (size_t)d->stride * d->fb_h); /* drop the console text */
    }
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);
    SDL_SendMouseMotion(window, 0, 0, window->w / 2, window->h / 2); /* start mid-screen */
    return 0;
}

static void MISTER_SetWindowSize(_THIS, SDL_Window *window)
{
    MISTER_Data *d = (MISTER_Data *)_this->driverdata;
    d->want_w = window->w;
    d->want_h = window->h;
    MISTER_SetFbSize(d, window->w, window->h);
}

static void MISTER_DestroyWindow(_THIS, SDL_Window *window)
{
}

/* ---------------------------------------------------------- framebuffer --- */

static Uint64 MISTER_ThreadCpu(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return (Uint64)ts.tv_sec * 1000000000ULL + (Uint64)ts.tv_nsec;
}

/* Copy one frame into the framebuffer, centred (or cropped) if the sizes differ. */
static void MISTER_Blit(MISTER_Data *d, const Uint8 *src, int w, int h, int pitch)
{
    int offx = (d->fb_w - w) / 2, offy = (d->fb_h - h) / 2;
    int x0 = 0, y0 = 0, x1 = w, y1 = h, y;

    if (offx + x0 < 0) x0 = -offx;
    if (offy + y0 < 0) y0 = -offy;
    if (offx + x1 > d->fb_w) x1 = d->fb_w - offx;
    if (offy + y1 > d->fb_h) y1 = d->fb_h - offy;
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    for (y = y0; y < y1; y++) {
        SDL_memcpy(d->map + (size_t)(y + offy) * d->stride + (size_t)(x0 + offx) * 4,
                   src + (size_t)y * pitch + (size_t)x0 * 4, (size_t)(x1 - x0) * 4);
    }
}

/* Wait for vsync (if enabled), then copy. Returns the time spent waiting and copying in counter ticks. */
static void MISTER_Show(MISTER_Data *d, const Uint8 *src, int w, int h, int pitch, Uint64 *wait, Uint64 *copy)
{
    Uint64 t0 = SDL_GetPerformanceCounter(), t1;

    if (d->vsync) {
        int arg = 0;
        ioctl(d->fd, FBIO_WAITFORVSYNC, &arg); /* timeouts are fine; just present */
    }
    t1 = SDL_GetPerformanceCounter();
    MISTER_Blit(d, src, w, h, pitch);
    *wait = t1 - t0;
    *copy = SDL_GetPerformanceCounter() - t1;
}

static int MISTER_PresentThread(void *arg)
{
    MISTER_Data *d = (MISTER_Data *)arg;

    SDL_LockMutex(d->lock);
    while (!d->quit) {
        int idx;
        Uint64 wait, copy;

        if (d->ready < 0) {
            SDL_CondWait(d->cond, d->lock);
            continue;
        }
        idx = d->busy = d->ready;
        d->ready = -1;
        SDL_UnlockMutex(d->lock);

        MISTER_Show(d, d->bufs[idx], d->buf_w, d->buf_h, d->buf_pitch, &wait, &copy);

        SDL_LockMutex(d->lock);
        d->st_wait += wait;
        d->st_copy += copy;
        d->st_shown++;
        d->busy = -1;
        SDL_CondBroadcast(d->cond);
    }
    SDL_UnlockMutex(d->lock);
    return 0;
}

/* Wait until the presenter has nothing queued or in flight (before the framebuffer is remapped). */
static void MISTER_Flush(MISTER_Data *d)
{
    if (!d->thread) {
        return;
    }
    SDL_LockMutex(d->lock);
    while (d->ready >= 0 || d->busy >= 0) {
        SDL_CondWait(d->cond, d->lock);
    }
    SDL_UnlockMutex(d->lock);
}

static void MISTER_StopPresenter(MISTER_Data *d)
{
    int i;

    if (d->thread) {
        MISTER_Flush(d);
        SDL_LockMutex(d->lock);
        d->quit = SDL_TRUE;
        SDL_CondBroadcast(d->cond);
        SDL_UnlockMutex(d->lock);
        SDL_WaitThread(d->thread, NULL);
        d->thread = NULL;
        d->quit = SDL_FALSE;
    }
    for (i = 0; i < 3; i++) {
        SDL_SIMDFree(d->bufs[i]);
        d->bufs[i] = NULL;
    }
    d->buf_w = d->buf_h = 0;
    if (d->cond) {
        SDL_DestroyCond(d->cond);
        d->cond = NULL;
    }
    if (d->lock) {
        SDL_DestroyMutex(d->lock);
        d->lock = NULL;
    }
}

static int MISTER_StartPresenter(MISTER_Data *d, int w, int h, int pitch)
{
    int i;

    MISTER_StopPresenter(d);
    for (i = 0; i < 3; i++) {
        d->bufs[i] = (Uint8 *)SDL_SIMDAlloc((size_t)pitch * h);
        if (!d->bufs[i]) {
            MISTER_StopPresenter(d);
            return SDL_OutOfMemory();
        }
        SDL_memset(d->bufs[i], 0, (size_t)pitch * h);
    }
    d->buf_w = w;
    d->buf_h = h;
    d->buf_pitch = pitch;
    d->work = 0;
    d->ready = d->busy = -1;
    d->lock = SDL_CreateMutex();
    d->cond = SDL_CreateCond();
    d->thread = SDL_CreateThread(MISTER_PresentThread, "mister-present", d);
    if (!d->lock || !d->cond || !d->thread) {
        MISTER_StopPresenter(d);
        return SDL_SetError("MiSTer: cannot start the presenter thread");
    }
    return 0;
}

static void MISTER_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    MISTER_Data *d = (MISTER_Data *)_this->driverdata;
    SDL_Surface *surface = (SDL_Surface *)SDL_SetWindowData(window, MISTER_SURFACE, NULL);

    MISTER_StopPresenter(d); /* the snapshots / swap buffers belong to this window */
    SDL_FreeSurface(surface);
}

static int MISTER_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    MISTER_Data *d = (MISTER_Data *)_this->driverdata;
    SDL_Surface *surface;
    int w, h;

    MISTER_DestroyWindowFramebuffer(_this, window);
    SDL_GetWindowSizeInPixels(window, &w, &h);
    *format = d->format;
    if (d->zerocopy && d->async) {
        /* SDL builds the window surface around these buffers and we swap them frame by frame */
        int p = w * 4;
        if (MISTER_StartPresenter(d, w, h, p) < 0) {
            return -1;
        }
        *pixels = d->bufs[d->work];
        *pitch = p;
        return 0;
    }
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, d->format);
    if (!surface) {
        return -1;
    }
    SDL_SetWindowData(window, MISTER_SURFACE, surface);
    *pixels = surface->pixels;
    *pitch = surface->pitch;
    return 0;
}

static void MISTER_ReadCtl(const char *path, SDL_bool *flag)
{
    FILE *f = fopen(path, "r");
    if (f) {
        int c = fgetc(f);
        if (c == '0' || c == '1') {
            *flag = (c == '1') ? SDL_TRUE : SDL_FALSE;
        }
        fclose(f);
    }
}

static void MISTER_PrintStats(MISTER_Data *d, Uint64 now)
{
    Uint64 freq = SDL_GetPerformanceFrequency();
    double secs = (double)(now - d->t0) / (double)freq, calls = (double)d->st_calls, shown;
    Uint64 wait, copy, nshown;
    FILE *out = d->stats_path ? fopen(d->stats_path, "a") : stderr;
    char stamp[16] = "";
    time_t tt = time(NULL);
    struct tm tmv;

    if (localtime_r(&tt, &tmv)) {
        strftime(stamp, sizeof(stamp), "%H:%M:%S", &tmv);
    }
    if (d->lock) {
        SDL_LockMutex(d->lock);
    }
    wait = d->st_wait; copy = d->st_copy; nshown = d->st_shown;
    d->st_wait = d->st_copy = d->st_shown = 0;
    if (d->lock) {
        SDL_UnlockMutex(d->lock);
    }
    shown = nshown ? (double)nshown : 1.0;
    if (out) {
        fprintf(out, "%s vsync=%d async=%d | game %.1f/s (drawing %.2f ms wall, %.2f ms cpu, snapshot %.2f ms) | shown %.1f/s (vsync wait %.2f ms, copy %.2f ms) | worst game gap %.0f ms\n",
                stamp, d->vsync ? 1 : 0, d->async ? 1 : 0, calls / secs,
                1000.0 * d->st_game / freq / (calls ? calls : 1), d->st_game_cpu / 1e6 / (calls ? calls : 1),
                1000.0 * d->st_snap / freq / (calls ? calls : 1), nshown / secs,
                1000.0 * wait / freq / shown, 1000.0 * copy / freq / shown, 1000.0 * d->max_gap / freq);
        if (out != stderr) {
            fclose(out);
        }
    }
    d->t0 = now;
    d->st_game = d->st_game_cpu = d->st_snap = d->st_calls = d->max_gap = 0;
}

static int MISTER_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    MISTER_Data *d = (MISTER_Data *)_this->driverdata;
    SDL_bool zc = (d->zerocopy && d->async && d->thread) ? SDL_TRUE : SDL_FALSE;
    SDL_Surface *surface = zc ? window->surface : (SDL_Surface *)SDL_GetWindowData(window, MISTER_SURFACE);
    Uint64 t_start = 0, t_snap = 0, cpu_start = 0;
    int i;

    if (!surface) {
        return SDL_SetError("MiSTer: window has no framebuffer surface");
    }
    if (!d->map) {
        return 0;
    }
    if (d->stats) {
        t_start = SDL_GetPerformanceCounter();
        cpu_start = MISTER_ThreadCpu();
        if (t_start - d->ctl_t >= SDL_GetPerformanceFrequency()) {
            /* live A/B switches: echo 0 > /tmp/mister_vsync  (or /tmp/mister_async) */
            SDL_bool was_async = d->async;
            d->ctl_t = t_start;
            MISTER_ReadCtl("/tmp/mister_vsync", &d->vsync);
            if (!d->zerocopy) {
                MISTER_ReadCtl("/tmp/mister_async", &d->async);
            }
            if (was_async && !d->async) {
                MISTER_Flush(d);
            }
        }
        if (d->prev_end) {
            d->st_game += t_start - d->prev_end; /* wall time the game spent between presents */
            d->st_game_cpu += cpu_start - d->prev_cpu_end;
            if (t_start - d->prev_end > d->max_gap) {
                d->max_gap = t_start - d->prev_end;
            }
        }
        d->st_calls++;
    }

    if (d->async) {
        int idx, other;

        if (!d->thread || d->buf_w != surface->w || d->buf_h != surface->h || d->buf_pitch != surface->pitch) {
            if (zc) {
                return SDL_SetError("MiSTer: window size changed under zero-copy presenting");
            }
            if (MISTER_StartPresenter(d, surface->w, surface->h, surface->pitch) < 0) {
                return -1;
            }
        }
        (void)rects;
        (void)numrects;
        if (!zc) {
            /* snapshot the whole surface so partial updates never leave a stale buffer behind */
            SDL_memcpy(d->bufs[d->work], surface->pixels, (size_t)surface->pitch * surface->h);
        }
        if (d->stats) {
            t_snap = SDL_GetPerformanceCounter();
        }
        SDL_LockMutex(d->lock);
        other = d->ready; /* a frame nobody showed yet: drop it and reuse its buffer */
        d->ready = d->work;
        if (other >= 0) {
            idx = other;
        } else {
            for (idx = 0; idx < 3; idx++) {
                if (idx != d->ready && idx != d->busy) {
                    break;
                }
            }
        }
        d->work = idx;
        SDL_CondBroadcast(d->cond);
        SDL_UnlockMutex(d->lock);
        if (zc) {
            surface->pixels = d->bufs[d->work]; /* the renderer draws the next frame into a free buffer */
        }
    } else {
        Uint64 wait, copy;

        (void)rects;
        (void)numrects;
        MISTER_Flush(d);
        MISTER_Show(d, (const Uint8 *)surface->pixels, surface->w, surface->h, surface->pitch, &wait, &copy);
        d->st_wait += wait;
        d->st_copy += copy;
        d->st_shown++;
        t_snap = t_start;
    }

    if (d->stats) {
        Uint64 now = SDL_GetPerformanceCounter();
        d->st_snap += t_snap - t_start;
        d->prev_end = now;
        d->prev_cpu_end = MISTER_ThreadCpu();
        if (!d->t0) {
            d->t0 = now;
        } else if (now - d->t0 >= 3 * SDL_GetPerformanceFrequency()) {
            MISTER_PrintStats(d, now);
        }
    }
    (void)i;
    return 0;
}

/* ------------------------------------------------------------ bootstrap --- */

static void MISTER_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device->driverdata);
    SDL_free(device);
}

static SDL_VideoDevice *MISTER_CreateDevice(void)
{
    SDL_VideoDevice *device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    MISTER_Data *d = (MISTER_Data *)SDL_calloc(1, sizeof(MISTER_Data));

    if (!device || !d) {
        SDL_free(device);
        SDL_free(d);
        SDL_OutOfMemory();
        return NULL;
    }
    d->fd = -1;
    d->vt_fd = -1;
    device->driverdata = d;

    device->VideoInit = MISTER_VideoInit;
    device->VideoQuit = MISTER_VideoQuit;
    device->GetDisplayModes = MISTER_GetDisplayModes;
    device->SetDisplayMode = MISTER_SetDisplayMode;
    device->PumpEvents = MISTER_PumpEvents;
    device->CreateSDLWindow = MISTER_CreateWindow;
    device->SetWindowSize = MISTER_SetWindowSize;
    device->DestroyWindow = MISTER_DestroyWindow;
    device->CreateWindowFramebuffer = MISTER_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = MISTER_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = MISTER_DestroyWindowFramebuffer;
    device->free = MISTER_DeleteDevice;
    return device;
}

VideoBootStrap MISTER_bootstrap = {
    MISTER_DRIVER_NAME, "MiSTer framebuffer video driver",
    MISTER_CreateDevice,
    NULL /* no ShowMessageBox implementation */
};

#endif /* SDL_VIDEO_DRIVER_MISTER */
