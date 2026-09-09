#!/usr/bin/env python3
"""P42 -- the path-coverage check.

Five times this project mistook an UNINSTRUMENTED path for a path the corpus
does not exercise.  Both read as zero firings, and only reading the code
distinguished them -- by hand, each time, and once in a report already given.

This makes it mechanical.  Every path a production declares must be either
FIRED by the witness harness or listed in the production's `unexercised` with
a reason.  Anything else is a hole in the instrument and this exits non-zero.

Run after compiler/witness.sh, which leaves the ledgers in /tmp/p42_witness.
"""
import glob, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import productions as PROD


def fired_paths(d):
    seen = set()
    for f in glob.glob(os.path.join(d, "*.ledger")):
        for line in open(f):
            m = re.match(r"^# path (\S+)\.(\S+)\s*$", line)
            if m:
                seen.add((m.group(1), m.group(2)))
    return seen


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "/tmp/p42_witness/now"
    seen = fired_paths(d)
    if not seen:
        print("pathcheck: NO LEDGERS in %s -- run witness.sh first" % d)
        return 2
    unaccounted, excused = PROD.check_path_coverage(seen)
    total = sum(len(p.paths) for p in PROD.P)
    print("pathcheck: %d/%d declared paths fired, %d excused, %d UNACCOUNTED"
          % (len(seen), total, len(excused), len(unaccounted)))
    if excused:
        print("\nexcused (declared unexercised, with a reason):")
        for name, path, why in sorted(excused):
            print("  %s.%s" % (name, path))
            print("      %s" % why)
    if unaccounted:
        print("\nUNACCOUNTED -- either the harness does not reach these, or they")
        print("are not instrumented.  BOTH READ AS ZERO; that is the whole point.")
        for name, path in sorted(unaccounted):
            print("  %s.%s" % (name, path))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
