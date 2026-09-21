#!/usr/bin/env python3
"""Symbolize an SDL_MISTER_PROFILE capture.  usage: profsym.py PROFILE [--top N] [--skip-idle]
Resolves each sampled pc against the unstripped libraries in work/ (same addresses as the deployed, stripped ones)."""
import bisect, collections, os, re, subprocess, sys
W = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'work')
UNSTRIPPED = {
    'vcmiclient': W + '/vcmi-install/vcmiclient', 'libvcmi.so': W + '/vcmi-install/libvcmi.so',
    'libNullkiller2.so': W + '/vcmi-install/AI/libNullkiller2.so', 'libNullkiller.so': W + '/vcmi-install/AI/libNullkiller.so',
    'libBattleAI.so': W + '/vcmi-install/AI/libBattleAI.so', 'libSDL2-2.0.so.0': W + '/prefix/lib/libSDL2-2.0.so.0.3200.10',
    'libtbb.so.12': W + '/prefix/lib/libtbb.so.12.17', 'libc.so.6': '/usr/arm-linux-gnueabihf/lib/libc.so.6',
    'libstdc++.so.6': None, 'libm.so.6': '/usr/arm-linux-gnueabihf/lib/libm.so.6'}
only = sys.argv[sys.argv.index('--tid') + 1] if '--tid' in sys.argv else None
path = sys.argv[1]; top = int(sys.argv[sys.argv.index('--top') + 1]) if '--top' in sys.argv else 30
maps = {}
for l in open(path + '.maps'):
    m = re.match(r'([0-9a-f]+)-([0-9a-f]+) \S+ ([0-9a-f]+) \S+ \S+\s+(\S+)$', l.strip())
    if m and int(m[3], 16) == 0 and '/' in m[4]:
        maps.setdefault(os.path.basename(m[4]), int(m[1], 16))
ranges = []
for l in open(path + '.maps'):
    m = re.match(r'([0-9a-f]+)-([0-9a-f]+) \S+ ([0-9a-f]+) \S+ \S+\s+(\S+)$', l.strip())
    if m and '/' in m[4]: ranges.append((int(m[1], 16), int(m[2], 16), os.path.basename(m[4])))
cache = {}
def syms(mod):
    if mod not in cache:
        f = UNSTRIPPED.get(mod)
        L = []
        if f and os.path.exists(f):
            for flag in (['-C', '-n'], ['-D', '-C', '-n']):
                out = subprocess.run(['arm-linux-gnueabihf-nm'] + flag + [f], capture_output=True, text=True).stdout
                for line in out.splitlines():
                    p = line.split(None, 2)
                    if len(p) == 3 and p[1] in 'tTwWiI':
                        try: L.append((int(p[0], 16), p[2]))
                        except ValueError: pass
                if L: break
        L.sort(); cache[mod] = (L, [a for a, _ in L])
    return cache[mod]
def resolve(pc):
    for lo, hi, mod in ranges:
        if lo <= pc < hi:
            base = maps.get(mod, lo); L, A = syms(mod)
            if A:
                i = bisect.bisect_right(A, (pc & ~1) - base) - 1
                if i >= 0: return mod, re.sub(r'\(.*', '', L[i][1])[:95]
            return mod, '?'
    return '?', '?'
callers = collections.Counter(); exc = collections.Counter(); mods = collections.Counter(); tids = collections.Counter(); total = 0
for l in open(path):
    p = l.split()
    if len(p) != 3: continue
    if only and p[0] != only: continue
    tid, pc = p[0], int(p[1], 16); mod, sym = resolve(pc)
    if mod == 'libc.so.6':
        cmod, csym = resolve(int(p[2], 16))
        callers[(cmod, csym)] += 1
    exc[(mod, sym)] += 1; mods[mod] += 1; tids[tid] += 1; total += 1
print('%d samples (~%.1f s of CPU across all threads)' % (total, total / 1000.0))
print('\nby module:'); [print('  %5.1f%%  %s' % (100.0 * v / total, k)) for k, v in mods.most_common(8)]
names = {}
if os.path.exists(path + '.threads'):
    for l in open(path + '.threads'):
        a = l.split(None, 1)
        if len(a) == 2: names[a[0]] = a[1].strip()
print('\nby thread (samples = ms of CPU):')
for t, v in tids.most_common(8): print('  %6d  %5.1f%%  %s (%s)' % (v, 100.0 * v / total, names.get(t, '?'), t))
print('\ntop functions (self time):')
for (mod, sym), v in exc.most_common(top): print('  %5.1f%%  %-16s %s' % (100.0 * v / total, mod, sym))

lib = sum(callers.values())
if lib:
    print('\nwho called into libc (by return address; %d samples):' % lib)
    for (mod, sym), v in callers.most_common(top): print('  %5.1f%%  %-16s %s' % (100.0 * v / lib, mod, sym))
