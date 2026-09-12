#!/usr/bin/env python3
"""difftest.py — the differential tester's driver (Project 48, Stage A).

    seed -> cgen.py -> prog.c -+-> gcc -fwrapv -O0/-O2 + harness -> native.txt
                               |
                               +-> lower_c.py -> prog.ir + prog.vmap
                                        -> lowerc_rig (emulator) -> ir.txt
                                                          compare -> verdict

Obligation (a) of DESIGN §2 lives here.  Two rules this file exists to keep:

  * gcc is the oracle and the ONLY oracle.  Nothing in this directory ever
    reads quest.ir2.book, and no test program is a game routine.
  * A disagreement is a STOP-AND-INVESTIGATE, not a counter.  Every one is
    shrunk to a minimal reproducer and written out with its seed.

`--census` is the headline (a001): corpus size does not substitute for
construct coverage, and every zero in the census table is a real hole.
"""

import argparse
import collections
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
COMPILER = os.path.dirname(HERE)                    # Work/compiler
WORK = os.path.dirname(COMPILER)                    # Work
GAME = os.path.join(WORK, "game")
LOWER_C = os.path.join(COMPILER, "lower_c.py")

GCC = ["gcc", "-std=c99", "-fwrapv", "-fno-strict-aliasing",
       "-Wno-overflow", "-Wall", "-Wextra", "-Wno-unused-but-set-variable",
       "-I" + GAME]

NATIVE_TIMEOUT = 20
RIG_TIMEOUT = 60


class Result:
    def __init__(self, verdict, detail="", first_diff=None):
        self.verdict = verdict          # AGREE / DISAGREE / REFUSED / UB / ERROR / SKIP
        self.detail = detail
        self.first_diff = first_diff

    def __repr__(self):
        return "%s %s" % (self.verdict, self.detail)


def run(cmd, cwd=None, timeout=30):
    try:
        p = subprocess.run(cmd, cwd=cwd, timeout=timeout,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return p.returncode, p.stdout.decode("utf-8", "replace"), \
            p.stderr.decode("utf-8", "replace")
    except subprocess.TimeoutExpired:
        return -9, "", "TIMEOUT after %ds" % timeout
    except FileNotFoundError as e:
        return -2, "", str(e)


def canonical(text):
    """Both sides print `name=%08X` lines (arrays `name[i]=%08X`), or a single
    `TRAP DERR17 file:line`.  Sorting removes any dependence on the order the
    two sides happened to walk their variables in."""
    lines = [l.strip() for l in text.splitlines() if l.strip()]
    trap = [l for l in lines if l.startswith("TRAP ")]
    if trap:
        return ["TRAP " + trap[0].split(None, 1)[1]]
    return sorted(l for l in lines if "=" in l)


def native_run(d, opt):
    exe = os.path.join(d, "native_O%s" % opt)
    rc, out, err = run(GCC + ["-O" + opt, "-o", exe,
                              os.path.join(d, "prog_main.c")], cwd=d, timeout=60)
    if rc != 0:
        return None, "gcc -O%s failed: %s" % (opt, err.strip()[:400])
    rc, out, err = run([exe], cwd=d, timeout=NATIVE_TIMEOUT)
    # exit 3 is quest_sub_check's DERR 17 trap; anything else non-zero is a fault
    if rc not in (0, 3):
        return None, "native -O%s exit %d %s" % (opt, rc, err.strip()[:200])
    return canonical(out), None


def ir_run(d, rig, addrbook, entry):
    rc, out, err = run([sys.executable, LOWER_C, os.path.join(d, "prog.c"),
                        "--routine", "t", "--entry", entry,
                        "-o", os.path.join(d, "prog.ir")], cwd=d, timeout=120)
    if rc == 2:
        return None, "REFUSE " + (err.strip().splitlines() or [""])[0][:300]
    if rc != 0:
        return None, "lower_c failed rc=%d %s" % (rc, err.strip()[:400])
    rc, out, err = run([rig, "--program", os.path.join(d, "prog.ir"),
                        "--vmap", os.path.join(d, "prog.vmap"),
                        "--addrbook", addrbook], cwd=d, timeout=RIG_TIMEOUT)
    if rc != 0:
        return None, "rig rc=%d %s" % (rc, (err.strip() or out.strip())[:400])
    return canonical(out), None


def first_difference(a, b):
    for i in range(max(len(a), len(b))):
        x = a[i] if i < len(a) else "<missing>"
        y = b[i] if i < len(b) else "<missing>"
        if x != y:
            return "native %s | ir %s" % (x, y)
    return None


def one_program(seed, klass, workdir, rig, addrbook, entry, want_ir):
    import cgen
    d = os.path.join(workdir, "s%06d_%s" % (seed, klass))
    if os.path.isdir(d):
        shutil.rmtree(d)
    meta = cgen.generate(seed, klass, d)

    n0, e0 = native_run(d, "0")
    if n0 is None:
        return Result("ERROR", e0), meta
    n2, e2 = native_run(d, "2")
    if n2 is None:
        return Result("ERROR", e2), meta
    if n0 != n2:
        # gcc disagreeing with itself across -O levels means the program is
        # UB and gcc is not an oracle for it.  Discard LOUDLY: this counter
        # being non-zero is a bug in cgen.py, not an interesting finding.
        return Result("UB", "gcc -O0 != -O2: " + (first_difference(n0, n2) or "")), meta

    if not want_ir:
        return Result("SKIP", "native-only (no compiler yet)"), meta

    ir, eir = ir_run(d, rig, addrbook, entry)
    if ir is None:
        if eir.startswith("REFUSE"):
            return Result("REFUSED", eir), meta
        return Result("ERROR", eir), meta
    if ir == n0:
        return Result("AGREE", ""), meta
    return Result("DISAGREE", "", first_difference(n0, ir)), meta


# ------------------------------------------------------------- shrinking --

def shrink(seed, klass, d, rig, addrbook, entry):
    """Delete whole simple statements from prog.c while the disagreement
    survives.  Only lines that are complete simple statements are candidates
    (no braces, no labels, no goto, no declaration), so the file stays
    syntactically valid without any brace accounting."""
    path = os.path.join(d, "prog.c")
    original = open(path).read().splitlines()

    def disagrees(lines):
        open(path, "w").write("\n".join(lines) + "\n")
        n0, _ = native_run(d, "0")
        if n0 is None:
            return False
        ir, _ = ir_run(d, rig, addrbook, entry)
        return ir is not None and ir != n0

    def deletable(l):
        s = l.strip()
        return (s.endswith(";") and "{" not in s and "}" not in s
                and not s.startswith("goto") and ":" not in s
                and not s.startswith(("int16_t", "uint16_t", "int32_t", "uint32_t")))

    lines = list(original)
    changed = True
    while changed:
        changed = False
        for i in range(len(lines) - 1, -1, -1):
            if not deletable(lines[i]):
                continue
            trial = lines[:i] + lines[i + 1:]
            if disagrees(trial):
                lines = trial
                changed = True
    open(path, "w").write("\n".join(lines) + "\n")
    return len(original), len(lines)


# ----------------------------------------------------------------- main --

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=50)
    ap.add_argument("--seed0", type=int, default=1)
    ap.add_argument("--classes", default="ABC")
    ap.add_argument("--work", default="/tmp/p48-difftest")
    ap.add_argument("--rig", default="/tmp/p48/lowerc_rig")
    ap.add_argument("--addrbook", default=os.path.join(WORK, "emulation",
                                                       "quest.addrbook"))
    ap.add_argument("--entry", default="THIEF",
                    help="addrbook entry the generated routine is emitted under")
    ap.add_argument("--native-only", action="store_true",
                    help="Stage A: run the gcc side only, the compiler not existing yet")
    ap.add_argument("--cases", action="store_true",
                    help="run the hand-written edge-case suite instead of generating")
    ap.add_argument("--census", action="store_true", default=True)
    ap.add_argument("--keep", action="store_true", help="keep every work dir")
    a = ap.parse_args()

    sys.path.insert(0, HERE)
    os.makedirs(a.work, exist_ok=True)
    want_ir = not a.native_only

    counts = collections.Counter()
    census = collections.Counter()
    programs_with = collections.Counter()
    failures = []

    if a.cases:
        return run_cases(a, want_ir)

    jobs = [(a.seed0 + i, k)
            for i in range(a.seeds)
            for k in a.classes]

    for seed, klass in jobs:
        res, meta = one_program(seed, klass, a.work, a.rig, a.addrbook,
                                a.entry, want_ir)
        counts[res.verdict] += 1
        for feat, n in meta["census"].items():
            census[feat] += n
            programs_with[feat] += 1
        d = os.path.join(a.work, "s%06d_%s" % (seed, klass))
        if res.verdict in ("DISAGREE", "ERROR", "UB"):
            failures.append((seed, klass, res))
            print("!! seed %d class %s: %s %s"
                  % (seed, klass, res.verdict, res.first_diff or res.detail))
            if res.verdict == "DISAGREE":
                before, after = shrink(seed, klass, d, a.rig, a.addrbook, a.entry)
                print("   shrunk %d -> %d lines; reproducer in %s"
                      % (before, after, d))
        elif not a.keep:
            shutil.rmtree(d, ignore_errors=True)

    print("\n=== verdicts over %d programs ===" % len(jobs))
    for k in ("AGREE", "DISAGREE", "REFUSED", "UB", "ERROR", "SKIP"):
        if counts[k]:
            print("  %-9s %d" % (k, counts[k]))
    if counts["UB"]:
        print("  NOTE: a non-zero UB count is a cgen.py bug, not a finding.")

    if a.census:
        print("\n=== construct census (a001: every zero here is a real hole) ===")
        print("  %-26s %8s %8s" % ("feature", "uses", "programs"))
        for feat in sorted(census):
            print("  %-26s %8d %8d" % (feat, census[feat], programs_with[feat]))
        print("  %d distinct features over %d programs" % (len(census), len(jobs)))

    return 1 if (counts["DISAGREE"] or counts["ERROR"] or counts["UB"]) else 0


def run_cases(a, want_ir):
    """The hand suite: each case is a `cases/NAME.c` in the subset plus a
    `cases/NAME.obs` listing its observables (name, kind, nelem), from which
    the native harness is generated exactly as cgen does it.  These are the
    boundaries the generator reaches only by luck (a001: n = 0, n = 31 and
    x = 0x80000000 for the signed shift; the promotion traps; short-circuit)."""
    import cgen
    casedir = os.path.join(HERE, "cases")
    bad = 0
    names = sorted(f[:-2] for f in os.listdir(casedir) if f.endswith(".c"))
    for name in names:
        d = os.path.join(a.work, "case_" + name)
        os.makedirs(d, exist_ok=True)
        shutil.copy(os.path.join(casedir, name + ".c"), os.path.join(d, "prog.c"))
        decls = []
        with open(os.path.join(casedir, name + ".obs")) as f:
            for line in f:
                line = line.split("#")[0].strip()
                if not line:
                    continue
                parts = line.split()
                decls.append((parts[0], parts[1], int(parts[2]) if len(parts) > 2 else 0))
        g = cgen.Gen(0, "B")
        g.decls = decls
        open(os.path.join(d, "prog_main.c"), "w").write(g.prog_main_c())
        n0, e0 = native_run(d, "0")
        if n0 is None:
            print("  %-22s ERROR %s" % (name, e0)); bad += 1; continue
        n2, _ = native_run(d, "2")
        if n2 != n0:
            print("  %-22s UB (gcc -O0 != -O2)" % name); bad += 1; continue
        if not want_ir:
            print("  %-22s native ok (%d lines)" % (name, len(n0)))
            continue
        ir, eir = ir_run(d, a.rig, a.addrbook, a.entry)
        if ir is None:
            print("  %-22s %s" % (name, eir)); bad += 1; continue
        if ir == n0:
            print("  %-22s AGREE" % name)
        else:
            print("  %-22s DISAGREE  %s" % (name, first_difference(n0, ir)))
            bad += 1
    print("\n%d cases, %d bad" % (len(names), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
