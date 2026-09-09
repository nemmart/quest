#!/usr/bin/env python3
"""crossings.py — the R39 detector, run once over the whole program.

R39 (docs/Project37/InadmissibleEvidence.md): an observation about a
hand-assembly routine is inadmissible as evidence about the PL/I compiler.
LOCK_FILE and UNLOCK_FILE were exposed by CONTROL FLOW CROSSING AN ADDRBOOK
BOUNDARY -- they branch into each other's ranges, which no two PL/I procedures
can do.  This runs that detector over every live entry, so that a third
hand-assembly unit is found deliberately rather than after it has contaminated
something.

A `.N@ADDR` entry is a NESTED piece of its parent procedure (the addrbook
splits one PL/I procedure into several entries), so flow between members of the
same FAMILY is normal and is not a crossing.  Only CROSS-FAMILY flow counts.

    python3 compiler/crossings.py                  (from the tree root)

Moved here from docs/Project37/ in P38: METHOD §16 makes this a standing
check run at the start of every routine-adding session, so it belongs with
the other standing tools.  P37's REPORT.md and InadmissibleEvidence.md still
quote the old path; they are the historical record of P37 and were left as
written.
"""
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
sys.path.insert(0, os.path.join(ROOT, "compiler"))

import ircmp as C  # noqa: E402
import readable as R  # noqa: E402

BOOK = os.path.join(ROOT, "emulation", "quest.ir2.book")
AB = os.path.join(ROOT, "emulation", "quest.addrbook")

GOTO_RE = re.compile(r"goto \[([0-9A-Fa-f, ]+)\]")


def family(name):
    """`ATTACK.3@7015E84A` -> `ATTACK`; a nested piece is the same procedure."""
    return name.split(".")[0]


def main():
    _hdr, blocks, _order = C.load_ir(BOOK)
    ents = R.load_addrbook(AB)

    # [entry, next-entry) spans, stacked entries included so the map is complete
    pcs = sorted(ents)
    span = {}
    for k, pc in enumerate(pcs):
        hi = pcs[k + 1] if k + 1 < len(pcs) else 0x70180000
        span[pc] = (pc, hi)

    owner = {}          # block pc -> addrbook entry pc
    for bpc in blocks:
        best = None
        for pc, (lo, hi) in span.items():
            if lo <= bpc < hi and (best is None or lo > span[best][0]):
                best = pc
        if best is not None:
            owner[bpc] = best

    crossings = collections.defaultdict(list)
    for bpc, stmts in blocks.items():
        src = owner.get(bpc)
        if src is None:
            continue
        for st in stmts:
            for m in GOTO_RE.finditer(st.raw):
                for t in m.group(1).split(","):
                    t = t.strip()
                    if not re.fullmatch(r"[0-9A-Fa-f]{8}", t):
                        continue
                    dst = owner.get(int(t, 16))
                    if dst is None or dst == src:
                        continue
                    fs, fd = family(ents[src]["name"]), family(ents[dst]["name"])
                    if fs == fd:
                        continue          # nested pieces of one procedure
                    crossings[(fs, fd)].append((bpc, int(t, 16)))

    print("R39 detector — cross-family control flow over %d addrbook entries\n" % len(ents))
    if not crossings:
        print("  none found")
        return
    pairs = sorted(crossings, key=lambda k: -len(crossings[k]))
    for (fs, fd) in pairs:
        ex = crossings[(fs, fd)][:3]
        print("  %-22s -> %-22s  %2d edge(s)   e.g. %s" % (
            fs, fd, len(crossings[(fs, fd)]),
            ", ".join("%08X->%08X" % e for e in ex)))

    print("\nMUTUAL pairs (each branches into the other — the LOCK_FILE signature):")
    seen = set()
    mutual = False
    for (fs, fd) in pairs:
        if (fd, fs) in crossings and (fd, fs) not in seen:
            seen.add((fs, fd))
            mutual = True
            print("  %s  <-->  %s" % (fs, fd))
    if not mutual:
        print("  none besides any listed above")


if __name__ == "__main__":
    main()
