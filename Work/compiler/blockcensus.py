#!/usr/bin/env python3
"""blockcensus.py — Project 55 measurement tool.  READ ONLY; changes nothing.

Counts the book's blocks per addrbook entry from `emulation/quest.blocks.split`,
counts ours per entry from a compiled `.ir`, and prints the pair.

Block structure is a graph question, not a slot question: nothing here needs
the §5.2 slot bijection, because a block is identified by its head address on
the book side and by its symbolic name on ours, and neither is a local.
"""

import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
EMU = os.path.join(os.path.dirname(HERE), "emulation")


def read_addrbook(path):
    """Return [(addr, name, live)] sorted by address.  Commented rows count as
    address delimiters (they are still code) but are marked not-live."""
    rows = []
    for line in open(path):
        s = line.strip()
        if not s:
            continue
        live = True
        if s.startswith("#"):
            s = s[1:].strip()
            live = False
        m = re.match(r"^([0-9A-F]{8})\s+(\S+)\s", s)
        if not m:
            continue
        rows.append((int(m.group(1), 16), m.group(2), live))
    rows.sort()
    return rows


def read_blocks_split(path):
    """Return {head_addr: [instruction text lines]} for every block."""
    blocks = {}
    cur = None
    for line in open(path, errors="replace"):
        s = line.rstrip("\r\n")
        m = re.match(r"^([0-9A-F]{8}):$", s.strip())
        if m:
            cur = int(m.group(1), 16)
            blocks[cur] = []
            continue
        if cur is not None and s.strip():
            blocks[cur].append(s.strip().rstrip("\r"))
    return blocks


def ranges(rows):
    """entry name -> (lo, hi) half-open, from the next entry address."""
    out = {}
    for i, (addr, name, live) in enumerate(rows):
        hi = rows[i + 1][0] if i + 1 < len(rows) else addr + 0x10000
        out[name] = (addr, hi, live)
    return out


def our_counts(irpath):
    counts = {}
    for line in open(irpath):
        m = re.match(r"^block\s+([A-Za-z_0-9.]+)\.b(\d+)\b", line)
        if m:
            counts[m.group(1)] = counts.get(m.group(1), 0) + 1
    return counts


def family_of(name):
    """FIRE.1@7016A3BD -> FIRE ; FIRE.1 -> FIRE ; FIRE -> FIRE"""
    base = name.split("@")[0]
    parts = base.split(".")
    if len(parts) > 1 and parts[-1].isdigit():
        return ".".join(parts[:-1])
    return base


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--blocks", default=os.path.join(EMU, "quest.blocks.split"))
    ap.add_argument("--addrbook", default=os.path.join(EMU, "quest.addrbook"))
    ap.add_argument("--ir", action="append", default=[],
                    help="compiled .ir file(s) to count our side from")
    ap.add_argument("--routine", action="append", default=[],
                    help="restrict the report to these entry names")
    ap.add_argument("--family", action="store_true",
                    help="fold nested .N entries into the parent family")
    args = ap.parse_args()

    rows = read_addrbook(args.addrbook)
    rng = ranges(rows)
    blocks = read_blocks_split(args.blocks)
    heads = sorted(blocks)

    book = {}
    for name, (lo, hi, live) in rng.items():
        key = family_of(name) if args.family else name
        n = sum(1 for h in heads if lo <= h < hi)
        book[key] = book.get(key, 0) + n

    ours = {}
    for p in args.ir:
        for k, v in our_counts(p).items():
            key = family_of(k) if args.family else k
            ours[key] = max(ours.get(key, 0), v)

    names = args.routine or sorted(set(list(ours) + list(book)))
    print(f"{'routine':22} {'ours':>6} {'book':>6} {'delta':>7}  {'ratio':>6}")
    for n in names:
        o = ours.get(n)
        b = book.get(n)
        d = (b - o) if (o is not None and b is not None) else None
        r = (b / o) if (o and b) else None
        print(f"{n:22} {('-' if o is None else o):>6} "
              f"{('-' if b is None else b):>6} "
              f"{('-' if d is None else f'{d:+d}'):>7}  "
              f"{('-' if r is None else f'{r:.2f}'):>6}")


if __name__ == "__main__":
    sys.exit(main())
