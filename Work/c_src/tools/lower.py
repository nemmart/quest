#!/usr/bin/env python3
# lower.py — quest.dis + quest.blocks (+ pushmap, argmap) -> quest.ir
#
# THE SPEC IS docs/IR.md (consolidated, normative; spec-wins).  This
# emitter implements it — grammar, wp/bp/M8 semantics, WPSH group
# stores, @addr borrow brackets, the byte-EA per-mode table — and any
# emitter/spec disagreement is a bug in one of them, to be reported,
# not papered over.  Byte-EA derivation record + the disassembler
# byte-operand defect this parser compensates for:
# docs/Project25/ByteEA.md and docs/DISASSEMBLER_BYTE_OPERANDS.md.
# (Historical phase-1 rationale: docs/Project23/IRPhase1.md.)
#
# Register-faithful 1:1, class-capped, no temps, no folding.
# Everything not in the cap is an embedded statement.  TOTAL: an
# all-embed block is valid output.
#
# Phase 1 cap (spec §4): NLDAI WLDAI / X,L x N,W LDA/STA (modes 0-3,
# direct or @-indirect via R) / XLEF LLEF / WMOV / WADD WSUB WADDI WSBI.
# The #-ops carry NO formulas here — IRExec calls the same
# EagleInstruction helpers the emulated instructions call (user ruling,
# Aug 28 2026, docs/Project23/WideCarry.md). lower.py only CLASSIFIES.
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
    """Returns (decorated_pcs, push_slot{pc->slot}, call_site{pc->(marker,[push_pcs])}).
    Push lines bind to the next call line (the file is grouped per site)."""
    pcs, push_slot, call_site, borrow_pcs = set(), {}, {}, set()
    pending = []
    for raw in open(path, encoding="ascii", errors="replace"):
        line = raw.split("#")[0].strip()
        mb = re.match(r"^borrow ([0-9A-Fa-f]{8}) ", line)
        if mb:
            pcs.add(int(mb.group(1), 16))
            borrow_pcs.add(int(mb.group(1), 16))
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
        return "0x%08X" % value
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
        return "0x%08X" % value
    # register base
    if wide:
        return "%s*2 + 0x%08X" % (base, disp & 0xFFFFFFFF)
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

IMM = re.compile(r"^(-?\d+) \(0x([0-9A-Fa-f]+)\)$")

def lower_one(pc, text, pushmap_pcs):
    """Return IR statement body (without @pc) or None -> embed."""
    if pc in pushmap_pcs:
        return None                      # decorated site: embed (or future argstore)
    body = text.rstrip(";").strip()
    m = re.match(r"^(\S+)\s*(.*)$", body)
    if not m:
        return None
    op, rest = m.group(1), m.group(2)
    args = [a.strip() for a in rest.split(",")] if rest else []

    if op == "NLDAI" and len(args) == 2:
        im = IMM.match(args[0])
        if im:
            return "ac%s = 0x%08X" % (args[1], sext(int(im.group(2), 16), 16) & 0xFFFFFFFF)
    if op == "WLDAI" and len(args) == 2:
        m32 = re.match(r"^0x([0-9A-Fa-f]{1,8})$", args[1])
        if m32:                       # WLDAI <ac>,<0xvalue> — note: ac FIRST
            return "ac%s = 0x%08X" % (args[0], int(m32.group(1), 16) & 0xFFFFFFFF)
    if op == "WMOV" and len(args) == 2:
        return "ac%s = ac%s" % (args[1], args[0])
    if op in ("WADD", "WSUB") and len(args) == 2:
        o = "#+" if op == "WADD" else "#-"
        return "ac%s = ac%s %s ac%s" % (args[1], args[1], o, args[0])
    if op == "WSBI" and len(args) == 2:
        return "ac%s = ac%s #- %s" % (args[1], args[1], args[0])
    if op == "WADDI" and len(args) == 2:
        im = IMM.match(args[1])           # WADDI <ac>,<value (0xhex)> — ac FIRST
        if im:
            return "ac%s = ac%s #+ 0x%08X" % (args[0], args[0], int(im.group(2), 16) & 0xFFFFFFFF)

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
            return "%s = %s" % (ac, e)
        cell = "M%d[%s]" % (width, e)
        if kind == "l":
            return "%s = %s" % (ac, cell) if width == 32 else "%s = sx16(%s)" % (ac, cell)
        return "%s = %s" % (cell, ac) if width == 32 else "%s = trunc16(%s)" % (cell, ac)

    # ---- P25 byte addressing (Machine::eagle_{x,l}_byte_indexed,
    # Memory::read_byte/write_byte; see byte_ea for the semantics).
    # M8 reads return the byte zero-extended; M8 stores write value&0xFF
    # (audit trail zx8).  M8 indices are RAW — byte pointers carry their
    # own segment (bits 31:29); the hardware applies no wrap at use.
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
            return "%s = %s" % (ac, e)
        if kind == "l":
            return "%s = M8[%s]" % (ac, e)
        return "M8[%s] = zx8(%s)" % (e, ac)
    if op in ("WLDB", "WSTB") and len(args) == 2:
        # registerRegister renders II,AA (Disassembler.java: bits 14:13
        # then 12:11; EagleGeneral::setup: AA=12:11, II=14:13).
        # WLDB: ac[AA] = read_byte(ac[II]); WSTB: write_byte(ac[II], ac[AA]&0xFF)
        ii, aa = args[0], args[1]
        if op == "WLDB":
            return "ac%s = M8[ac%s]" % (aa, ii)
        return "M8[ac%s] = zx8(ac%s)" % (ii, aa)
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

    out = ["ir 2",
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
                stmt = lower_one(pc, text, pushmap_pcs)
                if stmt is None:
                    out.append("  @%08X %s" % (pc, text))
                    census["embed"] += 1
                    census["last"] = "instr"
                else:
                    out.append("  %s ; %s" % (stmt, text))
                    census["expr"] += 1
                    census["last"] = "stmt"
        last_text = blocks[start][-1] if blocks[start] else ""
        wbr = re.match(r"^WBR .*\(0x([0-9A-Fa-f]{8})\);", last_text)
        if census["last"] == "stmt":
            fs = succs.get(start, [])
            if len(fs) != 1:
                die("block %08X ends in a lowered statement but has %d successors"
                    % (start, len(fs)))
            out.append("  goto %08X ; fall-through" % fs[0])
        elif census["last"] == "instr" and wbr:
            out[-1] = "  goto %s ; %s" % (wbr.group(1), last_text)
            census["embed"] -= 1
            census["goto"] = census.get("goto", 0) + 1
        out.append("")

    return 1

if __name__ == "__main__":
    main()
