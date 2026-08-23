#!/usr/bin/env python3
"""Generate the M4b push_map directly from the argmap + book.

Key insight (nemmart): a push_map entry is a pure function of
(callee, arg-number) -> area slot, keyed by the push PC. A given push PC
pushes a specific arg of a specific callee; that callee's frame is at a
fixed area address; so the destination slot is fixed REGARDLESS of which
LCALL the push belongs to. The argmap ("CALLEE argN at PC") already IS
the push_map, modulo looking up the callee's wfp and computing
slot = wfp - 10 - 2*N. No call-site association or window-walking needed.

Tranche split (for staged widening): A = single-word pushes to flat
callees; B = pushes at a WPSH pc (multi-word); C = XCALL callees (nested);
D = RETURN_MESSAGE. This script emits A and B (Project 18).
"""
import re, sys
BOOK=sys.argv[1] if len(sys.argv)>1 else 'quest.addrbook'
AM=sys.argv[2] if len(sys.argv)>2 else '../../Disassembled/quest.argmap'
DIS=sys.argv[3] if len(sys.argv)>3 else None  # optional, to split WPSH/XCALL

book={}   # name -> wfp
for l in open(BOOK):
    if l.startswith('#'): continue
    p=l.split()
    if len(p)>=6: book[p[1].split('@')[0]]=int(p[3],16)

# group argmap lines by (routine, pc): a WPSH pc carries several args
from collections import defaultdict
bypc=defaultdict(list)   # pc -> [(name,N)]
for l in open(AM):
    m=re.match(r'(\S+) arg(\d+) at ([0-9A-F]+)',l)
    if m: bypc[m.group(3).upper()].append((m.group(1),int(m.group(2))))

wpsh_pcs=xcall_callees=set()
if DIS:
    wpsh_pcs=set(m.group(1).upper() for l in open(DIS) for m in [re.match(r'([0-9a-f]{8}) WPSH',l)] if m)

for pc,args in sorted(bypc.items()):
    name=args[0][0]
    if name not in book: continue
    wfp=book[name]
    slots=[wfp-10-2*N for _,N in args]
    base=min(slots); w=len(args)
    # (routine/marker/tranche classification omitted here; see full generator)
