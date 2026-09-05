#!/usr/bin/env python3
# derr_clusters.py — Project 27 Part 1: the DERR bounds-check cluster census
# and the `assumed-foldable.txt` artifact (docs/Project27/PROMPT.md).
#
# ANALYSIS ONLY.  Reads text artifacts (quest.dis, quest.tags,
# quest.blocks.split, quest.synclist.split, quest.symbols); writes nothing
# the emulator reads.  No memory image is needed (Sep 5 ruling: Python for
# text, Java for the image).
#
# A CLUSTER is a connected set of skip instructions + DERR sink(s) with
# exactly two exits: the DERR (terminal, RTStubs terminal_table DERR.TRP
# kind 2 = ABORT) and ONE continuation pc.  Growth: start at the DERR,
# absorb skip predecessors while the exit set stays <= 1 (a skip whose
# other arm leaves for a second pc is NOT part of the cluster — it is
# unrelated control flow preceding the guard).
#
# FOLDABLE iff (a) exactly one continuation, (b) every interior pc's static
# predecessors (quest.tags inverted, all edge kinds) lie inside the
# cluster, (c) the guard is not itself interior to another cluster, and
# additionally (d) no interior pc is a BlockSync gate (BlockSync.cpp:
# c/j-block 'n' successors, SYSCALL-block successors — permanently listed)
# and (e) no interior pc is a condition-system entry (O.ON handler
# addresses, I.GOTO resume targets — derived from quest.dis below).
# Anything else: UNFOLDABLE with the reason (boundary 3: refuse, don't
# guess).
#
# Skip semantics: EagleCompute.cpp (WSEQ/WSNE/WSLT/WSLE/WSGT/WSGE :173-201,
# WUSGT/WUSGE :203-211, WSEQI/WSNEI/WSLEI/WSGTI :213-235, WUGTI/WULEI
# :360-368).  Rendering mirrors tools/lower.py SKIP_RR/SKIP_RI16/SKIP_RI32
# (lines 444-447, 607-620): registerRegister prints src,dst; XX==YY compares
# against 0 (dst=0 in the source); word immediates are sign-extended,
# wide immediates are unsigned 32-bit.  The folded condition is the path
# condition to the continuation, written as a transcription of the skips:
#   cond(K) = true;  cond(DERR) = false;
#   cond(skip t; fall f, skip s) = t ? cond(s) : cond(f)
#     -> `(t) && cond(s)`     when cond(f) is false
#     -> `!(t) && cond(f)`    when cond(s) is false
#     -> `((t) && cond(s)) || (!(t) && cond(f))` otherwise (none exist)
# with `x && true` -> `x`.  No other algebra.

import argparse, bisect, hashlib, re, sys, time
from collections import defaultdict

SKIP_RR = {"WSEQ": "==", "WSNE": "!=", "WSLT": "<s", "WSLE": "<=s",
           "WSGT": ">s", "WSGE": ">=s", "WUSGT": ">u", "WUSGE": ">=u"}
SKIP_RI16 = {"WSEQI": "==", "WSNEI": "!=", "WSLEI": "<=s", "WSGTI": ">s"}
SKIP_RI32 = {"WUGTI": ">u", "WULEI": "<=u"}
SKIPS = set(SKIP_RR) | set(SKIP_RI16) | set(SKIP_RI32)

O_ON, I_GOTO = 0x7017ED9B, 0x7017EC7C


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def sext16(v):
    return (v & 0x7FFF) - (v & 0x8000)


def hexc(v):
    return "0x%08X" % (v & 0xFFFFFFFF)          # == lower.py hexc


# ------------------------------------------------------------------ inputs

def parse_dis(path):
    """pc -> (mnemonic, args-string, full text).  Data lines (no mnemonic
    shape) are skipped."""
    ins = {}
    rx = re.compile(r"^([0-9a-fA-F]{8}) ([A-Z][A-Z0-9.#?]*)(?:\s+([^;]*))?;")
    for raw in open(path, encoding="ascii", errors="replace"):
        m = rx.match(raw.rstrip("\r\n"))
        if m:
            ins[int(m.group(1), 16)] = (m.group(2), (m.group(3) or "").strip(), raw.rstrip("\r\n"))
    return ins


def parse_tags(path):
    """pc -> ordered successor list (ALL edge kinds: n, j, c, u)."""
    succ = {}
    for raw in open(path, encoding="ascii", errors="replace"):
        toks = raw.split()
        if not toks:
            continue
        pc = int(toks[0], 16)
        succ[pc] = [int(t, 16) for t in toks[1:] if len(t) == 8 and re.fullmatch(r"[0-9A-Fa-f]{8}", t)]
    return succ


def parse_blocks_split(path):
    """Returns (starts:set, routine_of:{start->name}, gates:set).
    Gate derivation transcribes BlockSync.cpp:70-110: for c/j terminator
    lines the successors after the 'n' tag; for SYSCALL blocks every
    successor."""
    starts, routine_of, gates = set(), {}, set()
    cur, name, last_insn = None, None, ""
    for raw in open(path, encoding="ascii", errors="replace"):
        line = raw.rstrip("\r\n ").rstrip()
        if not line:
            continue
        if line.startswith("#"):
            name = line[1:].strip()
            continue
        m = re.match(r"^([0-9A-Fa-f]{8}):$", line)
        if m:
            cur = int(m.group(1), 16)
            starts.add(cur)
            routine_of[cur] = name
            last_insn = ""
            continue
        if re.match(r"^[ncju]( |$)", line):
            call_block = line[0] in "cj"
            syscall_block = last_insn.startswith("SYSCALL")
            after_n = line[0] == "n"
            for tok in line.split():
                if len(tok) == 1 and tok.islower():
                    after_n = tok == "n"
                    continue
                a = int(tok, 16)
                if a and (syscall_block or (call_block and after_n)):
                    gates.add(a)
            last_insn = ""
            continue
        last_insn = line
    return starts, routine_of, gates


def parse_synclist(path):
    return {int(l, 16) for l in (x.strip() for x in open(path)) if l and not l.startswith("#")}


# --------------------------------------------------------------- predicates

REG = re.compile(r"^[0-3]$")
IMM = re.compile(r"^(-?\d+) \((0x[0-9A-Fa-f]+)\)$")


def skip_predicate(op, args):
    """The emulator's skip test as an ir 3 comparison, or None if the
    operand text does not match the expected format (then the cluster
    is UNFOLDABLE: 'operand format')."""
    parts = [a.strip() for a in args.split(",")]
    if op in SKIP_RR and len(parts) == 2 and REG.match(parts[0]) and REG.match(parts[1]):
        x, y = parts
        rhs = "0" if x == y else "ac" + y          # src=acX, dst=acY; XX==YY -> dst=0
        return "ac%s %s %s" % (x, SKIP_RR[op], rhs)
    if op in SKIP_RI16 and len(parts) == 2 and REG.match(parts[0]):
        m = IMM.match(parts[1])
        if m:
            return "ac%s %s %s" % (parts[0], SKIP_RI16[op], hexc(sext16(int(m.group(2), 16))))
    if op in SKIP_RI32 and len(parts) == 2 and REG.match(parts[0]):
        m = IMM.match(parts[1])
        if m:
            return "ac%s %s %s" % (parts[0], SKIP_RI32[op], hexc(int(m.group(2), 16)))
    return None


# ------------------------------------------------------------------ clusters

class Cluster:
    def __init__(self, derr):
        self.derrs = {derr}
        self.skips = set()
        self.members = {derr}
        self.exits = set()
        self.guard = None
        self.cont = None
        self.interior = []          # block starts strictly inside
        self.reasons = []
        self.cond = None
        self.shape = None

    def foldable(self):
        return not self.reasons


def grow(derr, ins, succ, pred):
    """Absorb skip predecessors while the exit set stays <= 1."""
    c = Cluster(derr)
    changed = True
    while changed:
        changed = False
        for m in sorted(c.members):
            for p in sorted(pred.get(m, ())):
                if p in c.members or p not in ins:
                    continue
                op = ins[p][0]
                if op not in SKIPS:
                    continue
                trial = c.members | {p}
                exits = {s for q in trial for s in succ.get(q, ()) if s not in trial}
                if len(exits) <= 1:
                    c.members.add(p)
                    c.skips.add(p)
                    changed = True
    c.exits = {s for q in c.members for s in succ.get(q, ()) if s not in c.members}
    return c


def analyse(c, ins, succ, pred, starts, gates, cond_entries):
    # (a) exactly one continuation
    if not c.skips:
        p = sorted(pred.get(next(iter(c.derrs)), ()))
        c.reasons.append("no skip predecessor (preds: %s — %s)" %
                         (" ".join("%08X" % x for x in p) or "none",
                          " ".join(ins[x][0] for x in p if x in ins) or "?"))
    if len(c.exits) != 1:
        c.reasons.append("continuations: %d (%s)" %
                         (len(c.exits), " ".join("%08X" % x for x in sorted(c.exits))))
    else:
        c.cont = next(iter(c.exits))
    # entry: members with a predecessor outside the cluster
    entries = sorted(m for m in c.members if any(p not in c.members for p in pred.get(m, ())))
    if len(entries) == 1 and entries[0] in c.skips:
        c.guard = entries[0]
    elif len(entries) == 0:
        c.reasons.append("no entry (unreachable cluster)")
    else:
        # (b) — some non-guard member is entered from outside
        # pick the lowest skip as the nominal guard for reporting
        c.guard = min(c.skips) if c.skips else None
        for e in entries:
            if e != c.guard:
                outside = sorted(p for p in pred.get(e, ()) if p not in c.members)
                c.reasons.append("outside predecessor into %08X (%s) from %s" %
                                 (e, ins[e][0] if e in ins else "?",
                                  " ".join("%08X" % p for p in outside)))
    # interior block starts = block starts among members other than the
    # block that contains the guard (the guard is the block's terminator).
    if c.guard is not None:
        c.interior = sorted(m for m in c.members if m in starts and m != c.guard)
        # If the guard pc IS a block start, that block is the guard block and
        # stays listed; the delisted set is every other member start.
    # (d) gates
    for m in c.interior:
        if m in gates:
            c.reasons.append("interior %08X is a BlockSync gate" % m)
    # (e) condition-system entries
    for m in sorted(c.members):
        if m in cond_entries:
            c.reasons.append("interior %08X is a condition-system entry (%s)" % (m, cond_entries[m]))
    # predicates + shape + condition
    preds = {}
    for s in c.skips:
        op, args, _ = ins[s]
        t = skip_predicate(op, args)
        if t is None:
            c.reasons.append("operand format at %08X: %s" % (s, ins[s][2]))
        preds[s] = t
        ss = succ.get(s, [])
        if len(ss) != 2 or ss[0] != ss[0] or ss[0] >= ss[1]:
            c.reasons.append("skip %08X successors not [fall, skip] ascending: %s" %
                             (s, " ".join("%08X" % x for x in ss)))
    if c.guard is not None and c.foldable():
        order = walk_order(c, succ)
        c.shape = " / ".join(ins[s][0] for s in order)
        c.cond = path_cond(c, succ, preds)
    return c


def walk_order(c, succ):
    """Skips in control order from the guard (fall-through first)."""
    out, seen, stack = [], set(), [c.guard]
    while stack:
        m = stack.pop(0)
        if m in seen or m not in c.skips:
            continue
        seen.add(m)
        out.append(m)
        stack = [s for s in succ.get(m, ()) if s in c.skips] + stack
    return out


def path_cond(c, succ, preds):
    memo = {}

    def cond(pc):
        if pc == c.cont:
            return True
        if pc in c.derrs:
            return False
        if pc in memo:
            return memo[pc]
        fall, skip = succ[pc]
        t = preds[pc]
        cf, cs = cond(fall), cond(skip)
        if cf is False and cs is True:
            r = "(%s)" % t
        elif cf is False:
            r = "(%s) && %s" % (t, cs)
        elif cs is False and cf is True:
            r = "!(%s)" % t
        elif cs is False:
            r = "!(%s) && %s" % (t, cf)
        elif cf is True and cs is True:
            r = True
        else:
            r = "((%s) && %s) || (!(%s) && %s)" % (t, cs, t, cf)
        memo[pc] = r
        return r

    r = cond(c.guard)
    return r if isinstance(r, str) else str(r)


# ------------------------------------------------- condition-system entries

def condition_entries(ins):
    """pc -> how derived.  O.ON handler addresses (XLEF 2 before LJSR O.ON),
    I.GOTO resume targets (XLEF 2 before LJSR I.GOTO), and every
    XPEF/LPEF/XLEF/LLEF whose folded target is an instruction pc in the
    game listing (code addresses handed around as values)."""
    out = {}
    pcs = sorted(ins)
    idx = {pc: i for i, pc in enumerate(pcs)}
    lj = re.compile(r"^LJSR \[0x(7017ED9B|7017EC7C)\]")
    ef = re.compile(r"^(XLEF|LLEF|XPEF|LPEF) (?:([0-3]),)?\[[^\]]*\] \(0x([0-9A-Fa-f]{8})\)")
    for pc in pcs:
        op, args, text = ins[pc]
        m = lj.match(text.split(" ", 1)[1])
        if m:
            target_kind = "O.ON handler" if m.group(1).upper() == "7017ED9B" else "I.GOTO resume"
            # nearest preceding XLEF 2,[..] (0xADDR) within 6 instructions
            for back in range(1, 7):
                j = idx[pc] - back
                if j < 0:
                    break
                q = pcs[j]
                mm = ef.match(ins[q][2].split(" ", 1)[1])
                if mm and mm.group(1) == "XLEF" and mm.group(2) == "2":
                    a = int(mm.group(3), 16)
                    out[a] = "%s via %08X (%s)" % (target_kind, pc, ins[q][2].split(" ", 1)[1])
                    break
            else:
                out[-pc] = "%s at %08X: no XLEF 2 found within 6 insns" % (target_kind, pc)
            continue
        mm = ef.match(text.split(" ", 1)[1])
        if mm:
            a = int(mm.group(3), 16)
            if a in ins and a not in out:
                out[a] = "%s code-address operand at %08X" % (mm.group(1), pc)
    return out


# --------------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dis", default="../../Disassembled/quest.dis")
    ap.add_argument("--tags", default="../../Disassembled/quest.tags")
    ap.add_argument("--blocks", default="../c_src/quest.blocks.split")
    ap.add_argument("--synclist", default="../c_src/quest.synclist.split")
    ap.add_argument("--out", default=None, help="write assumed-foldable.txt here")
    ap.add_argument("--ir", default=None, help="quest.ir2.* to cross-check goto labels against the delisted set")
    ap.add_argument("--verbose", action="store_true", help="print every cluster")
    a = ap.parse_args()
    t0 = time.time()

    ins = parse_dis(a.dis)
    succ = parse_tags(a.tags)
    starts, routine_of, gates = parse_blocks_split(a.blocks)
    synclist = parse_synclist(a.synclist)
    pred = defaultdict(set)
    for s, ts in succ.items():
        for t in ts:
            pred[t].add(s)
    cond_entries = condition_entries(ins)

    derrs = sorted(pc for pc, (op, _, _) in ins.items() if op == "DERR")
    codes = defaultdict(int)
    for d in derrs:
        codes[ins[d][1]] += 1

    clusters, owner = [], {}
    for d in derrs:
        if d in owner:
            continue
        c = grow(d, ins, succ, pred)
        for m in c.members:
            if m in owner:
                c.reasons.append("overlaps cluster of DERR %08X" % owner[m])
            owner[m] = d
        # other DERRs inside this cluster
        for m in c.members:
            if m != d and ins.get(m, ("",))[0] == "DERR":
                c.derrs.add(m)
        clusters.append(analyse(c, ins, succ, pred, starts, gates, cond_entries))

    sorted_starts = sorted(starts)
    def block_of(pc):
        i = bisect.bisect_right(sorted_starts, pc) - 1
        return sorted_starts[i] if i >= 0 else None
    # (c) guard block interior to another cluster; and chain detection
    interior_all = {}
    for c in clusters:
        for m in c.interior:
            interior_all[m] = c
    guard_starts = {}
    for c in clusters:
        if c.guard is None:
            continue
        # block containing the guard: the greatest start <= guard
        gb = block_of(c.guard)
        guard_starts[c.guard] = gb
        c.guard_block = gb
        c.cont_outside = sorted(p for p in pred.get(c.cont, ()) if p not in c.members)
        if gb in interior_all and interior_all[gb] is not c:
            c.reasons.append("guard block %08X is interior to cluster of %08X" %
                             (gb, min(interior_all[gb].derrs)))
    chained = [c for c in clusters if c.cont in guard_starts.values() and c.foldable()]

    # ------------------------------------------------------------ report
    fold = [c for c in clusters if c.foldable()]
    unfold = [c for c in clusters if not c.foldable()]
    print("DERR embeds: %d (%s)" % (len(derrs), ", ".join("DERR %s: %d" % (k, v) for k, v in sorted(codes.items()))))
    print("clusters: %d  foldable: %d  unfoldable: %d" % (len(clusters), len(fold), len(unfold)))
    print("DERRs in foldable clusters: %d" % sum(len(c.derrs) for c in fold))
    print()
    print("Census by shape (skip sequence in control order / DERR code):")
    shape_count = defaultdict(int)
    for c in fold:
        shape_count[(c.shape, "+".join(sorted({ins[d][1] for d in c.derrs})))] += 1
    for (shape, code), n in sorted(shape_count.items(), key=lambda kv: -kv[1]):
        print("  %5d  %-22s DERR %s" % (n, shape, code))
    print()
    print("Unfoldable (%d):" % len(unfold))
    for c in unfold:
        print("  DERR %08X (%s) in %s: %s" % (min(c.derrs), ins[min(c.derrs)][1],
              routine_of.get(block_of(min(c.derrs)), "?"),
              "; ".join(c.reasons)))
    print()
    # interior / delist accounting
    interior = sorted({m for c in fold for m in c.interior})
    print("interior block starts to delist: %d  (all in synclist: %s; any gate: %s)" %
          (len(interior), all(m in synclist for m in interior), any(m in gates for m in interior)))
    per = defaultdict(int)
    for c in fold:
        per[len(c.interior)] += 1
    print("  interior count per cluster: %s" % ", ".join("%d blocks x%d" % (k, v) for k, v in sorted(per.items())))
    print("blocks: %d -> %d   synclist: %d -> %d   DERR embeds: %d -> %d" %
          (len(starts), len(starts) - len(interior), len(synclist), len(synclist) - len(interior),
           len(derrs), len(derrs) - sum(len(c.derrs) for c in fold)))
    print("chained clusters (continuation is another cluster's guard block): %d" % len(chained))
    k_in = [c for c in fold if not c.cont_outside]
    k_out = [c for c in fold if c.cont_outside]
    print("continuation K predecessors: all inside cluster: %d   has outside preds: %d" % (len(k_in), len(k_out)))
    kinds = defaultdict(int)
    for c in k_out:
        kinds[" ".join(sorted({ins[p][0] if p in ins else '?' for p in c.cont_outside}))] += 1
    for k, v in sorted(kinds.items(), key=lambda kv: -kv[1])[:12]:
        print("    %5d  outside-pred mnemonics: %s" % (v, k))
    print("guard is itself a block start (one-skip guard block): %d" % sum(1 for c in fold if c.guard in starts))
    # Option B accounting: K absorbed into the guard block and delisted too
    # (required by Machine.cpp:306 arrival counting if the guard block runs
    # K's instructions).  Chains: K == next cluster's guard block -> the next
    # cluster's assert lands in the same merged block.
    ks = [c.cont for c in k_in]
    print("option B (delist K as well): K distinct: %s  K in gates: %d  K cond-entry: %d  K in synclist: %d/%d" % (
        len(set(ks)) == len(ks), sum(k in gates for k in ks), sum(k in cond_entries for k in ks),
        sum(k in synclist for k in ks), len(ks)))
    delistB = set(interior) | set(ks)
    print("  delisted: %d   blocks/synclist: %d -> %d" % (len(delistB), len(starts), len(starts) - len(delistB)))
    by_guard_block = {c.guard_block: c for c in fold}
    depth = defaultdict(int)
    roots = 0
    for c in fold:
        if c.guard_block in delistB:
            continue                      # not a chain root: lives inside another merged block
        roots += 1
        d, cur = 1, c
        while cur.cont in by_guard_block:
            cur = by_guard_block[cur.cont]; d += 1
        depth[d] += 1
    print("  merged blocks (chain roots): %d  chain depth histogram: %s" % (
        roots, ", ".join("%d:%d" % kv for kv in sorted(depth.items()))))
    print()
    print("Condition-system entries derived: %d (%d O.ON handlers, %d I.GOTO resumes, %d code-address EF operands)" %
          (len([k for k in cond_entries if k > 0]),
           sum("O.ON" in v for v in cond_entries.values()),
           sum("I.GOTO" in v for v in cond_entries.values()),
           sum("code-address" in v for v in cond_entries.values())))
    for k, v in sorted(cond_entries.items()):
        if k < 0:
            print("  WARNING: %s" % v)
    hits = [(k, v) for k, v in cond_entries.items() if k in owner]
    print("  entries that are cluster members: %d" % len(hits))
    for k, v in hits:
        print("    %08X: %s" % (k, v))

    # skip-arm sanity: does every cluster skip have the dis-adjacent pc as fall?
    bad = 0
    for c in clusters:
        for s in c.skips:
            nxt = min((p for p in ins if p > s), default=None)
            if succ[s][0] != nxt:
                bad += 1
    print("  skips whose fall-through != dis-adjacent pc: %d" % bad)

    if a.ir:
        # IR-side check (PROMPT Part 1 item 5): every goto label that names a
        # delisted pc must be emitted by a block of the SAME cluster (the
        # guard block or an interior/K block).  Anything else is an outside
        # entry the loader would refuse post-fold — and a census miss.
        member_cluster = {}
        for c in fold:
            for m in c.members | {c.cont}:
                member_cluster[m] = c
            member_cluster[c.guard_block] = c
        cur, bad, nlab = None, [], 0
        for raw in open(a.ir, encoding="ascii", errors="replace"):
            m = re.match(r"^block ([0-9A-Fa-f]{8})", raw)
            if m:
                cur = int(m.group(1), 16)
                continue
            m = re.search(r"goto \[([0-9A-Fa-f, ]+)\]", raw)
            if m and cur is not None:
                for lab in m.group(1).split(","):
                    L = int(lab.strip(), 16)
                    if L in delistB:
                        nlab += 1
                        cc = member_cluster.get(cur)
                        if cc is None or not (L in cc.members or L == cc.cont):
                            bad.append((cur, L))       # chain links (L == cont of cur's cluster) are inside
        print("IR goto labels into the delisted set: %d; from outside the owning cluster (chain links excluded): %d" % (nlab, len(bad)))
        for cur, L in bad[:20]:
            print("    block %08X -> %08X" % (cur, L))

    if a.verbose:
        print()
        for c in clusters:
            print("%08X guard=%s cont=%s derr=%s skips=%s interior=%s%s" % (
                min(c.derrs), "%08X" % c.guard if c.guard else "-", "%08X" % c.cont if c.cont else "-",
                " ".join("%08X" % d for d in sorted(c.derrs)),
                " ".join("%08X:%s" % (s, ins[s][0]) for s in sorted(c.skips)),
                " ".join("%08X" % m for m in c.interior),
                ("  assert(%s)" % c.cond) if c.cond else ("  UNFOLDABLE: " + "; ".join(c.reasons))))

    if a.out:
        with open(a.out, "w") as f:
            f.write("# assumed-foldable — Project 27 DERR clusters folded on STATIC evidence\n")
            f.write("# tags sha256 %s\n" % sha256(a.tags))
            f.write("# dis sha256 %s\n" % sha256(a.dis))
            f.write("# blocks.split sha256 %s\n" % sha256(a.blocks))
            f.write("# rule: cluster = DERR + skip predecessors absorbed while exits<=1; FOLDABLE iff\n")
            f.write("#   one continuation, every interior pc's tags-predecessors inside the cluster,\n")
            f.write("#   guard block not interior to another cluster, no interior gate, no interior\n")
            f.write("#   condition-system entry (O.ON handler / I.GOTO resume / code-address EF operand).\n")
            f.write("# UNMODELLED: on-error (condition-system) entry into an interior pc that the\n")
            f.write("#   static graph does not show.  Discharged by the goto-graph project.\n")
            f.write("# line: guard_pc derr_pc code continuation interior_pcs... | evidence\n")
            f.write("# evidence: skips (pc:mnemonic), interior predecessor sets, folded condition\n")
            for c in sorted(fold, key=lambda c: c.guard):
                d = min(c.derrs)
                inter = " ".join("%08X" % m for m in c.interior)
                ev_pred = "; ".join("%08X<-{%s}" % (m, ",".join("%08X" % p for p in sorted(pred[m])))
                                    for m in c.interior)
                f.write("%08X %08X %s %08X %s | skips %s | preds %s | assert(%s, \"DERR %s @%08X\")\n" % (
                    c.guard, d, ins[d][1], c.cont, inter,
                    " ".join("%08X:%s" % (s, ins[s][0]) for s in walk_order(c, succ)),
                    ev_pred, c.cond, ins[d][1], d))
        print("\nwrote %s (%d lines)" % (a.out, len(fold)))
    print("runtime: %.2f s" % (time.time() - t0))


if __name__ == "__main__":
    main()
