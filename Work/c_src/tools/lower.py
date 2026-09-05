#!/usr/bin/env python3
# lower.py — quest.dis + quest.blocks (+ pushmap, argmap) -> quest.ir (ir 3)
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
    if not noload:
        return None                       # load form: deferred
    if not skip:
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
    if op in ("XNDO", "XWDO") and len(args) == 3 and REG.match(args[0]):   # EagleGeneral.cpp:167/:180
        e = parse_memop(pc, args[2], False)
        if e is not None:
            arg = int(args[1])
            target = (pc + 1 + arg) & 0xFFFFFFFF
            s = ctx.succs
            if len(s) != 2 or s[0] != ctx.next_pc(pc) or s[1] != target:
                die("%s at %08X: successors %s do not match [next, pc+1+%d]"
                    % (op, pc, ["%08X" % v for v in s], arg))
            acii = "ac" + args[0]
            width, f = (16, "nadd") if op == "XNDO" else (32, "add")
            t1, t2 = ctx.newt(), ctx.newt()
            return (["%s = %s(M%d[%s], 1)" % (t1, f, width, e),
                     "M%d[%s] = %s" % (width, e, t1),
                     "%s = (%s >s %s)" % (t2, t1, acii),
                     "%s = %s" % (acii, t1)],
                    goto2(s[0], s[1], t2))
    nv = nova_test(body, ctx)
    if nv is not None:
        stmts, test = nv
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
    a = ap.parse_args()

    dis = parse_dis(a.dis)
    dis_pcs = sorted(dis)
    dis_index = {pc: i for i, pc in enumerate(dis_pcs)}
    blocks, succs = parse_blocks(a.blocks)
    pushmap_pcs, push_slot, call_site, borrow_pcs = parse_pushmap(a.pushmap)
    starts = sorted(blocks)
    if a.all:
        pilot = list(starts)
    elif a.pilot:
        pilot = [int(p, 16) for p in a.pilot.split(",")]
    else:
        die("need --pilot or --all")

    out = ["ir 3",
           "mode %s" % ("book" if a.book else "stock"),
           "source  %s sha256=%s" % (a.dis, sha256(a.dis)),
           "blocks  %s sha256=%s" % (a.blocks, sha256(a.blocks)),
           "pushmap %s sha256=%s" % (a.pushmap, sha256(a.pushmap)),
           "argmap  %s sha256=%s" % (a.argmap, sha256(a.argmap)), ""]
    census = {"expr": 0, "embed": 0, "argpush": 0, "call": 0, "ret": 0,
              "goto": 0, "last": None}

    skipped = {}
    for start in pilot:
        mark = len(out)
        try:
            emit_block(start, blocks, succs, dis, dis_pcs, dis_index,
                       pushmap_pcs, push_slot, call_site, borrow_pcs, starts,
                       out, census, a)
        except Refuse as e:
            del out[mark:]                # drop partial block text
            reason = str(e).split(":")[0][:60]
            skipped.setdefault(reason, []).append(start)
            if not a.all:
                sys.stderr.write("lower.py: REFUSE: %s\n" % e)
                sys.exit(1)
    nblocks = len(pilot) - sum(len(v) for v in skipped.values())
    out.append("blocks %d" % nblocks)
    with open(a.out, "w", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print("wrote %s: %d blocks, %d expr, %d instr, %d argpush, %d call, "
          "%d ret, %d goto, %d skipped"
          % (a.out, nblocks, census["expr"], census["embed"], census["argpush"],
             census["call"], census["ret"], census["goto"],
             sum(len(v) for v in skipped.values())))
    for reason, lst in sorted(skipped.items()):
        print("  skipped %4d  %s  (e.g. %08X)" % (len(lst), reason, lst[0]))
    return


def emit_block(start, blocks, succs, dis, dis_pcs, dis_index,
               pushmap_pcs, push_slot, call_site, borrow_pcs, starts,
               out, census, a):
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
        term = None
        nbody = len(body_text)
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
            if pc in push_slot:
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
                        census["expr"] += 1
                        census["last"] = "stmt"
                    if term is not None:
                        if k != nbody - 1:
                            die("skip/jump %08X is not the last instruction of block %08X"
                                % (pc, start))
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
