#!/usr/bin/env python3
# P23 follow-on (user ruling): ALL skips separate basic blocks. P22's
# builder kept the skip-over-DERR guard idiom interior; that poisons
# straight-line reasoning for P24 t-places and forced IRExec's
# forward-skip accommodation. Rewrites quest.blocks splitting every
# interior skip into its own terminator; regenerates the identity
# synclist.
#
# Skip offsets EXTRACTED from EagleCompute.cpp (fixed-word skips; the
# shadow is one WORD): reg forms fall +1 / skip +2; 16-bit-immediate
# forms +2/+3; wide-immediate forms +3/+4. Any other skip-class
# mnemonic interior => REFUSE (extract first; no guessing, METHOD §8).
import re, sys

# Complete conditional-length table, mechanically extracted from the
# emulator source (two nets: ?N:M conditional returns + dual constant
# returns) — the hand-built first list missed WUSGE and cost a battery.
FALL = {"WSGT":1,"WSGE":1,"WSLE":1,"WSLT":1,"WSEQ":1,"WSNE":1,"WSKBO":1,"WSKBZ":1,
        "WSZB":1,"WSZBO":1,"WUSGE":1,"WUSGT":1,
        "FSEQ":1,"FSGE":1,"FSGT":1,"FSLE":1,"FSLT":1,"FSNE":1,
        "WCLM":1,"WMESS":1,"ENQT":1,"DEQUE":1,"ISZTS":1,"DSZTS":1,
        "WSGTI":2,"WSEQI":2,"WSNEI":2,"WSLEI":2,"NSANA":2,
        "XNISZ":2,"XNDSZ":2,"XWISZ":2,
        "WSANA":3,"WUGTI":3,"WULEI":3}
OTHER_SKIP = re.compile(r'^(WSNB|CLM|ISZ|DSZ|LNISZ|LNDSZ|LWISZ|SKP\S*)$')
ALC_SKIP = re.compile(r',S[A-Z]{2};\s*$')
CTL_OTHER = re.compile(r'^(WRTN|RTN|XJMP|LJMP|LCALL|XCALL|LJSR|XJSR|DSPA|WPOPJ|POPJ)$')

def die(msg):
    sys.stderr.write("split_skips: REFUSE: " + msg + "\n")
    sys.exit(1)

def main(blocks_path, dis_path, out_blocks, out_sync):
    rx = re.compile(r"^([0-9a-fA-F]{8}) ")
    dis_pcs = sorted(set(int(m.group(1),16) for m in
                         (rx.match(l) for l in open(dis_path, errors="replace")) if m))
    dis_index = {pc: i for i, pc in enumerate(dis_pcs)}

    # ---- parse: items are ('comment', text) or ('block', start, [lines], [edges])
    items, cs, cl, ce = [], None, [], []
    def close():
        nonlocal cs, cl, ce
        if cs is not None:
            items.append(("block", cs, cl, ce))
        cs, cl, ce = None, [], []
    for raw in open(blocks_path, errors="replace"):
        line = raw.rstrip("\r\n")
        m = re.match(r"^([0-9A-Fa-f]{8}):$", line)
        if m:
            close(); cs = int(m.group(1), 16); continue
        if line.startswith("#"):
            close(); items.append(("comment", line)); continue
        if not line:
            continue
        if re.match(r"^[ncj]( |$)", line):
            ce.append(line); continue
        if cs is None:
            die("instruction text outside any block: " + line)
        cl.append(line)
    close()

    # ---- split
    out, starts, n_split = [], [], 0
    for it in items:
        if it[0] == "comment":
            out.append(it[1]); continue
        _, start, lines, edges = it
        if start not in dis_index:
            die("block start %08X not in disassembly" % start)
        i0 = dis_index[start]
        pcs = dis_pcs[i0:i0+len(lines)]
        frag_start, frag = start, []
        for k, (pc, text) in enumerate(zip(pcs, lines)):
            frag.append(text)
            op = text.split()[0].rstrip(";")
            last = (k == len(lines)-1)
            if last:
                break                      # original edges close the block
            if op in FALL:
                fall = pc + FALL[op]
                if fall != pcs[k+1]:
                    die("%s at %08X: fall %08X != next dis pc %08X"
                        % (op, pc, fall, pcs[k+1]))
                starts.append(frag_start)
                out.append("%08X:" % frag_start)
                out.extend(f + "\r" for f in frag)
                out.append("n %08X %08X\r" % (fall, fall+1))
                out.append("\r")
                frag_start, frag = fall, []
                n_split += 1
            elif op == "WBR":
                m = re.search(r"\(0x([0-9A-Fa-f]{8})\)", text)
                if not m:
                    die("interior WBR at %08X without folded target" % pc)
                starts.append(frag_start)
                out.append("%08X:" % frag_start)
                out.extend(f + "\r" for f in frag)
                out.append("n %08X\r" % int(m.group(1), 16))
                out.append("\r")
                frag_start, frag = pcs[k+1], []
                n_split += 1
            elif op == "LJSR" and start == 0x7015BD6B:
                pass   # the standing exclusion (P22 REPORT §4): leave merged
            elif OTHER_SKIP.match(op) or ALC_SKIP.search(text) or CTL_OTHER.match(op):
                die("interior control %s at %08X unhandled" % (op, pc))
        starts.append(frag_start)
        out.append("%08X:" % frag_start)
        out.extend(f + "\r" for f in frag)
        out.extend(e + "\r" for e in edges)
        out.append("\r")

    with open(out_blocks, "w", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    with open(out_sync, "w", newline="\n") as f:
        f.write("# Gen-6 sync list - IDENTITY (every quest.blocks start); "
                "regenerated by split_skips.py\n")
        for s in starts:
            f.write("%08X\n" % s)
    print("blocks: %d -> %d (%d interior skips split)"
          % (sum(1 for x in items if x[0]=="block"), len(starts), n_split))

main(*sys.argv[1:5])
