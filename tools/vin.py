#!/usr/bin/env python3
# Virtual mouse+keyboard on the MiSTer, driven through a FIFO: /tmp/vin.cmd
#   slam | goto X Y | move DX DY | click [left|right] | dclick | key NAME | quit
import fcntl, os, struct, sys, time
UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_RELBIT = 0x40045564, 0x40045565, 0x40045566
UI_DEV_SETUP, UI_DEV_CREATE, UI_DEV_DESTROY = 0x405c5503, 0x5501, 0x5502
EV_SYN, EV_KEY, EV_REL = 0, 1, 2
BTN_LEFT, BTN_RIGHT = 0x110, 0x111
KEYS = {'ESC':1,'ENTER':28,'SPACE':57,'E':18,'H':35,'A':30,'UP':103,'DOWN':108,'LEFT':105,'RIGHT':106,'F4':62}
fd = os.open('/dev/uinput', os.O_WRONLY | os.O_NONBLOCK)
fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY); fcntl.ioctl(fd, UI_SET_EVBIT, EV_REL); fcntl.ioctl(fd, UI_SET_EVBIT, EV_SYN)
for k in [BTN_LEFT, BTN_RIGHT] + sorted(set(KEYS.values())) + list(range(2, 58)):
    fcntl.ioctl(fd, UI_SET_KEYBIT, k)
fcntl.ioctl(fd, UI_SET_RELBIT, 0); fcntl.ioctl(fd, UI_SET_RELBIT, 1)
fcntl.ioctl(fd, UI_DEV_SETUP, struct.pack('HHHH80sI', 3, 0x1209, 0x0002, 1, b'vin virtual mouse+kbd', 0))
fcntl.ioctl(fd, UI_DEV_CREATE)
def ev(t, c, v): os.write(fd, struct.pack('llHHi', 0, 0, t, c, v))
def sync(): ev(EV_SYN, 0, 0)
def move(dx, dy):
    while dx or dy:                       # small steps so nothing is clamped or dropped
        sx = max(-100, min(100, dx)); sy = max(-100, min(100, dy))
        if sx: ev(EV_REL, 0, sx)
        if sy: ev(EV_REL, 1, sy)
        sync(); dx -= sx; dy -= sy; time.sleep(0.004)
def click(btn=BTN_LEFT):
    ev(EV_KEY, btn, 1); sync(); time.sleep(0.06); ev(EV_KEY, btn, 0); sync()
def key(code):
    ev(EV_KEY, code, 1); sync(); time.sleep(0.06); ev(EV_KEY, code, 0); sync()
FIFO = '/tmp/vin.cmd'
if not os.path.exists(FIFO): os.mkfifo(FIFO)
print('ready', flush=True)
time.sleep(1.5)                            # let Main and SDL notice the new device
while True:
    with open(FIFO) as f:
        for line in f:
            a = line.split()
            if not a: continue
            c = a[0]
            if c == 'quit': fcntl.ioctl(fd, UI_DEV_DESTROY); sys.exit(0)
            elif c == 'slam': move(-3000, -3000)
            elif c == 'goto': move(-3000, -3000); time.sleep(0.05); move(int(a[1]), int(a[2]))
            elif c == 'move': move(int(a[1]), int(a[2]))
            elif c == 'click': click(BTN_RIGHT if len(a) > 1 and a[1] == 'right' else BTN_LEFT)
            elif c == 'dclick': click(); time.sleep(0.12); click()
            elif c == 'key': key(KEYS[a[1].upper()])
