#!/usr/bin/env python3
"""ircmp.py — compare two ir 6 texts routine by routine (Project 35).

    ircmp.py A.ir B.ir --routine NAME [--folded] [--addrbook quest.addrbook] [-n N]
    ircmp.py --selftest --book quest.ir2.book --addrbook quest.addrbook

A is the IR of record (book) side, B the translator side.  Every statement
is reported MATCH / RENAME / DIFF under exactly the seven equivalences ruled
for P35 (docs/Project35/PROMPT.md, PLAN.md §4, rulings R1-R3):

  1. block labels: blocks are compared by canonical position (DFS from the
     routine entry, successors in terminator order); goto targets by
     position; `site=`, `marker=`, `ret=` dropped.  Ruling R1: the `@pc` in
     a DERR assert message is provenance too — the message compares as
     `DERR nn`.
  2. frame slots: one bijection per routine between the two sides'
     positive frame offsets (`wp(ac3, d)`; byte forms `bp(ac3, k)` map as
     byte offsets).  A slot mapped to two offsets is a DIFF(slot).
  3. temporaries: tN renumbered by first appearance in the block;
     `t@<block>.k` twins keyed by the block's canonical position.
  4. literal addresses: `[@0xW:b, "text"]` and `[@0xW:b, n]` (a literal the
     book could not read) compare as `[@LIT, n]`, n the byte length.
  5. provenance comments: everything after `;` dropped.
  6. static addresses are NOT equivalences (compared verbatim).
  7. register choice is NOT an equivalence in the primary comparison;
     `--folded` applies readable.py's in-block register-folding pass
     (fold_block) to BOTH sides first.

MATCH  = identical after equivalences 1, 3, 4, 5 with the frame offsets
         literally equal (the translator's slot rule got it right).
RENAME = identical only after the slot bijection (equivalence 2).
DIFF   = anything else, with both texts and a class:
         reg / slot / border / expr / const-spelling / missing / extra / unknown.

The parser is compiler/readable.py's (imported, not copied); the ir 6
twin/claim/release lines it does not know are pre-lexed here.
"""
import argparse, collections, difflib, os, re, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import readable as R  # noqa: E402

REGS4 = ("ac0", "ac1", "ac2", "ac3")

# ----------------------------------------------------------------------------
# loading: a book-like file -> {pc: Block}; ir 6 twin lines pre-lexed
# ----------------------------------------------------------------------------

TWIN_RE = re.compile(r"t@([0-9A-Fa-f]{8})\.(\d+)")
CLAIM_RE = re.compile(r"^claim t@([0-9A-Fa-f]{8})\.(\d+), (ac[0-3])$")
NEGHEX_RE = re.compile(r"([(,] ?)-0x([0-9A-Fa-f]+)")
RELEASE_RE = re.compile(r"^release t@([0-9A-Fa-f]{8}), (ac[0-3])$")


class Stmt:
    """A parsed statement plus its raw text (comment stripped)."""
    __slots__ = ("kind", "st", "raw", "twins")

    def __init__(self, kind, st, raw, twins):
        self.kind, self.st, self.raw, self.twins = kind, st, raw, twins


def lex_twins(body):
    """Replace t@BLOCK.k by the placeholder id twinBLOCK_k (readable's lexer
    accepts it as an unknown identifier only via our own primary hook), and
    return (text, [(block, k)])."""
    twins = []

    def sub(m):
        twins.append((int(m.group(1), 16), int(m.group(2))))
        return "0x%08X" % (0x75000000 + len(twins))  # a numeric stand-in

    return TWIN_RE.sub(sub, body), twins


def parse_line(body):
    m = CLAIM_RE.match(body)
    if m:
        return Stmt("claim", (int(m.group(1), 16), int(m.group(2)), m.group(3)), body, [])
    m = RELEASE_RE.match(body)
    if m:
        return Stmt("release", (int(m.group(1), 16), m.group(2)), body, [])
    text, twins = lex_twins(body)
    # `-0xHHHH` (IR.md §5.8 negative bp displacements) -> the 32-bit word
    text = NEGHEX_RE.sub(lambda m: m.group(1) + "0x%08X" % ((-int(m.group(2), 16)) & 0xFFFFFFFF), text)
    st = R.parse_stmt(text)
    return Stmt(st[0], st, body, twins)


def load_ir(path):
    """-> (header lines, {pc: [Stmt]}, order list)."""
    header, blocks, order, cur = [], {}, [], None
    with open(path, encoding="ascii", errors="replace") as f:
        for raw in f:
            line = raw.rstrip("\r\n")
            if cur is None and not line.startswith("block "):
                if line.strip():
                    header.append(line)
                continue
            if line.startswith("block "):
                cur = int(line.split()[1], 16)
                blocks[cur] = []
                order.append(cur)
                continue
            if not line.strip():
                cur = None
                continue
            if line.startswith("blocks "):
                break
            body = line.partition(";")[0].strip()
            if not body:
                continue
            try:
                blocks[cur].append(parse_line(body))
            except R.ParseError as e:
                raise R.ParseError("%s block %08X: %s: %s" % (path, cur, e, body))
    return header, blocks, order


# ----------------------------------------------------------------------------
# routine extraction and canonical block order (equivalence 1)
# ----------------------------------------------------------------------------

def succs_of(stmts):
    if not stmts:
        return []
    last = stmts[-1]
    if last.kind == "goto":
        return list(last.st[1])
    if last.kind == "rt_call":
        return [last.st[3] + 4]
    if last.kind == "call":
        return [last.st[5]]
    if last.kind == "instr":
        # WSAVS entry block falls through; DERR is terminal.
        #
        # P36 ruling B (DEFECT FIX, not an equivalence): an embedded CALL
        # instruction — an undecorated LCALL/XCALL/LJSR/XJSR, which the
        # emitter leaves as an instruction when the site is argc-0 or not in
        # the pushmap — RETURNS, so its block falls through to the next block
        # in address order exactly as WSAVS's does.  Returning [] here stopped
        # the DFS dead: 43 of DIED's 73 blocks were unreached and fell back to
        # ADDRESS order, which on the translator's side (synthetic pcs) is
        # really EMISSION order and would have let block ordering drift
        # undetected.  With the fall-through, DIED is 73/73 reached.  This
        # forgives nothing — it makes equivalence 1's canonical order stricter
        # — and applies identically to both sides.
        if last.st[2].startswith(("WSAVS", "LCALL", "XCALL", "LJSR", "XJSR")):
            return ["fall"]
        return []
    return []


def routine_blocks(blocks, entry, lo, hi):
    """Canonical order: DFS from entry, successors in terminator order.
    Blocks in [lo, hi) not reached are appended in address order (reported)."""
    order, seen, stack = [], set(), [entry]
    pcs_sorted = sorted(pc for pc in blocks if lo <= pc < hi)
    while stack:
        pc = stack.pop()
        if pc in seen or pc not in blocks:
            continue
        seen.add(pc)
        order.append(pc)
        ss = succs_of(blocks[pc])
        if ss == ["fall"]:
            i = pcs_sorted.index(pc) if pc in pcs_sorted else -1
            ss = [pcs_sorted[i + 1]] if 0 <= i and i + 1 < len(pcs_sorted) else []
        for s in reversed(ss):  # push reversed so the first successor is visited first
            if lo <= s < hi and s not in seen:
                stack.append(s)
    unreached = [pc for pc in pcs_sorted if pc not in seen]
    return order + unreached, unreached


# ----------------------------------------------------------------------------
# canonical rendering (equivalences 1, 3, 4, 5; slots left as placeholders)
# ----------------------------------------------------------------------------

class Canon:
    def __init__(self, label_of, block_pc, twin_label):
        self.label_of = label_of      # pc -> canonical position (int) or None
        self.block_pc = block_pc
        self.twin_label = twin_label  # pc -> canonical position for twins
        self.tmap = {}
        self.slots = []               # slot keys in order of appearance in this statement
        self.regmode = False          # render registers as placeholders (DIFF classing)
        self.constmode = False

    def label(self, pc):
        p = self.label_of.get(pc)
        return "L%d" % p if p is not None else "L?%08X" % pc

    # -- expressions -----------------------------------------------------
    def expr(self, n):
        k = n[0]
        if k == "num":
            v = n[1]
            if self.constmode:
                return "K"
            return "0x%08X" % v if n[2] == "hex" else str(R.signed(v) if hasattr(R, "signed") else v)
        if k == "bp":
            return "0x%08X:%d" % (n[1], n[2])
        if k == "str":
            return '"%s"' % n[1]
        if k == "reg":
            if self.regmode and n[1] in REGS4:
                return "acX"
            return n[1]
        if k == "t":
            if n[1] not in self.tmap:
                self.tmap[n[1]] = "t%d" % (len(self.tmap) + 1)
            return self.tmap[n[1]]
        if k == "paren":
            return "(" + self.expr(n[1]) + ")"
        if k == "not":
            return "~" + self.expr(n[1])
        if k == "lnot":
            return "!" + self.expr(n[1])
        if k == "chain":
            s = self.expr(n[1])
            for op, e in n[2]:
                s += " " + op + " " + self.expr(e)
            return s
        if k == "mem":
            return "%s[%s]" % (n[1], self.expr(n[2]))
        if k in ("call", "effop", "cmp", "words"):
            name, args = n[1], n[2]
            # frame slot: wp(ac3, d) with d > 0 ; bp(ac3, k) with k > 0
            if name in ("wp", "bp") and len(args) == 2 and args[0] == ("reg", "ac3") \
                    and args[1][0] == "num":
                d = R.signed(args[1][1])
                if d > 0:
                    key = ("w" if name == "wp" else "b", d)
                    self.slots.append(key)
                    return "%s(ac3, SLOT%d)" % (name, len(self.slots) - 1)
            return "%s(%s)" % (name, ", ".join(self.expr(a) for a in args))
        if k == "lit":
            # equivalence 4 (extended): a literal compares by its length; the
            # book shows no text for data-segment literals (REFRESH_SCREEN's
            # 0x70000E07/0x70000DDF), so text cannot be required of both sides
            return "[@LIT, %d]" % len(n[2].encode().decode("unicode_escape"))
        if k == "located":
            addr, cnt, var = n[1], n[2], n[3]
            if addr[0] == "bp" and cnt is not None and cnt[0] == "num":
                return "[@LIT, %d%s]" % (cnt[1], " varying" if var else "")
            if cnt is None:
                return "[@%s, varying]" % self.expr(addr)
            return "[@%s, %s%s]" % (self.expr(addr), self.expr(cnt), " varying" if var else "")
        if k == "effnest":
            return self.expr(n[1])
        raise ValueError("canon: unknown node %r" % (k,))

    # -- statements ------------------------------------------------------
    def stmt(self, s):
        self.slots = []
        st, k = s.st, s.kind
        if k == "instr":
            # audit text only: mnemonic + operands, address dropped
            return "@ " + st[2].strip().rstrip(";")
        if k == "ret":
            return "ret"
        if k == "goto":
            return "goto [%s] %s" % (", ".join(self.label(l) for l in st[1]), self.expr(st[2]))
        if k == "rt_call":
            return "rt_call %s(%s)" % (st[1], ", ".join(self.expr(a) for a in st[2]))
        if k == "call":
            return "call %08X args=%d" % (st[1], st[2])
        if k == "assert":
            msg = st[2]
            if msg is not None:
                msg = re.sub(r"\s*@[0-9A-Fa-f]{8}$", "", msg)  # ruling R1
            return "assert(%s%s)" % (self.expr(st[1]), ', "%s"' % msg if msg is not None else "")
        if k == "assign":
            return "%s = %s" % (self.expr(st[1]), self.expr(st[2]))
        if k == "sassign":
            return "%s = %s" % (self.expr(st[1]), self.expr(st[2]))
        if k == "cmp":
            return "ac1 = cmp(%s, %s)" % (self.expr(st[1]), self.expr(st[2]))
        if k == "words":
            return "words(@%s, %s) = words(@%s, %s)" % (
                self.expr(st[1]), self.expr(st[3]), self.expr(st[2]), self.expr(st[3]))
        if k == "claim":
            return "claim t@%s.%d, %s" % (self.twin(st[0]), st[1], "acX" if self.regmode else st[2])
        if k == "release":
            return "release t@%s, %s" % (self.twin(st[0]), "acX" if self.regmode else st[1])
        raise ValueError("canon: unknown stmt kind %r" % k)

    def twin(self, pc):
        p = self.twin_label.get(pc)
        return "B%d" % p if p is not None else "B?%08X" % pc

    def render(self, s):
        """-> (canonical text with SLOTi placeholders, [slot keys])"""
        text = self.stmt(s)
        # twins that came through the numeric stand-in
        for i, (bpc, kk) in enumerate(s.twins):
            text = text.replace("0x%08X" % (0x75000000 + i + 1), "t@%s.%d" % (self.twin(bpc), kk))
        return text, list(self.slots)


def fill(text, keys, fmt):
    for i, key in enumerate(keys):
        text = text.replace("SLOT%d" % i, fmt(key))
    return text


def skeleton(text):
    """Slot-agnostic key used to pair statements before the bijection exists."""
    return re.sub(r"SLOT\d+", "SLOT", text)


# ----------------------------------------------------------------------------
# the comparison
# ----------------------------------------------------------------------------

class Result:
    def __init__(self, name):
        self.name = name
        self.total = self.match = self.rename = self.diff = 0
        self.classes = collections.Counter()
        self.diffs = []          # (block pos, a text, b text, class)
        self.bijection = {}      # a slot key -> b slot key
        self.conflicts = []
        self.blocks_a = self.blocks_b = 0
        self.unreached_a = self.unreached_b = []
        self.block_order_note = ""

    def pct(self):
        return 100.0 * (self.match + self.rename) / self.total if self.total else 0.0


def classify(ca, cb, ka, kb, canon_a, canon_b, sa, sb):
    """DIFF class for a paired statement pair that did not match."""
    if sa is None:
        return "extra"
    if sb is None:
        return "missing"
    # register choice only?
    canon_a.regmode = canon_b.regmode = True
    ra, _ = canon_a.render(sa)
    rb, _ = canon_b.render(sb)
    canon_a.regmode = canon_b.regmode = False
    if skeleton(ra) == skeleton(rb):
        return "reg"
    canon_a.constmode = canon_b.constmode = True
    ra2, _ = canon_a.render(sa)
    rb2, _ = canon_b.render(sb)
    canon_a.constmode = canon_b.constmode = False
    if skeleton(ra2) == skeleton(rb2):
        return "const-spelling"
    if sa.kind != sb.kind:
        return "unknown"
    if sa.kind == "goto" and skeleton(re.sub(r"L\d+", "L", ca)) == skeleton(re.sub(r"L\d+", "L", cb)):
        return "border"
    toks = lambda s: collections.Counter(re.findall(r"[A-Za-z_@]\w*|0x[0-9A-Fa-f:]+|-?\d+", skeleton(s)))
    if toks(ca) == toks(cb):
        return "expr"
    if sa.kind == sb.kind and sa.kind == "assign" and ca.split(" = ")[0] == cb.split(" = ")[0]:
        return "expr"
    return "unknown"


def compare_routine(name, A, B, entry_a, lo_a, hi_a, entry_b, lo_b, hi_b, ndiffs=25, folded=False,
                    addrbook_entry=None):
    ha, ba, oa = A
    hb, bb, ob = B
    order_a, unreached_a = routine_blocks(ba, entry_a, lo_a, hi_a)
    order_b, unreached_b = routine_blocks(bb, entry_b, lo_b, hi_b)
    if folded:
        ba = fold_side(ba, order_a, entry_a, addrbook_entry)
        bb = fold_side(bb, order_b, entry_b, addrbook_entry)
    res = Result(name)
    res.blocks_a, res.blocks_b = len(order_a), len(order_b)
    res.unreached_a, res.unreached_b = unreached_a, unreached_b
    pos_a = {pc: i for i, pc in enumerate(order_a)}
    pos_b = {pc: i for i, pc in enumerate(order_b)}
    canon_a = Canon(pos_a, None, pos_a)
    canon_b = Canon(pos_b, None, pos_b)
    bij, conflicts = {}, []
    pairs = []  # (block pos, sa, sb, ca, cb, ka, kb)
    nblocks = max(len(order_a), len(order_b))
    for i in range(nblocks):
        sa_list = ba[order_a[i]] if i < len(order_a) else []
        sb_list = bb[order_b[i]] if i < len(order_b) else []
        canon_a.tmap, canon_b.tmap = {}, {}
        ra = [canon_a.render(s) for s in sa_list]
        rb = [canon_b.render(s) for s in sb_list]
        sm = difflib.SequenceMatcher(None, [skeleton(t) for t, _ in ra], [skeleton(t) for t, _ in rb],
                                    autojunk=False)
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == "equal":
                for x, y in zip(range(i1, i2), range(j1, j2)):
                    pairs.append((i, sa_list[x], sb_list[y], ra[x], rb[y]))
            else:
                # pair the replaced runs positionally, then leftovers as missing/extra
                for x, y in zip(range(i1, i2), range(j1, j2)):
                    pairs.append((i, sa_list[x], sb_list[y], ra[x], rb[y]))
                for x in range(i1 + (j2 - j1), i2):
                    pairs.append((i, sa_list[x], None, ra[x], None))
                for y in range(j1 + (i2 - i1), j2):
                    pairs.append((i, None, sb_list[y], None, rb[y]))
    # pass 1: build the slot bijection from structurally paired statements
    for i, sa, sb, ra, rb in pairs:
        if sa is None or sb is None:
            continue
        (ta, ka), (tb, kb) = ra, rb
        if skeleton(ta) != skeleton(tb) or len(ka) != len(kb):
            continue
        for x, y in zip(ka, kb):
            if x in bij and bij[x] != y:
                conflicts.append((x, bij[x], y, i))
            elif x not in bij:
                bij[x] = y
    res.bijection = bij
    res.conflicts = conflicts
    fmt_lit = lambda key: str(key[1])
    # pass 2: classify
    for i, sa, sb, ra, rb in pairs:
        res.total += 1
        if sa is None or sb is None:
            cls = "extra" if sa is None else "missing"
            res.diff += 1
            res.classes[cls] += 1
            res.diffs.append((i, fill(*ra, fmt_lit) if ra else "—", fill(*rb, fmt_lit) if rb else "—", cls))
            continue
        (ta, ka), (tb, kb) = ra, rb
        lit_a, lit_b = fill(ta, ka, fmt_lit), fill(tb, kb, fmt_lit)
        if lit_a == lit_b:
            res.match += 1
            continue
        mapped_a = fill(ta, ka, lambda key: str(bij.get(key, ("?", "?"))[1]))
        if skeleton(ta) == skeleton(tb) and mapped_a == lit_b and all(k in bij for k in ka):
            res.rename += 1
            continue
        cls = classify(ta, tb, ka, kb, canon_a, canon_b, sa, sb)
        if cls not in ("reg", "const-spelling", "border", "expr", "unknown") or \
                (skeleton(ta) == skeleton(tb) and len(ka) == len(kb) and any(k in bij and bij[k] != k2 for k, k2 in zip(ka, kb))):
            cls = "slot" if skeleton(ta) == skeleton(tb) else cls
        res.diff += 1
        res.classes[cls] += 1
        res.diffs.append((i, lit_a, lit_b, cls))
    return res


# ----------------------------------------------------------------------------
# --folded: readable.py's in-block register folding on both sides
# ----------------------------------------------------------------------------

def fold_side(blocks, order, entry, ab_entry):
    """Run readable.fold_block over the routine's blocks and return a new
    {pc: [Stmt]} with the folded statements.  Twin/claim/release lines are
    kept verbatim (readable does not know them)."""
    rblocks = {}
    kept = {}
    for pc in order:
        b = R.Block(pc, pc & 0xF0000000)
        kept[pc] = []
        for s in blocks[pc]:
            if s.kind in ("claim", "release") or s.twins:
                kept[pc].append(s)      # opaque to the folder: re-inserted after
                b.stmts.append(("instr", pc, "TWIN"))  # an instruction is a barrier
                b.raws.append(s.raw)
            else:
                b.stmts.append(s.st)
                b.raws.append(s.raw)
        rblocks[pc] = b
    entries = {entry: dict(name="X", alloc=0, wfp=0, argc=(ab_entry or {}).get("argc", 0),
                           frame=(ab_entry or {}).get("frame", 0), variant="WSAVS", flags="", stacked=False)}
    sw = R.Switches()
    world = R.World(rblocks, entries, {}, {}, sw)
    world.routine_blocks[entry] = list(order)
    rt = R.Routine(world, entry)
    rt.analyse_reg_liveness()
    out = {}
    for pc in order:
        b = rblocks[pc]
        ctx = rt.ctx_for(b)
        stmts, _live = R.fold_block(b, ctx, R.Report())
        lst, ki = [], iter(kept[pc])
        for st in stmts:
            if st[0] == "instr" and st[2] == "TWIN":
                lst.append(next(ki))
            else:
                lst.append(Stmt(st[0], st, "", []))
        out[pc] = lst
    return out


# ----------------------------------------------------------------------------
# report
# ----------------------------------------------------------------------------

def report(res, ndiffs, out=sys.stdout):
    w = out.write
    w("== %s ==\n" % res.name)
    w("blocks: A %d  B %d" % (res.blocks_a, res.blocks_b))
    if res.unreached_a or res.unreached_b:
        w("   (unreached by DFS: A %s B %s)" % (
            " ".join("%08X" % p for p in res.unreached_a), " ".join("%08X" % p for p in res.unreached_b)))
    w("\n")
    w("statements: total %d  MATCH %d  RENAME %d  DIFF %d   MATCH+RENAME %.1f%%\n" % (
        res.total, res.match, res.rename, res.diff, res.pct()))
    if res.classes:
        w("DIFF classes: %s\n" % ", ".join("%s %d" % kv for kv in sorted(res.classes.items())))
    if res.bijection:
        items = sorted(res.bijection.items())
        w("slot bijection (A -> B): %s\n" % ", ".join(
            "%s%d->%d" % (k[0], k[1], v[1]) + ("" if k == v else "*") for k, v in items))
    if res.conflicts:
        w("slot CONFLICTS: %s\n" % "; ".join("A %s%d -> B %d and %d (block %d)" % (
            x[0], x[1], y[1], z[1], i) for x, y, z, i in res.conflicts))
    for i, a, b, cls in res.diffs[:ndiffs]:
        w("  DIFF(%s) block L%d\n    A: %s\n    B: %s\n" % (cls, i, a, b))
    if len(res.diffs) > ndiffs:
        w("  ... %d more\n" % (len(res.diffs) - ndiffs))


# ----------------------------------------------------------------------------
# routine location
# ----------------------------------------------------------------------------

def locate(name, addrbook):
    """(entry, lo, hi, entry dict) — the routine's address range from the
    addrbook (entry to the next entry, stacked entries included as
    boundaries)."""
    entries = R.load_addrbook(addrbook)
    pcs = sorted(entries)
    for i, pc in enumerate(pcs):
        if entries[pc]["name"] == name:
            hi = pcs[i + 1] if i + 1 < len(pcs) else pc + 0x100000
            return pc, pc, hi, entries[pc]
    raise SystemExit("routine %s not in %s" % (name, addrbook))


def locate_b(B, name):
    """The translator side: a header line `routine NAME entry <hex8> lo <hex8> hi <hex8>`."""
    for line in B[0]:
        m = re.match(r"routine (\S+) entry ([0-9A-Fa-f]{8}) lo ([0-9A-Fa-f]{8}) hi ([0-9A-Fa-f]{8})", line)
        if m and m.group(1) == name:
            return int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16)
    return None


# ----------------------------------------------------------------------------
# self-test
# ----------------------------------------------------------------------------

def selftest(book, addrbook, names=("PICK_X_Y", "UPDATE_SCREENS", "REFRESH_SCREEN", "DIED")):
    A = load_ir(book)
    ok = True
    for name in names:
        entry, lo, hi, e = locate(name, addrbook)
        # 1. identity
        r = compare_routine(name, A, A, entry, lo, hi, entry, lo, hi, addrbook_entry=e)
        ident = r.total > 0 and r.match == r.total
        # 2. slot permutation: rewrite every wp(ac3, d)/bp(ac3,k) d -> d + 100 on side B
        Bblocks = {}
        for pc in A[1]:
            if lo <= pc < hi:
                Bblocks[pc] = [parse_line(re.sub(r"\b(wp|bp)\(ac3, (\d+)\)", lambda m: "%s(ac3, %d)" % (
                    m.group(1), int(m.group(2)) + 100), s.raw)) for s in A[1][pc]]
        B = (A[0], Bblocks, A[2])
        r2 = compare_routine(name, A, B, entry, lo, hi, entry, lo, hi, addrbook_entry=e)
        slotted = sum(1 for pc in Bblocks for s in A[1][pc] if re.search(r"\b(wp|bp)\(ac3, \d+\)", s.raw))
        perm = r2.diff == 0 and r2.rename == slotted
        # 3. one register swap -> exactly one DIFF(reg)
        Cblocks = {pc: list(v) for pc, v in Bblocks.items()}
        first = None
        for pc in sorted(Cblocks):
            for i, s in enumerate(A[1][pc]):
                if s.kind == "assign" and s.st[1] == ("reg", "ac0") and s.st[2][0] == "num":
                    first = (pc, i)
                    break
            if first:
                break
        one = None
        if first:
            pc, i = first
            Cblocks[pc][i] = parse_line(A[1][pc][i].raw.replace("ac0 =", "ac1 =", 1))
            r3 = compare_routine(name, A, (A[0], Cblocks, A[2]), entry, lo, hi, entry, lo, hi, addrbook_entry=e)
            one = r3.diff == 1 and r3.classes.get("reg") == 1
        # 4. folded identity
        r4 = compare_routine(name, A, A, entry, lo, hi, entry, lo, hi, folded=True, addrbook_entry=e)
        fold_ident = r4.total > 0 and r4.match == r4.total
        line = "%-16s identity %s (%d stmts)  permutation %s (%d RENAME)  regswap %s  folded-identity %s (%d stmts)" % (
            name, "OK" if ident else "FAIL", r.total, "OK" if perm else "FAIL", r2.rename,
            "OK" if one else ("n/a" if one is None else "FAIL"), "OK" if fold_ident else "FAIL", r4.total)
        print(line)
        ok &= ident and perm and (one is not False) and fold_ident
    print("selftest", "PASS" if ok else "FAIL")
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a", nargs="?")
    ap.add_argument("b", nargs="?")
    ap.add_argument("--routine", action="append", default=[])
    ap.add_argument("--folded", action="store_true")
    ap.add_argument("--addrbook", default=os.path.join(os.path.dirname(__file__), "..", "emulation", "quest.addrbook"))
    ap.add_argument("--book", default=os.path.join(os.path.dirname(__file__), "..", "emulation", "quest.ir2.book"))
    ap.add_argument("-n", type=int, default=25)
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    t0 = time.time()
    if args.selftest:
        ok = selftest(args.book, args.addrbook)
        print("runtime %.1fs" % (time.time() - t0))
        sys.exit(0 if ok else 1)
    if not (args.a and args.b and args.routine):
        ap.error("A B --routine NAME")
    A, B = load_ir(args.a), load_ir(args.b)
    for name in args.routine:
        entry, lo, hi, e = locate(name, args.addrbook)
        lb = locate_b(B, name)
        if lb is None:
            lb = (entry, lo, hi)
        res = compare_routine(name, A, B, entry, lo, hi, lb[0], lb[1], lb[2], folded=args.folded, addrbook_entry=e)
        report(res, args.n)
    print("runtime %.2fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
