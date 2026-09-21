#!/usr/bin/env python3
# fbpan.py SECONDS : sample a sparse set of framebuffer rows ~every ms; report intervals between visible changes
import os, sys, time
vs = open('/sys/class/graphics/fb0/virtual_size').read().strip().split(','); W, H = int(vs[0]), int(vs[1])
stride = int(open('/sys/class/graphics/fb0/stride').read())
ox, oy = (W - 800) // 2, (H - 600) // 2
fd = os.open('/dev/fb0', os.O_RDONLY)
rows = [oy + y for y in range(24, 340, 16)]                       # map view (valid in both layouts)
srows = [oy + y for y in (220, 260, 300, 340, 420, 450, 480, 520, 560, 586)]  # right panel + resource bar: static UI
x0 = (ox + 12) * 4; n = 580 * 4
dur = float(sys.argv[1]); t_end = time.time() + dur
prev = None; changes = []; samples = 0; sprev = None; schanges = 0; sx = (ox + 614) * 4; sn = 176 * 4
pread = os.pread; now = time.time
while True:
    t = now()
    if t > t_end: break
    cur = b''.join(pread(fd, n, r * stride + x0) for r in rows)
    samples += 1
    scur = b''.join(pread(fd, sn, r * stride + sx) for r in srows)
    if sprev is not None and scur != sprev: schanges += 1
    sprev = scur
    if prev is not None and cur != prev:
        changes.append(t)
    prev = cur
# a frame update is a burst of row-by-row changes as the presenter copies; group changes closer than 8 ms
starts = []
for t in changes:
    if not starts or 1000 * (t - last_t) >= 8: starts.append(t)
    last_t = t
iv = sorted(1000 * (b - a) for a, b in zip(starts, starts[1:]))
print('samples %d (%.2f ms each)  raw changes %d  distinct frames %d in %.1f s = %.1f/s' % (samples, 1000 * dur / samples, len(changes), len(starts), dur, len(starts) / dur))
print('static UI regions: %d samples where the right panel / resource bar differed from the previous sample' % schanges)
if iv:
    def pct(p): return iv[min(len(iv) - 1, int(len(iv) * p))]
    print('interval between frames (ms): min %.1f  p10 %.1f  median %.1f  p90 %.1f  p99 %.1f  max %.1f' % (iv[0], pct(.1), pct(.5), pct(.9), pct(.99), iv[-1]))
    buckets = [(0, 14), (14, 19), (19, 26), (26, 36), (36, 52), (52, 120), (120, 1e9)]
    print('histogram:', '  '.join('%g-%g ms: %d' % (a, min(b, 999), sum(1 for v in iv if a <= v < b)) for a, b in buckets))
