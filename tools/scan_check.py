"""Standalone check for the BossSense NPC_ scanner logic.

Parses the NPC_ top-level group of a TES5 plugin exactly like the DLL does
(record headers, GRUP skipping, XXXX subrecords, zlib-compressed records)
and reports how many records the scoring scanner would keep.

Usage: python scan_check.py <plugin1> [plugin2 ...]
"""

import struct
import sys
import zlib

COMPRESSED = 0x00040000


def subrecords(data):
    pos = 0
    pending = None
    while pos + 6 <= len(data):
        sig = data[pos:pos + 4]
        size = struct.unpack_from("<H", data, pos + 4)[0]
        pos += 6
        if sig == b"XXXX":
            if pos + 4 > len(data):
                return
            pending = struct.unpack_from("<I", data, pos)[0]
            pos += 4
            continue
        real = pending if pending is not None else size
        pending = None
        if pos + real > len(data):
            return
        yield sig, data[pos:pos + real]
        pos += real


def parse_npc(record_data):
    edid = ""
    unique = False
    aidt = False
    for sig, payload in subrecords(record_data):
        if sig == b"EDID":
            edid = payload.split(b"\x00", 1)[0].decode("utf-8", "replace")
        elif sig == b"ACBS" and len(payload) >= 4:
            unique = bool(struct.unpack_from("<I", payload, 0)[0] & 0x20)
        elif sig == b"AIDT" and len(payload) >= 2:
            aggr = struct.unpack_from("<b", payload, 0)[0]
            conf = struct.unpack_from("<b", payload, 1)[0]
            aidt = aggr >= 2 and conf == 4
    return edid, unique, aidt


def walk(path):
    total = 0
    kept = 0
    edid_boss = 0
    uniques = 0
    aidts = 0
    misses = 0
    compressed = 0
    with open(path, "rb") as fh:
        data = fh.read()

    # TES4 header
    pos = 24 + struct.unpack_from("<I", data, 4)[0]

    while pos + 24 <= len(data):
        sig = data[pos:pos + 4]
        size = struct.unpack_from("<I", data, pos + 4)[0]
        if sig == b"GRUP":
            label = data[pos + 8:pos + 12]
            end = pos + size
            if end <= pos or end > len(data):
                break
            if label == b"NPC_":
                p = pos + 24
                while p + 24 <= end:
                    rsig = data[p:p + 4]
                    rsize = struct.unpack_from("<I", data, p + 4)[0]
                    rflags = struct.unpack_from("<I", data, p + 8)[0]
                    if rsig == b"GRUP":
                        # nested group: skip
                        gsize = rsize
                        p += gsize
                        continue
                    body = data[p + 24:p + 24 + rsize]
                    p += 24 + rsize
                    if rsig != b"NPC_":
                        continue
                    total += 1
                    if rflags & COMPRESSED:
                        compressed += 1
                        if len(body) < 4:
                            continue
                        try:
                            body = zlib.decompress(body[4:])
                        except zlib.error:
                            misses += 1
                            continue
                    edid, unique, aidt = parse_npc(body)
                    if "boss" in edid.lower():
                        edid_boss += 1
                    if unique:
                        uniques += 1
                    if aidt:
                        aidts += 1
                    if "boss" in edid.lower() or unique or aidt:
                        kept += 1
            pos = end
        else:
            pos += 24 + size

    print(f"{path}")
    print(f"  NPC_ records      : {total}")
    print(f"  stored by scanner : {kept}")
    print(f"  EDID has 'boss'   : {edid_boss}")
    print(f"  unique flag       : {uniques}")
    print(f"  AIDT aggressive+  : {aidts}")
    print(f"  compressed bodies : {compressed}")
    print(f"  parse failures    : {misses}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    for arg in sys.argv[1:]:
        walk(arg)
