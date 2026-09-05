#!/usr/bin/env python3
# p26cov.py <quest.ir2.book> <err-file(s)...>
# For every mnemonic lowered in P26 (recognized from the ';' audit comment on
# lowered statement/goto lines), report how many blocks carrying it were
# executed at least once (IRExec first-execution lines) across the given legs.
import re, sys, collections
book = sys.argv[1]
live = set()
for f in sys.argv[2:]:
    for l in open(f, errors="replace"):
        m = re.search(r"first execution of block ([0-9A-F]{8})", l)
        if m: live.add(m.group(1))
P26 = set("""WSGT WSGTI WSEQI WSNEI WUGTI WSEQ WSLE WSGE WSNE WSLEI WSLT WULEI WUSGE WUSGT
LWADD WNADI WADC WINC WADI XWADD WNEG XWSUB XWADI XWSBI WCOM WIOR WAND WXOR WANDI WIORI
WLSI WLSHI WHLV WMUL XWMUL WDIV LDAFP LDASP WMOVR CVWN XJMP WSZB WBTO WBTZ
NADI XNSUB XNADD NSBI XNADI NADDI NSUB NNEG XNSBI NMUL NADD LNADI LNADD LNSBI LNSUB XNMUL
XNDO XWDO SEX ZEX ANDI WXCH CRYTO WSKBO XNISZ WPSH WPOP
MOV.L# MOV.# ADD.O# COM.# ADD.# MOV.R# NEG.L#""".split())
blocks_with = collections.defaultdict(set)
cur = None
for l in open(book):
    l = l.rstrip("\n")
    if l.startswith("block "): cur = l.split()[1]; continue
    if not l or cur is None: continue
    if l.startswith("  @"): continue                     # still embedded
    m = re.search(r"; (\S+)", l)
    if not m: continue
    op = m.group(1).rstrip(";")
    if op in P26: blocks_with[op].add(cur)
tot_b = tot_l = 0
rows = []
for op in sorted(blocks_with, key=lambda o: -len(blocks_with[o])):
    b = blocks_with[op]; n = len(b & live)
    tot_b += len(b); tot_l += n
    rows.append("%s %d/%d" % (op, n, len(b)))
print("P26 newly-lowered mnemonic coverage (blocks live/blocks carrying, %d live blocks):" % len(live))
print("  " + "  ".join(rows))
print("  classes live: %d/%d; unreached: %s" % (
    sum(1 for op in blocks_with if blocks_with[op] & live), len(blocks_with),
    " ".join(sorted(op for op in blocks_with if not (blocks_with[op] & live))) or "none"))
