"""Dump struktur PINBALL.DAT (format PARTOUT 4.0), mengikuti FUN_010064e2 di Pinball.exe."""
import struct, sys, collections

SIZES = [2, -1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, -1, -1]  # DAT_010235d0


def parse(path):
    d = open(path, "rb").read()
    assert d[:20] == b"PARTOUT(4.0)RESOURCE"
    app = d[0x15:0x47].split(b"\0")[0].decode()
    desc = d[0x47:0xab].split(b"\0")[0].decode()
    file_size, n_groups, body_size, unk = struct.unpack_from("<IHIH", d, 0xab)
    p = 0xb7 + unk
    groups = []
    for _ in range(n_groups):
        cnt = d[p]; p += 1
        entries = []
        for _ in range(cnt):
            t = d[p]; p += 1
            sz = SIZES[t]
            if sz < 0:
                sz = struct.unpack_from("<I", d, p)[0]; p += 4
            entries.append((t, d[p:p + sz]))
            p += sz
        groups.append(entries)
    assert p == len(d), (p, len(d))
    return dict(app=app, desc=desc, file_size=file_size, body_size=body_size), groups


if __name__ == "__main__":
    hdr, groups = parse(sys.argv[1])
    print(hdr, "groups:", len(groups))
    c = collections.Counter(t for g in groups for t, _ in g)
    print("entry types:", dict(sorted(c.items())))
    for i, g in enumerate(groups[:int(sys.argv[2]) if len(sys.argv) > 2 else 12]):
        print(i, [(t, len(b), b[:24]) for t, b in g])
