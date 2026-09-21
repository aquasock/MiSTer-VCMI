/* Copy-speed test for the MiSTer framebuffer: which copy routine gets the most bandwidth into fb0?
   Usage: membench [bytes] */
#include <arm_neon.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

static void copy_neon64(uint8_t *d, const uint8_t *s, size_t n)
{
    for (size_t i = 0; i < n; i += 64) {
        __builtin_prefetch(s + i + 256);
        uint8x16x4_t v = vld1q_u8_x4(s + i);
        vst1q_u8_x4(d + i, v);
    }
}
static void copy_neon128(uint8_t *d, const uint8_t *s, size_t n)
{
    for (size_t i = 0; i < n; i += 128) {
        __builtin_prefetch(s + i + 384);
        uint8x16x4_t a = vld1q_u8_x4(s + i), b = vld1q_u8_x4(s + i + 64);
        vst1q_u8_x4(d + i, a);
        vst1q_u8_x4(d + i + 64, b);
    }
}
static void copy_ldm(uint8_t *d, const uint8_t *s, size_t n)
{
    for (size_t i = 0; i < n; i += 32) {
        __builtin_prefetch(s + i + 256);
        register uint32_t a0, a1, a2, a3, a4, a5, a6, a7;
        __asm__ volatile("ldmia %8, {%0-%7}\n\tstmia %9, {%0-%7}"
                         : "=&r"(a0), "=&r"(a1), "=&r"(a2), "=&r"(a3), "=&r"(a4), "=&r"(a5), "=&r"(a6), "=&r"(a7)
                         : "r"(s + i), "r"(d + i) : "memory");
    }
}
static void copy_memcpy(uint8_t *d, const uint8_t *s, size_t n) { memcpy(d, s, n); }

typedef void (*fn)(uint8_t *, const uint8_t *, size_t);

static void run(const char *name, fn f, uint8_t *dst, const uint8_t *src, size_t n)
{
    double best = 1e9, sum = 0; int reps = 40;
    for (int r = 0; r < reps; r++) {
        double t = now(); f(dst, src, n); t = now() - t;
        if (t < best) best = t; sum += t;
    }
    printf("  %-22s best %.2f ms (%.0f MB/s)   avg %.2f ms (%.0f MB/s)\n", name, best * 1e3, n / best / 1e6, sum / reps * 1e3, n / (sum / reps) / 1e6);
}

int main(int argc, char **argv)
{
    int fd = open("/dev/fb0", O_RDWR);
    struct fb_fix_screeninfo fix; struct fb_var_screeninfo var;
    ioctl(fd, FBIOGET_FSCREENINFO, &fix); ioctl(fd, FBIOGET_VSCREENINFO, &var);
    size_t n = (size_t)fix.line_length * var.yres;
    if (argc > 1) n = (size_t)atol(argv[1]);
    uint8_t *fb = mmap(NULL, fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    uint8_t *src = aligned_alloc(64, n), *ram = aligned_alloc(64, n);
    for (size_t i = 0; i < n; i++) src[i] = (uint8_t)(i * 7);
    printf("fb %ux%u stride %u, copying %zu bytes\n", var.xres, var.yres, fix.line_length, n);
    printf("to framebuffer:\n");
    run("memcpy (libc)", copy_memcpy, fb, src, n);
    run("NEON 64B + prefetch", copy_neon64, fb, src, n);
    run("NEON 128B + prefetch", copy_neon128, fb, src, n);
    run("LDM/STM 32B + prefetch", copy_ldm, fb, src, n);
    { double t = now(); for (int r = 0; r < 20; r++) { lseek(fd, 0, SEEK_SET); if (write(fd, src, n) < 0) break; } t = (now() - t) / 20;
      printf("  %-22s avg  %.2f ms (%.0f MB/s)\n", "write() to /dev/fb0", t * 1e3, n / t / 1e6); }
    printf("to ordinary RAM (for comparison):\n");
    run("memcpy (libc)", copy_memcpy, ram, src, n);
    run("NEON 64B + prefetch", copy_neon64, ram, src, n);
    return 0;
}
