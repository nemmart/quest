#!/usr/bin/env python3
"""Generate the P20 borrow lines of the push_map from the argmap + book.

Same discipline as the arg tranches: resolution happens ONCE here — the
map stores ABSOLUTE addresses, never an index or an offset formula, and
the loader/hooks only look up. `_PAIRS slotN at PC` resolves to
BASE + 4*N (0-indexed, flat — deliberately a DIFFERENT equation than
argN at wfp-10-2N), validated against the FINAL (post-shift) book:

  - the book's `borrow_slots` count must equal the argmap's `_PAIRS count`
  - every slotN must have exactly TWO pcs (the WPSH open, the WPOP close;
    order verified against quest.dis opcodes)
  - every resolved slot must lie in [BASE, BASE + 4*count)
  - the first book entry must sit exactly at BASE + 4*count
  - the result must equal docs/Project20/borrowmap.crosscheck.txt
    (coordinator scan — the two generators cross-validate, P18 precedent)

Any failure -> nonzero exit, nothing written.

Usage: gen_pushmap_borrows.py [addrbook] [argmap] [dis] [crosscheck] [out]
"""
import re, sys

BASE = 0x74000000
BOOK = sys.argv[1] if len(sys.argv) > 1 else 'quest.addrbook'
AM   = sys.argv[2] if len(sys.argv) > 2 else '../../Disassembled/quest.argmap'
DIS  = sys.argv[3] if len(sys.argv) > 3 else '../../Disassembled/quest.dis'
CC   = sys.argv[4] if len(sys.argv) > 4 else '../docs/Project20/borrowmap.crosscheck.txt'
OUT  = sys.argv[5] if len(sys.argv) > 5 else 'quest.pushmap.borrows'

errors = []
def err(m): errors.append(m)

# ---- book: borrow_slots + first entry base
book_slots = None
first_alloc = None
for l in open(BOOK):
    p = l.split()
    if l.startswith('#') or not p:
        continue
    if p[0] == 'borrow_slots':
        book_slots = int(p[1])
    elif len(p) >= 4 and first_alloc is None:
        first_alloc = int(p[2], 16)
if book_slots is None:
    err('book has no borrow_slots line')

# ---- argmap: _PAIRS count + slot pcs
count = None
slots = {}                  # n -> [pc, pc] in file order
for l in open(AM):
    p = l.split()
    if len(p) == 3 and p[:2] == ['_PAIRS', 'count']:
        count = int(p[2])
    m = re.match(r'_PAIRS slot(\d+) at ([0-9A-F]{8})$', l.strip())
    if m:
        slots.setdefault(int(m.group(1)), []).append(m.group(2))
if count is None:
    err('argmap has no _PAIRS count line')
if count != book_slots:
    err('argmap _PAIRS count %s != book borrow_slots %s' % (count, book_slots))
if sorted(slots) != list(range(count or 0)):
    err('slot numbers not exactly 0..%s' % ((count or 0) - 1))

# ---- dis: opcode at each pc (WPSH first, WPOP second)
op = {}
for l in open(DIS, errors='replace'):
    m = re.match(r'([0-9a-f]{8}) (WPSH|WPOP) ', l)
    if m:
        op[m.group(1).upper()] = m.group(2)
for n, pcs in sorted(slots.items()):
    if len(pcs) != 2:
        err('slot%d has %d pcs, want 2' % (n, len(pcs)))
        continue
    if op.get(pcs[0]) != 'WPSH' or op.get(pcs[1]) != 'WPOP':
        err('slot%d opcode order: %s=%s %s=%s' %
            (n, pcs[0], op.get(pcs[0]), pcs[1], op.get(pcs[1])))

# ---- resolve: slotN -> ABSOLUTE address, frozen here
lines = []
for n, pcs in sorted(slots.items()):
    a = BASE + 4 * n
    if not (BASE <= a < BASE + 4 * (count or 0)):
        err('slot%d resolves outside the reserved block' % n)
    for pc, kind in zip(pcs, ('WPSH', 'WPOP')):
        lines.append('borrow %s %08X   # slot%d %s' % (pc, a, n, kind))
if first_alloc is not None and count is not None and first_alloc != BASE + 4 * count:
    err('first book entry %08X != BASE + 4*count %08X' % (first_alloc, BASE + 4 * count))

# ---- cross-validate against the coordinator scan (pc, slot) pairs
ours = set()
for l in lines:
    p = l.split()
    ours.add((p[1], p[2]))
theirs = set()
for l in open(CC):
    m = re.match(r'borrow ([0-9A-F]{8}) ([0-9A-F]{8})', l)
    if m:
        theirs.add((m.group(1), m.group(2)))
if ours != theirs:
    err('crosscheck mismatch: only-ours=%s only-theirs=%s' %
        (sorted(ours - theirs)[:4], sorted(theirs - ours)[:4]))

if errors:
    for e in errors:
        print('ERROR:', e, file=sys.stderr)
    sys.exit(1)

with open(OUT, 'w') as f:
    f.write('# M4b P20 borrow map — %d WPSH/WPOP frame-borrow brackets.\n' % count)
    f.write('# borrow <pc> <slot>: opcode decides — WPSH stores AC[r] to the slot\n')
    f.write('# (not the stack), WPOP loads it back (does not pop). Slots are the\n')
    f.write('# reserved block [%08X, %08X); frames start above it.\n' %
            (BASE, BASE + 4 * count))
    f.write('\n'.join(lines) + '\n')
print('%s: %d borrow lines (%d pairs), block [%08X, %08X), crosscheck OK' %
      (OUT, len(lines), count, BASE, BASE + 4 * count))
