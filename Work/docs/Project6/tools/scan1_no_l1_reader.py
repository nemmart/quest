#!/usr/bin/env python3
"""Scan 1 — no-L1-reader scan for L2-private state candidates.

Partitions disassembly lines by layer (census parsed from Layering.md),
then finds every instruction in the L1 partition (game code + L1 runtime)
that could touch a candidate cell, in either encoding:
  - absolute:      [0x7000104C] etc.  (main-task wsb = 0x7000108C)
  - displacement:  +0x7FC0] family    (15-bit-signed negative disp off any base)
Emits every hit with address, symbol, layer, and matched pattern.
Adjudication of base-register contents is manual, downstream.
"""
import re, sys, bisect

WSB = 0x7000108C
# candidate cells: name -> wsb-relative word offset (negative)
CANDIDATES = {
    "chain_head":    -0x40,
    "oset_type":     -0x3E,
    "oset_key2":     -0x3C,
    "oset_code":     -0x3A,
    "walker_res2":   -0x38,
    "walker_res1":   -0x36,
    "osignal_arg4":  -0x2A,
}

def disp_pattern(off):
    # 15-bit signed encoding as it prints: 0x8000 + off (off negative)
    return 0x8000 + off

def parse_census(layering_path):
    rows = []
    for line in open(layering_path):
        m = re.match(r"\|\s*([^|]+?)\s*\|\s*([0-9A-Fa-f]{8})\s*\|\s*(L[0-3]\??)\s*\|", line)
        if m:
            rows.append((m.group(1), int(m.group(2), 16), m.group(3)))
    rows.sort(key=lambda r: r[1])
    return rows

def layer_of(addr, rows, starts):
    i = bisect.bisect_right(starts, addr) - 1
    if i < 0:
        return ("GAME", "L1")  # before first runtime symbol => game code
    name, start, layer = rows[i]
    return (name, layer)

def scan(dis_path, rows, starts, out):
    abs_pats = {n: f"[0X{(WSB + off):08X}]" for n, off in CANDIDATES.items()}
    disp_pats = {n: f"+0X{disp_pattern(off):04X}]" for n, off in CANDIDATES.items()}
    insn_re = re.compile(r"^([0-9a-f]{8})\s+(\S.*)$")
    hits = []
    for line in open(dis_path):
        m = insn_re.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        text = m.group(2)
        up = text.upper()
        for n, p in abs_pats.items():
            if p in up:
                hits.append((addr, text.strip(), n, "ABS"))
        for n, p in disp_pats.items():
            if p in up:
                hits.append((addr, text.strip(), n, "DISP"))
    for addr, text, cell, kind in hits:
        sym, layer = layer_of(addr, rows, starts)
        print(f"{addr:08X} {layer:4} {sym:24} {cell:14} {kind:4} {text}", file=out)
    return hits

def main():
    layering = sys.argv[1]
    dis_files = sys.argv[2:]
    rows = parse_census(layering)
    # census covers the runtime; game code below first runtime addr is L1 GAME
    starts = [r[1] for r in rows]
    print(f"# census symbols: {len(rows)}; wsb=0x{WSB:08X}")
    print("# addr     layer sym                      cell           enc  insn")
    for f in dis_files:
        print(f"# --- {f}")
        scan(f, rows, starts, sys.stdout)

if __name__ == "__main__":
    main()
