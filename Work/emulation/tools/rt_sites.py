#!/usr/bin/env python3
# rt_sites.py — Project 28 Phase A census (TEXT-ONLY; reads listings, runs
# nothing).  Three parts, matching docs/Project28/PROMPT.md Phase A:
#
#   1. the 987 game -> runtime call sites (`LCALL [..],argc; # ?NAME`):
#      callee, argc, the push window walked BACKWARD from the LCALL inside
#      the LCALL's quest.blocks.split block, the interleaved non-push
#      instructions, whether the window crosses the block start, the
#      argument expressions lower.py's pef_value renders for each push,
#      and whether each argument may be inlined at the call (pure in the
#      state at the LCALL) or needs a t-place (an interleaved instruction
#      writes a register the expression reads).  Arg order: the pushes are
#      argN first .. arg1 last (RTBridge::arg_pointer(n) = M32[wsp-2n] at
#      entry; the pushmap's `# XPEF arg2` line precedes `# LPEFB arg1`).
#   2. the callee side: for every callee, the routine body in quest-rt.dis
#      (entry to the next routine header) scanned for frame-slot register
#      reads/writes ([ac3+0x7FF8]=saved ac0, 0x7FFA=ac1, 0x7FFC=ac2 — the
#      WSAVR/WSAVS image, EagleStack.cpp:421-425: ac0 ac1 ac2 wfp ac3|c
#      above the LCALL marker), argument references (@[ac3+0xFFF4-2(n-1)]
#      = arg n), nested LCALLs, and the WSAVx frame kind.  Evidence for
#      docs/Project28/RTConventions.md; the hand-checked table cites it.
#   3. the P26 leftovers: LNDO sites, LDSP sites (table decoded from the
#      dis's VALID RANGE / JUMP TARGETS rendering and cross-checked against
#      quest.tags successor lists), and the Nova LOAD forms by
#      (op, CC, SS, KKK) shape.
#
# Refusal discipline (P25): a window that is not [pushes + XLEF/XWSTA], an
# argc/push-count mismatch, a window that leaves the block, an argument
# pef_value cannot render, an indirect (memory-reading) argument with an
# interleaved store — all REFUSE (kept in the census with the reason).
# Nothing here decides anything; it counts and reports.

import argparse, collections, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lower                       # pef_value, parse_dis, parse_blocks, Refuse, NOVA

PUSH_WORD = re.compile(r"^(X|L)PEF\s+(.+);$")
PUSH_BYTE = re.compile(r"^(X|L)PEFB\s+(.+);$")
WPSH = re.compile(r"^WPSH\s+([0-3]),([0-3]);$")
LCALL = re.compile(r"^LCALL\s+\[0x([0-9A-Fa-f]+)\],(\d+);\s*#\s*(\S+)")
XLEF = re.compile(r"^XLEF\s+([0-3]),(.+);$")
XWSTA = re.compile(r"^XWSTA\s+([0-3]),(.+);$")
MEMOP = re.compile(r"^(@?)\[(?:(pc|ac2|ac3)\+)?0x([0-9A-Fa-f]+)\](?: \(0x([0-9A-Fa-f:]+)\))?$")
MNEM = re.compile(r"^([A-Z][A-Z0-9.#]*)")

ALLOWED_INTERLEAVE = ("XLEF", "XWSTA")

# Which of the 987 sites' callees run natively today (hw/RTStubs.cpp
# translation_table; everything else is a logging stub over the emulated
# body).  Kept here so the census can print it; the citation is the table.
NATIVE_TODAY = {"?UNSIGNED_TO_CHAR", "?LIB_ERROR_CODE"}


def mnem(text):
    m = MNEM.match(text.strip())
    return m.group(1) if m else "?"


def memop_reads(operand):
    """(registers read, reads_memory) for a word/byte EA operand text."""
    m = MEMOP.match(operand.strip())
    if not m:
        return None, None
    at, base = m.group(1), m.group(2) or ""
    regs = {base} if base in ("ac2", "ac3") else set()
    return regs, bool(at)


def push_of(text):
    """Classify one instruction as a push: returns (kind, wides, regs_read,
    reads_memory) or None."""
    t = text.strip()
    m = WPSH.match(t)
    if m:
        x, a = int(m.group(1)), int(m.group(2))
        if a < x:
            lower.die("WPSH wraparound (never in QUEST — WPSH_WPOP.md): " + t)
        return "WPSH", a - x + 1, {"ac%d" % r for r in range(x, a + 1)}, False
    m = PUSH_BYTE.match(t)
    if m:
        regs, mem = memop_reads(m.group(2))
        return ("XPEFB" if m.group(1) == "X" else "LPEFB"), 1, regs, mem
    m = PUSH_WORD.match(t)
    if m:
        regs, mem = memop_reads(m.group(2))
        return ("XPEF" if m.group(1) == "X" else "LPEF"), 1, regs, mem
    return None


def index_blocks(blocks, dis_pcs, dis_index, dis):
    """Map every block's text body onto dis pcs (lower.py's own rule:
    consecutive dis pcs from the start) and return {pc: (start, pcs)}."""
    owner = {}
    for start, body in blocks.items():
        if start not in dis_index:
            lower.die("block start %08X not in dis" % start)
        i0 = dis_index[start]
        pcs = dis_pcs[i0:i0 + len(body)]
        if len(pcs) != len(body):
            lower.die("block %08X runs off the end of the dis" % start)
        for pc, text in zip(pcs, body):
            # sanity: the block text must be the dis text at that pc
            if dis[pc].split(";")[0].strip() != text.split(";")[0].strip():
                lower.die("block %08X text/dis mismatch at %08X: %r vs %r"
                          % (start, pc, text, dis[pc]))
            owner[pc] = (start, pcs)
    return owner


class Site:
    __slots__ = ("pc", "callee", "target", "argc", "block", "pushes",
                 "inter", "refuse", "crosses", "args", "needs_t", "shape")

    def __init__(self, pc, callee, target, argc):
        self.pc, self.callee, self.target, self.argc = pc, callee, target, argc
        self.block = None
        self.pushes = []        # [(pc, text, kind, wides, regs, mem)] in PROGRAM order (argN .. arg1)
        self.inter = []         # [(pc, text, mnemonic)] interleaved, program order
        self.refuse = None
        self.crosses = False
        self.args = []          # [(argno, expr, inline?)] arg1 .. argN
        self.needs_t = 0
        self.shape = ""


def census_sites(dis, dis_pcs, dis_index, owner):
    sites = []
    for pc in dis_pcs:
        m = LCALL.match(dis[pc])
        if not m or not m.group(3).startswith("?"):
            continue
        s = Site(pc, m.group(3), int(m.group(1), 16), int(m.group(2)))
        sites.append(s)
        if pc not in owner:
            s.refuse = "LCALL not in any block"
            continue
        start, pcs = owner[pc]
        s.block = start
        i = pcs.index(pc)
        wides = 0
        window = []             # walked backward
        j = i - 1
        while wides < s.argc and j >= 0:
            text = dis[pcs[j]]
            p = push_of(text)
            if p:
                wides += p[1]
                window.append((pcs[j], text, "push", p))
            else:
                window.append((pcs[j], text, "other", None))
            j -= 1
        if wides < s.argc:
            s.crosses = True
            s.refuse = "window crosses block start (only %d of %d wides in block)" % (wides, s.argc)
            continue
        if wides > s.argc:
            s.refuse = "WPSH overshoot: %d wides for argc %d" % (wides, s.argc)
            continue
        window.reverse()        # program order now
        # the window starts at its first PUSH; anything 'other' before the
        # first push was walked past only because we had not yet reached
        # the first push — it is not in the window.  But we walked
        # backward, so 'other' entries were only collected between pushes
        # or between the last push and the LCALL: all are interleaved.
        for wpc, text, kind, p in window:
            if kind == "push":
                s.pushes.append((wpc, text, p[0], p[1], p[2], p[3]))
            else:
                s.inter.append((wpc, text, mnem(text)))
        bad = [mn for _, _, mn in s.inter if mn not in ALLOWED_INTERLEAVE]
        if bad:
            s.refuse = "interleaved non-XLEF/XWSTA: " + ",".join(sorted(set(bad)))
        s.shape = "+".join(mn for _, _, mn in s.inter) or "contiguous"
        # argument expressions: pushes are argN..arg1 in program order.
        # Build arg1..argN by reversing.  A WPSH x,a with k wides supplies
        # k consecutive args: ac x pushed first (deepest), so ac a is the
        # lowest-numbered of its args.
        exprs = []              # program order, one per wide
        for wpc, text, kind, w, regs, mem in s.pushes:
            if kind == "WPSH":
                for r in sorted(regs):
                    exprs.append((wpc, "ac" + r[2:], {r}, False))
            else:
                try:
                    e = lower.pef_value(wpc, text)
                except lower.Refuse as ex:
                    e = None
                    if not s.refuse:
                        s.refuse = "pef_value refuses: " + str(ex).split(":")[0][:50]
                if e is None and not s.refuse:
                    s.refuse = "pef_value cannot render: " + text.split(";")[0]
                exprs.append((wpc, e, regs or set(), mem))
        # inline vs t-place: does any interleaved instruction AFTER the push
        # write a register the expression reads?  (XWSTA writes memory —
        # only an indirect (R[...]) argument can observe it: refuse those.)
        n = len(exprs)
        for k, (wpc, e, regs, mem) in enumerate(exprs):
            argno = n - k
            later = [(ipc, t, mn) for ipc, t, mn in s.inter if ipc > wpc]
            written = set()
            stores = 0
            for ipc, t, mn in later:
                mx = XLEF.match(t.strip())
                if mx:
                    written.add("ac" + mx.group(1))
                if XWSTA.match(t.strip()):
                    stores += 1
            inline = not (regs & written)
            if mem and stores and not s.refuse:
                s.refuse = "indirect argument with an interleaved store"
            if not inline:
                s.needs_t += 1
            s.args.append((argno, e, inline))
        s.args.reverse()        # arg1 .. argN
    return sites


# ------------------------------------------------------------- callee scan

RT_HDR = re.compile(r"^# (\S.*)$")
FRAME_RD = re.compile(r"^X[WN]LDA\s+([0-3]),\[ac3\+0x7FF([8ACE])\];")
FRAME_WR = re.compile(r"^X[WN]STA\s+([0-3]),\[ac3\+0x7FF([8ACE])\];")
ARG_REF = re.compile(r"@\[ac3\+0xFF([EF][0-9A-F])\]")
SLOT = {"8": "ac0", "A": "ac1", "C": "ac2", "E": "wfp"}


def scan_rt(rt_dis_path, callees):
    """callee name -> dict(entry, frame, reads, writes, args, calls, lines, body)"""
    lines = [l.rstrip("\r\n") for l in open(rt_dis_path, encoding="ascii", errors="replace")]
    # routine headers: '# ----' / '# NAME' / '# ----' then 'pc TEXT'
    hdr_at = {}                 # line index -> name
    for i, l in enumerate(lines):
        if l.startswith("# ") and not l.startswith("# ---") and i + 1 < len(lines) and lines[i + 1].startswith("# ---"):
            hdr_at[i] = l[2:].strip()
    hdr_idx = sorted(hdr_at)
    out = {}
    for name, entry in callees.items():
        # find the header whose following code starts at entry
        key = "%08x" % entry
        start_line = None
        for i, l in enumerate(lines):
            if l.lower().startswith(key + " "):
                start_line = i
                break
        if start_line is None:
            out[name] = None
            continue
        # body = from entry to the next routine header
        nxt = [h for h in hdr_idx if h > start_line]
        end_line = nxt[0] if nxt else len(lines)
        body = [l for l in lines[start_line:end_line] if re.match(r"^[0-9a-f]{8} ", l)]
        info = {"entry": entry, "frame": None, "reads": collections.Counter(),
                "writes": collections.Counter(), "args": collections.Counter(),
                "calls": [], "lines": len(body), "body": body,
                "argc_word": None}
        for l in body:
            pc, text = l[:8], l[9:]
            if info["frame"] is None and text.startswith("WSAV"):
                info["frame"] = text.split(";")[0]
            m = FRAME_RD.match(text)
            if m:
                info["reads"][SLOT[m.group(2)]] += 1
            m = FRAME_WR.match(text)
            if m:
                info["writes"][SLOT[m.group(2)]] += 1
            for a in ARG_REF.findall(text):
                # 0xFFF4 = -12 = arg1; each further arg is 2 words lower
                d = int(a, 16)          # 0xF4 = -12 = arg1; arg n is 2 words lower per n
                info["args"]["arg%d" % ((0xF4 - d) // 2 + 1)] += 1
            if "[ac3+0x7FF6]" in text or "[ac3+0xFFF6]" in text:
                info["argc_word"] = (info["argc_word"] or 0) + 1
            m = re.match(r"^LCALL\s+\[0x[0-9A-Fa-f]+\],(\d+);\s*#\s*(.*)$", text)
            if m:
                info["calls"].append(m.group(2).strip())
            if text.startswith("LJSR") or text.startswith("XJSR"):
                info["calls"].append(text.split(";")[0])
        out[name] = info
    return out


# ------------------------------------------------------------- leftovers

def leftovers(dis, dis_pcs, owner, tags_path):
    lndo = [(pc, dis[pc]) for pc in dis_pcs if dis[pc].startswith("LNDO")]
    ldsp = [(pc, dis[pc]) for pc in dis_pcs if dis[pc].startswith("LDSP")]
    # LDSP tables: the dis prints '<addr> VALID RANGE: [lo, hi]' then the
    # JUMP TARGETS list at the table address (folded operand).
    tags = {}
    if tags_path:
        for l in open(tags_path, encoding="ascii", errors="replace"):
            m = re.match(r"^([0-9A-Fa-f]{8}) n((?: [0-9A-Fa-f]{8})+)", l.rstrip())
            if m:
                tags[int(m.group(1), 16)] = [int(x, 16) for x in m.group(2).split()]
    return lndo, ldsp, tags


def ldsp_tables(dis_path, ldsp):
    """Decode the jump tables the dis prints for each LDSP folded target."""
    text = open(dis_path, encoding="ascii", errors="replace").read().replace("\r", "")
    out = {}
    for pc, ins in ldsp:
        m = re.match(r"^LDSP\s+([0-3]),\[pc\+0x([0-9A-Fa-f]+)\] \(0x([0-9A-Fa-f]+)\);", ins)
        if not m:
            out[pc] = None
            continue
        reg, tbl = int(m.group(1)), int(m.group(3), 16)
        mm = re.search(r"^%08x VALID RANGE: \[(-?\d+), (-?\d+)\]\n\s*JUMP TARGETS: ((?:.|\n)*?)\n\n" % tbl, text, re.M)
        if not mm:
            out[pc] = {"reg": reg, "table": tbl, "error": "table rendering not found"}
            continue
        lo, hi = int(mm.group(1)), int(mm.group(2))
        targets = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{8})", mm.group(3))]
        out[pc] = {"reg": reg, "table": tbl, "lo": lo, "hi": hi, "targets": targets}
    return out


def nova_loads(dis, dis_pcs, owner):
    shapes = collections.Counter()
    sites = []
    for pc in dis_pcs:
        if pc not in owner:
            continue
        body = dis[pc].split(";")[0].strip()
        m = lower.NOVA.match(body)
        if not m:
            continue
        op, carry, shift, noload, x, y, skip = m.groups()
        if noload:
            continue
        start, pcs = owner[pc]
        last = pcs[-1] == pc
        shapes[(op, carry or "-", shift or "-", skip or "-")] += 1
        sites.append((pc, body, start, last, bool(skip)))
    return shapes, sites


# ------------------------------------------------------------- report

def hx(v):
    return "%08X" % v


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dis", required=True)
    ap.add_argument("--blocks", required=True)
    ap.add_argument("--rt-dis", required=True)
    ap.add_argument("--tags", required=True)
    ap.add_argument("--sites-out", help="per-site TSV")
    ap.add_argument("--rt-bodies", help="write each callee's dis body here (dir)")
    a = ap.parse_args()

    dis = lower.parse_dis(a.dis)
    dis_pcs = sorted(dis)
    dis_index = {pc: i for i, pc in enumerate(dis_pcs)}
    blocks, succs = lower.parse_blocks(a.blocks)
    owner = index_blocks(blocks, dis_pcs, dis_index, dis)

    sites = census_sites(dis, dis_pcs, dis_index, owner)
    P = print

    # ---- Part 1
    P("## Part 1 — site census")
    P()
    P("sites: %d   (LCALL with a `# ?` callee comment, over %s)" % (len(sites), a.dis))
    ok = [s for s in sites if not s.refuse]
    P("argc == wides captured in-block: %d/%d   refused (any reason): %d   cross-block: %d"
      % (sum(1 for s in sites if s.block is not None and not s.crosses
             and sum(w for *_, w, _, _ in [(p[3], 0, 0) for p in s.pushes]) == s.argc), len(sites),
         len(sites) - len(ok), sum(1 for s in sites if s.crosses)))
    P()
    P("### by callee")
    P()
    P("| callee | sites | argc values | target | native today |")
    P("|---|---:|---|---|---|")
    byc = collections.defaultdict(list)
    for s in sites:
        byc[s.callee].append(s)
    for name in sorted(byc, key=lambda n: -len(byc[n])):
        ss = byc[name]
        argcs = collections.Counter(s.argc for s in ss)
        P("| %s | %d | %s | %08X | %s |" % (
            name, len(ss),
            ", ".join("%d (×%d)" % (k, v) for k, v in sorted(argcs.items())),
            ss[0].target, "yes" if name in NATIVE_TODAY else "no"))
    multi = [n for n in byc if len(set(s.argc for s in byc[n])) > 1]
    P()
    P("callees with more than one argc: %s" % (", ".join(sorted(multi)) or "none"))
    P()
    P("### by window shape (interleaved non-push mnemonics, program order)")
    P()
    shapes = collections.Counter(s.shape for s in sites)
    for sh, n in sorted(shapes.items(), key=lambda kv: -kv[1]):
        P("- %-24s %4d" % (sh, n))
    P()
    P("### push mnemonics")
    P()
    pm = collections.Counter()
    for s in sites:
        for _, _, kind, w, _, _ in s.pushes:
            pm[kind] += 1
    P(", ".join("%s %d" % kv for kv in sorted(pm.items())) + "   (total %d pushes, %d wides)"
      % (sum(pm.values()), sum(w for s in sites for _, _, _, w, _, _ in s.pushes)))
    P()
    P("### argument expression forms (all wides, arg1..argN)")
    P()
    forms = collections.Counter()
    for s in sites:
        for argno, e, inline in s.args:
            if e is None:
                forms["<unrenderable>"] += 1
            elif e.startswith("R["):
                forms["R[...] indirect"] += 1
            elif e.startswith("wp("):
                forms["wp(" + e[3:6] + ", d)"] += 1
            elif e.startswith("bp("):
                forms["bp(" + e[3:6] + ", d)"] += 1
            elif re.match(r"^ac[0-3]$", e):
                forms["acN (WPSH value)"] += 1
            elif re.match(r"^0x[0-9A-F]{8}:[01]$", e):
                forms["byte-pointer literal 0xW:b"] += 1
            elif re.match(r"^0x[0-9A-F]{8}$", e):
                forms["word constant"] += 1
            else:
                forms["other: " + e[:20]] += 1
    for f, n in sorted(forms.items(), key=lambda kv: -kv[1]):
        P("- %-30s %5d" % (f, n))
    P()
    P("### inline vs t-place")
    P()
    P("sites with every argument inline at the call: %d" % sum(1 for s in ok if s.needs_t == 0))
    P("sites needing t-places: %d (arguments needing a t: %d)"
      % (sum(1 for s in ok if s.needs_t), sum(s.needs_t for s in ok)))
    P()
    P("### refusals")
    P()
    rc = collections.Counter(s.refuse for s in sites if s.refuse)
    if not rc:
        P("none")
    for r, n in rc.items():
        P("- %d  %s  (e.g. %s)" % (n, r, hx(next(s.pc for s in sites if s.refuse == r))))
    P()
    P("### interleaved sites — what the interleaved instruction does")
    P()
    kinds = collections.Counter()
    for s in sites:
        for ipc, t, mn in s.inter:
            kinds[(s.callee, mn, t.split(";")[0].split(",")[0])] += 1
    for (c, mn, head), n in sorted(kinds.items(), key=lambda kv: -kv[1]):
        P("- %-24s %-6s %-12s %4d" % (c, mn, head, n))
    P()
    P("### worked examples")
    P()
    def show(s):
        P("    ; site %s  %s  argc %d  block %s  shape %s%s" % (
            hx(s.pc), s.callee, s.argc, hx(s.block) if s.block else "-", s.shape,
            ("  REFUSE: " + s.refuse) if s.refuse else ""))
        for wpc, t, kind, w, regs, mem in s.pushes:
            P("    %s %s" % (hx(wpc), t))
        for ipc, t, mn in s.inter:
            P("    %s %s   <- interleaved" % (hx(ipc), t))
        P("    %s %s" % (hx(s.pc), dis[s.pc]))
        P("    =>  rt_call %s(%s)" % (s.callee, ", ".join(
            (e or "<?>") + ("" if inline else " [t]") for _, e, inline in s.args)))
        P()
    seen = set()
    for s in sites:
        key = (s.callee, s.shape, s.needs_t > 0, bool(s.refuse))
        if key in seen:
            continue
        seen.add(key)
        show(s)

    if a.sites_out:
        with open(a.sites_out, "w") as f:
            f.write("# site\tcallee\targc\tblock\tshape\tneeds_t\trefuse\tpushes(program order)\targs(arg1..argN)\n")
            for s in sites:
                f.write("%s\t%s\t%d\t%s\t%s\t%d\t%s\t%s\t%s\n" % (
                    hx(s.pc), s.callee, s.argc, hx(s.block) if s.block else "-", s.shape,
                    s.needs_t, s.refuse or "-",
                    " | ".join("%s %s" % (hx(p), t.split(";")[0]) for p, t, *_ in s.pushes),
                    " | ".join("%s%s" % (e, "" if inl else "[t]") for _, e, inl in s.args)))

    # ---- Part 2
    P("## Part 2 — callee scan (quest-rt.dis; evidence, hand-checked in RTConventions.md)")
    P()
    callees = {name: byc[name][0].target for name in byc}
    rt = scan_rt(a.rt_dis, callees)
    P("| callee | entry | frame | lines | frame-slot reads (entry regs) | frame-slot writes (return regs) | arg refs | nested calls |")
    P("|---|---|---|---:|---|---|---|---|")
    for name in sorted(rt, key=lambda n: -len(byc[n])):
        i = rt[name]
        if i is None:
            P("| %s | %08X | (not found) | | | | | |" % (name, callees[name]))
            continue
        fmt = lambda c: ", ".join("%s×%d" % kv for kv in sorted(c.items())) or "-"
        P("| %s | %08X | %s | %d | %s | %s | %s | %s |" % (
            name, i["entry"], i["frame"] or "-", i["lines"], fmt(i["reads"]), fmt(i["writes"]),
            fmt(i["args"]), ", ".join(sorted(set(i["calls"]))) or "-"))
    if a.rt_bodies:
        os.makedirs(a.rt_bodies, exist_ok=True)
        for name, i in rt.items():
            if i:
                with open(os.path.join(a.rt_bodies, name.replace("?", "Q") + ".dis"), "w") as f:
                    f.write("\n".join(i["body"]) + "\n")
    P()

    # ---- Part 3
    P("## Part 3 — leftovers")
    P()
    lndo, ldsp, tags = leftovers(dis, dis_pcs, owner, a.tags)
    P("### LNDO (%d)" % len(lndo))
    for pc, t in lndo:
        st = owner.get(pc, (None, []))[0]
        P("- %s %s   block %s   last-in-block %s" % (hx(pc), t, hx(st) if st else "-",
                                                    owner[pc][1][-1] == pc if pc in owner else "-"))
    P()
    P("### LDSP (%d)" % len(ldsp))
    tbls = ldsp_tables(a.dis, ldsp)
    for pc, t in ldsp:
        info = tbls[pc]
        st = owner.get(pc, (None, []))[0]
        P("- %s %s   block %s   last-in-block %s" % (hx(pc), t, hx(st) if st else "-",
                                                    owner[pc][1][-1] == pc if pc in owner else "-"))
        if not info or "error" in info:
            P("  table: %s" % (info and info["error"]))
            continue
        tg = info["targets"]
        valid = [x for x in tg if x != 0xFFFFFFFF]
        P("  index ac%d, table %08X, range [%d, %d] -> %d entries, %d valid, %d -1 (fall to DERR at pc+3 = %s)"
          % (info["reg"], info["table"], info["lo"], info["hi"], len(tg), len(valid),
             len(tg) - len(valid), hx(pc + 3)))
        if len(tg) != info["hi"] - info["lo"] + 1:
            P("  !! entry count %d != hi-lo+1 %d" % (len(tg), info["hi"] - info["lo"] + 1))
        succ = tags.get(pc, [])
        want = []
        for x in valid + [pc + 3]:
            if x not in want:
                want.append(x)
        P("  quest.tags successors: %d; table's distinct valid targets + fall-through: %d; sets %s"
          % (len(succ), len(want), "MATCH" if set(succ) == set(want) else "DIFFER"))
        if set(succ) != set(want):
            P("    tags-only: %s" % " ".join(hx(x) for x in sorted(set(succ) - set(want))))
            P("    table-only: %s" % " ".join(hx(x) for x in sorted(set(want) - set(succ))))
        nb = [x for x in want if x not in blocks]
        P("  every target a block start: %s" % ("yes" if not nb else "NO: " + " ".join(hx(x) for x in nb)))
    P()
    shapes, nsites = nova_loads(dis, dis_pcs, owner)
    P("### Nova LOAD forms (%d, in blocks.split blocks)" % len(nsites))
    P()
    P("| op | CC | SS | skip | count |")
    P("|---|---|---|---|---:|")
    for (op, cc, ss, sk), n in sorted(shapes.items(), key=lambda kv: (-kv[1], kv[0])):
        P("| %s | %s | %s | %s | %d |" % (op, cc, ss, sk, n))
    P()
    withskip = [x for x in nsites if x[4]]
    P("with a skip: %d (all block-terminal: %s); without: %d (block-terminal by coincidence: %d)"
      % (len(withskip), all(x[3] for x in withskip),
         len(nsites) - len(withskip), sum(1 for x in nsites if not x[4] and x[3])))
    ops = collections.Counter(x[1].split()[0].split(".")[0].rstrip("#") for x in nsites)
    P("by op: " + ", ".join("%s %d" % kv for kv in sorted(ops.items())))
    P()
    P("sites:")
    for pc, body, start, last, sk in nsites:
        P("- %s %-22s block %s%s" % (hx(pc), body, hx(start), "  (terminal)" if last else ""))


if __name__ == "__main__":
    try:
        main()
    except lower.Refuse as e:
        sys.stderr.write("rt_sites.py: REFUSE: %s\n" % e)
        sys.exit(1)
