"""Baca konstanta dari Pinball.exe: peconst.py <alamat> [f|d|i|h|s] [jumlah]."""
import struct, sys

import os
EXE = os.environ.get("PINBALL_EXE", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "3dPinballWindows", "Pinball.exe"))
d = open(EXE, "rb").read()
pe = struct.unpack_from("<I", d, 0x3c)[0]
n = struct.unpack_from("<H", d, pe + 6)[0]
opt = struct.unpack_from("<H", d, pe + 20)[0]
secs = []
for i in range(n):
    s = pe + 24 + opt + 40 * i
    vs, va, rs, ro = struct.unpack_from("<IIII", d, s + 8)
    secs.append((va + 0x1000000, max(vs, rs), ro))


def off(v):
    for va, size, ro in secs:
        if va <= v < va + size:
            return ro + v - va
    raise ValueError(hex(v))


if __name__ == "__main__":
    addr = int(sys.argv[1], 16)
    kind = sys.argv[2] if len(sys.argv) > 2 else "d"
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    if kind == "s":
        o = off(addr)
        print(d[o:d.index(b"\0", o)].decode("latin1"))
    else:
        size = struct.calcsize(kind)
        for i in range(count):
            print(hex(addr + i * size), struct.unpack_from("<" + kind, d, off(addr + i * size))[0])
