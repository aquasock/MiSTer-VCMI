#!/usr/bin/env python3
# fbcap.py raw OUT            : dump the framebuffer to OUT
# fbcap.py burst N            : read N frames back to back, print timing and per-frame change counts for regions
import os, sys, time
vs = open('/sys/class/graphics/fb0/virtual_size').read().strip().split(','); W, H = int(vs[0]), int(vs[1])
stride = int(open('/sys/class/graphics/fb0/stride').read())
fd = os.open('/dev/fb0', os.O_RDONLY); size = stride * H
if sys.argv[1] == 'bbox':
    n = int(sys.argv[2]); fr = []; t = []
    for i in range(n):
        t.append(time.time()); fr.append(os.pread(fd, size, 0))
    x0, x1, y0, y1 = 10, 595, 10, 490
    last = None; lt = t[0]
    for i in range(1, n):
        cols = []; rws = []
        for y in range(y0, y1):
            a = fr[i-1][y*stride + x0*4: y*stride + x1*4]; b = fr[i][y*stride + x0*4: y*stride + x1*4]
            if a != b:
                rws.append(y)
                # first/last differing pixel in the row
                for x in range(0, (x1-x0)*4, 4):
                    if a[x:x+4] != b[x:x+4]: cols.append(x0 + x//4); break
                for x in range((x1-x0)*4-4, -1, -4):
                    if a[x:x+4] != b[x:x+4]: cols.append(x0 + x//4); break
        if rws:
            print('frame %3d  +%4.0f ms since last change  rows %d-%d (%d)  cols %d-%d' % (i, 1000*(t[i]-lt), rws[0], rws[-1], len(rws), min(cols), max(cols)))
            lt = t[i]
    sys.exit()
if sys.argv[1] == 'raw':
    open(sys.argv[2], 'wb').write(os.pread(fd, size, 0)); print(W, H, stride); sys.exit()
n = int(sys.argv[2]); frames = []; ts = []
for i in range(n):
    ts.append(time.time()); frames.append(os.pread(fd, size, 0))
def rows(f, x0, x1, y0, y1): return [f[y*stride + x0*4: y*stride + x1*4] for y in range(y0, y1)]
# The 800x600 window is centred in the framebuffer; adventure map layout: map ~x7-599, panel ~x607-793
ox, oy = (W - 800) // 2, (H - 600) // 2
regions = {'log window (static)': (ox+10, ox+595, oy+510, oy+548), 'hero list+buttons (static)': (ox+612, ox+790, oy+200, oy+380), 'hero info (static)': (ox+612, ox+790, oy+400, oy+585), 'resource bar (static)': (ox+10, ox+595, oy+578, oy+596), 'minimap': (ox+630, ox+780, oy+25, oy+170), 'map': (ox+10, ox+595, oy+10, oy+490)}
print('frames %d over %.3f s (%.1f ms each)  fb %dx%d' % (n, ts[-1]-ts[0], 1000*(ts[-1]-ts[0])/max(1, n-1), W, H))
for name, (x0, x1, y0, y1) in regions.items():
    r = [rows(f, x0, x1, y0, y1) for f in frames]
    changed = [sum(1 for a, b in zip(r[i], r[i+1]) if a != b) for i in range(n-1)]
    uniq = len(set(hash(tuple(x)) for x in r))
    print('%-27s rows changed between consecutive reads: %s' % (name, ' '.join(str(c) for c in changed)))
    print('%-27s distinct frames: %d of %d' % ('', uniq, n))

# bbox mode is a separate entry point: fbcap.py bbox N  -> for the map region, the bounding box and time of every change
