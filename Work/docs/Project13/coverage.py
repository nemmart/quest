#!/usr/bin/env python3
# coverage.py <rundir> <book>: per-routine table of clone gcalls vs redirect WSAVS; flags mismatches for live routines.
import sys, re, collections
run, book = sys.argv[1], sys.argv[2]
live, names = set(), {}
for l in open(book):
    if l.startswith('#') or not l.strip(): continue
    f = l.split(); live.add(int(f[0],16)); names[int(f[0],16)] = f[1]
# every book entry (commented too) for name lookup
for l in open(book):
    m = re.match(r'#?([0-9A-F]{8})\s+(\S+)', l)
    if m and not l.startswith('# '): names[int(m.group(1),16)] = m.group(2)
gc = collections.Counter(); gm = collections.Counter()
for l in open(run+'/gcalls.log'):
    m = re.search(r'caller=(\S+) target=([0-9A-F]{8})', l)
    if not m: continue
    if m.group(1) == 'QUEST2': gc[int(m.group(2),16)] += 1
    elif m.group(1) == 'QUEST1': gm[int(m.group(2),16)] += 1
    # QUEST_SERVER's game-range calls (its own binary, same symbol names) are not part of the pair
hj = collections.Counter()
for l in open(run+'/redirect.log'):
    m = re.search(r'WSAVS (\S+)\s+pc=([0-9A-F]{8})', l)
    if m: hj[int(m.group(2),16)] += 1
bad = 0
rows = sorted(set(gc)|set(gm)|set(hj), key=lambda a: -(gc[a]+gm[a]))
for a in rows:
    st = 'LIVE' if a in live else 'stacked'
    ok = ''
    if a in live and gc[a] != hj[a]: ok = '  <-- MISMATCH'; bad += 1
    if a not in live and hj[a]: ok = '  <-- HIJACK ON STACKED?'; bad += 1
    if gc[a] != gm[a]: ok += '  (master gcalls %d != clone %d)' % (gm[a], gc[a])
    print('  %-28s %-7s gcalls=%-6d redirect=%-6d%s' % (names.get(a,'%08X'%a), st, gc[a], hj[a], ok))
missing = [names[a] for a in live if gc[a] == 0]
print('  live routines NOT reached this run: %s' % (', '.join(sorted(missing)) or 'none'))
print('  cross-check: %s' % ('OK' if bad == 0 else '%d MISMATCH(ES)' % bad))
