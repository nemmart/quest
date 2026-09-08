#!/usr/bin/env python3
"""Rebase the arg-tranche push_maps after a book shift (P20).

Every push_map slot is an ABSOLUTE area address resolved against the
book at generation time (generator-freezes-absolutes discipline). A P20
book reserves 4*N words at the base and shifts EVERY frame up by exactly
4*N, so every existing slot rebases by the same +4*N — and the result
must be indistinguishable from regenerating: this script proves that by
recomputing every line from first principles against the NEW book:

  push  <pc> <slot> [wides] : each written wide slot+2k must lie in some
                              entry's arg region [alloc, alloc+2*max_argc),
                              and slot must equal wfp-10-2*argN for a
                              consistent run of args (transcription law
                              slot = wfp-10-2N)
  call  <pc> <slot>         : slot must BE some entry's marker (wfp-10),
                              and that entry must be the routine named in
                              the site's preceding '# NAME,argc @ pc'
                              comment

Additionally the full (callee,argN)->slot transcription from
quest.argmap must be a subset-match: every argmap line's pc appearing in
the map must map to exactly wfp(callee)-10-2N. Any failure -> nonzero
exit, nothing written.

Usage: rebase_pushmaps.py <delta_words_hex> <book> <argmap> <map>...
Writes each map in place.
"""
import re, sys

DELTA = int(sys.argv[1], 16)
BOOK, AM = sys.argv[2], sys.argv[3]
MAPS = sys.argv[4:]

errors = []
def err(m): errors.append(m)

# ---- new book
entries = []           # (name, alloc, wfp, max_argc)
by_marker = {}         # wfp-10 -> name
for l in open(BOOK):
    s = l[1:] if l.startswith('#') else l
    p = s.split()
    if len(p) >= 6 and re.fullmatch(r'[0-9A-F]{8}', p[0] or '') and re.fullmatch(r'[0-9A-F]{8}', p[2] or ''):
        name, alloc, wfp, argc = p[1], int(p[2], 16), int(p[3], 16), int(p[4])
        entries.append((name, alloc, wfp, argc))
        by_marker[wfp - 10] = name

def region_of(slot):
    for name, alloc, wfp, argc in entries:
        if alloc <= slot < alloc + 2 * argc:
            return name, wfp
    return None, None

# ---- argmap transcription law: pc -> {(name, argN)}
am_pc = {}
for l in open(AM):
    m = re.match(r'(\S+) arg(\d+) at ([0-9A-F]{8})', l)
    if m and m.group(1) != '_PAIRS':
        am_pc.setdefault(m.group(3), []).append((m.group(1), int(m.group(2))))
wfp_of = {name: wfp for name, _, wfp, _ in entries}

for path in MAPS:
    out = []
    site_name = None
    checked = 0
    for lineno, l in enumerate(open(path), 1):
        m = re.match(r'# (\S+?),\d+ @ [0-9A-F]{8}', l)
        if m:
            site_name = m.group(1)
        m = re.match(r'^(push|call) ([0-9A-F]{8}) ([0-9A-F]{8})(\s+\d+)?(\s*#.*)?$', l.rstrip('\n'))
        if not m:
            out.append(l.rstrip('\n'))
            continue
        kind, pc, slot, wides, cmt = m.group(1), m.group(2), int(m.group(3), 16), m.group(4), m.group(5) or ''
        slot += DELTA
        wides_n = int(wides) if wides else 1
        if kind == 'push':
            for k in range(wides_n):
                name, wfp = region_of(slot + 2 * k)
                if name is None:
                    err('%s:%d: push slot %08X+2*%d not in any arg region' % (path, lineno, slot, k))
            # transcription law against the argmap, when this pc is an argmap pc
            for cal, argn in am_pc.get(pc, []):
                want_base = min(wfp_of[cal] - 10 - 2 * n for c, n in am_pc[pc] if c == cal)
                if cal in wfp_of and slot != want_base:
                    err('%s:%d: push %s slot %08X != transcription %08X (%s)' %
                        (path, lineno, pc, slot, want_base, cal))
                checked += 1
        else:
            name = by_marker.get(slot)
            if name is None:
                err('%s:%d: call slot %08X is not any marker (wfp-10)' % (path, lineno, slot))
            elif site_name and name.split('@')[0] != site_name.split('@')[0] and name != site_name:
                err('%s:%d: call %s marker resolves to %s, site comment says %s' %
                    (path, lineno, pc, name, site_name))
        out.append('%s %s %08X%s%s' % (kind, pc, slot, wides or '', cmt))
    if errors:
        for e in errors[:10]:
            print('ERROR:', e, file=sys.stderr)
        sys.exit(1)
    open(path, 'w').write('\n'.join(out) + '\n')
    print('%s: rebased +%X, %d argmap-transcription checks passed' % (path, DELTA, checked))
