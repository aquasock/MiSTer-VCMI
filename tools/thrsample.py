#!/usr/bin/env python3
# thrsample.py SECONDS : sample every vcmiclient thread's state + kernel wait channel ~250 Hz, summarised every 3 s
import os, sys, time, collections
pid = [p for p in os.listdir('/proc') if p.isdigit() and 'bin/vcmiclient' in open('/proc/%s/cmdline' % p).read()][0]
SYS = {'240': 'futex', '422': 'futex64', '162': 'nanosleep', '263': 'clock_nanosleep', '407': 'clock_nanosleep64', '54': 'ioctl', '168': 'poll', '336': 'ppoll', '346': 'epoll_pwait', '252': 'epoll_wait', '3': 'read', '142': 'select', '175': 'rt_sigprocmask', '4': 'write', '2': 'fork', '5': 'open', '6': 'close', '19': 'lseek', '180': 'pread', '181': 'pwrite', '91': 'munmap', '192': 'mmap2'}
dur = float(sys.argv[1]); t_end = time.time() + dur
def tasks():
    out = {}
    for t in os.listdir('/proc/%s/task' % pid):
        try: out[t] = open('/proc/%s/task/%s/comm' % (pid, t)).read().strip()
        except Exception: pass
    return out
names = tasks()
def label(t): return 'MAIN' if t == pid else names.get(t, t)
win = collections.defaultdict(lambda: collections.Counter()); n = 0; t_win = time.time()
def flush():
    global win, n, t_win
    if not n: return
    line = time.strftime('%H:%M:%S') + ' (%d samples)' % n
    agg = collections.defaultdict(collections.Counter)
    for tid, c in win.items():
        agg[label(tid)].update(c)
    for lab in sorted(agg, key=lambda l: (l != 'MAIN', l)):
        c = agg[lab]; tot = sum(c.values()) or 1
        run = 100.0 * c['R'] / tot
        top = ' '.join('%s:%.0f%%' % (k, 100.0 * v / tot) for k, v in c.most_common(4) if k != 'R')
        if lab == 'MAIN' or run > 3 or lab in ('runNetwork', 'runServer'):
            line += '\n   %-12s run %3.0f%%  %s' % (lab, run, top)
    print(line, flush=True); win = collections.defaultdict(lambda: collections.Counter()); n = 0; t_win = time.time()
while time.time() < t_end:
    for t in list(names):
        try:
            st = open('/proc/%s/task/%s/stat' % (pid, t)).read().rsplit(')', 1)[1].split()[0]
            if st == 'R': win[t]['R'] += 1
            else:
                sc = open('/proc/%s/task/%s/syscall' % (pid, t)).read().split()
                nr = sc[0] if sc and sc[0].lstrip('-').isdigit() else '?'
                win[t][st + ':' + SYS.get(nr, 'sys' + nr)] += 1
        except Exception: pass
    n += 1
    if time.time() - t_win >= 3: flush()
    time.sleep(0.002)
flush()
