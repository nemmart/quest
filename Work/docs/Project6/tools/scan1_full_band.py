#!/usr/bin/env python3
"""Scan 1 v2 — full wsb-band no-L1-reader scan.

Scans ALL L1/L0 code (game + runtime, per the Layering.md census) for any
reference to the runtime's wsb-relative static band, in both encodings:
  - displacement: +0x7FC0..+0x7FDF off any base register (15-bit-signed
    negatives -0x40..-0x21; covers condition cells, SFALT/SFCON/DERR state,
    the area pointer cell, and neighbors)
  - absolute: [0x70001000..0x7000108F] (main-task wsb = 0x7000108C)
Every hit requires manual base-register-provenance adjudication (see
SESSION_REPORT_AUG13.md section 6 for the four adjudicated false positives).
Usage: scan1_full_band.py ../../Layering.md <dis files...>
"""
import re, sys, bisect

def main():
    rows = []
    for line in open(sys.argv[1]):
        m = re.match(r"\|\s*([^|]+?)\s*\|\s*([0-9A-Fa-f]{8})\s*\|\s*(L[0-3]\??)\s*\|", line)
        if m:
            rows.append((m.group(1), int(m.group(2), 16), m.group(3)))
    rows.sort(key=lambda r: r[1])
    starts = [r[1] for r in rows]
    insn = re.compile(r"^([0-9a-f]{8})\s+(\S.*)$")
    disp = re.compile(r"\+0X7F[CD][0-9A-F]\]")
    absw = re.compile(r"\[0X700010[0-8][0-9A-F]\]")
    for f in sys.argv[2:]:
        for line in open(f):
            m = insn.match(line)
            if not m:
                continue
            a = int(m.group(1), 16)
            t = m.group(2).upper()
            if disp.search(t) or absw.search(t):
                i = bisect.bisect_right(starts, a) - 1
                sym, layer = ('GAME', 'L1') if i < 0 else (rows[i][0], rows[i][2])
                if layer.startswith(('L1', 'L0')):
                    print(f"{a:08X} {layer:4} {sym:20} {m.group(2).strip()}")

if __name__ == "__main__":
    main()
