#!/usr/bin/env python3
"""check_assumptions.py — make `quest.assumptions` CHECKABLE, not merely readable.

The prompt's words: *"the assumption silently stops holding the day a
translated routine writes one of those words."*  A fact file nobody runs is a
fact file that decays, so every row is checked three ways:

  1. THE DISASSEMBLY LEG.  Each address has exactly ONE direct writer, at the
     recorded pc, and exactly the recorded address-taken sites, at the recorded
     call.  Catches a re-disassembly that changes the program's shape.

     "Sole writer" is about DIRECT stores, and that is the half a naive checker
     gets wrong: `LWSTA 0,[0xADDR]` writes the WORD, `LWSTA 0,@[0xADDR]` writes
     the POINTEE, and `LPEF [0xADDR]` hands the address to a runtime routine
     that writes the word itself.  Two of the three rows depend on that third
     form, so the checker counts all three.

  2. THE TRANSLATED-SOURCE LEG.  The one the prompt actually cares about: no
     `game/routines/*.c` may assign the declaring name (`SD_PTR = ...`).  Fires
     on the day it should.

  3. THE EMITTED-IR LEG.  No `.ir` given on the command line may store to one
     of the three literal addresses.  Wired into the difftest script, so it
     runs on every corpus run rather than on demand.

`--break N` fakes a violation in leg N so the checker is seen RED before it is
believed green (the -DP46_BROKEN_ALLOC precedent).
"""

import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.dirname(HERE)
REPO = os.path.dirname(WORK)
DIS = os.path.join(REPO, "Disassembled", "quest.dis")
ASSUMPTIONS = os.path.join(WORK, "emulation", "quest.assumptions")
ROUTINES = os.path.join(WORK, "game", "routines")

# the C name each address is declared under, for leg 2
DECLARED_AS = {"0x70000210": "SD_PTR", "0x70000212": "OBJ_PTR",
               "0x700007A0": None}


def rows(path):
    out = []
    for line in open(path):
        line = line.split("#")[0].strip()
        if not line:
            continue
        f = line.split()
        if len(f) != 5 or f[0] != "ptr-bit31-clear":
            fail("malformed row in %s: %s" % (path, line))
        out.append(dict(addr=f[1], writer=f[2].lower(),
                        taken=(None if f[3] == "-" else f[3].lower()),
                        callee=(None if f[4] == "-" else f[4])))
    return out


FAILURES = []


def fail(msg):
    FAILURES.append(msg)


def leg_disassembly(rs, broken=False):
    if not os.path.exists(DIS):
        fail("leg 1: %s not found" % DIS)
        return
    text = open(DIS, errors="replace").read()
    for r in rs:
        bare = r["addr"][2:]
        # direct stores: LWSTA/XWSTA/... reg,[0xADDR]   (NOT @[0xADDR])
        direct = re.findall(r"^([0-9a-f]{8}) +\w*ST[AB]? +[0-3],\[0x%s\];"
                            % bare, text, re.M | re.I)
        # address-taken: LPEF/XPEF [0xADDR]
        taken = re.findall(r"^([0-9a-f]{8}) +\w*PEFB? +\[0x%s\];" % bare,
                           text, re.M | re.I)
        if broken:
            direct = direct + ["deadbeef"]
        if len(direct) != 1:
            fail("leg 1: %s has %d direct writers (%s), expected exactly 1"
                 % (r["addr"], len(direct), ", ".join(direct) or "none"))
        elif direct[0].lower() != r["writer"]:
            fail("leg 1: %s's writer is %s, the file records %s"
                 % (r["addr"], direct[0], r["writer"]))
        want = [r["taken"]] if r["taken"] else []
        if sorted(t.lower() for t in taken) != sorted(want):
            fail("leg 1: %s address-taken at [%s], the file records [%s] — a "
                 "new by-reference site is a new writer"
                 % (r["addr"], ", ".join(taken) or "none",
                    ", ".join(want) or "none"))


def leg_translated(rs, broken=False):
    names = [DECLARED_AS[r["addr"]] for r in rs if DECLARED_AS.get(r["addr"])]
    if not os.path.isdir(ROUTINES):
        fail("leg 2: %s not found" % ROUTINES)
        return
    for fn in sorted(os.listdir(ROUTINES)):
        if not fn.endswith(".c"):
            continue
        src = open(os.path.join(ROUTINES, fn)).read()
        src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
        for name in names:
            # `SD_PTR = ...` but not `SD_PTR->field = ...` and not `==`
            if re.search(r"(?<![\w.>])%s\s*=(?!=)" % re.escape(name), src):
                fail("leg 2: %s assigns %s — the bit-31 assumption rested on "
                     "that word having ONE writer, in startup" % (fn, name))
    if broken:
        fail("leg 2: (--break) a translated routine assigns SD_PTR")


def leg_emitted(rs, irs, broken=False):
    lits = set()
    for r in rs:
        v = int(r["addr"], 16)
        lits.add(r["addr"].lower())
        lits.add("0x%08x" % v)
        lits.add(str(v))
    for path in irs:
        if not os.path.exists(path):
            continue
        for n, line in enumerate(open(path), 1):
            line = line.split(";")[0]
            m = re.match(r"\s*M(?:8|16|32)\[([^\]]+)\]\s*=", line)
            if not m:
                continue
            idx = m.group(1).strip().lower()
            if idx in lits:
                fail("leg 3: %s:%d stores to %s" % (path, n, idx))
    if broken:
        fail("leg 3: (--break) emitted IR stores to 0x70000210")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ir", nargs="*", help="emitted .ir files for leg 3")
    ap.add_argument("--assumptions", default=ASSUMPTIONS)
    ap.add_argument("--break", dest="brk", type=int, choices=[1, 2, 3],
                    help="teeth: fake a violation in this leg; the run MUST go red")
    a = ap.parse_args()

    rs = rows(a.assumptions)
    leg_disassembly(rs, broken=(a.brk == 1))
    leg_translated(rs, broken=(a.brk == 2))
    leg_emitted(rs, a.ir, broken=(a.brk == 3))

    if FAILURES:
        for f in FAILURES:
            sys.stderr.write("ASSUMPTION VIOLATED: %s\n" % f)
        return 2
    print("quest.assumptions: %d row(s), 3 legs, GREEN%s"
          % (len(rs), " (%d .ir scanned)" % len(a.ir) if a.ir else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
