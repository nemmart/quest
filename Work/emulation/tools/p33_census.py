#!/usr/bin/env python3
"""tools/p33_census.py — P33-B census of the 19 WMSP claim groups on top of
string_sites.py's evaluator (docs/Project33/CensusB.md).

For each group (from quest.strhooks): its blocks from the claim block to the
STASP's block, every WCMV in them with the CLAIM each temp operand lies in,
the consumer, and — the point of the exercise — the symbolic registers at
every block boundary inside the bracket (each is a listed entry, i.e. a
rendezvous at K=1), classified as: literal / frame / static / count /
temp-of-claim-k / computed.  A temp pointer of an INTERMEDIATE claim (not
the last) surviving to a boundary is what StringsDesign §2.1 says cannot
happen; this script says whether it does.

Usage (from Work/emulation/tools):
  python3 p33_census.py --strhooks ../quest.strhooks --dis ../../../Disassembled/quest.dis \
      --blocks ../quest.blocks.split --mem ../../../Disassembled/quest.mem \
      --symbols ../../../Disassembled/quest.symbols --synclist ../quest.synclist.p27 [--out FILE]
"""
import argparse, collections, re, sys
sys.path.insert(0, __file__.rsplit('/', 1)[0] if '/' in __file__ else '.')
import string_sites as S


def load_strhooks(path):
    rows = {}
    for line in open(path):
        if line.startswith('row '):
            t = line.split()
            r = {'id': int(t[1]), 'block': int(t[2], 16), 'routine': t[3], 'wmsp': [], 'stasp': []}
            for x in t[4:]:
                k, v = x.split('=')
                r[k] = v
            rows[r['id']] = r
        elif line.startswith('wmsp '):
            t = line.split()
            rid = int(t[2].split('=')[1]); n = int(t[3].split('=')[1])
            rows[rid]['wmsp'].append((n, int(t[1], 16)))
        elif line.startswith('stasp '):
            t = line.split()
            rows[int(t[2].split('=')[1])]['stasp'].append(int(t[1], 16))
    for r in rows.values():
        r['wmsp'].sort()
    return [rows[k] for k in sorted(rows)]


def claim_of(v, claims):
    """v: a pointer V. Return (claim index (1-based) among `claims`, word offset, byte offset) or None."""
    base = None; off = 0; boff = None
    if v.kind == 'bp':
        base, off = v.a, v.k
        boff = v.b.show() if v.b is not None else None
    elif v.kind == 'end' and v.tag and v.tag[0] == 'cont':
        r = claim_of(v.tag[2], claims)
        return (r[0], 'end-of-op', r[2]) if r else None
    else:
        base = v
    for i, cl in enumerate(claims, 1):
        d = S.subv(base, cl.base)
        if d.kind == 'const':
            # claim occupies words [base+2, top]; word offset 2 = first word of the temp
            return (i, d.k, boff if v.kind == 'bp' else 'word')
    return None


def describe(v, claims, ctx, is_count):
    if is_count:
        cls, det = S.classify_count(v, ctx)
        return '%s:%s' % (cls, det)
    cls, det, boff = S.classify_ptr(v, ctx)
    if cls == 'temp':
        c = claim_of(v, claims)
        if c:
            return 'temp[claim %d %s word%+d%s]' % (c[0], '%X' % claims[c[0] - 1].pc, c[1] if isinstance(c[1], int) else 0,
                                                 '' if c[2] in (None, 'word') else (' end' if c[1] == 'end-of-op' else ' +' + str(c[2])))
        return 'temp[?] ' + det
    return '%s:%s' % (cls, det[:60])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--strhooks', required=True)
    ap.add_argument('--dis', required=True); ap.add_argument('--blocks', required=True)
    ap.add_argument('--mem', required=True); ap.add_argument('--symbols', required=True)
    ap.add_argument('--synclist', required=True)
    ap.add_argument('--out')
    a = ap.parse_args()
    listed = set(int(l[:8], 16) for l in open(a.synclist) if re.match(r'^[0-9A-F]{8}', l))
    ins = S.load_dis(a.dis)
    blocks = S.load_blocks(a.blocks, ins)
    mem, regions = S.load_mem(a.mem)
    syms = S.load_symbols(a.symbols)
    ctx = S.Ctx(); ctx.mem, ctx.regions, ctx.syms = mem, regions, syms
    ctx.blocks, ctx.ins = blocks, ins
    ctx.by_start = {b.start: b for b in blocks}
    state, preds, entries = S.evaluate_all(blocks, ins, syms)
    ctx.state, ctx.preds = state, preds
    by_start = ctx.by_start
    out = open(a.out, 'w') if a.out else sys.stdout
    W = out.write
    W('# P33-B census (tools/p33_census.py) — 19 groups from quest.strhooks\n')
    W('# per block: WMSP claims, WCMV pieces (dst/src operand classes; temp[claim k]), consumer, STASP;\n')
    W('# then the registers at the block END (= the next listed entry, a rendezvous at K=1)\n\n')
    survivors = []
    for r in load_strhooks(a.strhooks):
        stasp = r['stasp'][0]
        first_pc = r['wmsp'][0][1]
        wmsp_pcs = [pc for _, pc in r['wmsp']]
        # the group's blocks: from the claim block, following successors, until the STASP's block
        stasp_blk = None
        for b in blocks:
            if b.instrs and b.instrs[0].pc <= stasp <= b.instrs[-1].pc:
                stasp_blk = b.start
        seen = []
        todo = [r['block']]
        while todo:
            s = todo.pop(0)
            if s in seen: continue
            seen.append(s)
            if s == stasp_blk: continue
            b = by_start[s]
            for t in b.succs:
                if t in by_start and by_start[t].func == b.func and s < t <= stasp_blk and t not in seen:
                    todo.append(t)
        seen.sort()
        W('== group row %d block %08X %s: claims %s stasp %08X; blocks %s\n' % (
            r['id'], r['block'], r['routine'], ' '.join('%X' % p for p in wmsp_pcs), stasp,
            ' '.join('%X%s' % (s, '' if s in listed else '(unlisted)') for s in seen)))
        # the claims as the evaluator saw them, in the claim block
        e0 = state[r['block']]
        claims = [s.claim for s in e0.sites if s.ins.mn == 'WMSP' and s.ins.pc in wmsp_pcs]
        if len(claims) != len(wmsp_pcs):
            W('   !! evaluator saw %d claims, table has %d\n' % (len(claims), len(wmsp_pcs)))
        for i, cl in enumerate(claims, 1):
            W('   claim %d @%X size(wides)=%s base=%s\n' % (i, cl.pc, cl.size.show(), cl.base.show()))
        for s in seen:
            e = state[s]
            W(' block %08X%s\n' % (s, '' if s in listed else ' (UNLISTED)'))
            for site in e.sites:
                x = site.ins
                if x.mn == 'WCMV':
                    cs = [c for c in site.claims] or claims
                    W('   WCMV @%X  dst=%s  dcnt=%s  src=%s  scnt=%s\n' % (
                        x.pc, describe(site.acs[2], claims, ctx, False), describe(site.acs[0], claims, ctx, True),
                        describe(site.acs[3], claims, ctx, False), describe(site.acs[1], claims, ctx, True)))
                elif x.mn == 'STASP':
                    W('   STASP @%X  wsp := %s\n' % (x.pc, site.acs[int(x.args)].show()))
            for x in by_start[s].instrs:
                if x.mn in ('LCALL', 'XCALL') and s != stasp_blk:
                    W('   %s @%X %s\n' % (x.mn, x.pc, x.args))
            if s != stasp_blk:
                regs = []
                for i in range(4):
                    v = e.ac[i]
                    d = describe(v, claims, ctx, i < 2)
                    regs.append('ac%d=%s' % (i, d))
                    if d.startswith('temp[claim'):
                        k = int(d.split()[1])
                        if k != len(claims):
                            survivors.append((r['id'], r['block'], s, i, d))
                W('   END: %s\n' % '  '.join(regs))
        W('\n')
    W('# intermediate-temp pointers at a block end (rendezvous): %d\n' % len(survivors))
    for sv in survivors:
        W('#   row %d block %08X: at end of %08X ac%d = %s\n' % sv)


if __name__ == '__main__':
    main()
