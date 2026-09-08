#!/usr/bin/env python3
"""build_address_book.py — M4a address book generator (docs/M4aDesign.md §2–§3).

Reads Disassembled/quest.dis + quest.symbols, finds every WSAVS/WSAVR
entry in the game range [QUEST, ?CHAR_TO_UNSIGNED), classifies each
(named / nested, argc from call sites, frame size, dynamic-stack use,
push/pop use, slot-patch return convention, mixed arity), and writes:

  quest.addrbook      — one line per routine; wave-one PURE routines
                        live, everything else present but commented out
  addrbook_report.md  — per-routine call sites and flags, census
                        verification against the design's recorded facts

Layout per routine (verbatim master frame image at a fixed base;
offsets from EagleStack WSAVS/WRTN + Machine::wide_push):

  alloc_base                       (16-word aligned)
    args:        max_argc wides    arg N at wfp-10-2N
    frame word:  1 wide            (psr<<16)|argc at wfp-10
    image:       5 wides           ac0 wfp-8, ac1 wfp-6, ac2 wfp-4,
                                   prev wfp wfp-2, ac3|c wfp+0
    ← wfp
    WSAVS space: frame_size WIDES  = 2*frame_size words [wfp+2 ..)
  next alloc_base

Size (words) = 2*max_argc + 2 + 10 + 2*frame_size, rounded up to 16; bases advance by size+16 (guaranteed gap; see hw/Machine.cpp T).
NOTE (correction to §3's prose): the WSAVS operand counts WIDES —
EagleStack does `wsp += frame_size*2` — so the space is 2*frame_size
words, not frame_size words.

Usage: build_address_book.py <Disassembled dir> [--out-book F] [--out-report F]
       [--live NAME,NAME,...]   restrict the LIVE (uncommented) set to
                                these names (default: all wave-one =
                                all callable = not nocall; B2 dropped the
                                prior "pure AND not nocall" restriction)
"""
import argparse
import os
import re
import sys
from collections import defaultdict, OrderedDict

BASE = 0x74000000
PAGE_WORDS = 1024

RE_INSN = re.compile(r'^([0-9a-f]{8})\s+([A-Z][A-Z0-9.#?]*)\s*(.*?);?\s*(#.*)?$')


def load_symbols(path):
    syms = {}
    with open(path) as f:
        for line in f:
            line = line.rstrip('\n')
            if not line:
                continue
            addr, name = line.split(' ', 1)
            addr = int(addr, 16)
            # keep the first name only for multi-name lines
            syms[addr] = name.split(' / ')[0].strip()
    return syms


def load_dis(path):
    """Return ordered list of (addr, mnemonic, operands) for code lines."""
    insns = []
    with open(path, errors='replace') as f:
        for line in f:
            line = line.rstrip('\r\n')
            m = RE_INSN.match(line)
            if not m:
                continue
            addr = int(m.group(1), 16)
            mnem = m.group(2)
            ops = m.group(3).strip()
            insns.append((addr, mnem, ops))
    return insns


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('disdir')
    ap.add_argument('--out-book', default='quest.addrbook')
    ap.add_argument('--out-report', default='addrbook_report.md')
    ap.add_argument('--live', default=None,
                    help='comma-separated names to leave LIVE (default: all callable = not nocall)')
    ap.add_argument('--also-live', default=None,
                    help='comma-separated names forced LIVE regardless of wave-one '
                         '(P17: QUEST, the hand-uncommented base record)')
    args = ap.parse_args()

    syms = load_symbols(os.path.join(args.disdir, 'quest.symbols'))
    insns = load_dis(os.path.join(args.disdir, 'quest.dis'))
    by_name = {v: k for k, v in syms.items()}
    game_lo = by_name['QUEST']
    game_hi = by_name['?CHAR_TO_UNSIGNED']

    # ---- entries: every WSAVS/WSAVR/WSSVS/WSSVR in the game range ----
    entries = OrderedDict()   # addr -> dict
    for i, (addr, mnem, ops) in enumerate(insns):
        if not (game_lo <= addr < game_hi):
            continue
        if mnem in ('WSAVS', 'WSAVR', 'WSSVS', 'WSSVR'):
            frame = int(ops.split(';')[0], 16)
            entries[addr] = dict(addr=addr, variant=mnem, frame=frame,
                                 name=None, nested=False, parent=None,
                                 sites=[], argcs=[], lcall_sites=[], xcall_sites=[],
                                 dyn=set(), push=set(), slot_patch=set(),
                                 xcall_link_forms=[], idx=i)
    if any(e['variant'] in ('WSSVS', 'WSSVR') for e in entries.values()):
        print('WARNING: WSSVS/WSSVR entry in game range (design §2 says none)',
              file=sys.stderr)

    # named game symbols in range (code symbols = symbols sitting on an entry)
    game_syms = sorted((a, n) for a, n in syms.items() if game_lo <= a < game_hi)
    named_entries = 0
    for a, n in game_syms:
        if a in entries:
            entries[a]['name'] = n
            named_entries += 1
        # symbols not on an entry are data or non-WSAVS code (report them)

    # ---- call sites ----
    re_lcall = re.compile(r'\[0x([0-9A-Fa-f]{8})\],(\d+)')
    # XCALL renders as `[pc+0xNNN] (0xTARGET),argc` since the Sep 5 2026
    # disassembler fix (wordIndirectArgument = ea,arg like LCALL; the old
    # `0,argc,[pc...]` form printed opcode bits as a phantom register).
    re_xcall = re.compile(r'^\[pc[+-]0x[0-9A-Fa-f]+\]\s*\(0x([0-9A-Fa-f]{8})\),(\d+)')
    unnamed_lcall_targets = []
    nonconforming_xcall = []
    for i, (addr, mnem, ops) in enumerate(insns):
        if mnem == 'LCALL':
            m = re_lcall.search(ops)
            if not m:
                continue
            tgt = int(m.group(1), 16)
            argc = int(m.group(2))
            if tgt in entries:
                e = entries[tgt]
                e['sites'].append(addr); e['argcs'].append(argc); e['lcall_sites'].append(addr)
                if e['name'] is None:
                    unnamed_lcall_targets.append((addr, tgt))
            elif game_lo <= tgt < game_hi:
                unnamed_lcall_targets.append((addr, tgt))
        elif mnem == 'XCALL':
            m = re_xcall.match(ops)
            if not m:
                continue
            tgt = int(m.group(1), 16)
            argc = int(m.group(2))
            if tgt in entries:
                e = entries[tgt]
                e['sites'].append(addr); e['argcs'].append(argc); e['xcall_sites'].append(addr)
                e['nested'] = True
                # static-link form: the instruction immediately before the XCALL
                prev = insns[i - 1]
                form = prev[1] + ' ' + prev[2]
                e['xcall_link_forms'].append(form)
                if not (form.startswith('WMOV 3,1') or form.startswith('WMOV 2,1')
                        or form.startswith('XWLDA 1,[ac3+0x7FFA]')):
                    nonconforming_xcall.append((addr, tgt, form))
            elif game_lo <= tgt < game_hi:
                nonconforming_xcall.append((addr, tgt, 'target not an entry'))

    # nested naming: PARENT.n@pc — enclosing named symbol by address
    ent_list = sorted(entries.values(), key=lambda e: e['addr'])
    named_addrs = [e['addr'] for e in ent_list if e['name']]
    nested_counter = defaultdict(int)
    for e in ent_list:
        if e['name'] is None:
            parent = None
            for a in named_addrs:
                if a <= e['addr']:
                    parent = a
                else:
                    break
            pname = entries[parent]['name'] if parent is not None else '?'
            e['parent'] = pname
            nested_counter[pname] += 1
            e['name'] = '%s.%d@%08X' % (pname, nested_counter[pname], e['addr'])
            e['nested'] = True

    # ---- per-segment instruction census: [entry_i, entry_{i+1}) ----
    # Segmentation caveat (recorded in the report): a named routine's code
    # AFTER a nested body is attributed to the nested entry's segment.
    ent_addrs = [e['addr'] for e in ent_list]
    seg_end = {}
    for k, a in enumerate(ent_addrs):
        seg_end[a] = ent_addrs[k + 1] if k + 1 < len(ent_addrs) else game_hi
    re_slot = re.compile(r'\[ac3\+0x7FF[89]\]')
    cur = None
    for addr, mnem, ops in insns:
        if not (game_lo <= addr < game_hi):
            continue
        if addr in entries:
            cur = entries[addr]
        if cur is None or addr >= seg_end[cur['addr']]:
            continue
        if mnem in ('WMSP', 'STASP', 'LDASP'):
            cur['dyn'].add(mnem)
        if mnem in ('WPSH', 'WPOP'):
            cur['push'].add(mnem)
        if mnem in ('XNSTA', 'XWSTA') and re_slot.search(ops):
            cur['slot_patch'].add(mnem + ' ' + ops.split(';')[0])

    # whole-symbol-range union (parents with nested bodies) for the report
    for e in ent_list:
        if not e['nested']:
            e['range_dyn'] = set(e['dyn']); e['range_push'] = set(e['push'])
    for e in ent_list:
        if e['nested'] and e['parent'] in by_name:
            p = entries[by_name[e['parent']]]
            p['range_dyn'] |= e['dyn']; p['range_push'] |= e['push']

    # ---- P20 borrow block: reserve N slots at the area base ----
    # quest.argmap's first line is `_PAIRS count N` (ArgWindows borrow
    # pass). Slot n lives at BASE + 4*n (0-indexed, flat — deliberately a
    # DIFFERENT equation than argN at wfp-10-2N); the block occupies
    # [BASE, BASE + 4*N) and code frames start ABOVE it. The shift is
    # DERIVED from N — nothing hardcoded. No argmap / no _PAIRS line ⇒
    # N=0 ⇒ the pre-P20 layout exactly.
    borrow_slots = 0
    argmap_path = os.path.join(args.disdir, 'quest.argmap')
    if os.path.exists(argmap_path):
        with open(argmap_path) as f:
            for line in f:
                p = line.split()
                if len(p) == 3 and p[0] == '_PAIRS' and p[1] == 'count':
                    borrow_slots = int(p[2])
                    break

    # ---- classification, sizing, bases ----
    live_filter = set(args.live.split(',')) if args.live else None
    also_live = set(args.also_live.split(',')) if args.also_live else set()
    base = BASE + 4 * borrow_slots
    for e in ent_list:
        e['max_argc'] = max(e['argcs']) if e['argcs'] else 0
        e['mixed'] = len(set(e['argcs'])) > 1
        e['pure'] = not e['dyn'] and not e['push']
        # nocall (M4aDesign §8): no LCALL/XCALL static caller — boot, task
        # entry, ON-unit bodies (entered by O?SIGNAL dispatch). The redirect's
        # "[wsp] = LCALL frame word" read is meaningless there: NOT a wave-one
        # candidate.
        e['nocall'] = not e['sites']
        e['wave_one'] = not e['nocall']   # B2: all-callable (nocall-only exclusion; was: pure and not nocall)
        e['live'] = (e['wave_one'] and (live_filter is None or e['name'] in live_filter)) \
                    or e['name'] in also_live
        words = 2 * e['max_argc'] + 2 + 10 + 2 * e['frame']
        e['size'] = (words + 15) & ~15
        e['alloc_base'] = base
        # wfp_base (R-C ruling, Project 14): args at alloc_base, then the
        # frame word (2) and the FOUR image wides below wfp (8) — the fifth
        # (ac3|carry) sits AT [wfp, wfp+2). Occupied span is exactly
        # [base, base + words); the earlier "+ 2 + 10" put the layout 2
        # words high and let the x%16==0 size class overflow its block.
        e['wfp_base'] = base + 2 * e['max_argc'] + 2 + 8
        # Stride = size + 16 (ruling, Stage 0b): guarantees
        # block_end (base+size) < next_base, so T's end-INCLUSIVE
        # containment (v <= base+size, one-past-end residue) is
        # unambiguous at every boundary.
        base += e['size'] + 16
    total_words = base - BASE
    total_pages = (total_words + PAGE_WORDS - 1) // PAGE_WORDS

    # ---- verification against design §2 ----
    n_named = sum(1 for e in ent_list if not e['nested'])
    n_nested = sum(1 for e in ent_list if e['nested'])
    n_xcall_sites = sum(len(e['xcall_sites']) for e in ent_list)
    n_wsavr = sum(1 for e in ent_list if e['variant'] == 'WSAVR')
    n_wsavs = sum(1 for e in ent_list if e['variant'] == 'WSAVS')
    n_slot = sum(1 for e in ent_list if e['slot_patch'])
    n_dyn = sum(1 for e in ent_list if e['dyn'])
    n_pure = sum(1 for e in ent_list if e['pure'])
    n_nocall = sum(1 for e in ent_list if e['nocall'])
    n_wave = sum(1 for e in ent_list if e['wave_one'])
    checks = [
        ('named game routines', n_named, 74),
        ('unnamed (nested) WSAVS entries', n_nested, 49),
        ('XCALL sites', n_xcall_sites, 63),
        ('non-conforming XCALL static-link sites', len(nonconforming_xcall), 0),
        ('unnamed LCALL targets in game range', len(unnamed_lcall_targets), 0),
        ('WSAVR entries', n_wsavr, 2),
        ('WSAVS entries', n_wsavs, 124),
        ('slot-patch routines', n_slot, 18),
        ('dyn (WMSP/STASP/LDASP) segments', n_dyn, 13),
    ]

    # ---- write the book ----
    with open(args.out_book, 'w') as f:
        f.write('# quest.addrbook — M4a address book (generated by tools/build_address_book.py)\n')
        f.write('# base %08X  total_words %d  pages %d  entries %d  live %d  borrow_slots %d\n'
                % (BASE, total_words, total_pages, len(ent_list),
                   sum(1 for e in ent_list if e['live']), borrow_slots))
        f.write('# layout: alloc_base | args (max_argc wides, arg N at wfp-10-2N) | frame word (wfp-10) |\n')
        f.write('#         image ac0 wfp-8, ac1 wfp-6, ac2 wfp-4, prev-wfp wfp-2, ac3|c wfp+0 | WSAVS space 2*frame words\n')
        f.write('# a line starting with # is NOT migrated (stays stacked). Hand-editable; keep the columns.\n')
        f.write('# entry     name                          alloc_base wfp_base   argc frame  variant flags\n')
        if borrow_slots:
            f.write('borrow_slots %d\n' % borrow_slots)   # P20: [BASE, BASE+4N) reserved; frames start at BASE+4N

        for e in ent_list:
            flags = []
            if e['dyn']: flags.append('dyn')
            if e['push']: flags.append('push')
            if e['mixed']: flags.append('mixed:' + '/'.join(str(a) for a in sorted(set(e['argcs']))))
            if e['slot_patch']: flags.append('slotpatch')
            if e['nested']: flags.append('nested')
            if e['nocall']: flags.append('nocall')
            fl = ','.join(flags) if flags else '-'
            line = '%08X    %-28s  %08X   %08X   %-4d 0x%02X   %-6s  %s' % (
                e['addr'], e['name'], e['alloc_base'], e['wfp_base'],
                e['max_argc'], e['frame'], e['variant'], fl)
            if e['live']:
                f.write(line + '\n')
            else:
                f.write('#' + line + '\n')

    # ---- write the report ----
    with open(args.out_report, 'w') as f:
        f.write('# addrbook_report.md — generated by tools/build_address_book.py\n\n')
        f.write('Game range: %08X (QUEST) .. %08X (?CHAR_TO_UNSIGNED). Base %08X, '
                'total %d words, %d pages, %d entries.\n\n' %
                (game_lo, game_hi, BASE, total_words, total_pages, len(ent_list)))
        f.write('## Census verification against M4aDesign.md §2\n\n')
        f.write('| fact | measured | design | ok |\n|---|---|---|---|\n')
        for label, got, want in checks:
            f.write('| %s | %d | %d | %s |\n' % (label, got, want, 'yes' if got == want else '**NO**'))
        f.write('| pure (no dyn, no push) | %d | ~74 | %s |\n\n' % (n_pure, 'yes' if abs(n_pure - 74) <= 3 else '**check**'))
        if unnamed_lcall_targets:
            f.write('Unnamed LCALL targets: %s\n\n' % ', '.join('%08X->%08X' % t for t in unnamed_lcall_targets))
        if nonconforming_xcall:
            f.write('Non-conforming XCALL sites: %s\n\n' % ', '.join('%08X->%08X (%s)' % t for t in nonconforming_xcall))
        f.write('Segmentation caveat: per-entry instruction census uses the address '
                'segment [entry, next entry). Code of a NAMED routine that follows a '
                'nested body is attributed to the nested entry. The `range_dyn/range_push` '
                'columns give the union over the whole named symbol range for parents.\n\n')
        f.write('Sizing correction to §3 prose: the WSAVS operand is in WIDES '
                '(EagleStack: wsp += frame_size*2), so the WSAVS space is 2*frame words.\n\n')
        f.write('## Wave-one list (pure AND has a static caller)\n\n')
        f.write(', '.join(e['name'] for e in ent_list if e['wave_one']) + '\n\n')
        f.write('## Pure but nocall (excluded from wave one, M4aDesign §8)\n\n')
        f.write(', '.join(e['name'] for e in ent_list if e['pure'] and e['nocall']) + '\n\n')
        f.write('## Excluded (dyn/push), with why\n\n')
        for e in ent_list:
            if not e['pure']:
                f.write('- %s: %s\n' % (e['name'], ' '.join(sorted(e['dyn'] | e['push']))))
        f.write('\n## Per-routine table\n\n')
        f.write('| entry | name | variant | frame | argc(max) | argcs seen | sites | pure | slot-patch | dyn | push | range dyn/push (parent) | link forms (nested) |\n')
        f.write('|---|---|---|---|---|---|---|---|---|---|---|---|---|\n')
        for e in ent_list:
            f.write('| %08X | %s | %s | 0x%02X | %d | %s | %d | %s | %s | %s | %s | %s | %s |\n' % (
                e['addr'], e['name'], e['variant'], e['frame'], e['max_argc'],
                '/'.join(str(a) for a in sorted(set(e['argcs']))) or '-',
                len(e['sites']), 'yes' if e['pure'] else 'no',
                '; '.join(sorted(e['slot_patch'])) or '-',
                ' '.join(sorted(e['dyn'])) or '-', ' '.join(sorted(e['push'])) or '-',
                ('' if e['nested'] else (' '.join(sorted(e.get('range_dyn', set()) | e.get('range_push', set()))) or '-')),
                ', '.join(sorted(set(e['xcall_link_forms']))) or '-'))
        f.write('\n## Call sites per routine\n\n')
        for e in ent_list:
            f.write('- %s (%08X): %s\n' % (e['name'], e['addr'],
                    ' '.join('%08X' % s for s in e['sites']) or '(none)'))
        f.write('\n## Symbols in the game range that are not WSAVS entries\n\n')
        f.write(', '.join('%s@%08X' % (n, a) for a, n in game_syms if a not in entries) + '\n')

    for label, got, want in checks:
        print('%-45s %5d (design %d)%s' % (label, got, want, '' if got == want else '  <-- MISMATCH'))
    print('pure %d of %d; nocall %d; wave-one %d; live %d; total %d words, %d pages' %
          (n_pure, len(ent_list), n_nocall, n_wave, sum(1 for e in ent_list if e['live']), total_words, total_pages))


if __name__ == '__main__':
    main()
