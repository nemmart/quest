#!/usr/bin/env python3
"""Generate the M4b push_map for tranches C (26 XCALL/nested sites) and
D (5 RETURN_MESSAGE sites) — Project 19.

Same law as tranches A/B (gen_pushmap.py): a push_map entry is a pure
function of (callee, arg-number) -> area slot, keyed by the push PC:
slot = callee_wfp - 10 - 2N; marker = wfp - 10. The argmap already IS
the push_map. Multi-arg pcs (WPSH) emit the 3-field form:
push <pc> <base_slot> <wides>, base = lowest slot = highest argN,
hook writes ascending (P18 REPORT §4).

Cross-validation built in (two independent paths must agree):
  1. transcription: argmap (callee,argN,pc) -> slot
  2. window walk:   quest.dis, walk back from each call pc collecting
     push opcodes until argc wides are accounted; nearest-call push =
     arg1, a WPSH XX,AA covers the next (AA-XX+1) arg numbers.
Any disagreement, unmatched pc, argc mismatch, out-of-region slot, or
bad marker -> nonzero exit, nothing written.

Usage: gen_pushmap_cd.py [addrbook] [argmap] [callsites] [dis]
Writes quest.pushmap.C, quest.pushmap.D next to the addrbook.
"""
import re, sys, os
from collections import defaultdict

BOOK = sys.argv[1] if len(sys.argv) > 1 else 'quest.addrbook'
AM   = sys.argv[2] if len(sys.argv) > 2 else '../../Disassembled/quest.argmap'
CS   = sys.argv[3] if len(sys.argv) > 3 else '../../Disassembled/quest.callsites'
DIS  = sys.argv[4] if len(sys.argv) > 4 else '../../Disassembled/quest.dis'

C_CALLEES = {  # the 8 nested procedures reached by argc>0 XCALL
    'BARGAIN.1@70160854', 'BOAT.1@70161AA3', 'CAST.4@70163682',
    'KILL_PLAYER.4@7016E5CB', 'LIST_PLAYERS.3@7016F556',
    'OP_EDIT.8@70175897', 'OP_EDIT.6@701757A8', 'OP_EDIT.4@70175672',
}
D_CALLEES = {'RETURN_MESSAGE'}

errors = []
def err(msg): errors.append(msg)

# ---- book: name -> (alloc_base, wfp_base, max_argc, entry_pc)
book = {}
for l in open(BOOK):
    if l.startswith('#') or not l.strip(): continue
    p = l.split()
    if len(p) >= 5:
        book[p[1]] = (int(p[2], 16), int(p[3], 16), int(p[4]), int(p[0], 16))

# ---- path 1: transcription from the argmap
# group by (name, pc): a WPSH pc carries several args of one callee
bypc = defaultdict(list)          # (name, pc) -> [argN...]
for l in open(AM):
    m = re.match(r'(\S+) arg(\d+) at ([0-9A-F]+)', l)
    if not m: continue
    name = m.group(1)
    if name in C_CALLEES or name in D_CALLEES:
        bypc[(name, m.group(3).upper())].append(int(m.group(2)))

def slots_for(name, args, pc):
    alloc, wfp, maxargc, _ = book[name]
    out = {}
    for N in sorted(args):
        s = wfp - 10 - 2 * N
        if not (alloc <= s < alloc + 2 * maxargc):
            err(f'{name} arg{N} at {pc}: slot {s:08X} outside arg region')
        out[N] = s
    if len(args) > 1:
        ns = sorted(args)
        if ns != list(range(ns[0], ns[-1] + 1)):
            err(f'{name} at {pc}: WPSH arg numbers not consecutive: {ns}')
    return out

transcribed = {}                   # pc -> (name, {N: slot})
for (name, pc), args in bypc.items():
    if name not in book:
        err(f'{name}: not in the book'); continue
    if pc in transcribed:
        err(f'push pc {pc} claimed by two callees'); continue
    transcribed[pc] = (name, slots_for(name, args, pc))

# ---- call sites for C & D from quest.callsites
sites = []                         # (name, argc, call_pc)
for l in open(CS):
    m = re.match(r'call (\S+),(\d+) at ([0-9A-F]+)', l)
    if not m: continue
    name = m.group(1)
    if name in C_CALLEES or name in D_CALLEES:
        sites.append((name, int(m.group(2)), m.group(3).upper()))
c_sites = [s for s in sites if s[0] in C_CALLEES]
d_sites = [s for s in sites if s[0] in D_CALLEES]
if len(c_sites) != 26: err(f'expected 26 C sites, found {len(c_sites)}')
if len(d_sites) != 5:  err(f'expected 5 D sites, found {len(d_sites)}')

# ---- path 2: window walk over the disassembly
dis = {}                           # pc -> instruction text
order = []                         # pcs in file order
for l in open(DIS):
    m = re.match(r'([0-9a-f]{8}) (.+)', l)
    if m:
        pc = int(m.group(1), 16)
        dis[pc] = m.group(2)
        order.append(pc)
idx = {pc: i for i, pc in enumerate(order)}

PUSH1 = re.compile(r'(XPEF|LPEF|XPEFB|LPEFB)\b')
WPSH  = re.compile(r'WPSH (\d+),(\d+)')

def walk_window(name, argc, call_pc):
    """Walk back from the call collecting pushes until argc wides are
    accounted. Returns {pc: (base_argN, wides)} or None on failure."""
    i = idx.get(int(call_pc, 16))
    if i is None:
        err(f'{name}@{call_pc}: call pc not in dis'); return None
    got = 0
    found = []                     # (pc, wides) nearest-call first
    j = i - 1
    while got < argc and j >= 0 and i - j < 40:
        pc = order[j]; text = dis[pc]
        mw = WPSH.match(text)
        if mw:
            wides = int(mw.group(2)) - int(mw.group(1)) + 1
            found.append((pc, wides)); got += wides
        elif PUSH1.match(text):
            found.append((pc, 1)); got += 1
        j -= 1
    if got != argc:
        err(f'{name}@{call_pc}: window walk found {got} wides, argc {argc}')
        return None
    out = {}                       # arg1 = nearest call; WPSH covers next w args
    n = 1
    for pc, wides in found:
        out[pc] = (n, wides)       # base_argN = lowest argN in the group
        n += wides
    return out

walked = {}                        # pc -> (name, base_argN, wides)
site_pushes = defaultdict(list)    # call_pc -> [push pcs]
for name, argc, call_pc in sites:
    w = walk_window(name, argc, call_pc)
    if w is None: continue
    for pc, (base_n, wides) in w.items():
        key = f'{pc:08X}'
        site_pushes[call_pc].append(key)
        prev = walked.get(key)
        if prev and prev != (name, base_n, wides):
            err(f'push pc {key}: window walk disagreement {prev} vs {(name, base_n, wides)}')
        walked[key] = (name, base_n, wides)

# ---- cross-validate the two paths
if set(transcribed) != set(walked):
    err(f'pc sets differ: only-argmap={sorted(set(transcribed)-set(walked))} '
        f'only-walk={sorted(set(walked)-set(transcribed))}')
for pc in sorted(set(transcribed) & set(walked)):
    tname, tslots = transcribed[pc]
    wname, base_n, wides = walked[pc]
    if tname != wname:
        err(f'{pc}: callee disagreement {tname} vs {wname}'); continue
    ns = sorted(tslots)
    if len(ns) != wides or ns[0] != base_n:
        err(f'{pc}: arg grouping disagreement argmap {ns} vs walk base={base_n} w={wides}')

# ---- markers
markers = {}                       # call_pc -> (name, marker_slot)
for name, argc, call_pc in sites:
    _, wfp, _, _ = book[name]
    markers[call_pc] = (name, wfp - 10)

if errors:
    for e in errors: print('ERROR:', e, file=sys.stderr)
    sys.exit(1)

# ---- emit
def emit(path, tranche, names, header):
    with open(path, 'w') as f:
        f.write(header)
        by_site = sorted((cp for cp in site_pushes
                          if markers[cp][0] in names), key=lambda x: x)
        for call_pc in by_site:
            name, marker = markers[call_pc]
            argc = next(a for n, a, c in sites if c == call_pc)
            f.write(f'# {name},{argc} @ {call_pc}\n')
            for pc in sorted(site_pushes[call_pc], reverse=True):
                cname, slots = transcribed[pc]
                ns = sorted(slots)
                base_slot = slots[max(ns)]    # highest argN = lowest slot
                op = dis[int(pc, 16)].split()[0].rstrip(';')
                if len(ns) > 1:
                    f.write(f'push {pc} {base_slot:08X} {len(ns)}   # {op} args{ns[0]}-{ns[-1]}\n')
                else:
                    f.write(f'push {pc} {base_slot:08X}   # {op} arg{ns[0]}\n')
            f.write(f'call {call_pc} {marker:08X}   # {name} marker\n')
    print(f'wrote {path}')

outdir = os.path.dirname(os.path.abspath(BOOK))
emit(os.path.join(outdir, 'quest.pushmap.C'), 'C', C_CALLEES,
     '# M4b caller map — Project 19 tranche C: 26 XCALL sites to the 8 nested\n'
     '# procedures. Same law as tranche A (slot = wfp-10-2N; marker = wfp-10);\n'
     '# the ONLY mechanism delta is the XCALL marker hook (EagleStack.cpp).\n')
emit(os.path.join(outdir, 'quest.pushmap.D'), 'D', D_CALLEES,
     '# M4b caller map — Project 19 tranche D: 5 RETURN_MESSAGE sites (all\n'
     '# LCALL; no new code). 70169B82 is the pass-by-reference site: its\n'
     '# WPSH 0,2 pushes three POINTER values to stack temporaries — the\n'
     '# pointers are redirected, the temps stay on the stack. RETURN_MESSAGE\n'
     '# never WRTNs (ends in SYSCALL 0310, retired) — expect a write-mode\n'
     '# WSAVS/WRTN imbalance of +1 per firing leg.\n')

# dedupe note: pcs shared between sites (same callee called twice from
# adjacent windows) appear once — the loader rejects duplicates.
print(f'C: {len(c_sites)} sites, {len(set(p for cp in site_pushes if markers[cp][0] in C_CALLEES for p in site_pushes[cp]))} push pcs')
print(f'D: {len(d_sites)} sites, {len(set(p for cp in site_pushes if markers[cp][0] in D_CALLEES for p in site_pushes[cp]))} push pcs')
print('cross-validation: transcription == window walk on all pcs')
