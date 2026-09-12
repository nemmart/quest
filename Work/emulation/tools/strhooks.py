#!/usr/bin/env python3
"""tools/strhooks.py — generate quest.strhooks, the P33-A checker hook table
(docs/Project29/StringsDesign.md §6.6; docs/Project33/REPORT.md).

Inputs: the P31 regenerated census raw tables (## WMSP claims / ## STASP
releases), quest.blocks.split (to find the block containing each group's
first WMSP — that block address IS the identity of p@b), and quest.dis
(to confirm every hooked pc is a WMSP/STASP in the listing; the loader
re-checks by decoding the word at launch).

One row per claim-group block (19), sorted by block address. The arena
LAYOUT is not here: P33-B moved it to quest.arena (tools/arena.py), one
temp per claim — s@<block>.<k> — because every master temp gets its own
arena twin (ruling, Sep 6 2026: exact by construction beats the liveness
argument that INIT_OBJ_TBL's group breaks). P33-A's provisional
arena=/cap= columns are gone.

Usage: strhooks.py --census <census_raw.txt> --blocks <quest.blocks.split>
                   --dis <quest.dis> --out <quest.strhooks>
"""
import argparse, bisect, hashlib, re, sys

ONPOP_PC = 0x7017EC9F      # I.GOTO landing stub: XWLDA 0,[ac3+2] (7017EC9D,2 words); STASP 0
# WRTNs that are unwind CUTS, not returns: they discard the frames above the
# one they pop, outstanding claims included (a signal raised from the
# ?WRITE_SCREEN inside a group). The hook unmaps/erases with the >= rule
# and does NOT assert delta == 0 for them.
UNWIND_WRTN = [0x7017EC9C,   # I.GOTO, live shape (STAFP 2; WRTN through the cursor frame)
               0x7017ECBC,   # I.GOTO, cross-segment shape (never observed live)
               0x7017EF88]   # R?SIGNAL/?ERROR root-frame return (P33-A finding; reachable via DEF?ON)


def sha16(path):
    return hashlib.sha256(open(path, 'rb').read()).hexdigest()[:16]


def load_census(path):
    claims = []   # (pc, routine, size_expr, sp_block)
    releases = [] # (pc, routine, sp_block, [claim pcs])
    sec = None
    for line in open(path):
        line = line.rstrip('\n')
        if line.startswith('## WMSP claims'):
            sec = 'c'; continue
        if line.startswith('## STASP releases'):
            sec = 'r'; continue
        if line.startswith('## '):
            sec = None; continue
        m = re.match(r'^([0-9A-F]{8}) (\S+)\s+(.*)$', line)
        if not m:
            continue
        pc, routine, rest = int(m.group(1), 16), m.group(2), m.group(3)
        if sec == 'c':
            sz = re.search(r'size\(wides\)=(.*?)\s+wsp_before=', rest)
            sp = re.search(r'wsp_before=\(?sp@([0-9A-F]{8})', rest)
            if not sz or not sp:
                sys.exit(f'strhooks: unparsable claim line: {line}')
            claims.append((pc, routine, sz.group(1).strip(), int(sp.group(1), 16)))
        elif sec == 'r':
            sp = re.search(r'wsp:=sp@([0-9A-F]{8})', rest)
            cl = re.search(r'claims on path: (.*)$', rest)
            if not sp or not cl:
                sys.exit(f'strhooks: unparsable release line: {line}')
            releases.append((pc, routine, int(sp.group(1), 16),
                             [int(x, 16) for x in cl.group(1).split(', ')]))
    return claims, releases


def load_blocks(path):
    starts = set()
    for line in open(path):
        m = re.match(r'^([0-9A-Fa-f]{8}):', line)
        if m:
            starts.add(int(m.group(1), 16))
    return sorted(starts)


def load_dis(path):
    mn = {}
    for line in open(path):
        m = re.match(r'^([0-9a-fA-F]{8}) ([A-Z.?]+)', line)
        if m:
            mn[int(m.group(1), 16)] = m.group(2)
    return mn


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--census', required=True)
    ap.add_argument('--blocks', required=True)
    ap.add_argument('--dis', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()

    claims, releases = load_census(a.census)
    blocks = load_blocks(a.blocks)
    dis = load_dis(a.dis)

    def block_of(pc):
        i = bisect.bisect_right(blocks, pc) - 1
        if i < 0:
            sys.exit(f'strhooks: pc {pc:08X} precedes every block')
        return blocks[i]

    claim_by_pc = {c[0]: c for c in claims}
    if len(claim_by_pc) != len(claims):
        sys.exit('strhooks: duplicate WMSP pc in census')

    # Groups: one per STASP, its claims-on-path (census order = execution order).
    groups = []
    seen = set()
    for pc, routine, sp, cl in releases:
        for c in cl:
            if c not in claim_by_pc:
                sys.exit(f'strhooks: release {pc:08X} names unknown claim {c:08X}')
            if c in seen:
                sys.exit(f'strhooks: claim {c:08X} in two groups')
            seen.add(c)
        blk = block_of(cl[0])
        for c in cl:
            if block_of(c) != blk:
                sys.exit(f'strhooks: group {blk:08X}: claim {c:08X} is in another block '
                         f'({block_of(c):08X}) — StringsDesign §1.3 violated')
        groups.append((blk, routine, cl, pc))
    if seen != set(claim_by_pc):
        sys.exit('strhooks: a claim is released by no STASP')
    groups.sort()
    if len(set(g[0] for g in groups)) != len(groups):
        sys.exit('strhooks: two groups share a block — p@b identity is the block')

    for pc in list(claim_by_pc) + [r[0] for r in releases]:
        if dis.get(pc) not in ('WMSP', 'STASP'):
            sys.exit(f'strhooks: {pc:08X} is {dis.get(pc)} in the listing, not WMSP/STASP')
    for pc in claim_by_pc:
        if dis[pc] != 'WMSP':
            sys.exit(f'strhooks: claim {pc:08X} is not a WMSP')
    for r in releases:
        if dis[r[0]] != 'STASP':
            sys.exit(f'strhooks: release {r[0]:08X} is not a STASP')

    out = []
    out.append('# quest.strhooks — P33-A checker hook table (StringsDesign.md §6.6; docs/Project33/REPORT.md)')
    out.append(f'# generated by tools/strhooks.py: census {a.census.split("/")[-1]} sha256 {sha16(a.census)}, '
               f'blocks.split sha256 {sha16(a.blocks)}, quest.dis sha256 {sha16(a.dis)}')
    out.append(f'# rows {len(groups)} wmsp {len(claims)} stasp {len(releases)} onpop 1 unwind {len(UNWIND_WRTN)}')
    out.append('# arena layout: quest.arena (tools/arena.py), one temp s@<block>.<k> per wmsp line (P33-B)')
    out.append('# row <id> <block> <routine> first=<pc> last=<pc> nclaims=<n>')
    out.append('# wmsp <pc> row=<id> n=<ordinal in group> size=<census expression, documentation only>')
    out.append('# stasp <pc> row=<id>')
    out.append('# onpop <pc>   (the I.GOTO landing-stub STASP: unmap + frame_exit for every wfp >= the restored wfp)')
    out.append('# unwind <pc>  (a WRTN that is an unwind cut: >= rule, outstanding claims discarded without complaint)')
    for i, (blk, routine, cl, stasp) in enumerate(groups, 1):
        out.append(f'row {i} {blk:08X} {routine} first={cl[0]:08X} last={cl[-1]:08X} nclaims={len(cl)}')
        for n, c in enumerate(cl, 1):
            expr = claim_by_pc[c][2].replace(' ', '')
            out.append(f'wmsp {c:08X} row={i} n={n} size={expr}')
        out.append(f'stasp {stasp:08X} row={i}')
    out.append(f'onpop {ONPOP_PC:08X}')
    for pc in UNWIND_WRTN:
        out.append(f'unwind {pc:08X}')
    open(a.out, 'w').write('\n'.join(out) + '\n')
    print(f'strhooks: {len(groups)} rows, {len(claims)} wmsp, {len(releases)} stasp -> {a.out}')


if __name__ == '__main__':
    main()
