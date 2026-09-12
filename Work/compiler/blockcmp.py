#!/usr/bin/env python3
"""blockcmp.py — Project 55 Part 2.  READ ONLY; changes nothing.

Compares the BLOCK STRUCTURE of our compiled IR against the book's
(`quest.ir2.book` — the target, per a001 Q1(a); `quest.blocks.split` is
provenance and reported alongside by blockcensus.py).

The method, and why it is this one
----------------------------------
Counting blocks does not say what a difference is MADE of, and an equal count
can sit on top of a structural mismatch in both directions (P55 q001 §2:
PICK_X_Y is 20/20 against blocks.split and -2 against the target).  So instead
of aligning block-to-block, both CFGs are NORMALISED by applying DESIGN §6's
merge rule to fixpoint:

    merge(A, B) legal when A's only successor is B and B's only predecessor
    is A.

Merge is the cheap rewrite.  Whatever survives merging on both sides is a
difference merge cannot close, which is exactly the question the census asks.
The merge counts themselves are the `merge`/`split` classification, with sign:
a merge available on OUR side that the book does not need is a `merge` entry;
one available on the BOOK's side is a `split` entry for us.

Classification of what survives is by CFG shape, not by statement text, so no
slot bijection is required (q001 §1).

Nothing here reads a statement to decide what the C should say.
"""

import argparse
import os
import re
import sys
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
EMU = os.path.join(os.path.dirname(HERE), "emulation")


# ------------------------------------------------------------------ parse --

class CFG:
    def __init__(self):
        self.succ = {}       # id -> [id]
        self.stmts = {}      # id -> [text]
        self.anchor = {}     # id -> str
        self.entry = None
        self.order = []

    def preds(self):
        p = defaultdict(list)
        for a, ss in self.succ.items():
            for s in ss:
                if s in self.succ:
                    p[s].append(a)
        return p


GOTO = re.compile(r"^\s*goto\s+\[([^\]]*)\]")
RETX = re.compile(r"\bret=([A-Za-z_0-9.]+)")
SITE = re.compile(r"\bsite=([0-9A-Fa-f]{8})\b")

# ircmp.py's succs_of, reused as the IDEA per q001 §1 (its code carries an
# ir 6 parser and a slot-bijection dependency this measurement does not have).
# P36 ruling B: a WSAVS entry block, and an undecorated call instruction the
# emitter left as an instruction, both RETURN and so fall through to the next
# block in address order.  Returning [] for these stops the DFS dead (43 of
# DIED's 73 blocks unreached).  Applied identically to both sides.
FALLS_THROUGH = ("WSAVS", "LCALL", "XCALL", "LJSR", "XJSR")

FALL = object()


def _terminator_succs(stmts, conv):
    """Successors from the block's last meaningful statement.  May return
    [FALL], meaning 'the next block in address order'."""
    if not stmts:
        return [FALL]
    last = stmts[-1]
    m = GOTO.match(last)
    if m:
        return [conv(t.strip()) for t in m.group(1).split(",") if t.strip()]
    s = last.strip()
    if s.startswith("ret"):
        return []
    if s.startswith("rt_call"):
        m = RETX.search(last)
        if m:                                  # our side spells it ret=
            return [conv(m.group(1))]
        m = SITE.search(last)                  # the book spells it site=
        return [int(m.group(1), 16) + 4] if m else []
    if s.startswith("call"):
        m = RETX.search(last)
        return [conv(m.group(1))] if m else []
    m = re.match(r"@[0-9A-Fa-f]{8}\s+(\S+)", s)
    if m and m.group(1).startswith(FALLS_THROUGH):
        return [FALL]
    return []


def _resolve_fall(g):
    """Replace FALL with the next block in address/emission order."""
    for i, a in enumerate(g.order):
        if g.succ[a] and g.succ[a][0] is FALL:
            g.succ[a] = [g.order[i + 1]] if i + 1 < len(g.order) else []


def parse_book(path, lo, hi):
    """quest.ir2.book -> CFG restricted to [lo, hi)."""
    g = CFG()
    cur = None
    raw = {}
    for line in open(path, errors="replace"):
        m = re.match(r"^block ([0-9A-F]{8})\b", line)
        if m:
            cur = int(m.group(1), 16)
            raw[cur] = []
            continue
        if cur is not None and line.startswith("  ") and line.strip():
            raw[cur].append(line.rstrip())
    for a in sorted(raw):
        if lo <= a < hi:
            g.stmts[a] = raw[a]
            g.succ[a] = _terminator_succs(raw[a], lambda t: int(t, 16))
            g.anchor[a] = ""
            g.order.append(a)
    _resolve_fall(g)
    g.succ = {a: [s for s in ss if s in g.succ] for a, ss in g.succ.items()}
    g.entry = lo
    return g


def parse_ours(path, entry):
    """A compiled .ir -> CFG for one entry's blocks."""
    g = CFG()
    cur = None
    raw = {}
    anch = {}
    for line in open(path):
        m = re.match(r"^block\s+([A-Za-z_0-9.]+\.b\d+)\s*(?:;\s*anchor=(\S+))?", line)
        if m:
            cur = m.group(1)
            raw[cur] = []
            anch[cur] = m.group(2) or ""
            continue
        if cur is not None and line.startswith("  ") and line.strip():
            raw[cur].append(line.rstrip())
    pref = entry + ".b"
    for b in raw:
        if b.startswith(pref):
            g.stmts[b] = raw[b]
            g.anchor[b] = anch[b]
            g.succ[b] = _terminator_succs(raw[b], lambda t: t)
            g.order.append(b)
    _resolve_fall(g)
    g.succ = {a: [s for s in ss if s in g.succ] for a, ss in g.succ.items()}
    g.entry = entry + ".b0"
    return g


# -------------------------------------------------------------- normalise --

def merge_fixpoint(g):
    """Apply DESIGN §6's merge to fixpoint.  Returns (merges_applied, groups)
    where groups maps a surviving block id to the list it absorbed."""
    succ = {a: list(ss) for a, ss in g.succ.items()}
    group = {a: [a] for a in succ}
    merges = 0
    changed = True
    while changed:
        changed = False
        pred = defaultdict(list)
        for a, ss in succ.items():
            for s in ss:
                pred[s].append(a)
        for a in list(succ):
            if a not in succ:
                continue
            ss = succ[a]
            if len(ss) != 1:
                continue
            b = ss[0]
            if b == a or b not in succ:
                continue
            if len(pred[b]) != 1 or pred[b][0] != a:
                continue
            if b == g.entry:
                continue
            succ[a] = succ[b]
            group[a] = group[a] + group[b]
            del succ[b]
            merges += 1
            changed = True
            break
    return merges, succ, group


def reachable(succ, entry):
    seen, stack = set(), [entry]
    while stack:
        n = stack.pop()
        if n in seen or n not in succ:
            continue
        seen.add(n)
        stack.extend(succ[n])
    return seen


def shape_signature(succ, entry):
    """Canonical DFS from the entry, successors in terminator order.  Returns
    the sequence of out-degrees, which is the coarsest shape invariant that
    does not depend on either side's naming (ircmp equivalence 1's idea)."""
    order, seen, stack = [], set(), [entry]
    while stack:
        n = stack.pop()
        if n in seen or n not in succ:
            continue
        seen.add(n)
        order.append(n)
        for s in reversed(succ[n]):
            if s not in seen:
                stack.append(s)
    return [len(succ[n]) for n in order], order


# ------------------------------------------------------------------ main ---

def analyse(name, ourg, bookg):
    om, osucc, ogroup = merge_fixpoint(ourg)
    bm, bsucc, bgroup = merge_fixpoint(bookg)
    our_r = reachable(osucc, ourg.entry)
    book_r = reachable(bsucc, bookg.entry)
    osig, _ = shape_signature(osucc, ourg.entry)
    bsig, _ = shape_signature(bsucc, bookg.entry)
    return {
        "ours": len(ourg.succ), "book": len(bookg.succ),
        "our_merges": om, "book_merges": bm,
        "our_reduced": len(our_r), "book_reduced": len(book_r),
        "our_sig": osig, "book_sig": bsig,
        "iso_by_shape": osig == bsig,
        "our_unreachable": len(osucc) - len(our_r),
        "book_unreachable": len(bsucc) - len(book_r),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--book", default=os.path.join(EMU, "quest.ir2.book"))
    ap.add_argument("--addrbook", default=os.path.join(EMU, "quest.addrbook"))
    ap.add_argument("--pair", action="append", required=True,
                    metavar="ENTRY=OURS.ir",
                    help="routine entry name and the .ir holding it")
    args = ap.parse_args()

    sys.path.insert(0, HERE)
    from blockcensus import read_addrbook, ranges
    rng = ranges(read_addrbook(args.addrbook))

    print(f"{'routine':16}{'ours':>5}{'book':>5}{'Δ':>5} | "
          f"{'oMrg':>5}{'bMrg':>5} | {'ourR':>5}{'bookR':>6}{'ΔR':>5} | shape")
    rows = {}
    for p in args.pair:
        entry, path = p.split("=", 1)
        lo, hi, _ = rng[entry]
        r = analyse(entry, parse_ours(path, entry), parse_book(args.book, lo, hi))
        rows[entry] = r
        print(f"{entry:16}{r['ours']:5}{r['book']:5}{r['book']-r['ours']:+5} | "
              f"{r['our_merges']:5}{r['book_merges']:5} | "
              f"{r['our_reduced']:5}{r['book_reduced']:6}"
              f"{r['book_reduced']-r['our_reduced']:+5} | "
              f"{'SAME' if r['iso_by_shape'] else 'differs'}")
    return rows


if __name__ == "__main__":
    main()
