#!/usr/bin/env python3
"""verify_frames.py — P45 re-derivation of the parent-frame slot witnesses.

Reads Disassembled/quest.dis directly.  For each NESTED child of a parent,
tracks which accumulator currently holds the STATIC LINK (the enclosing
frame pointer): it arrives in ac1 at entry (R42, corroborated by
M4aDesign.md's restore image: ac1 @ wfp-6) and is reloaded by
`XWLDA n,[ac3+0x7FFA]` (wp(fp,-6)).  A register stops holding the link when
it is written by any other instruction.  Every X-form reference whose base
is a link-holding register is an UPLEVEL reference; its displacement is a
parent-frame slot, its mnemonic width (N=16, W=32, XPEF=address) is the
width witness.

For the parent's OWN body, every `[ac3+d]` / `@[ac3+d]` reference while ac3
is believed to hold the frame (ac3 is cleared on `XWLDA 3,...`, `LWLDA 3`,
`WMOV x,3`, `WPOP 3,3`; restored on `LDAFP 3`/`WSAVS`/`WPSH 3,3;LDAFP 3`
sequences) is a direct witness of the parent's frame layout.

This is a linear scan, not a CFG walk: a link reloaded per block (R8) makes
that adequate, and every count is printed with its PCs so a human can check
it.  Displacements are printed as signed word offsets (0x7FFA -> -6).
"""
import re, sys, collections

DIS = sys.argv[1] if len(sys.argv) > 1 else "Disassembled/quest.dis"
LINE = re.compile(r"^([0-9a-f]{8}) (\S+?)(?: (.*?))?;")

def sdisp(hexs):
    v = int(hexs, 16)
    return v - 0x10000 if v >= 0x8000 else v

def parse(path):
    out = []
    for ln in open(path, errors="replace"):
        m = LINE.match(ln)
        if m:
            out.append((int(m.group(1), 16), m.group(2), m.group(3) or ""))
    return out

INS = parse(DIS)
IDX = {pc: i for i, (pc, _, _) in enumerate(INS)}

XREF = re.compile(r"(@?)\[ac([0-3])\+0x([0-9A-Fa-f]+)\]")

def dest_regs(mn, ops):
    """Registers written by this instruction (best effort, MV/Eagle)."""
    if mn in ("WSAVS", "WRTN", "DERR", "LCALL", "XCALL", "LJSR", "XJMP", "WBR",
              "XPEF", "XPEFB", "WPSH", "XWSTA", "XNSTA", "XLSTA", "LWSTA",
              "LNSTA", "WSTB", "WSTI", "WUGTI", "WSGT", "WSLE", "WSGE", "WSNE",
              "WSEQ", "WSLT", "WSGTI", "WSLEI", "WSEQI", "WSNEI", "WSKBO",
              "WSKBZ", "WSZB", "WSNB", "WBTO", "WBTZ", "WCMV", "WCMP", "WBLM",
              "SYSCALL", "XWSTAI"):
        return set()
    if mn == "LDAFP":
        return {int(ops.strip())}
    if mn == "WPOP":
        a, b = ops.split(",")
        a, b = int(a), int(b)
        return set(range(min(a, b), max(a, b) + 1)) if a <= b else {a, b}
    if mn in ("WMOV", "WADD", "WSUB", "WMUL", "WDIV", "WADC", "WAND", "WIOR",
              "WXOR", "WNEG", "WCOM", "WLSH", "WASH", "WLDB", "WSTB"):
        # two-register: source,dest
        parts = ops.split(",")
        return {int(parts[-1])} if mn != "WSTB" else set()
    m = re.match(r"(\d),", ops)
    if m and mn.startswith(("XW", "XN", "XL", "LW", "LN", "LL", "NLDAI",
                            "WLDAI", "WNADI", "WIORI", "WANDI", "WLDA",
                            "WLDB", "NLDA", "XLEF", "XLEFB", "WINC", "WDEC",
                            "WLSHI", "WASHI", "WSUBI", "WADDI", "WMULS",
                            "WDIVS", "WLOB", "WCMV", "CVWN", "WSTI")):
        return {int(m.group(1))}
    # Nova-style "MOV# a,b,skip" and "COM.# a,b" — the # form does not load
    if mn.endswith("#") or ".#" in mn:
        return set()
    m2 = re.match(r"\d,(\d)", ops)
    if m2 and mn in ("MOV", "MOVL", "MOV.L", "COM", "NEG", "INC", "ADC", "SUB",
                     "ADD", "AND", "MOVZR", "MOVZL", "MOVOR", "MOVR", "MOVS",
                     "ADCL", "NEGL", "INCL", "COML"):
        return {int(m2.group(1))}
    return set()

def scan_child(name, lo, hi, link_in_ac1=True):
    holds = {1} if link_in_ac1 else set()
    refs = []
    for pc, mn, ops in INS:
        if pc < lo or pc >= hi:
            continue
        if mn == "WSAVS":
            holds = {1} if link_in_ac1 else set()
            continue
        # link reload: XWLDA n,[ac3+0x7FFA] with ac3 = frame
        m = re.match(r"(\d),\[ac3\+0x7FFA\]", ops)
        if mn == "XWLDA" and m:
            d = int(m.group(1))
            holds = (holds - {d}) | {d}
            refs.append((pc, "LINK-LOAD", None, f"ac{d}"))
            continue
        # uplevel references through a link register
        for ind, base, disp in XREF.findall(ops):
            b = int(base)
            if b in holds:
                width = ("addr" if mn.startswith("XPEF") else
                         "16" if mn[1] == "N" else "32" if mn[1] == "W" else mn)
                refs.append((pc, mn, sdisp(disp), f"{ind}[ac{b}+{disp}]", width))
        # WMOV 1,2 copies the link (FIRE.1 wrinkle) / WMOV n,3 etc.
        if mn == "WMOV":
            s, d = (int(x) for x in ops.split(","))
            if s in holds:
                holds = holds | {d}
            else:
                holds = holds - {d}
            continue
        holds -= dest_regs(mn, ops)
    return refs

def scan_parent(name, lo, hi):
    ac3_is_fp = True
    refs = []
    for pc, mn, ops in INS:
        if pc < lo or pc >= hi:
            continue
        if mn in ("WSAVS", "LDAFP") and (mn == "WSAVS" or ops.strip() == "3"):
            ac3_is_fp = True
            continue
        if ac3_is_fp:
            for ind, base, disp in XREF.findall(ops):
                if base == "3":
                    width = ("addr" if mn.startswith("XPEF") else
                             "16" if mn[1] == "N" else "32" if mn[1] == "W" else mn)
                    refs.append((pc, mn, sdisp(disp), f"{ind}[ac3+{disp}]", width))
        if 3 in dest_regs(mn, ops):
            ac3_is_fp = False
    return refs

def report(title, refs):
    print(f"\n=== {title}")
    by = collections.defaultdict(list)
    for r in refs:
        if r[1] == "LINK-LOAD":
            continue
        by[r[2]].append(r)
    for d in sorted(by):
        rows = by[d]
        widths = collections.Counter(r[4] for r in rows)
        forms = collections.Counter(("indirect" if r[3].startswith("@") else "direct") for r in rows)
        print(f"  slot {d:+d}: {len(rows)} refs  widths={dict(widths)} forms={dict(forms)}")
        for r in rows:
            print(f"      {r[0]:08x} {r[1]} {r[3]}")
    nl = sum(1 for r in refs if r[1] == "LINK-LOAD")
    print(f"  link loads: {nl}")

if __name__ == "__main__":
    fam = [("FIRE.1", 0x7016A3BD, 0x7016A461),
           ("FIRE.2", 0x7016A461, 0x7016A4F8),
           ("FIRE.3", 0x7016A4F8, 0x7016AA35)]
    for n, lo, hi in fam:
        report(n, scan_child(n, lo, hi))
    report("FIRE (own body, ac3=fp)", scan_parent("FIRE", 0x70169D69, 0x7016A3BD))
    report("LIST_PLAYERS.3", scan_child("LIST_PLAYERS.3", 0x7016F556, 0x7016F5D0))
    report("LIST_PLAYERS (own body, ac3=fp)", scan_parent("LIST_PLAYERS", 0x7016EB87, 0x7016EC57))
