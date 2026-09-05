#!/usr/bin/env python3
# lower.py — quest.dis + quest.blocks (+ pushmap, argmap) -> quest.ir (ir 4)
#
# THE SPEC IS docs/IR.md (consolidated, normative; spec-wins).  This
# emitter implements it — the P26 grammar (goto [labels] e terminator,
# strict booleans, s/u comparisons, word layer, the effectful op family,
# t-places, stack-register reads, ind()), wp/bp/M8 semantics (P25), WPSH
# group stores, the byte-EA per-mode table — and any emitter/spec
# disagreement is a bug in one of them, to be reported, not papered
# over.  Census + per-mnemonic semantics with emulator source citations:
# docs/Project26/Census.md.  Byte-EA derivation record + the disassembler
# byte-operand defect this parser compensates for:
# docs/Project25/ByteEA.md and docs/DISASSEMBLER_BYTE_OPERANDS.md.
# (Historical phase-1 rationale: docs/Project23/IRPhase1.md.)
#
# Register-faithful 1:1, class-capped, no folding; t-places only where the
# instruction itself needs scratch (borrow brackets, WXCH, XNDO, Nova
# tests).  Everything not in the cap is an embedded @addr instruction.
# TOTAL: an all-embed block is valid output.
#
# Effectful ops carry NO formulas here — IRExec calls the same
# EagleInstruction helpers the emulated instructions call (user ruling,
# Aug 28 2026, docs/Project23/WideCarry.md; P23 same-helpers principle).
# lower.py only CLASSIFIES and renders operands.  Every skip's signedness
# and every derived test is read from the emulator source cited in
# Census.md, never from the ISA name.
#
# Exclusions (P22 REPORT §4): block 7015BD6B (interior LJSR) and any
# block containing ENQT/DEQUE text or an XCT site — refused entirely,
# even fully-embedded.
#
# P27 (Sep 5 2026, docs/Project27/Census.md, ruling F1 = A): DERR
# bounds-check clusters listed in --assumed-foldable (tools/
# derr_clusters.py; refused unless its tags sha256 matches --tags) fold
# into the GUARD block: the guard skip becomes `assert(<path condition
# to the continuation>, "DERR nn @derr_pc")` followed by `goto [K] 0`;
# the cluster's interior blocks (second skip, DERR) are not emitted and
# are delisted from the shipped sync list (--synclist-in/--synclist-out,
# identity minus the interiors of the clusters ACTUALLY folded — a guard
# block that refuses keeps its interiors emitted and listed: never a
# half-fold).  The condition is re-derived here from the skips through
# lower_one and cross-checked against the artifact's text.  K stays a
# listed block (Machine.cpp:306 arrival counting).

import argparse, hashlib, re, sys

XCT_SITES = {0x7017E9F6, 0x7017ECF4}
EXCLUDED_BLOCKS = {0x7015BD6B}
SEG_MASK, SEG_KEEP = 0x0FFFFFFF, 0xF0000000

class Refuse(Exception):
    pass

def die(msg):
    raise Refuse(msg)

def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

def sext(v, bits):
    m = 1 << (bits - 1)
    return (v & (m - 1)) - (v & m)

# ---------------------------------------------------------------- inputs

def parse_dis(path):
    """pc -> instruction text (semicolon-terminated line body)."""
    ins = {}
    rx = re.compile(r"^([0-9a-fA-F]{8}) (.+)$")
    for raw in open(path, encoding="ascii", errors="replace"):
        line = raw.rstrip("\r\n")
        m = rx.match(line)
        if m:
            ins[int(m.group(1), 16)] = m.group(2).strip()
    return ins

def parse_blocks(path):
    """block start pc -> instruction-text list (order preserved)."""
    blocks, cur, body, succs = {}, None, [], {}
    for raw in open(path, encoding="ascii", errors="replace"):
        line = raw.rstrip("\r\n")          # CRLF tolerated (P22 ruling c)
        if not line or line.startswith("#"):
            continue
        m = re.match(r"^([0-9A-Fa-f]{8}):$", line)
        if m:
            if cur is not None:
                blocks[cur] = body
            cur, body = int(m.group(1), 16), []
            continue
        m = re.match(r"^n((?: [0-9A-Fa-f]{8})+)$", line)
        if m and cur is not None:
            succs[cur] = [int(x, 16) for x in m.group(1).split()]
            continue
        # P28: a call block's edge line is `c <callee> n <return pc>` — the
        # return pc is the block's (only) game-side successor.
        m = re.match(r"^c [0-9A-Fa-f]{8} n((?: [0-9A-Fa-f]{8})+)$", line)
        if m and cur is not None:
            succs[cur] = [int(x, 16) for x in m.group(1).split()]
            continue
        if re.match(r"^[ncj]( |$)", line):
            continue                        # other edge lines: c(all)/j(ump)
        if cur is None:
            die("blocks: instruction text before any block header: " + line)
        body.append(line)
    if cur is not None:
        blocks[cur] = body
    return blocks, succs

def parse_pushmap(path):
    """Returns (decorated_pcs, push_slot{pc->slot}, call_site{pc->(marker,[push_pcs])}, borrow{pc->slot}).
    Push lines bind to the next call line (the file is grouped per site)."""
    pcs, push_slot, call_site, borrow_pcs = set(), {}, {}, {}
    pending = []
    for raw in open(path, encoding="ascii", errors="replace"):
        line = raw.split("#")[0].strip()
        mb = re.match(r"^borrow ([0-9A-Fa-f]{8}) ([0-9A-Fa-f]{8})$", line)
        if mb:
            pcs.add(int(mb.group(1), 16))
            borrow_pcs[int(mb.group(1), 16)] = int(mb.group(2), 16)   # pc -> slot (P26 t-places)
            continue
        m = re.match(r"^(push|call) ([0-9A-Fa-f]{8}) ([0-9A-Fa-f]{8})(?: (\d+))?$", line)
        if not m:
            if line:
                die("pushmap line unparseable (fix the parser, don't skip): " + line)
            continue
        kind, pc, slot = m.group(1), int(m.group(2), 16), int(m.group(3), 16)
        wides = int(m.group(4)) if m.group(4) else 1
        pcs.add(pc)
        if kind == "push":
            push_slot[pc] = (slot, wides)
            pending.append(pc)
        else:
            call_site[pc] = (slot, pending)
            pending = []
    return pcs, push_slot, call_site, borrow_pcs

# ----------------------------------------------------- operand rendering

def seg_of(pc):
    return pc & SEG_KEEP

def ea_expr(pc, base, raw_disp, wide, folded=None):
    """Render the EA expression per Machine::eagle_{x,l}_resolve_indirect.
    Segment wrap ((e & 0x0FFFFFFF) | seg) is an EXECUTOR rule declared in
    the block header, not per-statement syntax; lower.py refuses any
    absolute/folded EA outside the block's segment so the uniform wrap is
    provably identity-or-hardware-exact. R[e] = deref e then follow bit 31
    (indirect bit implied by R). Returns (expr, indirect)."""
    if wide:
        indirect = (raw_disp & 0x80000000) != 0
        disp = raw_disp & 0x7FFFFFFF
        disp = sext(disp, 31) if base else disp
    else:
        indirect = (raw_disp & 0x8000) != 0
        disp = raw_disp & 0x7FFF
        disp = sext(disp, 15) if base else disp
    if base == "":
        e_abs = (disp & SEG_MASK) | seg_of(pc)
        if wide and (disp & ~SEG_MASK) not in (0, seg_of(pc)):
            die("L-absolute EA 0x%08X outside block segment at %08X" % (disp, pc))
        e = "0x%08X" % e_abs
    elif base == "pc":
        if folded is None:
            die("pc-relative EA without folded address at %08X" % pc)
        if (folded & ~SEG_MASK) != seg_of(pc):
            die("pc-relative EA 0x%08X outside block segment at %08X" % (folded, pc))
        e = "0x%08X" % folded
    else:
        # P25: wp(base, disp) — the executor applies the word segment
        # wrap (copy_segment), which the hardware applies to base-indexed
        # word EAs.  Replaces both the naked sum (old XLEF/LLEF value
        # emission, latent unwrapped inconsistency) and the spelled-out
        # mask (old pef_value).  Indirect base forms keep R[base + d]:
        # the R index gets the executor wrap already (identical result).
        if indirect:
            e = "%s + %d" % (base, disp)
        else:
            e = "wp(%s, %d)" % (base, disp)
    if indirect:
        e = "R[%s]" % e
    return e, indirect

# [base+0xHHHH] with optional @ prefix and optional " (0xADDR)" fold.
# Byte forms fold as " (0xWORD:B)" — word address + byte select (P25).
MEMOP = re.compile(r"^(@?)\[(?:(pc|ac2|ac3)\+)?0x([0-9A-Fa-f]+)\](?: \(0x([0-9A-Fa-f]+)\))?$")
BMEMOP = re.compile(r"^(@?)\[(?:(pc|ac2|ac3)\+)?0x([0-9A-Fa-f]+)\]"
                    r"(?: \(0x([0-9A-Fa-f]+):([01])\))?$")

def parse_memop(pc, text, wide):
    m = MEMOP.match(text.strip())
    if not m:
        return None
    at, base, disp, folded = m.group(1), m.group(2) or "", int(m.group(3), 16), m.group(4)
    folded = int(folded, 16) if folded else None
    # P25: the dis strips the indirect bit into the '@' prefix (word L
    # forms print e.g. "@[0x70000210]" for raw 0xF0000210).  The prefix
    # is authoritative; reconstruct the bit.  A set bit WITHOUT the
    # prefix is an inconsistent rendering: refuse.
    disp = reconcile_at(pc, text, at, disp, wide)
    e, indirect = ea_expr(pc, base, disp, wide, folded)
    if bool(at) != indirect:
        die("@-prefix vs indirect bit disagree at %08X: %s" % (pc, text))
    return e

def bplit(v):
    """Byte-pointer literal in the dis fold notation: 0xW:b, value w*2+b
    (user ruling, Aug 29: dump addresses are word-addressed; make the IR
    text greppable against them — b is the byte select, always 0/1)."""
    return "0x%08X:%d" % ((v >> 1) & 0x7FFFFFFF, v & 1)


def byte_ea(pc, operand, wide):
    """Byte-EA operand -> VALUE expression, per Machine::eagle_{x,l}_
    byte_indexed (P25; semantics read from the emulator source, NOT the
    IR.md §8 parked formula — see Project25/ByteEA.md):
      X ii=0:   set_byte_segment(seg, raw16)       (raw16 NOT sign-extended)
      X ii=1:   (pc+1)*2 + sext16(raw16)           (no masking; constant)
      X ii=2/3: set_byte_segment(seg, ac*2+sext16) -> bp(acN, d)
      L ii=0:   raw wide, unchanged                 (constant)
      L ii=1:   (pc+1)*2 + raw wide                 (no masking; constant)
      L ii=2/3: ac*2 + raw wide                     (no masking; acN*2 + d)
    The dis renders bit 31 of an L-form byte displacement as an '@'
    prefix with the bit stripped from the number (disassembler defect,
    flagged; fix owned by the user).  Reconstruct: raw = printed | bit31.
    X-form byte displacements are full 16-bit fields with NO indirect
    bit: an '@' there is unrenderable -> refuse.
    Returns the expression string, or None if the operand shape is
    unrecognized (caller embeds).  Malformed recognized shapes refuse."""
    mm = BMEMOP.match(operand.strip())
    if not mm:
        return None
    at, base = mm.group(1), mm.group(2) or ""
    disp = int(mm.group(3), 16)
    fold_w = int(mm.group(4), 16) if mm.group(4) else None
    fold_b = int(mm.group(5)) if mm.group(5) else 0
    seg3 = (pc >> 28) & 0x7
    if wide:
        if at:
            if disp & 0x80000000:
                die("@ prefix AND bit 31 set at %08X: %s" % (pc, operand))
            disp |= 0x80000000        # user ruling: '@' -> high bit
    else:
        if at:
            die("@ on X-form byte operand (no indirect bit exists) at %08X: %s"
                % (pc, operand))
        if disp > 0xFFFF:
            die("X-form byte displacement > 16 bits at %08X: %s" % (pc, operand))
    opw = pc + 1                      # operand word address (address+1)
    if base == "":
        if wide:
            value = disp & 0xFFFFFFFF                     # L ii=0: raw
        else:
            value = ((disp & 0x1FFFFFFF) | (seg3 << 29)) & 0xFFFFFFFF
        return bplit(value)
    if base == "pc":
        if wide:
            value = (opw * 2 + disp) & 0xFFFFFFFF
        else:
            value = (opw * 2 + (sext(disp, 16) & 0xFFFFFFFF)) & 0xFFFFFFFF
        if fold_w is not None:
            folded = ((fold_w * 2) + fold_b) & 0xFFFFFFFF
            if folded != value:
                die("byte fold 0x%08X:%d != computed 0x%08X at %08X: %s"
                    % (fold_w, fold_b, value, pc, operand))
        return bplit(value)
    # register base
    if wide:
        return "%s*2 + %s" % (base, bplit(disp & 0xFFFFFFFF))
    d = sext(disp, 16)
    return "bp(%s, %d)" % (base, d)


WPSHRR = re.compile(r"^WPSH\s+([0-3]),([0-3]);$")

def push_stores(pc, text, slot_wides):
    """One decorated push -> list of arg-slot store statement bodies, or
    None if inexpressible (site embeds).  wides==1: a single value store
    (word or byte EA).  WPSH x,a (P25): the emulated hook writes AC[XX]
    at the base slot ASCENDING (EagleStack.cpp WPSH, P18 tranche B,
    ordering verified there); one instruction -> wides addressless
    stores.  registerRegister renders XX,AA.  Group size must equal the
    map's wides (same check the emulated hook enforces)."""
    slot, wides = slot_wides
    m = WPSHRR.match(text.strip())
    if m:
        xx, aa = int(m.group(1)), int(m.group(2))
        if ((aa - xx) & 3) + 1 != wides:
            die("WPSH group size != map wides at %08X: %s (wides=%d)"
                % (pc, text, wides))
        return ["M32[0x%08X] = ac%d" % (slot + 2 * k, (xx + k) & 3)
                for k in range(wides)]
    if wides != 1:
        return None                   # multi-wide non-WPSH: unknown shape
    v = pef_value(pc, text)
    if v is None:
        return None
    return ["M32[0x%08X] = %s" % (slot, v)]


def reconcile_at(pc, text, at, disp, wide):
    """The dis renders the word-form indirect bit inconsistently: X forms
    keep the bit IN the printed displacement with a decorative '@'
    (e.g. "@[ac3+0xFFEC]"); L forms strip it into the '@'
    (e.g. "@[0x70000210]" for raw 0xF0000210).  Accept both; refuse the
    genuinely inconsistent rendering (bit set, no '@')."""
    ibit = 0x80000000 if wide else 0x8000
    if disp & ibit:
        if not at:
            die("indirect bit set but no @ prefix at %08X: %s" % (pc, text))
        return disp                   # X convention: bit retained
    if at:
        return disp | ibit            # L convention: reconstruct
    return disp


def pef_value(pc, text):
    """XPEF/LPEF/XPEFB/LPEFB operand -> the pushed EA as a VALUE
    expression (wp/bp per P25 ruling; R[...] used raw for indirect;
    byte pointers per byte_ea).  None if not expressible."""
    m = re.match(r"^(X|L)PEF(B?)\s+(.+);$", text.strip())
    if not m:
        return None
    wide = m.group(1) == "L"
    if m.group(2) == "B":
        return byte_ea(pc, m.group(3), wide)
    mm = MEMOP.match(m.group(3).strip())
    if not mm:
        return None
    at, base = mm.group(1), mm.group(2) or ""
    disp = int(mm.group(3), 16)
    folded = int(mm.group(4), 16) if mm.group(4) else None
    disp = reconcile_at(pc, text, at, disp, wide)
    e, indirect = ea_expr(pc, base, disp, wide, folded)
    return e            # R[...] full address / constant / wp(base, d)


# ---------------------------------------------------------------- rt_call (P28)
#
# docs/Project28/{Census,RTConventions}.md; spec docs/IR.md §3/§6 (ir 4).
# A game -> runtime `LCALL [..],argc; # ?NAME` is ALWAYS the last
# instruction of its block (987/987; the CFG's `c <callee> n <site+4>`
# edge).  Its argument pushes (XPEF/LPEF/WPSH, walked backward from the
# LCALL inside the block) fold into the terminator
#     rt_call ?NAME(e1, ..., eN) site=<pc>
# in PL/I order: e1 = argument 1 = the LAST push (RTBridge::arg_pointer(n)
# = M32[wsp-2n] at entry, hw/RTBridge.cpp:111).  The executor pushes eN
# first through Machine::wide_push and runs the LCALL at `site` through
# the normal instruction path.  Interleaved XLEF/XWSTA (the ac2 register
# argument of ?UNSIGNED_TO_CHAR; compiler spills) lower as the ordinary
# statements they are, in program order, before the rt_call.  Refuse,
# don't guess: anything outside [pushes + XLEF/XWSTA], an argc mismatch,
# a window leaving the block, an argument pef_value cannot render, an
# indirect (memory-reading) argument with an interleaved store, an
# argument reading a register an interleaved XLEF writes (would need a
# t-place; 0 today — refused rather than emitted untested), an argc
# outside the callee's known set — the site stays embedded, censused.

RT_LCALL = re.compile(r"^LCALL\s+\[0x([0-9A-Fa-f]+)\],(\d+);\s*#\s*(\?\S+)")
RT_PUSH_WORD = re.compile(r"^(X|L)PEF\s+(.+);$")
RT_PUSH_BYTE = re.compile(r"^(X|L)PEFB\s+(.+);$")
RT_WPSH = re.compile(r"^WPSH\s+([0-3]),([0-3]);$")
RT_XLEF = re.compile(r"^XLEF\s+([0-3]),(.+);$")
RT_XWSTA = re.compile(r"^XWSTA\s+([0-3]),(.+);$")
RT_MEMOP = re.compile(r"^(@?)\[(?:(pc|ac2|ac3)\+)?0x([0-9A-Fa-f]+)\](?: \(0x([0-9A-Fa-f:]+)\))?$")

# argc SET per callee — docs/Project28/RTConventions.md (Sep 5 census;
# ruling F2: a site whose argc is not in its callee's set REFUSES).
RT_ARGC = {"?WRITE_SCREEN": {2, 5}, "?RANDOM_NUMBER": {3}, "?UNSIGNED_TO_CHAR": {1},
           "?DELAY": {1}, "?READ": {4, 6, 7}, "?CHAR_TO_UNSIGNED": {1},
           "?OPEN_FILE": {2}, "?CLOSE_FILE": {1}, "?OPEN_SHARED_IO_FILE": {5},
           "?GET_SHARED_PAGE": {4}, "?WRITE": {3, 6}, "?CREATE_TASK": {2},
           "?AWAIT_CONSOLE_INTERRUPT": {0}, "?LOOKUP_PORT": {3}, "?LIB_ERROR_CODE": {0},
           "?CONNECT": {1}, "?CURRENT_PID": {0}, "?READ_SCREEN": {3}}

def rt_push_of(text):
    """(kind, wides, regs_read, reads_memory) for a push instruction, else None."""
    t = text.strip()
    m = RT_WPSH.match(t)
    if m:
        x, a = int(m.group(1)), int(m.group(2))
        if a < x:
            die("WPSH wraparound (never in QUEST — WPSH_WPOP.md): " + t)
        return "WPSH", a - x + 1, {"ac%d" % r for r in range(x, a + 1)}, False
    for rx, kinds in ((RT_PUSH_BYTE, ("XPEFB", "LPEFB")), (RT_PUSH_WORD, ("XPEF", "LPEF"))):
        m = rx.match(t)
        if m:
            mm = RT_MEMOP.match(m.group(2).strip())
            regs = {mm.group(2)} if mm and mm.group(2) in ("ac2", "ac3") else set()
            return kinds[0 if m.group(1) == "X" else 1], 1, regs, bool(mm and mm.group(1))
    return None

class RTSite:
    """One runtime call site's window.  pushes/inter in PROGRAM order;
    args in PL/I order (arg1 first); refuse = reason or None."""
    def __init__(self, pc, callee, target, argc):
        self.pc, self.callee, self.target, self.argc = pc, callee, target, argc
        self.pushes, self.inter, self.args = [], [], []
        self.refuse = None
        self.crosses = False
        self.needs_t = 0
        self.shape = ""
    def line(self):
        return "rt_call %s(%s) site=%08X" % (self.callee, ", ".join(e for _, e, _ in self.args), self.pc)

def rt_window(pc, block_pcs, dis):
    """The site at pc (an RT LCALL) with its window inside block_pcs (the
    block's pcs in order).  Returns an RTSite (refuse set on any reason)."""
    m = RT_LCALL.match(dis[pc])
    if not m:
        return None
    s = RTSite(pc, m.group(3), int(m.group(1), 16), int(m.group(2)))
    i = block_pcs.index(pc)
    wides, window, j = 0, [], i - 1
    while wides < s.argc and j >= 0:
        text = dis[block_pcs[j]]
        p = rt_push_of(text)
        if p:
            wides += p[1]
        window.append((block_pcs[j], text, p))
        j -= 1
    if wides < s.argc:
        s.crosses = True
        s.refuse = "window crosses block start"
        return s
    if wides > s.argc:
        s.refuse = "WPSH overshoot"
        return s
    window.reverse()
    for wpc, text, p in window:
        if p:
            s.pushes.append((wpc, text, p[0], p[1], p[2], p[3]))
        else:
            mn = text.split()[0] if text.split() else "?"
            s.inter.append((wpc, text, mn))
    bad = sorted({mn for _, _, mn in s.inter if mn not in ("XLEF", "XWSTA")})
    if bad:
        s.refuse = "interleaved non-XLEF/XWSTA: " + ",".join(bad)
    s.shape = "+".join(mn for _, _, mn in s.inter) or "contiguous"
    if s.callee not in RT_ARGC:
        s.refuse = s.refuse or "callee not in RTConventions table"
    elif s.argc not in RT_ARGC[s.callee]:
        s.refuse = s.refuse or "argc outside the callee's known set"
    exprs = []                       # program order, one per wide
    for wpc, text, kind, w, regs, mem in s.pushes:
        if kind == "WPSH":
            for r in sorted(regs):   # ac x pushed first = deepest = higher-numbered arg
                exprs.append((wpc, r, {r}, False))
        else:
            e = None
            try:
                e = pef_value(wpc, text)
            except Refuse as ex:
                s.refuse = s.refuse or ("pef_value refuses: " + str(ex).split(":")[0][:50])
            if e is None:
                s.refuse = s.refuse or ("pef_value cannot render: " + text.split(";")[0])
            exprs.append((wpc, e, regs or set(), mem))
    n = len(exprs)
    for k, (wpc, e, regs, mem) in enumerate(exprs):
        written, stores = set(), 0
        for ipc, t, mn in s.inter:
            if ipc > wpc:
                mx = RT_XLEF.match(t.strip())
                if mx:
                    written.add("ac" + mx.group(1))
                if RT_XWSTA.match(t.strip()):
                    stores += 1
        inline = not (regs & written)
        if mem and stores:
            s.refuse = s.refuse or "indirect argument with an interleaved store"
        if not inline:
            s.needs_t += 1
            s.refuse = s.refuse or "argument reads a register an interleaved XLEF writes (t-place form not emitted)"
        s.args.append((n - k, e, inline))
    s.args.reverse()                 # arg1 .. argN
    return s

def rt_slice_ok(site, slice_):
    """Slice gating (Phase B landing order): 1 = contiguous ?WRITE_SCREEN,
    2 = every contiguous site, 3 = all sites (interleaved included)."""
    if slice_ >= 3:
        return True
    if site.shape != "contiguous":
        return False
    if slice_ == 2:
        return True
    return slice_ == 1 and site.callee == "?WRITE_SCREEN"

# ---- LDSP jump tables (P28): the dis renders the table at the folded
# operand as "<tbl> VALID RANGE: [lo, hi]" + "JUMP TARGETS: ..." lines
# (parse_dis keeps only the first as an instruction-shaped line, so the
# table is read from the raw file here).  Semantics EagleGeneral.cpp:251-
# 260: L=M32[tbl-4], H=M32[tbl-2]; in range and entry != -1 -> jump; else
# fall through to pc+3 (the DERR 17 sink, terminal by ruling).
def parse_ldsp_tables(dis_path):
    text = open(dis_path, encoding="ascii", errors="replace").read().replace("\r", "")
    out = {}
    for m in re.finditer(r"^([0-9a-fA-F]{8}) VALID RANGE: \[(-?\d+), (-?\d+)\]\n\s*JUMP TARGETS: ((?:.|\n)*?)\n\n", text, re.M):
        tbl = int(m.group(1), 16)
        targets = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{8})", m.group(4))]
        out[tbl] = (int(m.group(2)), int(m.group(3)), targets)
    return out

LDSP_TABLES = {}          # filled by main() from --dis

# ------------------------------------------------------------- lowering
#
# P26 (docs/Project26/Census.md; spec docs/IR.md rev 3): lower_one returns
# (statements, terminator) or None.  Statements are addressless bodies;
# the terminator, when present, is a complete `goto [..] e` body and the
# instruction MUST be the block's last line (split-CFG invariant: every
# conditional-length instruction ends its block — verified by the census,
# refused here if violated).  Effectful ops (add sub mul div cvwn ash
# nadd nsub nmul) appear ONLY at statement root; pure exprs are
# parenthesized per class (no precedence reliance).  Every formula here
# is read from the emulator source cited in Census.md §2 — not from the
# ISA name.

IMM = re.compile(r"^(-?\d+) \(0x([0-9A-Fa-f]+)\)$")
REG = re.compile(r"^[0-3]$")

def imm_word(a):
    """'dec (0xHHHH)' word immediate -> raw 16-bit value, or None."""
    m = IMM.match(a)
    return int(m.group(2), 16) & 0xFFFF if m else None

def hexc(v):
    return "0x%08X" % (v & 0xFFFFFFFF)

def rr(args):
    """registerRegister renders XX,YY (source, destination)."""
    if len(args) == 2 and REG.match(args[0]) and REG.match(args[1]):
        return "ac" + args[0], "ac" + args[1]
    return None, None

class BlockCtx:
    """Per-block lowering state: t-place allocator, borrow slot -> t,
    successors, dis adjacency (for skip fall-through verification)."""
    def __init__(self, start, succs, dis_pcs, dis_index, borrow_slot):
        self.start = start
        self.succs = succs
        self.dis_pcs = dis_pcs
        self.dis_index = dis_index
        self.borrow_slot = borrow_slot     # pc -> slot for borrow pcs
        self.slot_t = {}                   # slot -> tN (open bracket)
        self.nt = 0
        self.leftovers = False             # P28: LNDO / LDSP / Nova LOAD forms
    def newt(self):
        self.nt += 1
        return "t%d" % self.nt
    def next_pc(self, pc):
        i = self.dis_index[pc] + 1
        return self.dis_pcs[i] if i < len(self.dis_pcs) else None
    def skip_exits(self, pc):
        """(fall, skip) for a skip-class terminator: the CFG lists the
        successors ascending = [no-skip, skip]; the no-skip successor must
        be the dis-adjacent pc.  Anything else refuses."""
        s = self.succs
        if len(s) != 2 or not (s[0] < s[1]):
            die("skip at %08X: block %08X has successors %s, expected 2 ascending"
                % (pc, self.start, ["%08X" % x for x in s]))
        if s[0] != self.next_pc(pc):
            die("skip at %08X: no-skip successor %08X != next pc" % (pc, s[0]))
        return s[0], s[1]

def goto2(fall, skip, test):
    return "goto [%08X, %08X] %s" % (fall, skip, test)

# ---- Nova ALU (NovaCompute.cpp:8-80), no-load forms only (P26 ruling R3).
# The test is DERIVED from the emulator's field decomposition, never from
# the mnemonic: src = (acX & 0xFFFF) | carry-in<<16; op; shift/carry per
# SS; skip per KKK on (c, 17-bit-ish result).  With N (#) set nothing is
# written (:63-66) so the instruction is a pure test.  Load forms (43)
# are DEFERRED (Census.md §2d) and stay embedded.
NOVA = re.compile(r"^(MOV|ADD|SUB|COM|NEG|ADC|INC|AND)(?:\.([ZOC]?)([LRS]?))?(#?) "
                  r"([0-3]),([0-3])(?:,(SKP|SZC|SNC|SZR|SNR|SEZ|SBN))?$")

def nova_test(body, ctx):
    m = NOVA.match(body)
    if not m:
        return None
    op, carry, shift, noload, x, y, skip = m.groups()
    carry, shift = carry or "", shift or ""     # `ADD 2,0` has no `.CC SS` group at all
    if not noload and not ctx.leftovers:
        return None                       # load form: behind --leftovers (P28)
    if noload and not skip:
        die("no-load Nova op without a skip is a no-op: %s" % body)
    acx, acy = "ac" + x, "ac" + y
    stmts = []
    # carry-in (CC): 0 -> c, Z -> 0, O -> 1, C -> complement of c
    cin = {"": "lsh(c, 16)", "Z": None, "O": "0x00010000", "C": "(lsh(c, 16) ^ 0x00010000)"}[carry]
    src16 = "(%s & 0xFFFF)" % acx
    s = src16 if cin is None else "(%s | %s)" % (src16, cin)
    dst16 = "(%s & 0xFFFF)" % acy
    val = {"COM": "(%s ^ 0xFFFF)" % s,
           "NEG": "((%s ^ 0xFFFF) + 1)" % s,
           "MOV": s,
           "INC": "(%s + 1)" % s,
           "ADC": "((%s ^ 0xFFFF) + %s)" % (s, dst16),
           "SUB": "((%s ^ 0xFFFF) + %s + 1)" % (s, dst16),
           "ADD": "(%s + %s)" % (s, dst16),
           "AND": "(%s & (%s | 0x00010000))" % (s, dst16)}[op]
    t = ctx.newt()
    stmts.append("%s = %s" % (t, val))
    # shift (SS): carry bit and result per the emulator
    if shift == "":
        cbit = "(lsh(%s, -16) & 1)" % t
        res = "(%s & 0xFFFF)" % t
    elif shift == "L":
        cbit = "(lsh(%s, -15) & 1)" % t
        res = "(((%s & 0xFFFF) * 2) | (lsh(%s, -16) & 1))" % (t, t)
    elif shift == "R":
        cbit = "(%s & 1)" % t
        res = "(lsh(%s, -1) & 0xFFFF)" % t
    else:  # S
        cbit = "(lsh(%s, -16) & 1)" % t
        res = "(((%s & 0xFF) * 0x100) | (lsh(%s & 0xFF00, -8)))" % (t, t)
    # skip (KKK)
    cz = "(%s == 0)" % cbit
    cn = "(%s == 1)" % cbit
    rz = "(%s == 0)" % res
    rn = "(%s != 0)" % res
    if not noload:
        # P28 LOAD form (ruling Sep 5): the emulator's writes at
        # NovaCompute.cpp:63-66 — c then ac[YY] = the shifted value, whose
        # high half is what the SS arm leaves (zero, except that SS=1 keeps
        # bit 16 = old bit 15 — replicated, see Project28/REPORT.md).  The
        # manual says bits 16-31 are UNDEFINED (HWFindings_Sep5.md §3), so
        # the high half is a don't-care by spec; the IR matches the emulator
        # because the strict surface compares the whole register.
        stmts.append("c = %s" % cbit)
        stmts.append("%s = %s" % (acy, res))
    if not skip:
        return stmts, None
    test = {"SKP": "1", "SZC": cz, "SNC": cn, "SZR": rz, "SNR": rn,
            "SEZ": "(%s || %s)" % (cz, rz), "SBN": "(%s && %s)" % (cn, rn)}[skip]
    return stmts, test

# ---- skip family (EagleCompute.cpp: WSEQ/WSNE/WSLT/WSLE/WSGT/WSGE :175-203,
# WUSGT/WUSGE :205-213, W*I :215-237, WUGTI/WULEI :362-370).  XX==YY compares
# the register against 0 (dst=0 in the source).  Signedness per row is the
# source's cast, never guessed.
SKIP_RR = {"WSEQ": "==", "WSNE": "!=", "WSLT": "<s", "WSLE": "<=s",
           "WSGT": ">s", "WSGE": ">=s", "WUSGT": ">u", "WUSGE": ">=u"}
SKIP_RI16 = {"WSEQI": "==", "WSNEI": "!=", "WSLEI": "<=s", "WSGTI": ">s"}
SKIP_RI32 = {"WUGTI": ">u", "WULEI": "<=u"}

def lower_one(pc, text, pushmap_pcs, ctx):
    """Return (stmts, terminator) or None -> embed."""
    body = text.rstrip(";").strip()
    if pc in pushmap_pcs:
        # P26: borrow-bracket WPSH/WPOP (the P20 borrow map) -> t-place
        # save/restore.  Everything else decorated stays an instruction
        # (or is handled by the site machinery in emit_block).
        if pc in ctx.borrow_slot:
            m = re.match(r"^(WPSH|WPOP) ([0-3]),([0-3])$", body)
            if not m or m.group(2) != m.group(3):
                die("borrow bracket at %08X is not WPSH/WPOP r,r: %s" % (pc, body))
            slot, r = ctx.borrow_slot[pc], "ac" + m.group(2)
            if m.group(1) == "WPSH":
                if slot in ctx.slot_t:
                    die("borrow slot %08X pushed twice in block %08X" % (slot, ctx.start))
                t = ctx.newt(); ctx.slot_t[slot] = t
                return ["%s = %s" % (t, r)], None
            t = ctx.slot_t.pop(slot, None)
            if t is None:
                die("borrow WPOP at %08X without its WPSH in block %08X" % (pc, ctx.start))
            return ["%s = %s" % (r, t)], None
        return None
    m = re.match(r"^(\S+)\s*(.*)$", body)
    if not m:
        return None
    op, rest = m.group(1), m.group(2)
    args = [a.strip() for a in rest.split(",")] if rest else []
    S = lambda *stmts: (list(stmts), None)

    # ---- loads / constants (unchanged from P23/P25)
    if op == "NLDAI" and len(args) == 2:
        v = imm_word(args[0])
        if v is not None:
            return S("ac%s = %s" % (args[1], hexc(sext(v, 16))))
    if op == "WLDAI" and len(args) == 2:
        m32 = re.match(r"^0x([0-9A-Fa-f]{1,8})$", args[1])
        if m32:                       # WLDAI <ac>,<0xvalue> — ac FIRST
            return S("ac%s = %s" % (args[0], hexc(int(m32.group(1), 16))))
    if op == "WMOV" and len(args) == 2:
        return S("ac%s = ac%s" % (args[1], args[0]))

    # ---- effectful family (statement root; helpers named in Census.md §2b)
    x, y = rr(args)
    if op in ("WADD", "WSUB") and x:
        return S("%s = %s(%s, %s)" % (y, "add" if op == "WADD" else "sub", y, x))
    if op == "WADC" and x:                         # :51 add(~src, dst)
        return S("%s = add(%s, ~%s)" % (y, y, x))
    if op == "WINC" and x:                         # :87 add(1, src) -> dst
        return S("%s = add(%s, 1)" % (y, x))
    if op == "WNEG" and x:                         # :29 sub(src, 0)
        return S("%s = sub(0, %s)" % (y, x))
    if op in ("WMUL",) and x:                      # :57 mul(src, dst)
        return S("%s = mul(%s, %s)" % (y, y, x))
    if op == "WDIV" and x:                         # :63 -> EagleInstruction::div
        return S("%s = div(%s, %s)" % (y, y, x))
    if op in ("NADD", "NSUB", "NMUL") and x:       # :121/:126/:136
        return S("%s = %s(%s, %s)" % (y, {"NADD": "nadd", "NSUB": "nsub", "NMUL": "nmul"}[op], y, x))
    if op == "NNEG" and x:                         # :131 narrow_sub(src, 0)
        return S("%s = nsub(0, %s)" % (y, x))
    if op in ("WADI", "WSBI", "NADI", "NSBI") and len(args) == 2 and REG.match(args[1]):
        k = int(args[0])                           # tinyImmediateRegister: imm(1..4),ac
        if not 1 <= k <= 4:
            die("tiny immediate out of range at %08X: %s" % (pc, body))
        f = {"WADI": "add", "WSBI": "sub", "NADI": "nadd", "NSBI": "nsub"}[op]
        return S("ac%s = %s(ac%s, %d)" % (args[1], f, args[1], k))
    if op == "WADDI" and len(args) == 2:
        im = IMM.match(args[1])                    # WADDI <ac>,<value (0xhex)>
        if im:
            return S("ac%s = add(ac%s, %s)" % (args[0], args[0], hexc(int(im.group(2), 16))))
    if op == "WNADI" and len(args) == 2:           # :316 sext16(word) via add
        v = imm_word(args[1])
        if v is not None:
            return S("ac%s = add(ac%s, %s)" % (args[0], args[0], hexc(sext(v, 16))))
    if op == "NADDI" and len(args) == 2:           # :151 narrow_add(word, dst)
        v = imm_word(args[1])
        if v is not None:
            return S("ac%s = nadd(ac%s, %s)" % (args[0], args[0], hexc(v)))
    if op == "CVWN" and len(args) == 1 and REG.match(args[0]):   # :80 -> cvwn
        return S("ac%s = cvwn(ac%s)" % (args[0], args[0]))
    memeff = {"LWADD": ("add", 32, True), "XWADD": ("add", 32, False),
              "XWSUB": ("sub", 32, False), "LWSUB": ("sub", 32, True),
              "XWMUL": ("mul", 32, False), "LWMUL": ("mul", 32, True),
              "XNADD": ("nadd", 16, False), "LNADD": ("nadd", 16, True),
              "XNSUB": ("nsub", 16, False), "LNSUB": ("nsub", 16, True),
              "XNMUL": ("nmul", 16, False), "LNMUL": ("nmul", 16, True)}
    if op in memeff and len(args) == 2 and REG.match(args[0]):
        f, width, wide = memeff[op]
        e = parse_memop(pc, args[1], wide)
        if e is None:
            return None
        return S("ac%s = %s(ac%s, M%d[%s])" % (args[0], f, args[0], width, e))
    memrmw = {"XWADI": ("add", 32, False), "XWSBI": ("sub", 32, False),
              "XNADI": ("nadd", 16, False), "XNSBI": ("nsub", 16, False),
              "LNADI": ("nadd", 16, True), "LNSBI": ("nsub", 16, True)}
    if op in memrmw and len(args) == 2:
        f, width, wide = memrmw[op]
        k = int(args[0])
        if not 1 <= k <= 4:
            die("tiny immediate out of range at %08X: %s" % (pc, body))
        e = parse_memop(pc, args[1], wide)
        if e is None:
            return None
        # M16 store truncates (spec §5); the effectful op is NOT wrapped
        # in trunc16 (ruling R6).
        return S("M%d[%s] = %s(M%d[%s], %d)" % (width, e, f, width, e, k))

    # ---- word layer (pure)
    if op == "WCOM" and x:
        return S("%s = ~%s" % (y, x))
    if op in ("WAND", "WIOR", "WXOR") and x:
        return S("%s = %s %s %s" % (y, y, {"WAND": "&", "WIOR": "|", "WXOR": "^"}[op], x))
    if op in ("WANDI", "WIORI", "WXORI") and len(args) == 2 and REG.match(args[0]):
        m32 = IMM.match(args[1])                   # registerWideImmediate: dec (0xHHHHHHHH)
        if m32:
            return S("ac%s = ac%s %s %s" % (args[0], args[0],
                     {"WANDI": "&", "WIORI": "|", "WXORI": "^"}[op], hexc(int(m32.group(2), 16))))
    if op == "ANDI" and len(args) == 2 and REG.match(args[0]):     # :170 16-bit imm, zero-extended
        v = imm_word(args[1])
        if v is not None:
            return S("ac%s = ac%s & %s" % (args[0], args[0], hexc(v)))
    if op == "WLSI" and len(args) == 2 and REG.match(args[1]):     # :102 logical_shift(dst, k)
        k = int(args[0])
        if not 1 <= k <= 4:
            die("tiny immediate out of range at %08X: %s" % (pc, body))
        return S("ac%s = lsh(ac%s, %d)" % (args[1], args[1], k))
    if op == "WLSHI" and len(args) == 2 and REG.match(args[0]):    # :327 sext8 of the word
        v = imm_word(args[1])
        if v is not None:
            return S("ac%s = lsh(ac%s, %d)" % (args[0], args[0], sext(v & 0xFF, 8)))
    if op == "WMOVR" and len(args) == 1 and REG.match(args[0]):    # :161 uint32 >> 1
        return S("ac%s = lsh(ac%s, -1)" % (args[0], args[0]))
    if op == "WHLV" and len(args) == 1 and REG.match(args[0]):     # :157 int32 >> 1 (root ash: ovr += 0)
        return S("ac%s = ash(ac%s, -1)" % (args[0], args[0]))
    if op == "SEX" and x:
        return S("%s = sx16(%s)" % (y, x))
    if op == "ZEX" and x:
        return S("%s = zx16(%s)" % (y, x))
    if op == "WXCH" and x:
        t = ctx.newt()
        return S("%s = %s" % (t, x), "%s = %s" % (x, y), "%s = %s" % (y, t))
    if op == "LDAFP" and len(args) == 1 and REG.match(args[0]):    # EagleStack.cpp:527
        return S("ac%s = wfp" % args[0])
    if op == "LDASP" and len(args) == 1 and REG.match(args[0]):    # EagleStack.cpp:512
        return S("ac%s = wsp" % args[0])
    if op == "CRYTO" and not args:                                 # EagleGeneral.cpp:127
        return S("c = 1")
    # ---- bit-in-memory (EagleCompute.cpp WBTZ :257, WBTO :268): base =
    # XX==YY ? 0 : eagle_resolve_indirect(acX) [ind()], + (acY >>> 4);
    # bit = 0x8000 >> (acY & 15); the M16 index wrap is copy_segment.
    if op in ("WBTZ", "WBTO") and x:
        base = "0" if x == y else "ind(%s)" % x
        cell = "M16[%s + lsh(%s, -4)]" % (base, y)
        mask = "lsh(0x8000, 0 - (%s & 15))" % y
        if op == "WBTZ":
            return S("%s = %s & ~%s" % (cell, cell, mask))
        return S("%s = %s | %s" % (cell, cell, mask))

    # ---- terminators: skips (MathDesign §1/§2), XJMP, Nova tests, loops
    if op in SKIP_RR and x:
        fall, skip = ctx.skip_exits(pc)
        rhs = "0" if x == y else y                 # src=acX vs dst=acY; XX==YY: dst = 0
        return [], goto2(fall, skip, "(%s %s %s)" % (x, SKIP_RR[op], rhs))
    if op in SKIP_RI16 and len(args) == 2 and REG.match(args[0]):
        v = imm_word(args[1])
        if v is not None:
            fall, skip = ctx.skip_exits(pc)
            return [], goto2(fall, skip, "(ac%s %s %s)" % (args[0], SKIP_RI16[op], hexc(sext(v, 16))))
    if op in SKIP_RI32 and len(args) == 2 and REG.match(args[0]):
        m32 = IMM.match(args[1])                   # registerWideImmediate: dec (0xHHHHHHHH)
        if m32:
            fall, skip = ctx.skip_exits(pc)
            return [], goto2(fall, skip, "(ac%s %s %s)" % (args[0], SKIP_RI32[op], hexc(int(m32.group(2), 16))))
    if op == "WSZB" and x:                                          # :279
        fall, skip = ctx.skip_exits(pc)
        base = "0" if x == y else "ind(%s)" % x
        cell = "M16[%s + lsh(%s, -4)]" % (base, y)
        bit = "(lsh(%s, 0 - (15 - (%s & 15))) & 1)" % (cell, y)
        return [], goto2(fall, skip, "(%s == 0)" % bit)
    if op == "WSKBO" and len(args) == 1 and re.match(r"^\d+$", args[0]):   # :252
        n = int(args[0])
        if not 0 <= n <= 31:
            die("WSKBO bit out of range at %08X: %s" % (pc, body))
        fall, skip = ctx.skip_exits(pc)
        return [], goto2(fall, skip, "((lsh(ac0, %d) & 1) == 1)" % (-(31 - n)))
    if op == "XNISZ" and len(args) == 1:                            # :447
        e = parse_memop(pc, args[0], False)
        if e is not None:
            fall, skip = ctx.skip_exits(pc)
            return ["M16[%s] = (M16[%s] + 1) & 0xFFFF" % (e, e)], goto2(fall, skip, "(M16[%s] == 0)" % e)
    if op == "XJMP" and len(args) == 1:
        # Direct pc-relative only (ruling R2); indirect (@) stays embedded.
        mm = re.match(r"^\[pc\+0x([0-9A-Fa-f]+)\] \(0x([0-9A-Fa-f]{8})\)$", args[0])
        if mm:
            tgt = int(mm.group(2), 16)
            if (tgt & ~SEG_MASK) != seg_of(pc):
                die("XJMP target %08X outside block segment at %08X" % (tgt, pc))
            if tgt not in ctx.succs:
                die("XJMP target %08X not among successors of %08X" % (tgt, ctx.start))
            return [], "goto [%08X] 0" % tgt
    if (op in ("XNDO", "XWDO") or (op == "LNDO" and ctx.leftovers)) \
            and len(args) == 3 and REG.match(args[0]):   # EagleGeneral.cpp:167/:180/:225
        # P28: LNDO is XNDO with an L-form (wide) EA and a 4-word length —
        # same narrow_add helper, same test, fall-through pc+4 (:236).
        e = parse_memop(pc, args[2], op == "LNDO")
        if e is not None:
            arg = int(args[1])
            target = (pc + 1 + arg) & 0xFFFFFFFF
            s = ctx.succs
            nxt = ctx.next_pc(pc)
            if op == "LNDO" and nxt != pc + 4:
                die("LNDO at %08X: dis successor %08X is not pc+4" % (pc, nxt))
            if len(s) != 2 or s[0] != nxt or s[1] != target:
                die("%s at %08X: successors %s do not match [next, pc+1+%d]"
                    % (op, pc, ["%08X" % v for v in s], arg))
            acii = "ac" + args[0]
            width, f = (16, "nadd") if op in ("XNDO", "LNDO") else (32, "add")
            t1, t2 = ctx.newt(), ctx.newt()
            return (["%s = %s(M%d[%s], 1)" % (t1, f, width, e),
                     "M%d[%s] = %s" % (width, e, t1),
                     "%s = (%s >s %s)" % (t2, t1, acii),
                     "%s = %s" % (acii, t1)],
                    goto2(s[0], s[1], t2))
    if op == "LDSP" and ctx.leftovers and len(args) == 2 and REG.match(args[0]):
        # P28 (Census.md §4, option A1): EagleGeneral.cpp:251-260.  L/H and
        # the entries come from the dis's rendering of the table (LDSP_TABLES);
        # in range and entry != -1 -> that target, else fall through to pc+3
        # = the DERR 17 sink (terminal by ruling; its block stays an embedded
        # instruction = a verified terminal pair).  Out of range -> assert
        # (P27's detach pairing); a -1 entry -> the sink's label.
        mm = re.match(r"^\[pc\+0x([0-9A-Fa-f]+)\] \(0x([0-9A-Fa-f]{8})\)$", args[1])
        if not mm:
            die("LDSP at %08X: operand not a folded pc-relative table: %s" % (pc, body))
        tbl = int(mm.group(2), 16)
        if tbl not in LDSP_TABLES:
            die("LDSP at %08X: no table rendering for %08X in the dis" % (pc, tbl))
        lo, hi, targets = LDSP_TABLES[tbl]
        if len(targets) != hi - lo + 1:
            die("LDSP at %08X: table has %d entries for range [%d, %d]" % (pc, len(targets), lo, hi))
        sink = pc + 3
        if ctx.next_pc(pc) != sink:
            die("LDSP at %08X: dis successor is not pc+3" % pc)
        labels = [sink if t == 0xFFFFFFFF else t for t in targets]
        want = set(labels) | {sink}
        if set(ctx.succs) != want:
            die("LDSP at %08X: successors %s != table targets + sink %s"
                % (pc, ["%08X" % v for v in ctx.succs], ["%08X" % v for v in sorted(want)]))
        acx = "ac" + args[0]
        stmts = ['assert((%d <=s %s) && (%s <=s %d), "DERR 17 @%08X")' % (lo, acx, acx, hi, sink)]
        return stmts, "goto [%s] (%s - %d)" % (", ".join("%08X" % L for L in labels), acx, lo)
    nv = nova_test(body, ctx)
    if nv is not None:
        stmts, test = nv
        if test is None:                  # P28: load form without a skip
            return stmts, None
        if test == "1":
            # SKP: unconditional skip of the next word (the `ADC c,c,SKP` /
            # `WSUB c,c` idiom).  Follow lists ONE successor, pc+2 (the
            # skipped word is not a block start); canonical exit is
            # `goto [L] 0` (IR.md §3).  NovaCompute.cpp:70.
            if ctx.succs != [pc + 2]:
                die("SKP at %08X: successors %s != [pc+2]" % (pc, ["%08X" % x for x in ctx.succs]))
            return stmts, "goto [%08X] 0" % (pc + 2)
        fall, skip = ctx.skip_exits(pc)
        return stmts, goto2(fall, skip, test)

    # ---- loads/stores (P23) and byte addressing (P25), unchanged
    ldst = {"XWLDA": ("l", 32, False), "LWLDA": ("l", 32, True),
            "XNLDA": ("l", 16, False), "LNLDA": ("l", 16, True),
            "XWSTA": ("s", 32, False), "LWSTA": ("s", 32, True),
            "XNSTA": ("s", 16, False), "LNSTA": ("s", 16, True),
            "XLEF":  ("e", 0,  False), "LLEF":  ("e", 0,  True)}
    if op in ldst and len(args) == 2:
        kind, width, wide_form = ldst[op]
        e = parse_memop(pc, args[1], wide_form)
        if e is None:
            return None
        ac = "ac%s" % args[0]
        if kind == "e":
            return S("%s = %s" % (ac, e))
        cell = "M%d[%s]" % (width, e)
        if kind == "l":
            return S("%s = %s" % (ac, cell) if width == 32 else "%s = sx16(%s)" % (ac, cell))
        return S("%s = %s" % (cell, ac) if width == 32 else "%s = trunc16(%s)" % (cell, ac))
    bldst = {"XLEFB": ("e", False), "LLEFB": ("e", True),
             "XLDB":  ("l", False), "LLDB":  ("l", True),
             "XSTB":  ("s", False), "LSTB":  ("s", True)}
    if op in bldst and len(args) == 2:
        kind, wide_form = bldst[op]
        e = byte_ea(pc, args[1], wide_form)
        if e is None:
            return None
        ac = "ac%s" % args[0]
        if kind == "e":
            return S("%s = %s" % (ac, e))
        if kind == "l":
            return S("%s = M8[%s]" % (ac, e))
        return S("M8[%s] = zx8(%s)" % (e, ac))
    if op in ("WLDB", "WSTB") and len(args) == 2:
        ii, aa = args[0], args[1]
        if op == "WLDB":
            return S("ac%s = M8[ac%s]" % (aa, ii))
        return S("M8[ac%s] = zx8(ac%s)" % (ii, aa))
    return None

# ----------------------------------------------------------------- main

# ---------------------------------------------------------- P27 DERR folds

def parse_assumed_foldable(path, tags_path, dis_path):
    """guard_pc -> dict(derr, code, cont, interior[list], assert_text).
    Refuses unless the artifact's tags/dis sha256 match the inputs."""
    folds, tags_sha, dis_sha = {}, None, None
    for raw in open(path, encoding="ascii", errors="replace"):
        line = raw.rstrip("\r\n")
        if line.startswith("# tags sha256 "):
            tags_sha = line.split()[-1]
        elif line.startswith("# dis sha256 "):
            dis_sha = line.split()[-1]
        if not line or line.startswith("#"):
            continue
        head, _, evidence = line.partition(" | ")
        toks = head.split()
        if len(toks) < 4:
            die("assumed-foldable line unparseable: " + line)
        guard, derr, code, cont = int(toks[0], 16), int(toks[1], 16), toks[2], int(toks[3], 16)
        interior = [int(t, 16) for t in toks[4:]]
        m = re.search(r"\| (assert\(.*\))$", line)
        if not m:
            die("assumed-foldable line lacks the assert text: " + line)
        if guard in folds:
            die("assumed-foldable: duplicate guard %08X" % guard)
        folds[guard] = dict(derr=derr, code=code, cont=cont, interior=interior,
                            assert_text=m.group(1), skips=re.search(r"skips ([^|]*)", evidence).group(1).strip())
    if tags_sha is None or tags_sha != sha256(tags_path):
        die("assumed-foldable tags sha256 %s does not match %s (%s)" % (tags_sha, tags_path, sha256(tags_path)))
    if dis_sha is not None and dis_sha != sha256(dis_path):
        die("assumed-foldable dis sha256 does not match %s" % dis_path)
    return folds

GOTO2 = re.compile(r"^goto \[([0-9A-F]{8}), ([0-9A-F]{8})\] \((.*)\)$")

def skip_test(pc, block_start, dis, succs, dis_pcs, dis_index):
    """(fall, skip, test) of a skip instruction via lower_one itself, so the
    folded condition is exactly the terminator lower.py would have emitted.
    succs is keyed by BLOCK START; the guard sits at the end of its block,
    every interior skip is a block start of its own (split_skips)."""
    if block_start not in succs:
        die("fold: no CFG successors for block %08X (skip %08X)" % (block_start, pc))
    ctx = BlockCtx(block_start, succs[block_start], dis_pcs, dis_index, {})
    low = lower_one(pc, dis[pc], set(), ctx)
    if low is None or low[1] is None:
        die("fold: %08X is not a lowerable skip: %s" % (pc, dis[pc]))
    m = GOTO2.match(low[1])
    if not m:
        die("fold: %08X terminator is not a two-way goto: %s" % (pc, low[1]))
    return int(m.group(1), 16), int(m.group(2), 16), m.group(3)

def fold_condition(guard, guard_block, f, dis, succs, dis_pcs, dis_index):
    """Path condition to the continuation, transcribed from the skips
    (derr_clusters.py header rule): cond(K)=true, cond(DERR)=false,
    cond(skip) = t ? cond(skip-arm) : cond(fall-arm)."""
    derr, cont = f["derr"], f["cont"]
    members = set(f["interior"]) | {guard, derr}
    memo = {}
    def cond(pc):
        if pc == cont:
            return True
        if pc == derr:
            return False
        if pc not in members:
            die("fold %08X: path leaves the cluster at %08X" % (guard, pc))
        if pc in memo:
            return memo[pc]
        fall, skip, t = skip_test(pc, guard_block if pc == guard else pc, dis, succs, dis_pcs, dis_index)
        cf, cs = cond(fall), cond(skip)
        if cf is False and cs is True:
            r = "(%s)" % t
        elif cf is False:
            r = "(%s) && %s" % (t, cs)
        elif cs is False and cf is True:
            r = "!(%s)" % t
        elif cs is False:
            r = "!(%s) && %s" % (t, cf)
        else:
            die("fold %08X: both arms of %08X reach the continuation" % (guard, pc))
        memo[pc] = r
        return r
    r = cond(guard)
    if not isinstance(r, str):
        die("fold %08X: degenerate condition %r" % (guard, r))
    return r

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dis", required=True)
    ap.add_argument("--blocks", required=True)
    ap.add_argument("--pushmap", required=True)
    ap.add_argument("--argmap", required=True)
    ap.add_argument("--pilot", help="comma-separated block-start pcs (hex) to emit")
    ap.add_argument("--book", action="store_true",
                    help="emit call/argpush at decorated sites (M4 book runs "
                         "ONLY; the loader refuses book-mode IR in stock runs). "
                         "Default: stock — decorated sites stay instructions.")
    ap.add_argument("--all", action="store_true",
                    help="attempt every listed block; SKIP (with censused reason) "
                         "any that refuses — omission is safe: absent = emulated")
    ap.add_argument("--out", required=True)
    ap.add_argument("--assumed-foldable", help="P27 DERR cluster artifact (docs/Project27/assumed-foldable.txt)")
    ap.add_argument("--tags", help="quest.tags the artifact was built from (sha256 checked)")
    ap.add_argument("--synclist-in", help="identity sync list (quest.synclist.split)")
    ap.add_argument("--synclist-out", help="write the shipped list: identity minus folded interiors")
    ap.add_argument("--rt-slice", type=int, default=0,
                    help="P28 rt_call emission: 0 = off (RT LCALLs + pushes stay instructions), "
                         "1 = contiguous ?WRITE_SCREEN sites, 2 = every contiguous site, "
                         "3 = all sites (interleaved XLEF/XWSTA windows included)")
    ap.add_argument("--leftovers", action="store_true",
                    help="P28 leftovers: lower LNDO, the LDSP pair (assert + goto table) and the "
                         "Nova LOAD forms (pure; high half as the emulator leaves it)")
    ap.add_argument("--rt-census", help="write the per-site rt_call ledger (emitted / refused + reason)")
    a = ap.parse_args()
    global LDSP_TABLES
    LDSP_TABLES = parse_ldsp_tables(a.dis)
    if a.assumed_foldable and not a.tags:
        die("--assumed-foldable needs --tags")
    if bool(a.synclist_in) != bool(a.synclist_out):
        die("--synclist-in and --synclist-out go together")

    dis = parse_dis(a.dis)
    dis_pcs = sorted(dis)
    dis_index = {pc: i for i, pc in enumerate(dis_pcs)}
    blocks, succs = parse_blocks(a.blocks)
    pushmap_pcs, push_slot, call_site, borrow_pcs = parse_pushmap(a.pushmap)
    starts = sorted(blocks)
    folds = parse_assumed_foldable(a.assumed_foldable, a.tags, a.dis) if a.assumed_foldable else {}
    interior_of = {}                      # interior start -> guard
    for g, f in folds.items():
        if g not in dis_index:
            die("fold guard %08X not in dis" % g)
        if f["cont"] not in blocks:
            die("fold %08X: continuation %08X is not a block start" % (g, f["cont"]))
        for m in f["interior"]:
            if m not in blocks:
                die("fold %08X: interior %08X is not a block start" % (g, m))
            if m in interior_of:
                die("fold %08X: interior %08X shared with fold %08X" % (g, m, interior_of[m]))
            interior_of[m] = g
    if a.all:
        pilot = list(starts)
    elif a.pilot:
        pilot = [int(p, 16) for p in a.pilot.split(",")]
    else:
        die("need --pilot or --all")

    out = ["ir 4",
           "mode %s" % ("book" if a.book else "stock"),
           "source  %s sha256=%s" % (a.dis, sha256(a.dis)),
           "blocks  %s sha256=%s" % (a.blocks, sha256(a.blocks)),
           "pushmap %s sha256=%s" % (a.pushmap, sha256(a.pushmap)),
           "argmap  %s sha256=%s" % (a.argmap, sha256(a.argmap)), ""]
    census = {"expr": 0, "embed": 0, "argpush": 0, "call": 0, "ret": 0,
              "goto": 0, "assert": 0, "rt_call": 0, "last": None,
              "rt_sites": []}               # P28 ledger: (site, callee, argc, emitted, reason)

    # P27: a guard block is lowered with its fold; its interiors are held
    # back and emitted only if the guard REFUSES (totality: never a half
    # fold).  Guard blocks are found by their terminator pc = the guard.
    guard_block = {}
    bidx = 0
    for g in sorted(folds):
        while bidx + 1 < len(starts) and starts[bidx + 1] <= g:
            bidx += 1
        gb = starts[bidx]
        if gb not in blocks or dis_pcs[dis_index[gb] + len(blocks[gb]) - 1] != g:
            die("fold %08X: guard is not the last instruction of block %08X" % (g, gb))
        if gb in interior_of:
            die("fold %08X: guard block %08X is interior to fold %08X" % (g, gb, interior_of[gb]))
        if gb in guard_block:
            die("block %08X is the guard block of two folds" % gb)
        guard_block[gb] = g
    folded, unfolded = [], []
    held = set(interior_of) & set(pilot)

    skipped = {}
    for start in pilot:
        if start in held:
            continue
        mark = len(out)
        fold = folds[guard_block[start]] if start in guard_block else None
        try:
            emit_block(start, blocks, succs, dis, dis_pcs, dis_index,
                       pushmap_pcs, push_slot, call_site, borrow_pcs, starts,
                       out, census, a, fold, guard_block.get(start))
            if fold is not None:
                folded.append(guard_block[start])
        except Refuse as e:
            del out[mark:]                # drop partial block text
            reason = str(e).split(":")[0][:60]
            skipped.setdefault(reason, []).append(start)
            if fold is not None:
                unfolded.append(guard_block[start])
                sys.stderr.write("lower.py: FOLD REFUSED at guard %08X (interiors re-emitted, kept listed): %s\n"
                                 % (guard_block[start], e))
            if not a.all:
                sys.stderr.write("lower.py: REFUSE: %s\n" % e)
                sys.exit(1)
    for g in unfolded:                    # totality: the whole cluster stays fragmented
        for m in folds[g]["interior"]:
            if m in held:
                held.discard(m)
                emit_block(m, blocks, succs, dis, dis_pcs, dis_index,
                           pushmap_pcs, push_slot, call_site, borrow_pcs, starts,
                           out, census, a, None, None)
    nblocks = len(pilot) - sum(len(v) for v in skipped.values()) - len(held)
    out.append("blocks %d" % nblocks)
    with open(a.out, "w", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print("wrote %s: %d blocks, %d expr, %d instr, %d argpush, %d call, "
          "%d ret, %d goto, %d assert, %d rt_call, %d skipped"
          % (a.out, nblocks, census["expr"], census["embed"], census["argpush"],
             census["call"], census["ret"], census["goto"], census["assert"],
             census["rt_call"], sum(len(v) for v in skipped.values())))
    rts = census["rt_sites"]
    if rts:
        emitted = [r for r in rts if r[3]]
        refused = [r for r in rts if not r[3]]
        reasons = {}
        for r in refused:
            reasons.setdefault(r[4], []).append(r[0])
        print("  rt_sites: emitted=%d refused=%d (slice %d)" % (len(emitted), len(refused), a.rt_slice))
        for why, lst in sorted(reasons.items()):
            print("    refused %4d  %s  (e.g. %08X)" % (len(lst), why, lst[0]))
        if a.rt_census:
            with open(a.rt_census, "w", newline="\n") as f:
                f.write("# P28 rt_call ledger: site callee argc emitted reason  (lower.py --rt-slice %d)\n" % a.rt_slice)
                for site, callee, argc, ok, why in sorted(rts):
                    f.write("%08X %s %d %s %s\n" % (site, callee, argc, "emitted" if ok else "REFUSED", why or "-"))
    for reason, lst in sorted(skipped.items()):
        print("  skipped %4d  %s  (e.g. %08X)" % (len(lst), reason, lst[0]))
    if folds:
        print("  P27 folds: %d folded, %d refused (interiors kept), %d interior blocks delisted"
              % (len(folded), len(unfolded), len(held)))
    if a.synclist_out:
        identity = [l.rstrip("\r\n") for l in open(a.synclist_in)]
        entries = [int(l, 16) for l in identity if l and not l.startswith("#")]
        if set(entries) != set(blocks):
            die("--synclist-in is not the identity list of --blocks")
        keep = [e for e in entries if e not in held]
        with open(a.synclist_out, "w", newline="\n") as f:
            f.write("# Gen-6 sync list, P27 (docs/Project27/Census.md, ruling F1=A): identity minus\n")
            f.write("# the interior block starts (second skip, DERR) of every DERR cluster that\n")
            f.write("# lower.py actually folded.  %d entries = %d - %d.\n" % (len(keep), len(entries), len(held)))
            f.write("# tags   %s sha256=%s\n" % (a.tags, sha256(a.tags)))
            f.write("# blocks %s sha256=%s\n" % (a.blocks, sha256(a.blocks)))
            f.write("# folds  %s sha256=%s\n" % (a.assumed_foldable, sha256(a.assumed_foldable)))
            f.write("# ir     %s\n" % a.out)
            for e in keep:
                f.write("%08X\n" % e)
        print("  wrote %s: %d entries (%d delisted)" % (a.synclist_out, len(keep), len(held)))
    return


def emit_block(start, blocks, succs, dis, dis_pcs, dis_index,
               pushmap_pcs, push_slot, call_site, borrow_pcs, starts,
               out, census, a, fold=None, guard=None):
    if True:
        if start not in blocks:
            die("pilot %08X is not a quest.blocks start" % start)
        if start in EXCLUDED_BLOCKS:
            die("pilot %08X is on the exclusion list" % start)
        body_text = blocks[start]
        for t in body_text:
            if re.search(r"\b(ENQT|DEQUE)\b", t):
                die("pilot %08X contains ENQT/DEQUE — excluded" % start)
        out.append("block %08X seg 0x%08X" % (start, start & SEG_KEEP))
        # site expressibility: a decorated LCALL block lowers its pushes
        # only if EVERY decorated push of the site is in this block and
        # is an expressible word-form XPEF/LPEF (B-forms next tranche)
        body_pcs = set(dis_pcs[dis_index[start]:dis_index[start]+len(blocks[start])])
        lowerable_sites = {}
        for pc in body_pcs:
            if pc in call_site:
                marker, pushes = call_site[pc]
                ok = all(p in body_pcs for p in pushes)
                if ok:
                    for p in pushes:
                        if push_stores(p, dis[p], push_slot[p]) is None:
                            ok = False    # inexpressible push: site embeds
                            break
                if ok and not (dis[pc].startswith("LCALL") or dis[pc].startswith("XCALL")):
                    ok = False            # unknown decorated call opcode
                if not a.book:
                    ok = False            # stock emission: sites stay instructions
                # P25 (user ruling, Aug 29): borrow brackets no longer veto
                # the block — bracket WPSH/WPOPs are decorated pcs, so
                # lower_one embeds them as @addr instructions (all hooks
                # fire; the bracket's note_arg_write/pop nets zero and the
                # site's args= never counted it).
                lowerable_sites[pc] = ok
        if start not in dis_index:
            die("block start %08X has no disassembly line" % start)
        i0 = dis_index[start]
        ctx = BlockCtx(start, succs.get(start, []), dis_pcs, dis_index, borrow_pcs)
        ctx.leftovers = a.leftovers
        term = None
        nbody = len(body_text)
        # P28 rt_call: a game -> runtime LCALL is block-final (987/987).  Its
        # window is walked backward inside this block; when the site is
        # emittable in the requested slice, its push pcs fold into the
        # terminator and are not emitted on their own.
        rt_site, rt_push_pcs = None, set()
        block_pcs = dis_pcs[i0:i0 + nbody]
        if nbody and RT_LCALL.match(dis[block_pcs[-1]]):
            rt_site = rt_window(block_pcs[-1], block_pcs, dis)
            if rt_site is not None:
                if rt_site.refuse is None and rt_site.pc + 4 not in blocks:
                    rt_site.refuse = "site+4 is not a block start"
                if rt_site.refuse is None and succs.get(start, []) != [rt_site.pc + 4]:
                    rt_site.refuse = "block successor list is not [site+4]"
                emit_rt = rt_site.refuse is None and rt_slice_ok(rt_site, a.rt_slice)
                census["rt_sites"].append((rt_site.pc, rt_site.callee, rt_site.argc, emit_rt,
                                           rt_site.refuse or ("" if emit_rt else "outside --rt-slice %d" % a.rt_slice)))
                if emit_rt:
                    rt_push_pcs = {p[0] for p in rt_site.pushes}
                else:
                    rt_site = None
        for k, expected in enumerate(body_text):
            if i0 + k >= len(dis_pcs):
                die("disassembly ends inside block %08X" % start)
            pc = dis_pcs[i0 + k]           # dis adjacency IS the pc sequence
            if pc in XCT_SITES:
                die("pilot %08X contains XCT site %08X — excluded" % (start, pc))
            text = dis[pc]
            if text.rstrip() != expected.rstrip():
                die("dis/blocks text mismatch at %08X:\n  dis:    %s\n  blocks: %s"
                    % (pc, text, expected))
            if term is not None:
                die("terminator emitted before the last instruction in block %08X (at %08X)"
                    % (start, pc))
            handled = False
            if pc in rt_push_pcs:
                handled = True            # folded into the rt_call terminator (echoed there)
            elif rt_site is not None and pc == rt_site.pc:
                out.append("  %s ; %s  <- %s" % (rt_site.line(), text,
                           " ".join(p[1].split(";")[0].strip() + ";" for p in rt_site.pushes)))
                census["rt_call"] += 1
                census["last"] = "rt_call"
                term = rt_site.line()
                handled = True
            elif pc in push_slot:
                site = next((c for c, (mk, ps) in call_site.items() if pc in ps), None)
                if site is not None and lowerable_sites.get(site):
                    stores = push_stores(pc, text, push_slot[pc])
                    for i, s in enumerate(stores):
                        tail = " ; %s" % text if i == 0 else \
                               " ; ^ wide %d/%d" % (i + 1, len(stores))
                        out.append("  %s%s" % (s, tail))
                    census["argpush"] += len(stores)
                    census["last"] = "stmt"
                    handled = True
            elif pc in call_site and lowerable_sites.get(pc):
                marker, pushes = call_site[pc]
                m = (re.search(r"\(0x([0-9A-Fa-f]{8})\)", text)   # folded/resolved
                     or re.search(r"\[0x([0-9A-Fa-f]{8})\]", text))  # absolute
                if not m:
                    die("decorated call at %08X unparseable: %s" % (pc, text))
                wides_sum = sum(push_slot[p][1] for p in pushes)
                # ret = next dis pc after the call (adjacency; no length table)
                ridx = dis_index[pc] + 1
                ret = dis_pcs[ridx] if ridx < len(dis_pcs) else 0
                if ret == 0:
                    die("call at %08X has no successor in dis" % pc)
                out.append("  call %08X args=%d marker=%08X site=%08X ret=%08X ; %s"
                           % (int(m.group(1), 16), wides_sum, marker, pc, ret, text))
                census["call"] += 1
                census["last"] = "call"
                handled = True
            elif text.strip().rstrip(";").strip() == "WRTN":
                out.append("  ret ; WRTN;")
                census["ret"] += 1
                census["last"] = "ret"
                handled = True
            if not handled:
                low = lower_one(pc, text, pushmap_pcs, ctx)
                if low is None:
                    out.append("  @%08X %s" % (pc, text))
                    census["embed"] += 1
                    census["last"] = "instr"
                else:
                    stmts, term = low
                    for i, st in enumerate(stmts):
                        tail = " ; %s" % text if i == 0 else ""
                        out.append("  %s%s" % (st, tail))
                        census["assert" if st.startswith("assert(") else "expr"] += 1
                        census["last"] = "stmt"
                    if term is not None:
                        if k != nbody - 1:
                            die("skip/jump %08X is not the last instruction of block %08X"
                                % (pc, start))
                        if fold is not None and pc == guard:
                            # P27 fold (ruling A): assert + goto [K] 0.  The
                            # guard block's successors must all be cluster
                            # members or K; the condition is re-derived from
                            # the skips and must equal the artifact's text.
                            allowed = set(fold["interior"]) | {fold["derr"], fold["cont"]}
                            if not set(ctx.succs) <= allowed:
                                die("fold %08X: guard block successors %s leave the cluster"
                                    % (guard, ["%08X" % x for x in ctx.succs]))
                            cond = fold_condition(guard, start, fold, dis, succs, dis_pcs, dis_index)
                            stmt = 'assert(%s, "DERR %s @%08X")' % (cond, fold["code"], fold["derr"])
                            if stmt != fold["assert_text"]:
                                die("fold %08X: derived %s != artifact %s" % (guard, stmt, fold["assert_text"]))
                            out.append("  %s ; P27 fold of %s" % (stmt, fold["skips"]))
                            out.append("  goto [%08X] 0 ; continuation" % fold["cont"])
                            census["assert"] += 1
                            census["goto"] += 1
                            census["last"] = "goto"
                        else:
                            out.append("  %s ; %s" % (term, text))
                            census["goto"] += 1
                            census["last"] = "goto"
                    elif not stmts:
                        die("lower_one returned nothing for %08X" % pc)
        if ctx.slot_t:
            die("borrow bracket left open at block %08X end" % start)
        last_text = blocks[start][-1] if blocks[start] else ""
        wbr = re.match(r"^WBR .*\(0x([0-9A-Fa-f]{8})\);", last_text)
        if census["last"] == "stmt":
            fs = succs.get(start, [])
            if len(fs) != 1:
                die("block %08X ends in a lowered statement but has %d successors"
                    % (start, len(fs)))
            out.append("  goto [%08X] 0 ; fall-through" % fs[0])
            census["goto"] += 1
        elif census["last"] == "instr" and wbr:
            out[-1] = "  goto [%s] 0 ; %s" % (wbr.group(1), last_text)
            census["embed"] -= 1
            census["goto"] += 1
        out.append("")

    return 1

if __name__ == "__main__":
    main()
