#!/usr/bin/env python3
"""Print a FIT activity's records as CSV: seconds,speed_mps,hr_bpm.

Minimal FIT decoder: enough for the record messages the app writes.
Missing values are printed as 0. Usage: fit_csv.py <file.fit>
"""

import struct
import sys

RECORD = 20
TIMESTAMP, HEART_RATE, ENHANCED_SPEED = 253, 3, 73
FORMATS = {0: "B", 1: "b", 2: "B", 0x83: "h", 0x84: "H", 0x85: "i", 0x86: "I",
           0x8B: "H", 0x8C: "I", 0x0A: "B", 0x0D: "B", 0x88: "f", 0x89: "d"}
INVALID = {"B": 0xFF, "H": 0xFFFF, "I": 0xFFFFFFFF}


def records(data):
    end = data[0] + struct.unpack("<I", data[4:8])[0]
    pos, defs = data[0], {}
    while pos < end:
        head = data[pos]
        pos += 1
        if head & 0xC0 == 0x40:  # definition
            endian = "<" if data[pos + 1] == 0 else ">"
            mesg = struct.unpack(endian + "H", data[pos + 2:pos + 4])[0]
            count = data[pos + 4]
            pos += 5
            fields = [tuple(data[pos + 3 * i:pos + 3 * i + 3]) for i in range(count)]
            pos += 3 * count
            dev = []
            if head & 0x20:
                count = data[pos]
                dev = [tuple(data[pos + 1 + 3 * i:pos + 4 + 3 * i]) for i in range(count)]
                pos += 1 + 3 * count
            defs[head & 0x0F] = (mesg, endian, fields, dev)
            continue
        mesg, endian, fields, dev = defs[(head >> 5) & 3 if head & 0x80 else head & 0x0F]
        values = {}
        for num, size, base in fields:
            fmt = FORMATS.get(base)
            if fmt and size == struct.calcsize(fmt):
                value = struct.unpack(endian + fmt, data[pos:pos + size])[0]
                if value != INVALID.get(fmt.upper()):
                    values[num] = value
            pos += size
        pos += sum(size for _, size, _ in dev)
        if mesg == RECORD:
            yield values


start = None
for r in records(open(sys.argv[1], "rb").read()):
    start = r[TIMESTAMP] if start is None else start
    print(f"{r[TIMESTAMP] - start},{r.get(ENHANCED_SPEED, 0) / 1000:.3f},{r.get(HEART_RATE, 0)}")
