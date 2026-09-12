#!/usr/bin/env python3
"""callgraph.py — the game call graph, extracted from the book (Project 47).

Sources: emulation/quest.ir2.book (call statements + unlifted LCALL/XCALL
embeds), emulation/quest.addrbook (entries, families, flags),
Disassembled/quest.dis (O.ON / I.GOTO / task-spawn targets, WSAVS census),
Disassembled/quest.symbols (the two game-segment routines the addrbook
omits), Disassembled/quest.callsites (independent cross-check).

NODE = FAMILY (docs/Project47/q001-plan-gate.md §2; Salvage F3/F9/F8):
  * `X.N@ADDR` entries are pieces of procedure X — one node.
  * R40 multiple-ENTRY pairs are one node with two entry points.
  * LOCK_FILE+UNLOCK_FILE are one node, kind=asm, out of translation scope.
  * `#` nocall `.N@` entries are ON-unit bodies inside their family's node;
    the two top-level nocall entries (C_A_LISTENER, TERRAIN_HELP) are nodes.
  * SQR31?3 (kind=library) and INIT_SHARED_DATA (kind=game) are added from
    quest.symbols — game-segment code with no addrbook line.

EDGE CLASSES (never merged):
  call    game->game LCALL/XCALL, decorated (`call` stmt) or embedded
  signal  O.ON establisher -> ON-unit body           (DESIGN §11)
  goto    I.GOTO non-local goto -> label              (all intra-family)
  spawn   ?CREATE_TASK task body address              (QUEST -> C_A_LISTENER)
  rt_call and calls into the runtime are NOT edges (DESIGN §9.3).

    python3 compiler/callgraph.py --check            cross-checks, hazards
    python3 compiler/callgraph.py --leaves
    python3 compiler/callgraph.py --callers X | --callees X
    python3 compiler/callgraph.py --depth [X]        height (leaf = 0)
    python3 compiler/callgraph.py --strata
    python3 compiler/callgraph.py --frontier A,B,..  next candidates given replaced set
    python3 compiler/callgraph.py --unresolved
    python3 compiler/callgraph.py --profile X        size + constructs for Order.md
    python3 compiler/callgraph.py --coverage DIR     results dir with *.err traces
    python3 compiler/callgraph.py --edges [X]        every edge, or those touching X
    python3 compiler/callgraph.py --json             the whole graph as JSON
"""
import argparse
import collections
import functools
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
TOP = os.path.abspath(os.path.join(ROOT, ".."))
sys.path.insert(0, HERE)
import readable as R  # noqa: E402

BOOK = os.path.join(ROOT, "emulation", "quest.ir2.book")
AB = os.path.join(ROOT, "emulation", "quest.addrbook")
DIS = os.path.join(TOP, "Disassembled", "quest.dis")
SYMS = os.path.join(TOP, "Disassembled", "quest.symbols")
CALLSITES = os.path.join(TOP, "Disassembled", "quest.callsites")

RT_BASE = 0x7017DDBB          # first runtime address (lowest `?`/`X.` symbol)
GAME_END = 0x70180000

# R40 pairs and the R39 unit: second entry -> node name (the first entry's).
MERGE = {"DISPLAY_MAP": "CREATE_MAP",
         "TRANSPORT_SUNDAR": "TRANSPORT_TERRAK",
         "UNLOCK_FILE": "LOCK_FILE"}
SYNTHETIC = {0x7015BD20: ("SQR31?3", "library"),
             0x7015BE23: ("INIT_SHARED_DATA", "game")}
TASK_SPAWN = {0x7015C390: 0x70165713}   # WLDAI 0,C_A_LISTENER -> ?CREATE_TASK


def family(name):
    return name.split(".")[0].split("@")[0]


class Edge:
    __slots__ = ("cls", "site", "src_entry", "src", "tgt_entry", "tgt", "args",
                 "form", "note")

    def __init__(self, cls, site, src_entry, src, tgt_entry, tgt, args, form, note=""):
        self.cls, self.site, self.src_entry, self.src = cls, site, src_entry, src
        self.tgt_entry, self.tgt, self.args, self.form, self.note = tgt_entry, tgt, args, form, note

    def row(self, g):
        return "%-6s %08X  %-30s -> %-30s %s%s" % (
            self.cls, self.site, g.ename(self.src_entry), g.ename(self.tgt_entry) if self.tgt_entry else "?",
            self.form, ("  " + self.note) if self.note else "")


class Graph:
    def __init__(self):
        self.ents = R.load_addrbook(AB)
        for pc, (nm, kind) in SYNTHETIC.items():
            self.ents[pc] = dict(pc=pc, name=nm, flags=kind, stacked=(kind == "library"),
                                 argc=0, frame=0, variant="WSAVS", alloc=0, wfp=0)
        self.pcs = sorted(self.ents)
        self.syms = {}
        for line in open(SYMS):
            p = line.split()
            if len(p) == 2:
                self.syms.setdefault(int(p[0], 16), p[1])
        # nodes
        self.node_of_entry = {}
        self.nodes = collections.OrderedDict()
        for pc in self.pcs:
            e = self.ents[pc]
            fam = family(e["name"])
            node = MERGE.get(fam, fam)
            self.node_of_entry[pc] = node
            n = self.nodes.setdefault(node, dict(name=node, entries=[], units=[], kind="game"))
            if "." in e["name"]:
                (n["units"] if "nocall" in e["flags"] else n["entries"]).append(pc)
            else:
                n["entries"].append(pc)
            if e["variant"] == "WSAVR":
                n["kind"] = "asm"
            if e["flags"] == "library":
                n["kind"] = "library"
            if "nocall" in e["flags"] and "." not in e["name"]:
                n["kind"] = "unit"           # top-level ON-unit / task body
        self.blocks = self._load_book()
        self.dis = self._load_dis()
        self.edges = []
        self._extract()
        self._index()

    # ---- loading -----------------------------------------------------
    def _load_book(self):
        blocks, cur = collections.OrderedDict(), None
        for line in open(BOOK, errors="replace"):
            line = line.rstrip("\n")
            if line.startswith("block "):
                cur = int(line.split()[1], 16)
                blocks[cur] = []
            elif not line.strip():
                cur = None
            elif cur is not None:
                blocks[cur].append(line.strip())
        return blocks

    def _load_dis(self):
        ins = collections.OrderedDict()
        for line in open(DIS, errors="replace"):
            m = re.match(r"^([0-9a-f]{8}) (.*)", line)
            if m:
                ins[int(m.group(1), 16)] = m.group(2).rstrip()
        return ins

    def owner_entry(self, pc):
        """Nearest addrbook entry at or below pc. Every WSAVS in the game is an
        entry (132 = 130 + 2 synthetic; --check verifies), so this is exact at
        entry level except for F10 interleaving, and always right at family."""
        if pc >= GAME_END:
            return None
        lo = None
        for e in self.pcs:
            if e <= pc:
                lo = e
            else:
                break
        return lo

    def node_of_pc(self, pc):
        e = self.owner_entry(pc)
        return self.node_of_entry.get(e) if e is not None else None

    def ename(self, pc):
        return self.ents[pc]["name"] if pc in self.ents else self.syms.get(pc, "%08X" % pc)

    # ---- extraction --------------------------------------------------
    def _xlef_target(self, site):
        """O.ON / I.GOTO take their label from `XLEF 2,[pc+d] (0xADDR)` two
        words before the LJSR (the book lifts it to `ac2 = ...`)."""
        t = self.dis.get(site - 2, "")
        m = re.search(r"XLEF 2,\[pc\+0x[0-9A-Fa-f]+\] \(0x([0-9A-Fa-f]{8})\)", t)
        return int(m.group(1), 16) if m else None

    def _extract(self):
        E = self.edges
        for bpc, stmts in self.blocks.items():
            src_entry = self.owner_entry(bpc)
            src = self.node_of_entry.get(src_entry)
            for s in stmts:
                m = re.match(r"call ([0-9A-F]{8}) args=(\d+) .*site=([0-9A-F]{8})", s)
                if m:
                    self._call(src_entry, src, int(m.group(3), 16), int(m.group(1), 16),
                               int(m.group(2)), "stmt")
                    continue
                m = re.match(r"@([0-9A-F]{8}) (LCALL|XCALL|LJSR) (.*?);(?: # (.*))?$", s)
                if not m:
                    continue
                site, mn, op = int(m.group(1), 16), m.group(2), m.group(3)
                nm = (m.group(4) or "").strip()
                if mn == "LJSR":
                    if nm == "O.ON":
                        t = self._xlef_target(site)
                        E.append(Edge("signal", site, src_entry, src, t,
                                      self.node_of_entry.get(self.owner_entry(t)) if t else None,
                                      0, "embed", "" if t else "UNRESOLVED"))
                    elif nm == "I.GOTO":
                        t = self._xlef_target(site)
                        E.append(Edge("goto", site, src_entry, src, self.owner_entry(t) if t else None,
                                      self.node_of_pc(t) if t else None, 0, "embed",
                                      "label %08X" % t if t else "UNRESOLVED"))
                    continue
                tm = (re.search(r"\(0x([0-9A-F]{8})\)", op) or re.search(r"\[0x([0-9A-F]{8})\]", op))
                argc = re.search(r",(\d+)\s*$", op)
                if not tm:
                    E.append(Edge("call", site, src_entry, src, None, None, 0, "embed",
                                  "UNRESOLVED (no static target): " + op))
                    continue
                self._call(src_entry, src, site, int(tm.group(1), 16),
                           int(argc.group(1)) if argc else 0, "embed")
        for site, body in TASK_SPAWN.items():
            e = self.owner_entry(site)
            E.append(Edge("spawn", site, e, self.node_of_entry[e], body,
                          self.node_of_entry.get(body), 0, "dis", "?CREATE_TASK body"))

    def _call(self, src_entry, src, site, tgt, argc, form):
        if tgt >= RT_BASE:
            return                                    # runtime: not an edge (§9.3)
        tnode = self.node_of_entry.get(tgt)
        note = ""
        if tgt not in self.ents:
            note = "target not an entry"
        if src and self.nodes[src]["kind"] == "asm":
            note = (note + " " if note else "") + "asm-origin (R39)"
        self.edges.append(Edge("call", site, src_entry, src, tgt, tnode, argc, form, note))

    def _index(self):
        self.callees = collections.defaultdict(collections.Counter)
        self.callers = collections.defaultdict(collections.Counter)
        self.intra = []
        for e in self.edges:
            if e.cls != "call" or e.tgt is None:
                continue
            if e.src == e.tgt:
                self.intra.append(e)
                continue
            if self.nodes[e.tgt]["kind"] == "library":
                continue                              # SQR31?3: free like $N (a001 D1)
            self.callees[e.src][e.tgt] += 1
            self.callers[e.tgt][e.src] += 1
        self.block_owner = {b: self.node_of_pc(b) for b in self.blocks}
        self.node_blocks = collections.defaultdict(list)
        for b, n in self.block_owner.items():
            if n:
                self.node_blocks[n].append(b)

    # ---- queries -----------------------------------------------------
    def leaves(self):
        return sorted(n for n in self.nodes if not self.callees[n] and self.nodes[n]["kind"] != "library")

    @functools.lru_cache(None)
    def height(self, n):
        cs = [c for c in self.callees[n] if c != n]
        return 0 if not cs else 1 + max(self.height(c) for c in cs)

    def cycles(self):
        color, out = {}, []

        def dfs(u, stack):
            color[u] = 1
            stack.append(u)
            for v in self.callees[u]:
                if color.get(v, 0) == 1:
                    out.append(stack[stack.index(v):] + [v])
                elif color.get(v, 0) == 0:
                    dfs(v, stack)
            stack.pop()
            color[u] = 2
        for n in self.nodes:
            if color.get(n, 0) == 0:
                dfs(n, [])
        return out

    def strata(self):
        s = collections.defaultdict(list)
        for n in self.nodes:
            if self.nodes[n]["kind"] != "library":
                s[self.height(n)].append(n)
        return {h: sorted(v) for h, v in sorted(s.items())}

    def frontier(self, replaced):
        """Nodes not yet replaced whose every game callee is replaced (or
        asm/library) — the zero-cost candidates under DESIGN §9.4."""
        out = []
        for n in self.nodes:
            if n in replaced or self.nodes[n]["kind"] in ("library", "asm", "unit"):
                continue
            unmet = [c for c in self.callees[n]
                     if c not in replaced and self.nodes[c]["kind"] not in ("library",)]
            if not unmet:
                out.append(n)
        return sorted(out)

    def unresolved(self):
        return [e for e in self.edges if e.tgt_entry is None or "UNRESOLVED" in e.note]

    def unreached(self):
        """Nodes with no inbound edge of ANY class."""
        inb = collections.defaultdict(set)
        for e in self.edges:
            if e.tgt and e.src != e.tgt:
                inb[e.tgt].add(e.cls)
        return {n: sorted(inb[n]) for n in self.nodes if not inb[n]}

    def stmt_count(self, n):
        return sum(len(self.blocks[b]) for b in self.node_blocks[n])

    MARK = [("float", r"@[0-9A-F]{8} (X?[FL]?F[A-Z]+|F[A-Z]+) "),
            ("bit", r"\b(WBTO|WBTZ|WSZB|WSNB|WSTB)\b"),
            ("string", r"^\[@"),
            ("twin", r"\bt@[0-9A-F]{8}"),
            ("claim", r"^(claim|release)\b"),
            ("varying", r"\bcvwn\("),
            ("byteptr", r"\bbp\("),
            ("doloop", r"\b(XNDO|XWDO)\b"),
            ("select", r"\bLDSP\b"),
            ("uplevel", r"\[ac[23]\+0x7FFA\]|wp\(ac[23], -6\)"),
            ("wcmv", r"\bWCMV\b"),
            ("embed", r"^@[0-9A-F]{8} ")]

    def profile(self, n):
        node = self.nodes[n]
        blocks = self.node_blocks[n]
        cnt = collections.Counter()
        rt = collections.Counter()
        for b in blocks:
            for s in self.blocks[b]:
                for k, rx in self.MARK:
                    if re.search(rx, s):
                        cnt[k] += 1
                m = re.match(r"rt_call (\S+?)\(", s)
                if m:
                    rt[m.group(1)] += 1
        return dict(name=n, kind=node["kind"],
                    entries=[self.ename(p) for p in node["entries"]],
                    units=[self.ename(p) for p in node["units"]],
                    height=self.height(n), blocks=len(blocks), stmts=self.stmt_count(n),
                    callees=dict(self.callees[n]), callers=dict(self.callers[n]),
                    callee_sites=sum(self.callees[n].values()),
                    caller_sites=sum(self.callers[n].values()),
                    signals=sum(1 for e in self.edges if e.cls == "signal" and e.src == n),
                    constructs=dict(cnt), rt_calls=dict(rt))

    def coverage(self, resdir):
        hit = set()
        for f in glob.glob(os.path.join(resdir, "*.err")):
            for m in re.finditer(r"first execution of block ([0-9A-F]{8})", open(f, errors="replace").read()):
                hit.add(int(m.group(1), 16))
        per = {}
        for n in self.nodes:
            bs = self.node_blocks[n]
            hb = [b for b in bs if b in hit]
            per[n] = dict(blocks=len(bs), hit=len(hb), stmts=self.stmt_count(n),
                          stmts_hit=sum(len(self.blocks[b]) for b in hb))
        return hit, per

    def check(self):
        out = []
        # 1. every WSAVS/WSAVR in the game range is an entry
        ws = [pc for pc, t in self.dis.items() if re.match(r"WSAV[SR]", t) and pc < RT_BASE]
        missing = [pc for pc in ws if pc not in self.ents]
        out.append("WSAVS/WSAVR in game range: %d; not an entry: %s" % (len(ws), ["%08X" % p for p in missing] or "none"))
        # 2. site count vs quest.callsites
        cs = 0
        for line in open(CALLSITES):
            if line.startswith("call "):
                cs += 1
        mine = [e for e in self.edges if e.cls == "call"]
        synth = [e for e in mine if e.tgt_entry in SYNTHETIC]
        out.append("game call sites: %d (quest.callsites %d + %d to synthetic nodes)" % (len(mine), cs, len(synth)))
        assert len(mine) == cs + len(synth), "site census disagrees with quest.callsites"
        # 3. embed inventory
        emb = collections.Counter()
        for stmts in self.blocks.values():
            for s in stmts:
                m = re.match(r"@[0-9A-F]{8} (LCALL|XCALL|LJSR)\b", s)
                if m:
                    emb[m.group(1)] += 1
        out.append("embeds: %s" % dict(emb))
        # 4. hazard 2: one level of nesting
        bad = [e for e in mine if e.tgt_entry in self.ents and "." in self.ents[e.tgt_entry]["name"] and e.src != e.tgt]
        xc = [e for e in mine if "XCALL" in self.dis.get(e.site, "")]
        out.append("XCALL sites %d, all intra-family: %s; cross-family calls to .N@ entries: %d" % (len(xc), all(e.src == e.tgt for e in xc), len(bad)))
        # 5. signal / goto resolution and family
        for cls in ("signal", "goto"):
            es = [e for e in self.edges if e.cls == cls]
            out.append("%s edges %d, unresolved %d, cross-family %d" % (cls, len(es), sum(1 for e in es if e.tgt_entry is None), sum(1 for e in es if e.tgt and e.src != e.tgt)))
        # 6. indirect calls
        ind = [pc for pc, t in self.dis.items() if pc < RT_BASE and re.match(r"(LCALL|XCALL|LJSR)\b.*\[ac", t)]
        out.append("register-indirect call forms in game range: %d" % len(ind))
        # 7. cycles
        out.append("cycles: %s" % (self.cycles() or "none"))
        out.append("unresolved edges: %d" % len(self.unresolved()))
        out.append("nodes with no inbound edge of any class: %s" % sorted(self.unreached()))
        return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--leaves", action="store_true")
    ap.add_argument("--callers")
    ap.add_argument("--callees")
    ap.add_argument("--depth", nargs="?", const="*")
    ap.add_argument("--strata", action="store_true")
    ap.add_argument("--frontier", help="comma-separated replaced set ('' for the empty set)")
    ap.add_argument("--unresolved", action="store_true")
    ap.add_argument("--profile")
    ap.add_argument("--coverage", metavar="DIR")
    ap.add_argument("--edges", nargs="?", const="*")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()
    g = Graph()
    if a.check:
        print("\n".join(g.check()))
    if a.leaves:
        for n in g.leaves():
            print("%-22s kind=%-7s callers=%d sites=%d stmts=%d" % (n, g.nodes[n]["kind"], len(g.callers[n]), sum(g.callers[n].values()), g.stmt_count(n)))
    if a.callers:
        for c, k in sorted(g.callers[a.callers].items()):
            print("%-22s %d site(s)" % (c, k))
    if a.callees:
        for c, k in sorted(g.callees[a.callees].items()):
            print("%-22s %d site(s)  kind=%s h=%d" % (c, k, g.nodes[c]["kind"], g.height(c)))
    if a.depth:
        for n in (g.nodes if a.depth == "*" else [a.depth]):
            print("%-22s h=%d" % (n, g.height(n)))
    if a.strata:
        for h, ns in g.strata().items():
            print("h=%d (%d): %s" % (h, len(ns), ", ".join(ns)))
    if a.frontier is not None:
        rep = set(x for x in a.frontier.split(",") if x)
        for n in g.frontier(rep):
            print("%-22s h=%d callers=%d stmts=%d" % (n, g.height(n), len(g.callers[n]), g.stmt_count(n)))
    if a.unresolved:
        for e in g.unresolved():
            print(e.row(g))
        for n, _ in g.unreached().items():
            print("node %-22s no inbound edge of any class" % n)
    if a.profile:
        print(json.dumps(g.profile(a.profile), indent=1))
    if a.coverage:
        hit, per = g.coverage(a.coverage)
        print("blocks executed %d of %d in the book" % (len(hit), len(g.blocks)))
        for n, p in sorted(per.items(), key=lambda kv: -kv[1]["hit"] / max(1, kv[1]["blocks"])):
            print("%-22s h=%d blocks %4d/%-4d stmts %5d/%-5d %s" % (
                n, g.height(n), p["hit"], p["blocks"], p["stmts_hit"], p["stmts"],
                "LEAF" if n in g.leaves() else ""))
    if a.edges:
        for e in g.edges:
            if a.edges == "*" or a.edges in (e.src, e.tgt):
                print(e.row(g))
    if a.json:
        print(json.dumps(dict(
            nodes={n: dict(v, entries=[g.ename(p) for p in v["entries"]], units=[g.ename(p) for p in v["units"]],
                           height=g.height(n), stmts=g.stmt_count(n)) for n, v in g.nodes.items()},
            edges=[dict(cls=e.cls, site="%08X" % e.site, src=e.src, src_entry=g.ename(e.src_entry),
                        tgt=e.tgt, tgt_entry=g.ename(e.tgt_entry) if e.tgt_entry else None,
                        args=e.args, form=e.form, note=e.note) for e in g.edges]), indent=1))


if __name__ == "__main__":
    main()
