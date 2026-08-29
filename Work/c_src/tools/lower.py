#!/usr/bin/env python3
# P23 lower.py — quest.dis + quest.blocks (+ pushmap, argmap) -> quest.ir
# Spec: docs/Project23/IRPhase1.md. Register-faithful 1:1, class-capped,
# no temps, no folding. Everything not in the cap is an embedded
# statement. TOTAL: an all-embed block is valid output.
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
        e = "%s + %d" % (base, disp) if disp >= 0 else "%s - %d" % (base, -disp)
    if indirect:
        e = "R[%s]" % e
    return e, indirect

# [base+0xHHHH] with optional @ prefix and optional " (0xADDR)" fold
MEMOP = re.compile(r"^(@?)\[(?:(pc|ac2|ac3)\+)?0x([0-9A-Fa-f]+)\](?: \(0x([0-9A-Fa-f]+)\))?$")

def parse_memop(pc, text, wide):
    m = MEMOP.match(text.strip())
    if not m:
        return None
    at, base, disp, folded = m.group(1), m.group(2) or "", int(m.group(3), 16), m.group(4)
    folded = int(folded, 16) if folded else None
    e, indirect = ea_expr(pc, base, disp, wide, folded)
    if bool(at) != indirect:
        die("@-prefix vs indirect bit disagree at %08X: %s" % (pc, text))
    return e

def pef_value(pc, text):
    """XPEF/LPEF operand -> the pushed EA as a VALUE expression (explicit
    segment wrap; R[...] result used raw for indirect). None if not
    expressible."""
    m = re.match(r"^(X|L)PEF\s+(\S+);", text.strip())
    if not m:
        return None
    wide = m.group(1) == "L"
    mm = MEMOP.match(m.group(2))
    if not mm:
        return None
    at, base, disp, folded = mm.group(1), mm.group(2) or "", int(mm.group(3), 16), mm.group(4)
    folded = int(folded, 16) if folded else None
    e, indirect = ea_expr(pc, base, disp, wide, folded)
    if bool(at) != indirect:
        die("@-prefix vs indirect bit disagree at %08X: %s" % (pc, text))
    if indirect:
        return e                      # R[...] resolves to a full address
    if base in ("", "pc"):
        return e                      # already an absolute constant
    return "((%s) & 0x0FFFFFFF) | 0x%08X" % (e, pc & SEG_KEEP)

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
                        if push_slot[p][1] != 1 or pef_value(p, dis[p]) is None:
                            ok = False    # WPSH/multi-wide: next tranche
                            break
                if ok and not (dis[pc].startswith("LCALL") or dis[pc].startswith("XCALL")):
                    ok = False            # unknown decorated call opcode
                if not a.book:
                    ok = False            # stock emission: sites stay instructions
                if body_pcs & borrow_pcs:
                    ok = False            # borrow-decorated block: next tranche
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
                    v = pef_value(pc, text)
                    out.append("  M32[0x%08X] = %s ; %s" % (push_slot[pc][0], v, text))
                    census["argpush"] += 1
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
