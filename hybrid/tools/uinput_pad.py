#!/usr/bin/env python3
"""Virtual gamepad for end-to-end input tests on the MiSTer.
Buttons go through Main_MiSTer -> hps_io -> hybrid core -> shared memory -> game.
usage: uinput_pad.py "<ms> <button> <hold_ms>" ...
  buttons: a b x y l r select start up down left right (MiSTer names: A east, B south)"""
import fcntl, os, struct, sys, time

UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_ABSBIT = 0x40045564, 0x40045565, 0x40045567
UI_DEV_SETUP, UI_ABS_SETUP, UI_DEV_CREATE, UI_DEV_DESTROY = 0x405C5503, 0x401C5504, 0x5501, 0x5502
EV_SYN, EV_KEY, EV_ABS = 0, 1, 3
ABS_X, ABS_Y, ABS_HAT0X, ABS_HAT0Y = 0, 1, 16, 17
KEYS = {"b": 0x130, "a": 0x131, "x": 0x133, "y": 0x134, "l": 0x136, "r": 0x137, "select": 0x13A, "start": 0x13B}
HAT = {"up": (ABS_HAT0Y, -1), "down": (ABS_HAT0Y, 1), "left": (ABS_HAT0X, -1), "right": (ABS_HAT0X, 1)}

fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
fcntl.ioctl(fd, UI_SET_EVBIT, EV_ABS)
for k in KEYS.values():
    fcntl.ioctl(fd, UI_SET_KEYBIT, k)
for axis, lo, hi in ((ABS_X, -32768, 32767), (ABS_Y, -32768, 32767), (ABS_HAT0X, -1, 1), (ABS_HAT0Y, -1, 1)):
    fcntl.ioctl(fd, UI_SET_ABSBIT, axis)
    # struct uinput_abs_setup { __u16 code; struct input_absinfo { value, minimum, maximum, fuzz, flat, resolution } }
    fcntl.ioctl(fd, UI_ABS_SETUP, struct.pack("HHiiiiii", axis, 0, 0, lo, hi, 0, 0, 0))
fcntl.ioctl(fd, UI_DEV_SETUP, struct.pack("HHHH80sI", 3, 0x1d6b, 0x0105, 1, b"hybrid test gamepad", 0))
fcntl.ioctl(fd, UI_DEV_CREATE)
time.sleep(1.5)  # Main_MiSTer picks the device up

def emit(type_, code, value):
    os.write(fd, struct.pack("llHHi", 0, 0, type_, code, value))

events = []
for arg in sys.argv[1:]:
    t, name, hold = arg.split()
    events.append((int(t), name, 1))
    events.append((int(t) + int(hold), name, 0))
events.sort()

start = time.monotonic()
for t, name, value in events:
    delay = start + t / 1000.0 - time.monotonic()
    if delay > 0:
        time.sleep(delay)
    if name in KEYS:
        emit(EV_KEY, KEYS[name], value)
    else:
        emit(EV_ABS, HAT[name][0], HAT[name][1] * value)
    emit(EV_SYN, 0, 0)
time.sleep(0.5)
fcntl.ioctl(fd, UI_DEV_DESTROY)
os.close(fd)
