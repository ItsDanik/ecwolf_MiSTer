#!/usr/bin/env python3
"""Print what the hybrid core publishes (run on the MiSTer with the core loaded).
usage: status.py [seconds]   samples the status block 20 times per second"""
import mmap, os, struct, sys, time

fd = os.open("/dev/mem", os.O_RDONLY | os.O_SYNC)
m = mmap.mmap(fd, 0x1000, mmap.MAP_SHARED, mmap.PROT_READ, offset=0x30000000)
last = None
end = time.monotonic() + (float(sys.argv[1]) if len(sys.argv) > 1 else 0)
while True:
    ctrl = struct.unpack_from("<4I", m, 0)
    st = struct.unpack_from("<32I", m, 0x40)
    line = "joy1 %08x joy2 %08x analog1 %08x osd %08x%08x keys %s" % (
        st[4], st[5], st[6], st[9], st[8], "".join("%08x" % k for k in st[16:32]).lstrip("0") or "0")
    if line != last:
        print("field %d ctrl %08x %08x: %s" % (st[1], ctrl[0], ctrl[1], line), flush=True)
        last = line
    if time.monotonic() >= end:
        break
    time.sleep(0.05)
