#!/usr/bin/env python3
"""translate.py — a restricted C subset (game/quest_rt.h) -> ir 6 text (Project 35).

    translate.py game/routines/PICK_X_Y.c --routine PICK_X_Y [-o out.ir]

Three stages (docs/Project35/PLAN.md §5):
  1. pycparser -> a checked subset AST.  Anything outside the subset REFUSES
     loudly (Refuse: the construct, the line, the reason).
  2. AST -> DG-shaped micro-operations through a MODEL of the DG PL/I code
     generator: register choice by the cost model, frame slots by the
     declaration-order + lowest-free-even-slot temp pool with liveness,
     CSE temporaries for scaled subscripts and element addresses.  Every
     rule is numbered R<n> and written down in compiler/CODEGEN_RULES.md
     with the instruction pair that motivated it; the comparator
     (ircmp.py) is what falsifies them (ruling R3: a register rule is a
     declared belief, never a refusal).
  3. micro-ops -> ir 6 text in lower.py's spellings (hexc for every
     constant an instruction carries; `add(x, 1)` for WINC; the Nova 17-bit
     test shape; wp()/bp()/R[] pointer forms; assert + goto for DERR).

Output: an ir 6 book-like file with synthetic block pcs (canonical order,
0x00001000 + 0x10*k), a header line `routine NAME entry lo hi` for ircmp,
and rt_call sites at continuation-4.  Nothing here executes.
"""
import argparse, collections, json, os, re, sys, time

from pycparser import c_ast, parse_file

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, HERE)
import readable as R  # noqa: E402  (addrbook loader)


BIT_FNS = ("BIT", "BIT_SET", "BIT_CLR", "BIT_PUT")


# ---------------------------------------------------------------------------
# Symbolic addresses (P36).
#
# A string statement's address operand is not a register: lower.py prints the
# expression the master's registers UNFOLD to, as computed by
# emulation/tools/string_sites.py's Evaluator and printed by its `ir_word`.
# The classes below are a faithful PORT of that model -- V/_terms/_mk/add/addv
# and ir_const/ir_word (string_sites.py:221-360, :1521-1580) -- restricted to
# the kinds a translated routine can produce (const, load, fp, lin).  It is a
# port on purpose: if the two ever disagree that is a FINDING, not a place to
# adjust this file until the 13 sites happen to line up.
#
# The one behaviour that matters most and is easy to get wrong: `_mk` merges
# the terms of a linear form in an OrderedDict, so THE TERM ORDER IS THE ORDER
# THE TERMS WERE ADDED -- which is the order the instructions ran.  DIED's two
# blanking statements differ by exactly this:
#   7016605B  base already in ac2, XWADD the scaled temp
#             -> (M32[0x70000210] + (sx16(M16[0x70000216]) * 686) - 0x271)
#   7016606F  scaled in ac1, LWADD the base
#             -> ((sx16(M16[0x70000216]) * 686) + M32[0x70000210] - 0x260)
# ---------------------------------------------------------------------------

class Sym:
    """kinds: const(k) | load(a=addr, b=bits, ind) | fp | lin(a=[(c, atom)], k)"""
    __slots__ = ("kind", "a", "b", "k", "ind")

    def __init__(self, kind, a=None, b=None, k=0, ind=False):
        self.kind, self.a, self.b, self.k, self.ind = kind, a, b, k, ind

    def key(self):
        """The merge key of string_sites._mk (its V.show()); only needs to be
        injective over the kinds we build."""
        if self.kind == "const":
            return "c%d" % self.k
        if self.kind == "fp":
            return "fp"
        if self.kind == "load":
            return "%s[%s]%s" % (self.b, self.a.key(), "i" if self.ind else "")
        return "(" + "".join("%+d*%s" % (c, t.key()) for c, t in self.a) + "%+d)" % self.k


def s_const(k):
    return Sym("const", k=k)


def s_load(addr, bits, ind=False):
    return Sym("load", a=addr, b=bits, ind=ind)


def _terms(v):
    if v.kind == "const":
        return [], v.k
    if v.kind == "lin":
        return list(v.a), v.k
    return [(1, v)], 0


def _mk(terms, k):
    merged = collections.OrderedDict()
    for c, t in terms:
        key = t.key()
        if key in merged:
            merged[key] = (merged[key][0] + c, t)
        else:
            merged[key] = (c, t)
    terms = [(c, t) for c, t in merged.values() if c != 0]
    if not terms:
        return s_const(k)
    if len(terms) == 1 and terms[0][0] == 1 and k == 0:
        return terms[0][1]
    return Sym("lin", a=terms, k=k)


def s_addk(a, k):
    if k == 0:
        return a
    t, c = _terms(a)
    return _mk(t, c + k)


def s_addv(a, b):
    ta, ca = _terms(a)
    tb, cb = _terms(b)
    return _mk(ta + tb, ca + cb)


def s_mulk(a, k):
    t, c = _terms(a)
    return _mk([(cc * k, tt) for cc, tt in t], c * k)


def ir_const(k):
    """string_sites.ir_const: small values decimal, large values hex, and the
    sign carried in the text (NOT translate.py's 8-digit hexc)."""
    k &= 0xFFFFFFFF
    if k >= 0x80000000:
        s = k - 0x100000000
        return str(s) if s > -256 else "-0x%X" % -s
    return str(k) if k < 256 else "0x%X" % k


def ir_word(v):
    """string_sites.ir_word, restricted to the kinds a translation builds."""
    if v.kind == "const":
        return ir_const(v.k)
    if v.kind == "fp":
        return "ac3"
    if v.kind == "load":
        inner = ir_word(v.a)
        if v.ind:
            return "R[%s]" % inner
        return ("sx16(M16[%s])" if v.b == 16 else "M32[%s]") % inner
    if v.kind == "lin":
        if len(v.a) == 1 and v.a[0][0] == 1 and v.a[0][1].kind == "fp":
            return "wp(ac3, %d)" % v.k
        parts = []
        for c, t in v.a:
            ts = ir_word(t)
            if c == 1:
                parts.append(("+", ts))
            elif c == -1:
                parts.append(("-", ts))
            elif c > 0:
                parts.append(("+", "(%s * %d)" % (ts, c)))
            else:
                parts.append(("-", "(%s * %d)" % (ts, -c)))
        s = ""
        for i, (sg, ts) in enumerate(parts):
            s += (ts if i == 0 and sg == "+" else (" %s %s" % (sg, ts) if i else "0 - " + ts))
        if v.k:
            s += (" + %s" % ir_const(v.k)) if v.k > 0 else (" - %s" % ir_const(-v.k))
        return "(" + s + ")" if len(parts) + (1 if v.k else 0) > 1 else s
    raise Refuse("symbolic address kind %s is not renderable" % v.kind)


class Refuse(Exception):
    pass


def refuse(node, why):
    coord = getattr(node, "coord", None)
    raise Refuse("REFUSE %s: %s (%s)" % (coord, why, type(node).__name__))


def hexc(v):
    """lower.py's constant spelling for a word an instruction carries."""
    return "0x%08X" % (v & 0xFFFFFFFF)


# ---------------------------------------------------------------------------
# the world layout (declarations.json)
# ---------------------------------------------------------------------------

class Layout:
    def __init__(self, path):
        d = json.load(open(path))
        self.statics = d["statics"]
        self.direct = d["direct"]
        self.tables = d["tables"]

    def static(self, name):
        return self.statics.get(name)

    def direct_field(self, ptr, field):
        f = self.direct.get(ptr, {}).get(field)
        if f is None:
            return None
        return f["K"], f["width"]

    def table(self, name):
        return self.tables.get(name)


# ---------------------------------------------------------------------------
# values the code generator tracks in registers
# ---------------------------------------------------------------------------
# A register holds a "content" descriptor:
#   None                     unknown / garbage                cost 0
#   ("var", key)             cached copy of a memory value    cost 0  (R7)
#   ("dup", "acN")           duplicate of another register     cost 1  (R7)
#   ("const", k)             a known immediate                 cost 2  (R7)
#   ("live", tag)            a value still needed in this stmt cost 3  (R7)
#   ("addr", key)            an address (base pointer / element address) cost 0

COST = {"var": 0, "addr": 0, "kconst": 0, "dup": 1, "const": 2, "live": 3}


class Regs:
    def __init__(self):
        self.c = {"ac0": None, "ac1": None, "ac2": None}
        self.stamp = {"ac0": 0, "ac1": 0, "ac2": 0}
        self.t = 0

    def cost(self, r):
        v = self.c[r]
        return 0 if v is None else COST[v[0]]

    def touch(self, r):
        self.t += 1
        self.stamp[r] = self.t

    def pick(self, avoid=()):
        """R7: the register with the lowest protection cost; ties to the
        lowest-numbered register."""
        best = None
        for r in ("ac0", "ac1", "ac2"):
            if r in avoid:
                continue
            k = (self.cost(r), r)
            if best is None or k < best:
                best = k
        return best[1]

    def set(self, r, content):
        self.c[r] = content
        self.touch(r)
        # a duplicate loses its twin when either is overwritten
        for q, v in self.c.items():
            if q != r and v is not None and v[0] == "dup" and v[1] == r:
                self.c[q] = None

    def find(self, content):
        """A register holding this content; a `var`/`live` pair with the same
        key and a `const`/`kconst` pair with the same value are the same."""
        for r, v in self.c.items():
            if v == content:
                return r
        tag, key = content
        alt = {"var": "live", "live": "var", "const": "kconst", "kconst": "const"}.get(tag)
        if alt:
            for r, v in self.c.items():
                if v == (alt, key):
                    return r
        return None

    def reset(self):
        for r in self.c:
            self.c[r] = None

    def invalidate_var(self, key):
        for r, v in self.c.items():
            if v is not None and v[0] == "var" and v[1] == key:
                self.c[r] = None


# ---------------------------------------------------------------------------
# frame: declared locals + the temp pool (R1-R3)
# ---------------------------------------------------------------------------

class Frame:
    def __init__(self, nlocal_words):
        self.next_local = 2
        self.locals = {}          # name -> (slot, width)
        self.temps = {}           # slot -> tag of the temp living there (None = free)
        self.limit = 2 * nlocal_words

    def declare(self, name, width):
        """R1/R2: declared locals take even slots 2, 4, 6, ... in declaration
        order, one wide slot each, 16-bit locals included."""
        slot = self.next_local
        self.locals[name] = (slot, width)
        self.next_local += 2
        return slot

    hw = None                 # high-water mark (first never-allocated word)
    strings = None            # [(slot, words, tag or None)]

    def alloc_temp(self, tag, busy=()):
        """R3: the lowest free even slot above the declared locals; a slot is
        free when its temp is dead (the caller has decided liveness) and it
        was never part of a string temporary (R3b: the two pools are disjoint,
        REFRESH_SCREEN's 4/18/20 vs 6/22/34)."""
        if self.hw is None:
            self.hw, self.strings = self.next_local, []
        s = self.next_local
        while (s in self.temps and self.temps[s] is not None) or s in busy or self.in_string(s):
            s += 2
        self.temps[s] = tag
        self.hw = max(self.hw, s + 2)
        return s

    def in_string(self, s):
        return any(a <= s < a + n for a, n, _ in self.strings)

    def alloc_string(self, tag, words):
        """R3b: a string temporary reuses a dead string temporary of sufficient
        size, else takes the frame's high-water mark (a bump allocation)."""
        if self.hw is None:
            self.hw, self.strings = self.next_local, []
        for i, (a, n, t) in enumerate(self.strings):
            if t is None and n >= words:
                self.strings[i] = (a, n, tag)
                return a
        a = self.hw + (self.hw & 1)
        self.strings.append((a, words, tag))
        self.hw = a + words
        return a

    def free_string(self, slot):
        self.strings = [(a, n, None if a == slot else t) for a, n, t in self.strings]

    def free_temp(self, slot):
        self.temps[slot] = None

    def slot_of_temp(self, tag):
        for s, t in self.temps.items():
            if t == tag:
                return s
        return None


# ---------------------------------------------------------------------------
# expression typing (widths): 16-bit values come from int16_t; 32 from int32_t
# ---------------------------------------------------------------------------

class Val:
    """A value the generator has produced: where it is and how wide."""
    __slots__ = ("kind", "reg", "text", "width", "const")

    def __init__(self, kind, reg=None, text=None, width=32, const=None):
        self.kind, self.reg, self.text, self.width, self.const = kind, reg, text, width, const
        # kind: "reg" (in a register), "mem" (a memory operand text usable by an
        # instruction: M32[...] / sx16(M16[...])), "const"


# ---------------------------------------------------------------------------
# the memory image (Disassembled/quest.mem): literal text -> byte address.
# Used only so the emitted `[@0xW:b, "text"]` carries the real address (an
# equivalence-4 nicety and a check that the source text exists in the image).
# ---------------------------------------------------------------------------

class MemImage:
    def __init__(self, path):
        self.words = {}
        for line in open(path, errors="replace"):
            m = re.match(r"^([0-9A-Fa-f]{8})\s+((?:[0-9A-Fa-f]{4}\s+)+)", line)
            if not m:
                continue
            a = int(m.group(1), 16)
            for k, w in enumerate(m.group(2).split()):
                self.words[a + k] = int(w, 16)
        self.blob, self.base = None, None

    def find(self, text):
        """-> (word address, byte) of the first occurrence, or None."""
        if self.blob is None:
            keys = sorted(self.words)
            if not keys:
                return None
            self.base = keys[0]
            top = keys[-1]
            blob = bytearray(2 * (top - self.base + 1))
            for a, w in self.words.items():
                i = 2 * (a - self.base)
                blob[i], blob[i + 1] = w >> 8, w & 0xFF
            self.blob = bytes(blob)
        data = text.encode("latin-1")
        i = self.blob.find(data)
        if i < 0:
            return None
        return self.base + i // 2, i % 2


# ---------------------------------------------------------------------------
# the translator
# ---------------------------------------------------------------------------

class Block:
    def __init__(self, label):
        self.label = label
        self.lines = []
        self.term = None       # ("goto", [labels], expr) | ("rt_call", text) | ("ret",) | ("instr", text)
        self.pc = None


class Translator:
    def __init__(self, layout, addrbook, routine_name):
        self.L = layout
        self.entries = R.load_addrbook(addrbook)
        self.entry = None
        for pc, e in self.entries.items():
            if e["name"] == routine_name:
                self.entry, self.entry_pc = e, pc
        if self.entry is None:
            raise Refuse("routine %s not in the addrbook" % routine_name)
        self.name = routine_name
        self.addrbook_by_name = {}
        for pc, e in self.entries.items():
            d = dict(e); d["pc"] = pc
            self.addrbook_by_name.setdefault(e["name"], d)
        self.blocks = []
        self.cur = None
        self.labels = {}       # C label -> Block
        self.nlabel = 0
        self.regs = Regs()
        self.frame = None
        self.args = {}         # param name -> (N, width, const)
        self.arg_count_param = None
        self.cse = {}          # CSE temps: key -> dict(slot, last_stmt)
        self.elem_sym = {}     # ekey -> the symbolic element address (P36)
        self.stmt_index = 0
        self.uses = {}         # pre-pass: key -> [stmt indices]
        self.var_refs = {}     # pre-pass: stmt index -> {variable name: reference count}
        self.ref_seq = {}      # element (table, var) -> running reference count (R10 positions)
        self.elem_refs = {}    # pre-pass: stmt index -> {table:{subscript: count}}

    # -- blocks ---------------------------------------------------------
    def new_block(self, label=None):
        if label is None:
            label = "B%d" % self.nlabel
            self.nlabel += 1
        b = Block(label)
        self.blocks.append(b)
        return b

    def start(self, b):
        """Make b current; if the current block did not terminate, it falls
        through with `goto [b] 0`."""
        if self.cur is not None and self.cur.term is None:
            if not self.cur.lines and self.cur in self.blocks and self.cur.label not in self.labels \
                    and not self.is_target(self.cur.label):
                self.blocks.remove(self.cur)        # an empty block is not a block
            elif self.cur.label == "entry" and len(self.cur.lines) == 1:
                self.cur.term = ("fall", b.label)   # the instruction is the exit
            else:
                self.cur.term = ("goto", [b.label], "0")
        self.cur = b

    fp_dirty = False

    def is_target(self, label):
        for b in self.blocks:
            t = b.term
            if t and t[0] == "goto" and label in t[1]:
                return True
            if t and t[0] == "rt_call" and t[1].endswith("site=" + label):
                return True
            if t and t[0] in ("call", "instr") and t[2] == label:
                return True
            if t and t[0] == "fall" and t[1] == label:
                return True
        return False

    def emit(self, text, uses_fp=None):
        """R24: a string statement (WCMV) leaves ac3 pointing into the source;
        `ac3 = wfp` (LDAFP) is emitted lazily, just before the next statement
        that addresses the frame (REFRESH_SCREEN 70176ABA vs 70176ADF)."""
        if uses_fp is None:
            uses_fp = "ac3" in text
        if self.fp_dirty and uses_fp:
            self.cur.lines.append("ac3 = wfp")
            self.fp_dirty = False
        self.cur.lines.append(text)

    def terminate(self, term):
        self.cur.term = term
        self.cur = None

    def c_label(self, name):
        if name not in self.labels:
            self.labels[name] = Block(name)
        return self.labels[name]

    # -- entry ----------------------------------------------------------
    protos = {}

    def collect_protos(self, ast):
        for ext in ast.ext:
            if isinstance(ext, c_ast.Decl) and isinstance(ext.type, c_ast.FuncDecl):
                params = ext.type.args.params if ext.type.args else []
                ws = []
                for p in params:
                    t = p.type
                    if isinstance(t, c_ast.PtrDecl) and isinstance(t.type, c_ast.TypeDecl) \
                            and isinstance(t.type.type, c_ast.IdentifierType):
                        n = t.type.type.names[-1]
                        ws.append({"int16_t": 16, "int32_t": 32}.get(n, 32))
                    else:
                        ws.append(32)
                self.protos[ext.name] = ws

    def translate(self, fdef):
        decl = fdef.decl
        params = decl.type.args.params if decl.type.args else []
        n = 0
        for p in params:
            if isinstance(p.type, c_ast.TypeDecl) and p.name == "arg_count":
                self.arg_count_param = p.name   # R12: the marker word
                continue
            if not isinstance(p.type, c_ast.PtrDecl):
                refuse(p, "parameters are by-reference pointers (PL/I)")
            n += 1
            width = self.width_of(p.type.type)
            const = "const" in (p.type.type.quals or [])
            self.args[p.name] = (n, width, const)
        if n != self.entry["argc"]:
            raise Refuse("%s declares %d arguments; the addrbook says argc %d" % (self.name, n, self.entry["argc"]))
        self.frame = Frame(self.entry["frame"])
        body = fdef.body.block_items or []
        # declarations first (R1)
        stmts = []
        for it in body:
            if isinstance(it, c_ast.Decl):
                if it.init is not None:
                    refuse(it, "initialised locals are not in the subset")
                self.frame.declare(it.name, self.width_of(it.type))
            else:
                stmts.append(it)
        self.prepass(stmts)
        # entry block: the WSAVS instruction alone (its fall-through is the exit)
        # entry block: the WSAVS instruction, then the first statements; when
        # a label follows at once the instruction is the block's exit (the
        # book's 701761E7) — start() handles that with a `fall` terminator
        e = self.new_block("entry")
        self.cur = e
        self.emit("@%08X WSAVS 0x%04X;" % (self.entry_pc, self.entry["frame"]))
        for k, st in enumerate(stmts):
            self.begin_stmt(st)
            self.is_last_stmt = (k == len(stmts) - 1)
            self.stmt(st)
        if self.cur is not None and self.cur.term is None:
            if not self.cur.lines and self.cur.label not in self.labels:
                self.blocks.remove(self.cur)         # nothing follows the last statement
            else:
                self.terminate(("ret",))             # PL/I END = return
        return self.render()

    def begin_stmt(self, node):
        """R8b: a value marked live for the previous statement is just a
        cached copy now (cost 0); the per-statement reference counters and
        the freed-slot set restart."""
        self.stmt_index = self.stmt_no[id(node)]
        for r, v in self.regs.c.items():
            if v is not None and v[0] == "live":
                if r == self.keep_live:
                    continue          # R21b: the loop register stays protected through the body's first statement
                self.regs.c[r] = ("var", v[1])
            elif v is not None and v[0] == "const":
                self.regs.c[r] = ("kconst", v[1])   # R7a: known but unprotected
        self.keep_live = None
        self._freed = set()
        self._refs_done = {}
        self._var_done = {}
        # R7b: a cached variable this statement will read is protected from the start
        refs = self.var_refs.get(self.stmt_index, {})
        for r, v in self.regs.c.items():
            if v is not None and v[0] == "var" and isinstance(v[1], tuple) and len(v[1]) == 2 \
                    and v[1][0] in ("arg", "local") and refs.get(v[1][1], 0) > 0:
                self.regs.c[r] = ("live", v[1])
        if self.frame.strings:
            for a, n, t in self.frame.strings:
                if t is not None:
                    self.frame.free_string(a)

    def width_of(self, tdecl):
        while isinstance(tdecl, (c_ast.PtrDecl, c_ast.ArrayDecl)):
            tdecl = tdecl.type
        names = tdecl.type.names if isinstance(tdecl.type, c_ast.IdentifierType) else None
        if names is None:
            refuse(tdecl, "struct locals are not in the subset")
        if names[-1] in ("int16_t",):
            return 16
        if names[-1] in ("int32_t", "int"):
            return 32
        refuse(tdecl, "type %s not in the subset" % " ".join(names))

    # -- pre-pass: where are subscripts / elements referenced (for R3/R9/R10 liveness) --
    @staticmethod
    def sub_stmts(st):
        """The statement children of a statement (its own index precedes theirs)."""
        if isinstance(st, c_ast.For):
            return [st.stmt]
        if isinstance(st, c_ast.If):
            return [x for x in (st.iftrue, st.iffalse) if x is not None]
        if isinstance(st, c_ast.Label):
            return [st.stmt] if st.stmt is not None else []
        if isinstance(st, c_ast.Compound):
            return list(st.block_items or [])
        return []

    def number_stmts(self, stmts):
        """Flat statement numbering (a header, then its statement children)."""
        for st in stmts:
            self.stmt_no[id(st)] = len(self.stmt_no)
            self.number_stmts(self.sub_stmts(st))

    def prepass(self, stmts):
        self.stmt_no = {}
        self.number_stmts(stmts)
        bitwords = set()
        def mark(n):
            if isinstance(n, c_ast.FuncCall) and getattr(n.name, "name", "") in BIT_FNS:
                w = n.args.exprs[0]
                if isinstance(w, c_ast.StructRef) and isinstance(w.name, c_ast.ArrayRef):
                    bitwords.add(id(w.name))
            for _, ch in n.children():
                mark(ch)
        for st in stmts:
            mark(st)
        def walk(n, i):
            # a BIT word's element reference feeds the BIT-BASE key, not the
            # R9 scaled key: the compiler consumes i*stride into 16*i*stride
            # and the two temps are different values (70166046 saves P*686 to
            # slot 10; 70166296 saves 16*P*686 to slot 8).
            if isinstance(n, c_ast.ArrayRef) and isinstance(n.name, c_ast.ID) and id(n) in bitwords:
                for _, ch in n.children():
                    walk(ch, i)
                return
            if isinstance(n, c_ast.ArrayRef) and isinstance(n.name, c_ast.ID):
                sub = self.subscript_key(n.subscript)
                if sub is not None:
                    self.uses.setdefault(("scaled", n.name.name, sub), []).append(i)
                    d = self.elem_refs.setdefault(i, {})
                    d[(n.name.name, sub)] = d.get((n.name.name, sub), 0) + 1
            if isinstance(n, c_ast.FuncCall) and getattr(n.name, "name", "") in BIT_FNS:
                w = n.args.exprs[0]
                if isinstance(w, c_ast.StructRef) and isinstance(w.name, c_ast.ArrayRef):
                    sub = self.subscript_key(w.name.subscript)
                    if sub is not None:
                        self.uses.setdefault(("bitbase", w.name.name.name, sub), []).append(i)
            if isinstance(n, c_ast.ID):
                self.uses.setdefault(("id", n.name), []).append(i)
                d = self.var_refs.setdefault(i, {})
                d[n.name] = d.get(n.name, 0) + 1
            if isinstance(n, c_ast.Assignment) and isinstance(n.lvalue, c_ast.ID):
                self.uses.setdefault(("assign", n.lvalue.name), []).append(i)
            for _, ch in n.children():
                walk(ch, i)
        def visit(lst):
            for st in lst:
                i = self.stmt_no[id(st)]
                subs = self.sub_stmts(st)
                if subs:
                    for _, ch in st.children():
                        if not isinstance(ch, c_ast.Node) or any(ch is x for x in subs):
                            continue
                        walk(ch, i)
                    visit(subs)
                else:
                    walk(st, i)
        visit(stmts)

    def subscript_key(self, node):
        if isinstance(node, c_ast.FuncCall) and node.name.name == "SUB":
            node = node.args.exprs[0]
        if isinstance(node, c_ast.ID):
            return node.name
        return None

    def var_changes_between(self, name, i, j):
        """Is variable `name` assigned in statements (i, j]?"""
        return any(i < k <= j for k in self.uses.get(("assign", name), []))

    def last_use_of(self, key):
        u = self.uses.get(key, [])
        return max(u) if u else -1

    # -- statements -----------------------------------------------------
    def stmt(self, st):
        if isinstance(st, c_ast.Label):
            b = self.c_label(st.name)
            self.blocks.append(b)
            self.start(b)
            self.regs.reset()          # R8: register knowledge resets at a label
            if st.stmt is not None:
                self.stmt(st.stmt)
            return
        if isinstance(st, c_ast.Goto):
            self.terminate(("goto", [self.c_label(st.name).label], "0"))
            self.start(self.new_block())
            return
        if isinstance(st, c_ast.Return):
            if st.expr is not None:
                refuse(st, "value returns are not in the subset (PL/I procedures here return nothing)")
            self.terminate(("ret",))
            self.start(self.new_block())
            return
        if isinstance(st, c_ast.If):
            return self.if_stmt(st)
        if isinstance(st, c_ast.Compound):
            for it in st.block_items or []:
                self.begin_stmt(it)
                self.stmt(it)
            return
        if isinstance(st, c_ast.For):
            return self.for_stmt(st)
        if isinstance(st, c_ast.Continue):
            if not self.loop_stack:
                refuse(st, "continue outside a loop")
            self.terminate(("goto", [self.loop_stack[-1]["incr"].label], "0"))
            self.start(self.new_block())
            return
        if isinstance(st, c_ast.Assignment):
            return self.assign(st)
        if isinstance(st, c_ast.FuncCall):
            if st.name.name in ("BIT_SET", "BIT_CLR", "BIT_PUT"):
                return self.bit_stmt(st)
            return self.call_stmt(st)
        refuse(st, "statement kind not in the subset")

    def one_word(self, st):
        """R13: the DG compiler's `IF c THEN S` for a one-instruction S emits
        skip-if-NOT-c over S.  One-word statements: goto, return, x = -x."""
        if isinstance(st, c_ast.Compound):
            if len(st.block_items or []) != 1:
                return False
            st = st.block_items[0]
        return isinstance(st, (c_ast.Goto, c_ast.Return, c_ast.Continue))

    def if_stmt(self, st):
        if st.iffalse is not None or not self.one_word(st.iftrue):
            return self.if_multi(st)
        then = st.iftrue.block_items[0] if isinstance(st.iftrue, c_ast.Compound) else st.iftrue
        self.begin_stmt(then) if id(then) in self.stmt_no else None
        # the test, negated (skip when NOT c)
        test = self.condition(st.cond, negate=True)
        pos_test = self.NEGTEST.get(test)
        head = self.cur
        then_b = self.new_block()
        cont_b = self.new_block()
        self.terminate(("goto", [then_b.label, cont_b.label], test))
        # R13c: record the diamond so the R19 pass can INVERT it if the
        # one-word S turns out to need an XJMP (70166057: MOV.L# skips over
        # `WBR cont` and falls into `XJMP reincarnate`).
        if isinstance(then, c_ast.Goto) and pos_test is not None:
            self.if_diamonds.append((head, then_b, cont_b, pos_test))
        saved = dict(self.regs.c), dict(self.regs.stamp)
        self.cur = then_b
        self.stmt(then)          # terminates with goto/ret and starts a stray block
        stray = self.cur
        if stray is not None and not stray.lines and stray.term is None:
            self.blocks.remove(stray)
        self.cur = cont_b
        # R8a: knowledge persists past a one-word THEN that writes no register
        self.regs.c, self.regs.stamp = saved

    loop_stack = []

    def for_stmt(self, st):
        """R21: the DG DO-loop idiom.  `for (v = a; v <= lim; v++)` with a
        16-bit v:  v = a through the loop register ac1 (NLDAI 1 / XNSTA 1);
        test `goto [exit_branch, body] (ac1 <=s lim)`; the exit branch is
        `goto [after] 0`, or `ret` when nothing follows the loop (R22: the
        compiler's branch-to-END is a WRTN); a `goto body` block skips the
        increment block; the increment block (the `continue` target) is
        XNDO: ac1 = lim; t1 = nadd(M16[v], 1); M16[v] = t1; t2 = (t1 >s ac1);
        ac1 = t1; goto [body, exit] t2.  (UPDATE_SCREENS 7017D635..7017D64A.)"""
        init, cond, nxt = st.init, st.cond, st.next
        if not (isinstance(init, c_ast.Assignment) and isinstance(init.lvalue, c_ast.ID)
                and isinstance(cond, c_ast.BinaryOp) and cond.op == "<=" and isinstance(cond.left, c_ast.ID)
                and cond.left.name == init.lvalue.name and isinstance(nxt, c_ast.UnaryOp)
                and nxt.op in ("p++", "++") and isinstance(nxt.expr, c_ast.ID) and nxt.expr.name == init.lvalue.name):
            refuse(st, "only `for (v = a; v <= lim; v++)` (PL/I DO v = a TO lim) is in the subset")
        vname = init.lvalue.name
        if vname not in self.frame.locals or self.frame.locals[vname][1] != 16:
            refuse(st, "the DO variable must be a 16-bit local")
        vslot = self.frame.locals[vname][0]
        a = self.value(init.rvalue, want_reg=False)
        if a.kind != "const":
            refuse(st, "DO initial value must be a constant (only form seen)")
        const_lim = isinstance(cond.right, c_ast.Constant)
        if not const_lim:
            if not isinstance(cond.right, c_ast.ID) or cond.right.name not in self.frame.locals:
                refuse(st, "DO limit must be a local variable or a constant")
            lim = self.value(cond.right, want_reg=True)      # the limit first: it is live for the test
            self.regs.c[lim.reg] = ("live", "lim")
            lslot = self.frame.locals[cond.right.name][0]
        # the loop register: the cost model's pick (ac1 in UPDATE_SCREENS where
        # ac0 holds the limit, ac0 in REFRESH_SCREEN)
        lr = self.regs.pick()
        self.emit("%s = %s" % (lr, hexc(a.const)))
        self.regs.set(lr, ("const", a.const))
        self.emit("M16[wp(ac3, %d)] = trunc16(%s)" % (vslot, lr))
        incr_b, body_b, after_b = self.new_block(), self.new_block(), self.new_block()
        last_stmt = self.is_last_stmt
        if const_lim:
            # R21a: constant bounds — no entry test; the init block jumps over
            # the increment block straight into the body (REFRESH_SCREEN 70176B06)
            self.terminate(("goto", [body_b.label], "0"))
        else:
            exit_b, skip_b = self.new_block(), self.new_block()
            self.terminate(("goto", [exit_b.label, skip_b.label], "(%s <=s %s)" % (lr, lim.reg)))
            self.cur = exit_b
            self.terminate(("ret",) if last_stmt else ("goto", [after_b.label], "0"))
            self.cur = skip_b
            self.terminate(("goto", [body_b.label], "0"))
        # increment block (continue target): XNDO with the limit in the loop register
        self.cur = incr_b
        if const_lim:
            self.emit("%s = %s" % (lr, hexc(int(cond.right.value, 0))))
        else:
            self.emit("%s = sx16(M16[wp(ac3, %d)])" % (lr, lslot))
        self.emit("t1 = nadd(M16[wp(ac3, %d)], 1)" % vslot)
        self.emit("M16[wp(ac3, %d)] = t1" % vslot)
        self.emit("t2 = (t1 >s %s)" % lr)
        self.emit("%s = t1" % lr)
        self.terminate(("goto", [body_b.label, after_b.label], "t2"))
        # body: at its head only the loop register is known (and protected)
        self.cur = body_b
        self.regs.reset()
        self.regs.set(lr, ("live", ("local", vname)))
        self.loop_var, self.loop_reg = vname, lr
        self.loop_stack.append(dict(incr=incr_b, after=after_b))
        self.fp_dirty = False
        self.keep_live = lr
        self.run_body(st.stmt)
        self.loop_stack.pop()
        self.loop_var = None
        if self.cur is not None and self.cur.term is None:
            self.terminate(("goto", [incr_b.label], "0"))
        elif self.cur is not None and not self.cur.lines and self.cur.term is None:
            self.blocks.remove(self.cur)
        stray = self.cur
        if stray is not None and not stray.lines and stray.term is None and stray in self.blocks:
            self.blocks.remove(stray)
        if last_stmt:
            self.cur = after_b
            self.terminate(("ret",))
            self.cur = None
        else:
            self.cur = after_b
            self.regs.reset()

    loop_var = None
    loop_reg = None
    keep_live = None
    is_last_stmt = False

    def if_multi(self, st):
        """R13b: `if (c) {body} [else {alt}]` with a body longer than one
        instruction: skip-if-c over a `goto else` block; the body ends with
        `goto after` when an else part follows (REFRESH_SCREEN 70176A93..ABA)."""
        test = self.condition(st.cond, negate=False)
        skip_b, body_b, after_b = self.new_block(), self.new_block(), self.new_block()
        else_b = self.new_block() if st.iffalse is not None else after_b
        self.terminate(("goto", [skip_b.label, body_b.label], test))
        self.cur = skip_b
        self.terminate(("goto", [else_b.label], "0"))
        self.cur = body_b
        self.regs.reset()
        self.run_body(st.iftrue)
        if self.cur is not None and self.cur.term is None:
            if st.iffalse is not None:
                self.terminate(("goto", [after_b.label], "0"))
            else:
                self.join(after_b)
                self.regs.reset()
                return
        if st.iffalse is not None:
            self.cur = else_b
            self.regs.reset()
            self.run_body(st.iffalse)
            if self.cur is not None and self.cur.term is None:
                self.join(after_b)
                self.regs.reset()
                return
        self.cur = after_b
        self.regs.reset()

    def join(self, after_b):
        """Fall into after_b from the current block.  When the current block
        is an empty call continuation (a target), after_b IS that block: the
        code after the if starts at the continuation (REFRESH_SCREEN 70176ABA)."""
        cur = self.cur
        if cur is not None and cur.term is None and not cur.lines and self.is_target(cur.label):
            for b in self.blocks:
                t = b.term
                if t and t[0] == "goto" and after_b.label in t[1]:
                    b.term = (t[0], [cur.label if l == after_b.label else l for l in t[1]], t[2])
            if after_b in self.blocks:
                self.blocks.remove(after_b)
            return
        self.start(after_b)

    def run_body(self, body):
        if isinstance(body, c_ast.Compound):
            for it in body.block_items or []:
                self.begin_stmt(it)
                self.stmt(it)
        else:
            self.begin_stmt(body)
            self.stmt(body)

    # -- conditions --------------------------------------------------------
    NEG = {"==": "!=", "!=": "==", "<": ">=", ">=": "<", ">": "<=", "<=": ">"}
    SKIP = {"==": "==", "!=": "!=", "<": "<s", "<=": "<=s", ">": ">s", ">=": ">=s"}

    if_diamonds = []
    NEGTEST = {}

    def condition(self, cond, negate):
        t = self._condition(cond, negate)
        if negate:
            self.NEGTEST[t] = self._flip(t)
        return t

    @staticmethod
    def _flip(t):
        """The textual complement of a lowered test (the skip's other sense)."""
        for a, b in ((" == ", " != "), (" != ", " == "), (" >s ", " <=s "),
                     (" <=s ", " >s "), (" >u ", " <=u "), (" <=u ", " >u "),
                     (" <s ", " >=s "), (" >=s ", " <s ")):
            if t.count(a) == 1:
                return t.replace(a, b)
        return None

    def _condition(self, cond, negate):
        if isinstance(cond, c_ast.UnaryOp) and cond.op == "!":
            return self.condition(cond.expr, not negate)
        if isinstance(cond, c_ast.FuncCall) and cond.name.name == "BIT":
            # R29: a bit reference is ALWAYS materialised to 0 / -1 (WSUB v,v;
            # WSZB; WADC v,v) and the condition is then the R14b sign test on
            # it -- the compiler does not fold the skip into the branch.
            # 70166054..57: WSUB 0,0; WSZB 2,1; WADC 0,0; MOV.L# 0,0,SNC.
            vr = self.bit_value_reg(cond)
            t = self.tplace()
            self.emit("%s = ((%s & 0xFFFF) | lsh(c, 16))" % (t, vr))
            return "((lsh(%s, -15) & 1) %s 1)" % (t, "!=" if negate else "==")
        if not isinstance(cond, c_ast.BinaryOp) or cond.op not in self.NEG:
            refuse(cond, "condition must be a comparison")
        op = self.NEG[cond.op] if negate else cond.op
        lhs = self.value(cond.left, want_reg=True)
        self.flush_elem_save()          # R10: the save precedes the test
        rhs = cond.right
        if isinstance(rhs, c_ast.Constant):
            k = int(rhs.value, 0)
            if k == 0 and lhs.width == 16 and op in ("==", "!="):
                # R14: a 16-bit value tested against zero uses the Nova
                # MOV# r,r,SZR/SNR form (t = 17-bit source; test the low 16)
                t = self.tplace()
                self.emit("%s = ((%s & 0xFFFF) | lsh(c, 16))" % (t, lhs.reg))
                return "((%s & 0xFFFF) %s 0)" % (t, op)
            if k == 0 and lhs.width == 16 and op in ("<", ">="):
                # R14b: a 16-bit sign test is MOV.L# r,r,SNC/SZC (the sign bit)
                t = self.tplace()
                self.emit("%s = ((%s & 0xFFFF) | lsh(c, 16))" % (t, lhs.reg))
                return "((lsh(%s, -15) & 1) %s 1)" % (t, "==" if op == "<" else "!=")
            # R15: wide skip with immediate, constant spelled hexc
            return "(%s %s %s)" % (lhs.reg, self.SKIP[op], hexc(k))
        r = self.value(rhs, want_reg=True)
        return "(%s %s %s)" % (lhs.reg, self.SKIP[op], r.reg)

    def tplace(self):
        n = sum(1 for l in self.cur.lines if re.match(r"t\d+ =", l)) + 1
        return "t%d" % n

    # -- assignment ---------------------------------------------------------
    def assign(self, st):
        if st.op != "=":
            refuse(st, "compound assignment not in the subset")
        lv = st.lvalue
        if isinstance(st.rvalue, c_ast.FuncCall) and "$" in st.rvalue.name.name:
            v = self.rt_call(st.rvalue)          # result in ac0 (RTConventions)
        elif isinstance(lv, c_ast.ArrayRef):
            return self.indexed_store(lv, st.rvalue)
        else:
            v = self.value(st.rvalue, want_reg=True)
        self.flush_elem_save()
        self.store(lv, v)

    def narrow_check(self, lv, v):
        """P36 ruling 1: no silent 32->16 narrowing.  The DG compiler emits a
        CHECKED convert (CVWN) here; the source of record must say so with
        `cvwn(e)`, so a 32-bit value reaching a 16-bit destination without one
        is a REFUSAL, not an inserted instruction."""
        if v.width == 32:
            refuse(lv, "32-bit value stored to a 16-bit destination without "
                       "an explicit cvwn() (P36 ruling 1)")

    def store(self, lv, v):
        """Store register value v into lvalue lv (R16: a 32-bit result
        narrowed to a 16-bit target is cvwn'd first; the store is trunc16)."""
        if isinstance(lv, c_ast.ID) and lv.name in self.frame.locals:
            slot, width = self.frame.locals[lv.name]
            r = self.to_reg(v)
            if width == 16:
                self.narrow_check(lv, v)
                self.emit("M16[wp(ac3, %d)] = trunc16(%s)" % (slot, r))
            else:
                self.emit("M32[wp(ac3, %d)] = %s" % (slot, r))
            self.regs.invalidate_var(("local", lv.name))
            self.regs.set(r, ("var", ("local", lv.name)))
            return
        if isinstance(lv, c_ast.UnaryOp) and lv.op == "*" and isinstance(lv.expr, c_ast.ID) and lv.expr.name in self.args:
            n, width, const = self.args[lv.expr.name]
            if const:
                refuse(lv, "write through a const parameter")
            r = self.to_reg(v)
            if width == 16:
                self.narrow_check(lv, v)
                self.emit("M16[R[ac3 + -%d]] = trunc16(%s)" % (10 + 2 * n, r))
            else:
                self.emit("M32[R[ac3 + -%d]] = %s" % (10 + 2 * n, r))
            self.regs.invalidate_var(("arg", lv.expr.name))
            self.regs.set(r, ("var", ("arg", lv.expr.name)))
            return
        refuse(lv, "lvalue form not in the subset")

    def indexed_store(self, lv, rhs):
        """R23: `T[i].f[r][c] = e` — the address first (each subscript
        checked, scaled, added to the running partial sum, the partial saved
        to a temp while more dimensions follow; then the base; WMOV to ac2),
        then the value, then the store (UPDATE_SCREENS 7017D67A..7017D6A7)."""
        subs = []
        node = lv
        while isinstance(node, c_ast.ArrayRef):
            subs.append(node.subscript)
            node = node.name
        subs.reverse()
        if not (isinstance(node, c_ast.StructRef) and node.type == "." and isinstance(node.name, c_ast.ArrayRef)):
            refuse(lv, "indexed store must be TABLE[i].field[...]")
        tname = node.name.name.name
        t = self.L.table(tname)
        fld = t["fields"].get(node.field.name)
        if fld is None or "dims" not in fld:
            refuse(lv, "%s.%s is not a multi-dimensional field" % (tname, node.field.name))
        if len(subs) != len(fld["dims"]):
            refuse(lv, "wrong number of subscripts")
        vname = self.subscript_key(node.name.subscript)
        skey = ("scaled", tname, vname)
        i = self.stmt_index
        partial = None      # text of the running sum's memory operand
        if skey in self.cse and self.cse[skey]["slot"] is not None:
            partial = self.cse[skey]["slot"]
        else:
            refuse(lv, "indexed store needs the scaled subscript in a temp (only form seen)")
        base_addr = self.L.static(t["base"])["addr"]
        r = None
        for d, (sub, stride) in enumerate(zip(subs, fld["strides"])):
            v = self.value(sub, want_reg=True)
            r = v.reg
            if stride == 2:
                self.emit("%s = add(%s, %s)" % (r, r, r))                  # WADD r,r  (x2)
            else:
                kr = self.regs.pick(avoid=(r,))
                self.emit("%s = %s" % (kr, hexc(stride)))
                self.regs.set(kr, ("const", stride))
                self.emit("%s = mul(%s, %s)" % (r, r, kr))
            self.emit("%s = add(%s, M32[wp(ac3, %d)])" % (r, r, partial))      # XWADD
            self.regs.set(r, ("live", "partial"))
            if self.last_use_of(skey) <= i and self.frame.temps.get(partial) == skey:
                self.frame.free_temp(partial)
                self._freed.add(partial)
                self.cse[skey]["slot"] = None
            if d < len(subs) - 1:
                slot = self.frame.alloc_temp(("partial", d))
                self.emit("M32[wp(ac3, %d)] = %s" % (slot, r))
                partial = slot
            else:
                if self.frame.temps.get(partial, None) is not None and self.frame.temps[partial][0] == "partial":
                    self.frame.free_temp(partial)
        self.emit("%s = add(%s, M32[%s])" % (r, r, hexc(base_addr)))         # LWADD
        if r != "ac2":
            self.emit("ac2 = %s" % r)
            self.regs.set("ac2", ("dup", r))
        self.regs.c["ac2"] = ("live", "store-addr")
        v = self.value(rhs, want_reg=True, avoid=("ac2",))
        K, width = fld["K"], fld["width"]
        if width == 32:
            self.emit("M32[wp(ac2, %d)] = %s" % (K, v.reg))
        else:
            self.narrow_check(lv, v)
            self.emit("M16[wp(ac2, %d)] = trunc16(%s)" % (K, v.reg))
        self.regs.c["ac2"] = ("addr", ("stored",))

    # -- PL/I bit references (P36, R26-R29) ----------------------------------
    def bit_word(self, node):
        """Decompose a BIT*() word argument into (tname, t, subscript, K).
        Only a record element field is in the subset (DIED's 25 bit ops are
        all PLAYER[SUB(PLAYER_NUM,10)].<word>)."""
        if not (isinstance(node, c_ast.StructRef) and node.type == "."
                and isinstance(node.name, c_ast.ArrayRef)):
            refuse(node, "BIT(): the word must be a record element field T[i].f")
        tname = node.name.name.name
        t = self.L.table(tname)
        if t is None:
            refuse(node, "table %s not in declarations" % tname)
        fld = t["fields"].get(node.field.name)
        if fld is None:
            refuse(node, "%s.%s not in declarations" % (tname, node.field.name))
        if fld["width"] != 16:
            refuse(node, "BIT(): %s.%s is not a 16-bit word" % (tname, node.field.name))
        return tname, t, node.name.subscript, fld["K"]

    def bit_address(self, wordnode, nbit):
        """R26: the bit address is `16 * <scaled subscript> + (16*K + n)`, in a
        register, applied to a base pointer the instruction resolves
        indirectly (WSZB/WBTO/WBTZ take acS = base, acD = bit offset).
        Returns (base_reg, off_reg)."""
        if not (0 <= nbit <= 15):
            refuse(wordnode, "BIT(): bit index %d outside 0..15" % nbit)
        tname, t, sub, K = self.bit_word(wordnode)
        bounds = None
        if isinstance(sub, c_ast.FuncCall) and sub.name.name == "SUB":
            bounds = int(sub.args.exprs[1].value, 0)
            sub = sub.args.exprs[0]
        if not isinstance(sub, c_ast.ID):
            refuse(sub, "BIT(): subscript must be a variable (optionally SUB(v, n))")
        vname = sub.name
        i = self.stmt_index
        skey = ("scaled", tname, vname)
        bkey = ("bitbase", tname, vname)
        base_addr = self.L.static(t["base"])["addr"]
        disp = 16 * K + nbit

        # R27: `16*scaled` is a CSE temp of its own (distinct from the R9
        # scaled temp: one is i*stride, the other 16*i*stride) when later
        # statements take another bit of the same element.  701662A2 reloads
        # slot 8 for each of the nine WBTZs at 70166296.
        if self.cse.get(bkey, {}).get("slot") is not None:
            slot = self.cse[bkey]["slot"]
            r = self.regs.pick()
            self.emit("%s = M32[wp(ac3, %d)]" % (r, slot))              # XWLDA
            self.regs.set(r, ("live", bkey))
            self.emit("%s = add(%s, %s)" % (r, r, hexc(disp)))          # WNADI
            self.regs.set(r, ("live", ("bitoff", tname, vname)))
            if self.last_use_of(bkey) <= i:
                self.frame.free_temp(slot)
                self._freed.add(slot)
                self.cse[bkey]["slot"] = None
            b = self.bit_base_reg(base_addr, bkey, avoid=(r,))
            return b, r

        # the subscript, bounds-checked and scaled by the stride (R11)
        v = self.value(sub, want_reg=True)
        r = v.reg
        if bounds is not None:
            self.bounds_check(r, bounds)
        kr = self.regs.pick(avoid=(r,))
        self.emit("%s = %s" % (kr, hexc(t["stride"])))
        self.regs.set(kr, ("const", t["stride"]))
        self.emit("%s = mul(%s, %s)" % (r, r, kr))                      # WMUL
        self.regs.set(r, ("live", skey))

        # R26a: the *16 multiply's destination is the SCALED VALUE's register
        # (R11, multiply in place), UNLESS the scaled value is still needed
        # after the multiply -- then it is the constant's register, and the
        # IR reads `mul(16, scaled)`.  70166283 `WMUL 2,0` (dest ac0, P*686
        # dead) vs 70166046 `WMUL 0,2` (dest ac2, P*686 saved to slot 10
        # afterwards) and 70166296 `WMUL 0,2` (dest ac2, P*686 needed for the
        # element address at the end of the block).
        scaled_live = any(j > i for j in self.uses.get(skey, [])) or self.scaled_pending
        c16 = self.regs.find(("const", 16))
        if c16 is None or c16 == r:
            c16 = self.regs.pick(avoid=(r,))
            self.emit("%s = %s" % (c16, hexc(16)))
            self.regs.set(c16, ("const", 16))
        if scaled_live:
            off = c16
            self.emit("%s = mul(%s, %s)" % (off, off, r))               # WMUL r,c16
            self.regs.c[r] = ("live", skey)
        else:
            off = r
            self.emit("%s = mul(%s, %s)" % (off, off, c16))             # WMUL c16,r
        self.regs.set(off, ("live", bkey))

        # R27 (save) / R26b (the WMOV): when `16*scaled` recurs it is saved to
        # a temp and stays where it is (70166296 XWSTA 2,[ac3+8], no WMOV);
        # when it does not recur AND the multiply landed in the constant's
        # register, the product is copied to an R7 pick before the
        # displacement add (70166046 WMOV 2,1).
        if any(j > i for j in self.uses.get(bkey, [])):
            slot = self.frame.alloc_temp(bkey)
            self.emit("M32[wp(ac3, %d)] = %s" % (slot, off))
            self.cse[bkey] = dict(slot=slot, stmt=i)
        elif off is c16:
            nr = self.regs.pick(avoid=(off,))
            self.emit("%s = %s" % (nr, off))                            # WMOV
            self.regs.set(nr, ("live", bkey))
            self.regs.c[off] = ("dup", nr)
            off = nr
        self.emit("%s = add(%s, %s)" % (off, off, hexc(disp)))           # WNADI
        self.regs.set(off, ("live", ("bitoff", tname, vname)))
        # R9 for the unscaled subscript: the save is DEFERRED past the base
        # load, to just before the operation (70166052 XWSTA 0,[ac3+0xA]).
        if scaled_live and self.cse.get(skey, {}).get("slot") is None \
                and any(j > i for j in self.uses.get(skey, [])):
            self.scaled_pending = (r, skey, i)
        b = self.bit_base_reg(base_addr, bkey, avoid=(off,))
        return b, off

    scaled_pending = None

    def bit_base_reg(self, base_addr, bkey, avoid):
        """R28: the record base of a bit reference is loaded by the R7 pick
        avoiding the offset register (70166283 LWLDA 1, 70166296 LWLDA 1,
        70166046 LWLDA 2 after the offset moved out of ac2), and stays
        PROTECTED while the `16*scaled` temp is alive -- the nine WBTZs of
        70166296 all reuse the one LWLDA 1."""
        key = ("bitptr", base_addr)
        r = self.regs.find(("addr", key)) or self.regs.find(("live", key))
        if r is None or r in avoid:
            r = self.regs.pick(avoid=avoid)
            self.emit("%s = M32[%s]" % (r, hexc(base_addr)))            # LWLDA
        alive = self.cse.get(bkey, {}).get("slot") is not None
        self.regs.set(r, ("live", key) if alive else ("addr", key))
        return r

    def flush_scaled_pending(self):
        if self.scaled_pending:
            r, skey, i = self.scaled_pending
            slot = self.frame.alloc_temp(skey)
            self.emit("M32[wp(ac3, %d)] = %s" % (slot, r))
            self.cse[skey] = dict(slot=slot, stmt=i)
            self.scaled_pending = None

    BIT_MEM = "M16[ind(%s) + lsh(%s, -4)]"

    def bit_stmt(self, call):
        """R29: the three bit operations.  WBTO sets, WBTZ clears, WSZB skips
        when the bit is zero (EagleCompute.cpp:261/272/283; the bit is
        numbered from the MSB, so the mask is `lsh(0x8000, -(A & 15))`)."""
        op = call.name.name
        args = call.args.exprs
        nbit = int(args[1].value, 0)
        b, off = self.bit_address(args[0], nbit)
        self.flush_scaled_pending()
        mem = self.BIT_MEM % (b, off)
        mask = "lsh(0x8000, 0 - (%s & 15))" % off
        if op == "BIT_SET":
            self.emit("%s = %s | %s" % (mem, mem, mask))
            return
        if op == "BIT_CLR":
            self.emit("%s = %s & ~%s" % (mem, mem, mask))
            return
        # BIT_PUT(w, n, e): WBTO, then the value's sign test, then WBTZ
        # (70166376..82: set the bit, materialise e, clear it again when e is
        # zero -- the compiler's unconditional-set-then-undo shape).
        self.emit("%s = %s | %s" % (mem, mem, mask))
        vr = self.bit_value_reg(args[2])
        clr, join = self.new_block(), self.new_block()
        t = self.tplace()
        self.emit("%s = ((%s & 0xFFFF) | lsh(c, 16))" % (t, vr))
        self.terminate(("goto", [clr.label, join.label], "((lsh(%s, -15) & 1) == 1)" % t))
        self.cur = clr
        self.emit("%s = %s & ~%s" % (mem, mem, mask))
        self.terminate(("goto", [join.label], "0"))
        self.cur = join

    def bit_value_reg(self, e):
        """A bit reference used as a VALUE materialises as 0 / -1: `WSUB v,v`,
        the WSZB skip, `WADC v,v` (70166054..56).  `!` is the PL/I `^` and
        lowers to WCOM (70166376 `ac2 = ~ac2`)."""
        neg = False
        while isinstance(e, c_ast.UnaryOp) and e.op == "!":
            neg = not neg
            e = e.expr
        if not (isinstance(e, c_ast.FuncCall) and e.name.name == "BIT"):
            refuse(e, "a bit destination takes a bit reference (optionally negated)")
        nbit = int(e.args.exprs[1].value, 0)
        b, off = self.bit_address(e.args.exprs[0], nbit)
        self.flush_scaled_pending()
        vr = self.regs.pick(avoid=(b, off))
        self.emit("%s = sub(%s, %s)" % (vr, vr, vr))                    # WSUB v,v
        set_b, join = self.new_block(), self.new_block()
        self.terminate(("goto", [set_b.label, join.label], self.bit_test(b, off)))
        self.cur = set_b
        self.emit("%s = add(%s, ~%s)" % (vr, vr, vr))                   # WADC v,v
        self.terminate(("goto", [join.label], "0"))
        self.cur = join
        self.regs.set(vr, ("live", "bitval"))
        if neg:
            self.emit("%s = ~%s" % (vr, vr))                            # WCOM
            self.regs.set(vr, ("live", "bitval"))
        return vr

    def bit_test(self, b, off):
        return "((lsh(%s, 0 - (15 - (%s & 15))) & 1) == 0)" % (self.BIT_MEM % (b, off), off)

    def to_reg(self, v):
        if v.kind == "reg":
            return v.reg
        r = self.regs.pick()
        self.emit("%s = %s" % (r, v.text))
        self.regs.set(r, ("const", v.const) if v.kind == "const" else None)
        return r

    # -- values -------------------------------------------------------------
    def value(self, e, want_reg=False, avoid=()):
        """Evaluate e.  Returns a Val; with want_reg the value is in a register."""
        if isinstance(e, c_ast.Constant):
            k = int(e.value, 0)
            v = Val("const", text=hexc(k), width=32, const=k)
            if want_reg:
                r = self.regs.find(("const", k))
                if r is None:
                    r = self.regs.pick(avoid)
                    self.emit("%s = %s" % (r, hexc(k)))      # NLDAI/WLDAI
                    self.regs.set(r, ("const", k))
                return Val("reg", reg=r, width=32, const=k)
            return v
        if isinstance(e, c_ast.ID):
            if e.name in self.frame.locals:
                slot, width = self.frame.locals[e.name]
                key = ("local", e.name)
                r = self.regs.find(("var", key))
                if r is not None:
                    self.var_read(r, key, e.name)
                    return Val("reg", reg=r, width=width)
                text = "M32[wp(ac3, %d)]" % slot if width == 32 else "sx16(M16[wp(ac3, %d)])" % slot
                return self.load(text, width, key, want_reg, avoid, name=e.name)
            if e.name == self.arg_count_param:
                # R12: the two-arity marker word, read with XNLDA 0,[ac3-9]
                key = ("marker",)
                r = self.regs.find(("var", key))
                if r is not None:
                    return Val("reg", reg=r, width=16)
                return self.load("sx16(M16[wp(ac3, -9)])", 16, key, want_reg, avoid)
            if e.name in self.args:
                refuse(e, "a by-reference parameter is used as *%s" % e.name)
            s = self.L.static(e.name)
            if s is not None:
                key = ("static", e.name)
                r = self.regs.find(("var", key))
                if r is not None:
                    return Val("reg", reg=r, width=s["width"])
                text = "M32[%s]" % hexc(s["addr"]) if s["width"] == 32 else "sx16(M16[%s])" % hexc(s["addr"])
                return self.load(text, s["width"], key, want_reg, avoid)
            refuse(e, "unknown identifier %s" % e.name)
        if isinstance(e, c_ast.UnaryOp) and e.op == "*" and isinstance(e.expr, c_ast.ID) and e.expr.name in self.args:
            n, width, const = self.args[e.expr.name]
            key = ("arg", e.expr.name)
            r = self.regs.find(("var", key))
            if r is not None:
                self.var_read(r, key, e.expr.name)
                return Val("reg", reg=r, width=width)
            text = "M32[R[ac3 + -%d]]" % (10 + 2 * n) if width == 32 else "sx16(M16[R[ac3 + -%d]])" % (10 + 2 * n)
            return self.load(text, width, key, want_reg, avoid, name=e.expr.name)
        if isinstance(e, c_ast.StructRef):
            return self.field(e, want_reg, avoid)
        if isinstance(e, c_ast.BinaryOp):
            return self.binop(e, avoid)
        if isinstance(e, c_ast.FuncCall) and e.name.name == "ABS":
            # the PL/I ABS builtin: `goto [neg, join] (r >=s 0); neg: r = sub(0, r)`
            # (WSGE r,r / WNEG r,r: UPDATE_SCREENS 7017D658..65C)
            v = self.value(e.args.exprs[0], want_reg=True, avoid=avoid)
            r = v.reg
            neg, join = self.new_block(), self.new_block()
            self.terminate(("goto", [neg.label, join.label], "(%s >=s 0)" % r))
            self.cur = neg
            self.emit("%s = sub(0, %s)" % (r, r))
            self.terminate(("goto", [join.label], "0"))
            self.cur = join
            self.regs.set(r, ("live", "abs"))
            return Val("reg", reg=r, width=32)
        if isinstance(e, c_ast.FuncCall) and e.name.name in ("cvwn", "sx16", "trunc16"):
            # P36 ruling 1: conversions are explicit.  `cvwn(e)` is the checked
            # 32->16 convert the compiler inserts before a 16-bit store; the
            # value it yields is already 16-bit, so the store's type-driven
            # auto-narrow must NOT emit a second cvwn (hence width=16 below).
            op = e.name.name
            inner = e.args.exprs[0]
            if isinstance(inner, c_ast.FuncCall) and "$" in getattr(inner.name, "name", ""):
                v = self.rt_call(inner)          # result in ac0 (RTConventions)
            else:
                v = self.value(inner, want_reg=True, avoid=avoid)
            if op == "cvwn":
                if v.width == 16:
                    refuse(e, "cvwn() of a value that is already 16-bit")
                self.emit("%s = cvwn(%s)" % (v.reg, v.reg))
                self.regs.set(v.reg, ("live", "cvwn"))
            else:
                self.emit("%s = %s(%s)" % (v.reg, op, v.reg))
                self.regs.set(v.reg, ("live", op))
            return Val("reg", reg=v.reg, width=16)
        if isinstance(e, c_ast.FuncCall) and e.name.name == "SUB":
            v = self.value(e.args.exprs[0], want_reg=True, avoid=avoid)
            self.bounds_check(v.reg, int(e.args.exprs[1].value, 0))
            return v
        if isinstance(e, c_ast.Cast):
            v = self.value(e.expr, want_reg=True, avoid=avoid)
            w = self.width_of(e.to_type.type)
            if w == 16 and v.width == 32:
                self.emit("%s = cvwn(%s)" % (v.reg, v.reg))
                return Val("reg", reg=v.reg, width=16)
            return v
        refuse(e, "expression form not in the subset")

    def var_read(self, r, key, name):
        """R7b: a register holding a variable that this statement references
        again is protected (UPDATE_SCREENS 7017D67A: *y stays in ac1 for the
        column expression, so *x takes ac2)."""
        self._var_done[name] = self._var_done.get(name, 0) + 1
        total = self.var_refs.get(self.stmt_index, {}).get(name, 0)
        self.regs.c[r] = ("live", key) if self._var_done[name] < total else ("var", key)

    def load(self, text, width, key, want_reg, avoid, name=None):
        if not want_reg:
            return Val("mem", text=text, width=width)
        r = self.regs.pick(avoid)
        self.emit("%s = %s" % (r, text))
        self.regs.set(r, ("var", key))
        if name is not None:
            self.var_read(r, key, name)
        return Val("reg", reg=r, width=width)

    # -- record fields ---------------------------------------------------------
    def base_reg(self, ptr_name):
        """R5: a base pointer used for indexing is loaded into ac2 (LWLDA 2)."""
        s = self.L.static(ptr_name)
        key = ("base", ptr_name)
        if self.regs.c["ac2"] == ("addr", key):
            return "ac2"
        self.emit("ac2 = M32[%s]" % hexc(s["addr"]))
        self.regs.set("ac2", ("addr", key))
        return "ac2"

    def field(self, e, want_reg, avoid):
        if e.type == "->" and isinstance(e.name, c_ast.ID):
            ptr = e.name.name
            f = self.L.direct_field(ptr, e.field.name)
            if f is None:
                refuse(e, "%s->%s not in declarations" % (ptr, e.field.name))
            K, width = f
            b = self.base_reg(ptr)
            text = "M32[wp(%s, %d)]" % (b, K) if width == 32 else "sx16(M16[wp(%s, %d)])" % (b, K)
            return self.load(text, width, ("field", ptr, K), want_reg, avoid)
        if e.type == "." and isinstance(e.name, c_ast.ArrayRef):
            tname = e.name.name.name
            t = self.L.table(tname)
            if t is None:
                refuse(e, "table %s not in declarations" % tname)
            fld = t["fields"].get(e.field.name)
            if fld is None:
                refuse(e, "%s.%s not in declarations" % (tname, e.field.name))
            K, width = fld["K"], fld["width"]
            areg = self.element_address(tname, t, e.name.subscript)
            text = "M32[wp(%s, %d)]" % (areg, K) if width == 32 else "sx16(M16[wp(%s, %d)])" % (areg, K)
            return self.load(text, width, ("elem", tname, K), want_reg, avoid)
        refuse(e, "field reference form not in the subset")

    def element_address(self, tname, t, sub):
        """The element address base + i*stride in a register (the field's K is
        the raw displacement, so the 1-based origin is already folded: R4).
        Rules R9 (scaled-subscript CSE temp), R10 (element-address temp), R5/R6
        (address arithmetic proceeds in place when the subscript is already
        in a register, else through ac2)."""
        bounds = None
        if isinstance(sub, c_ast.FuncCall) and sub.name.name == "SUB":
            bounds = int(sub.args.exprs[1].value, 0)
            sub = sub.args.exprs[0]
        if not isinstance(sub, c_ast.ID):
            refuse(sub, "subscript must be a variable (optionally SUB(v, n))")
        vname = sub.name
        i = self.stmt_index
        skey = ("scaled", tname, vname)
        ekey = ("elem", tname, vname)
        base = t["base"]
        base_addr = self.L.static(base)["addr"]
        # R10: an element-address temp, saved at the first reference and
        # consumed by the first later statement that references the element
        # more than once (PICK_X_Y 70176208 -> 70176226; 70176217 recomputes)
        pos = self.ref_seq.get((tname, vname), 0)
        self.ref_seq[(tname, vname)] = pos + 1
        if ekey in self.cse and self.cse[ekey].get("slot") is not None:
            slot = self.cse[ekey]["slot"]
            if pos >= self.cse[ekey]["pos"] + 2:
                self.emit("ac2 = M32[wp(ac3, %d)]" % slot)
                self.regs.set("ac2", ("addr", ekey))
                self.frame.free_temp(slot)
                self._freed.add(slot)
                self.cse[ekey]["slot"] = None
                self.note_ref(tname, vname)
                return "ac2"
        if self.regs.c["ac2"] in (("addr", ekey), ("live", ekey)):
            self.note_ref(tname, vname)
            return "ac2"
        # R31: when the record base is ALREADY in ac2 -- left there by a bit
        # reference's R28 load -- and the scaled subscript is in a temp, the
        # compiler adds the TEMP TO THE BASE (`XWADD 2,[ac3+0xA]`) instead of
        # reloading the temp and adding the base.  The two orders are visible
        # in the folded address of the string statement that follows:
        # 7016605B gives `(base + scaled - K)`, 7016606F `(scaled + base - K)`.
        if self.regs.c["ac2"] == ("addr", ("bitptr", base_addr)) \
                and self.cse.get(skey, {}).get("slot") is not None \
                and not self.var_changes_between(vname, self.cse[skey]["stmt"], i):
            slot = self.cse[skey]["slot"]
            self.emit("ac2 = add(ac2, M32[wp(ac3, %d)])" % slot)         # XWADD
            self.regs.set("ac2", ("addr", ekey))
            self.elem_sym[ekey] = s_addv(s_load(s_const(base_addr), 32),
                                         self.subscript_sym(vname, t["stride"]))
            if self.last_use_of(skey) <= i:
                self.frame.free_temp(slot)
                self._freed.add(slot)
                self.cse[skey]["slot"] = None
            self.note_ref(tname, vname)
            return "ac2"
        # scaled subscript
        if skey in self.cse and self.cse[skey]["slot"] is not None and not self.var_changes_between(vname, self.cse[skey]["stmt"], i):
            slot = self.cse[skey]["slot"]
            self.emit("ac2 = M32[wp(ac3, %d)]" % slot)          # XWLDA 2
            self.regs.set("ac2", ("live", skey))
            if self.last_use_of(skey) <= i:
                self.frame.free_temp(slot)                      # R3: dies at its last use
                self._freed.add(slot)
                self.cse[skey]["slot"] = None
            self.emit("ac2 = add(ac2, M32[%s])" % hexc(base_addr))   # LWADD
            self.regs.set("ac2", ("addr", ekey))
            self.elem_sym[ekey] = s_addv(self.subscript_sym(vname, t["stride"]),
                                         s_load(s_const(base_addr), 32))
            self.note_ref(tname, vname)
            return "ac2"
        # first computation: the subscript value
        lhs_from_reg = self.regs.find(("var", ("local", vname))) is not None
        v = self.value(sub, want_reg=True)
        r = v.reg
        if bounds is not None:
            self.bounds_check(r, bounds)
        # R11: multiply by the stride with the constant in a register (WMUL)
        kr = self.regs.pick(avoid=(r,))
        self.emit("%s = %s" % (kr, hexc(t["stride"])))
        self.regs.set(kr, ("const", t["stride"]))
        self.emit("%s = mul(%s, %s)" % (r, r, kr))
        self.regs.set(r, ("live", skey))
        # R9: CSE temp for the scaled subscript when it recurs; R9a: not when
        # the subscript value was the DO-loop register (UPDATE_SCREENS 7017D64E)
        from_loop_reg = (vname == self.loop_var and r == self.loop_reg and lhs_from_reg)
        if any(j > i for j in self.uses.get(skey, [])) and not from_loop_reg:
            slot = self.frame.alloc_temp(skey)
            self.emit("M32[wp(ac3, %d)] = %s" % (slot, r))
            self.cse[skey] = dict(slot=slot, stmt=i)
        self.emit("%s = add(%s, M32[%s])" % (r, r, hexc(base_addr)))
        self.regs.set(r, ("addr", ekey))
        self.elem_sym[ekey] = s_addv(self.subscript_sym(vname, t["stride"]),
                                     s_load(s_const(base_addr), 32))
        if r != "ac2":
            self.emit("ac2 = %s" % r)                             # WMOV r,2  (R5)
            self.regs.set("ac2", ("dup", r))
            self.regs.c[r] = ("live", ekey)
        # R10: save the element address for a later multi-reference statement
        later = [j for j in self.uses.get(skey, []) if j > i]
        if len(later) >= 2 and not from_loop_reg:
            self.pending_elem_save = (r, ekey, i, pos)
        self.note_ref(tname, vname)
        return "ac2"

    def subscript_sym(self, vname, stride):
        """The symbolic value of `i * stride` for a subscript that is a static
        or a frame local (the only two DIED uses)."""
        st = self.L.static(vname)
        if st is not None:
            atom = s_load(s_const(st["addr"]), st["width"])
        elif vname in self.frame.locals:
            slot, width = self.frame.locals[vname]
            atom = s_load(s_addv(Sym("fp"), s_const(slot)), width)
        else:
            raise Refuse("no symbolic form for subscript %s" % vname)
        return s_mulk(atom, stride)

    def note_ref(self, tname, vname):
        """R6: the element address in ac2 is protected (cost 3) while this
        statement still has references to the element; after the last one it
        is an ordinary address (cost 0) and a field load may land in ac2
        (PICK_X_Y 70176208 vs 70176226)."""
        key = (tname, vname)
        self._refs_done[key] = self._refs_done.get(key, 0) + 1
        total = self.elem_refs.get(self.stmt_index, {}).get(key, 0)
        ekey = ("elem", tname, vname)
        reloadable = ekey in self.cse and self.cse[ekey].get("slot") is not None
        if self.regs.c["ac2"] is not None and self.regs.c["ac2"][1] == ekey:
            self.regs.c["ac2"] = ("live", ekey) if self._refs_done[key] < total and not reloadable else ("addr", ekey)

    def bounds_check(self, r, n):
        """R17: PL/I subscript check = the P27 fold: assert + goto in the guard
        block; the bound compares `>s` when it fits 16 bits, `>u` otherwise."""
        cmp = ">s" if n <= 0x7FFF else ">u"
        self.emit('assert(!(%s %s %s) && (%s >s 0), "DERR 17 @%s")' % (r, cmp, hexc(n), r, self.cur.label))
        nb = self.new_block()
        self.terminate(("goto", [nb.label], "0"))
        self.cur = nb

    # -- arithmetic -----------------------------------------------------------------
    def complexity(self, e):
        """R20: operand order.  The DG compiler evaluates the more complex
        operand first (a field reference needs an address register; a plain
        variable does not): `P.fm589 - *x` goes left to right, `*x - (P.fm589
        - 5)` evaluates the parenthesised right operand first (7017D64E vs
        7017D67A)."""
        if isinstance(e, c_ast.StructRef):
            return 2
        if isinstance(e, c_ast.BinaryOp):
            return 1 + max(self.complexity(e.left), self.complexity(e.right))
        if isinstance(e, c_ast.FuncCall):
            return 1 + max([self.complexity(a) for a in (e.args.exprs if e.args else [])] + [0])
        return 0

    def binop(self, e, avoid):
        if e.op not in ("+", "-", "*", "/"):
            refuse(e, "operator %s not in the subset" % e.op)
        rhs = e.right
        if not isinstance(rhs, c_ast.Constant) and self.complexity(rhs) > self.complexity(e.left):
            rv = self.value(rhs, want_reg=True, avoid=avoid)
            self.regs.c[rv.reg] = ("live", "rhs")
            lhs = self.value(e.left, want_reg=True, avoid=avoid + (rv.reg,))
            r = lhs.reg
            op = {"+": "add", "-": "sub", "*": "mul", "/": "div"}[e.op]
            self.emit("%s = %s(%s, %s)" % (r, op, r, rv.reg))
            self.regs.set(r, ("live", "res"))
            self.regs.c[rv.reg] = ("var", ("dead", rv.reg))
            return Val("reg", reg=r, width=32)
        lhs = self.value(e.left, want_reg=True, avoid=avoid)
        r = lhs.reg
        self.regs.c[r] = ("live", "lhs")
        if isinstance(rhs, c_ast.Constant):
            k = int(rhs.value, 0)
            if e.op == "+" and k == 1:
                self.emit("%s = add(%s, 1)" % (r, r))                 # WINC
                self.regs.set(r, ("live", "sum"))
                return Val("reg", reg=r, width=32)
            if e.op in ("+", "-"):
                kk = k if e.op == "+" else -k
                if -0x8000 <= kk <= 0x7FFF:
                    self.emit("%s = add(%s, %s)" % (r, r, hexc(kk)))     # WNADI (sign-extended)
                    self.regs.set(r, ("live", "sum"))
                    return Val("reg", reg=r, width=32)
                refuse(e, "constant beyond 16 bits")
            kr = self.regs.pick(avoid=(r,))
            self.emit("%s = %s" % (kr, hexc(k)))
            self.regs.set(kr, ("const", k))
            op = "mul" if e.op == "*" else "div"
            self.emit("%s = %s(%s, %s)" % (r, op, r, kr))
            if op == "div" and lhs.width == 16:
                self.emit("%s = cvwn(%s)" % (r, r))                    # R16: 16-bit quotient
                self.regs.set(r, ("live", "quot"))
                return Val("reg", reg=r, width=16)
            self.regs.set(r, ("live", "prod"))
            return Val("reg", reg=r, width=32)
        rv = self.value(rhs, want_reg=False, avoid=(r,))
        if rv.kind == "mem":
            # a 32-bit memory operand goes straight into XWADD/LWADD; a 16-bit
            # one must be loaded first (there is no narrow-to-wide add)
            if e.op in ("+", "-") and rv.width == 32:
                self.emit("%s = %s(%s, %s)" % (r, "add" if e.op == "+" else "sub", r, rv.text))
                self.regs.set(r, ("live", "sum"))
                return Val("reg", reg=r, width=32)
            rv = self.value(rhs, want_reg=True, avoid=(r,))
        op = {"+": "add", "-": "sub", "*": "mul", "/": "div"}[e.op]
        self.emit("%s = %s(%s, %s)" % (r, op, r, rv.reg))
        self.regs.set(r, ("live", "res"))
        return Val("reg", reg=r, width=32)

    pending_elem_save = None

    def flush_elem_save(self):
        if self.pending_elem_save:
            r, ekey, i, pos = self.pending_elem_save
            slot = self.frame.alloc_temp(ekey)
            self.emit("M32[wp(ac3, %d)] = %s" % (slot, r))
            self.cse[ekey] = dict(slot=slot, stmt=i, pos=pos)
            self.regs.c[r] = ("addr", ekey)
            self.pending_elem_save = None

    # -- runtime calls ---------------------------------------------------------------
    def rt_call(self, call):
        name = call.name.name
        base, _, arity = name.partition("$")
        arity = int(arity)
        args = call.args.exprs if call.args else []
        if len(args) != arity:
            refuse(call, "%s called with %d arguments" % (name, len(args)))
        # R18: TMP arguments are evaluated left to right; each is stored to its
        # temp slot immediately unless a TMP slot of this call reuses a temp
        # this statement still referenced, in which case all TMP stores are
        # deferred until every TMP is evaluated (PICK_X_Y 701761E9 vs 70176226)
        temps = []
        busy_before = set(s for s, t in self.frame.temps.items() if t is not None)
        pushes = []
        defer = False
        widths = self.protos.get(name, [32] * arity)
        for idx, a in enumerate(args):
            if isinstance(a, c_ast.FuncCall) and a.name.name == "TMP":
                w = widths[idx]
                v = self.value(a.args.exprs[0], want_reg=True, avoid=tuple(t[0] for t in temps if t))
                self.flush_elem_save()
                slot = self.frame.alloc_temp(("tmp", idx))
                if slot in busy_before or slot in self.reused_this_stmt():
                    defer = True
                temps.append((v.reg, slot, w))
                self.regs.c[v.reg] = ("live", ("tmp", idx))
                if not defer:
                    self.store_temp(slot, v.reg, w)
                    self.regs.c[v.reg] = ("const", v.const) if v.const is not None else ("var", ("tmp", slot))
                    temps[-1] = None
                pushes.append("wp(ac3, %d)" % slot)
            elif isinstance(a, c_ast.Constant) and a.type == "string" or \
                    (isinstance(a, c_ast.FuncCall) and a.name.name == "CAT"):
                # a CHAR expression argument: PL/I builds it in a fixed temp and
                # passes a VARYING dummy (REFRESH_SCREEN 70176ABA, 70176B10)
                piece, n = self.string_value(a)
                slot = self.frame.alloc_string(("dummy", idx), 1 + (n + 1) // 2)
                self.emit("[@wp(ac3, %d), %d varying] = %s" % (slot, n, piece))
                self.string_residue()
                pushes.append("wp(ac3, %d)" % slot)
            else:
                pushes.append(a)
        for t in temps:
            if t is not None:
                self.store_temp(t[1], t[0], t[2])
                self.regs.c[t[0]] = ("var", ("tmp", t[1]))
        # address arguments, evaluated in push order (right to left)
        texts = [None] * arity
        for idx in reversed(range(arity)):
            p = pushes[idx]
            texts[idx] = p if isinstance(p, str) else self.address_of(p)
        cont = self.new_block()
        if self.fp_dirty and "ac3" in ", ".join(texts):
            self.emit("ac3 = wfp", uses_fp=False)
            self.fp_dirty = False
        self.terminate(("rt_call", "rt_call ?%s(%s) site=%s" % (base, ", ".join(texts), cont.label)))
        for slot in [t[1] if t else None for t in temps]:
            pass
        for s, t in list(self.frame.temps.items()):
            if t is not None and t[0] == "tmp":
                self.frame.free_temp(s)                # the callee's arguments die with the call
        self.cur = cont
        self.regs.reset()
        self.regs.set("ac0", ("live", "ret"))       # RTConventions: returns in ac0
        return Val("reg", reg="ac0", width=32)

    def store_temp(self, slot, r, w):
        if w == 16:
            self.emit("M16[wp(ac3, %d)] = trunc16(%s)" % (slot, r))
        else:
            self.emit("M32[wp(ac3, %d)] = %s" % (slot, r))

    # -- strings --------------------------------------------------------------------
    def literal(self, node):
        text = node.value[1:-1].encode().decode("unicode_escape")
        addr = self.mem.find(text) if self.mem is not None else None
        if addr is None:
            return '[@0x00000000:0, "%s"]' % text, len(text)
        return '[@0x%08X:%d, "%s"]' % (addr[0], addr[1], text), len(text)

    def string_residue(self):
        """After WCMV: ac0 = 0 (the count ran down), ac1 clobbered, ac2 = the
        destination's end, ac3 = the source's end (R24)."""
        self.regs.set("ac0", ("const", 0))
        self.regs.c["ac1"] = None
        self.regs.set("ac2", ("addr", ("wcmv-end",)))
        self.fp_dirty = True

    def string_value(self, node):
        """-> (piece text usable as a WCMV source, byte length).  A literal is
        its own piece; CAT(a, b) is built in a fixed CHAR temporary: a one-byte
        piece by WSTB (XLEFB ac2 first when it opens the temp; at the WCMV
        destination end otherwise), a longer piece by WCMV."""
        if isinstance(node, c_ast.Constant) and node.type == "string":
            return self.literal(node)
        if not (isinstance(node, c_ast.FuncCall) and node.name.name == "CAT"):
            refuse(node, "string expression form not in the subset")
        parts = [self.string_operand(x) for x in node.args.exprs]
        n = sum(ln for _, ln, _ in parts)
        slot = self.frame.alloc_string(("cat", id(node)), (n + 1) // 2)
        off = 0
        for k, (piece, ln, sub_slot) in enumerate(parts):
            if ln == 1:
                if k == 0:
                    self.emit("ac2 = bp(ac3, %d)" % (2 * slot))            # XLEFB 2
                    self.regs.set("ac2", ("live", "strdst"))
                ch = ord(piece[0]) if isinstance(piece, str) and len(piece) == 1 else None
                if ch is None:
                    refuse(node, "one-byte piece must be a literal")
                r = self.regs.pick(avoid=("ac2",))
                self.emit("%s = %s" % (r, hexc(ch)))
                self.regs.set(r, ("const", ch))
                self.emit("M8[ac2] = zx8(%s)" % r)                       # WSTB 2,r
                if k < len(parts) - 1:
                    self.emit("ac2 = add(ac2, 1)")                      # WINC 2,2
            else:
                self.emit("[@bp(ac3, %d), %d] = %s" % (2 * slot + off, ln, piece))   # WCMV
                self.string_residue()
            if sub_slot is not None:
                self.frame.free_string(sub_slot)
            off += ln
        return "[@bp(ac3, %d), %d]" % (2 * slot, n), n

    def string_operand(self, node):
        """-> (piece or 1-char text, length, temp slot to free after use)"""
        if isinstance(node, c_ast.Constant) and node.type == "string":
            text = node.value[1:-1].encode().decode("unicode_escape")
            if len(text) == 1:
                return text, 1, None
            piece, n = self.literal(node)
            return piece, n, None
        piece, n = self.string_value(node)
        m = re.match(r"\[@bp\(ac3, (\d+)\)", piece)
        return piece, n, int(m.group(1)) // 2 if m else None

    def reused_this_stmt(self):
        return getattr(self, "_freed", set())

    def address_of(self, a):
        if isinstance(a, c_ast.UnaryOp) and a.op == "&":
            t = a.expr
            if isinstance(t, c_ast.StructRef) and t.type == "->":
                ptr = t.name.name
                f = self.L.direct_field(ptr, t.field.name)
                if f is None:
                    refuse(t, "%s->%s not in declarations" % (ptr, t.field.name))
                b = self.base_reg(ptr)
                return "wp(%s, %d)" % (b, f[0])
            if isinstance(t, c_ast.StructRef) and t.type == "." and isinstance(t.name, c_ast.ArrayRef):
                # &T[i].f : the element address in ac2 (R5/R9/R10), the field's
                # raw displacement K on top.  DIED 70166108/10A:
                # `M32[0x74009B5E] = wp(ac2, 11496)`.
                tname = t.name.name.name
                tt = self.L.table(tname)
                if tt is None:
                    refuse(t, "table %s not in declarations" % tname)
                fld = tt["fields"].get(t.field.name)
                if fld is None:
                    refuse(t, "%s.%s not in declarations" % (tname, t.field.name))
                areg = self.element_address(tname, tt, t.name.subscript)
                return "wp(%s, %d)" % (areg, fld["K"])
            if isinstance(t, c_ast.ID):
                if t.name in self.frame.locals:
                    return "wp(ac3, %d)" % self.frame.locals[t.name][0]
                s = self.L.static(t.name)
                if s is not None:
                    return hexc(s["addr"])
            refuse(a, "address form not in the subset")
        if isinstance(a, c_ast.ID) and a.name in self.args:
            return "M32[R[ac3 + -%d]]" % (10 + 2 * self.args[a.name][0]) if False else refuse(a, "passing a parameter on is not yet in the subset")
        refuse(a, "argument form not in the subset (TMP(e) or &lvalue)")

    def string_field_stmt(self, st):
        """R32: a located string statement whose destination is a RECORD FIELD.
        The address operand is not a register but the expression the master's
        registers unfold to -- lower.py prints what string_sites.py's Evaluator
        computed -- so it is rendered from the symbolic element address
        (`ir_word`), with the field's raw K folded in.  A varying destination
        absorbs the compiler's XNSTA length-word store when dst_count ==
        src_count (IR.md §5.8, P32).
        7016605B / 7016606F: the two 32-blank assignments of DIED."""
        name = st.name.name
        args = st.args.exprs
        if name == "words_copy":
            d = self.field_address_sym(args[0])
            src = self.field_address_sym(args[1])
            k = int(args[2].value, 0)
            self.emit("words(@%s, %d) = words(@%s, %d)" % (ir_word(d), k, ir_word(src), k))
            self.regs.c["ac1"] = None
            self.regs.set("ac2", ("addr", ("wblm-end",)))
            self.regs.set("ac3", ("addr", ("wblm-end",)))
            self.fp_dirty = True
            return
        dst = self.field_address_sym(args[0])
        n = int(args[1].value, 0)
        piece, plen = self.literal(args[2])
        varying = " varying" if name == "assign_varying" else ""
        if varying and plen != n:
            refuse(st, "a varying destination is only absorbed when the source "
                       "count equals the capacity (IR.md 5.8); %d vs %d" % (plen, n))
        self.emit("[@%s, %d%s] = %s" % (ir_word(dst), n, varying, piece))
        self.string_residue()

    def field_address_sym(self, a):
        """The symbolic WORD address of `&T[i].f` (or `&T[i].f.len`)."""
        if not (isinstance(a, c_ast.UnaryOp) and a.op == "&"):
            refuse(a, "a string destination must be &T[i].field")
        t = a.expr
        if isinstance(t, c_ast.StructRef) and t.type == "." and isinstance(t.name, c_ast.StructRef):
            t = t.name            # &T[i].f.data / .len -> the field itself
        if not (isinstance(t, c_ast.StructRef) and t.type == "."
                and isinstance(t.name, c_ast.ArrayRef)):
            refuse(a, "a string destination must be &T[i].field")
        tname = t.name.name.name
        tt = self.L.table(tname)
        if tt is None:
            refuse(t, "table %s not in declarations" % tname)
        fld = tt["fields"].get(t.field.name)
        if fld is None:
            refuse(t, "%s.%s not in declarations" % (tname, t.field.name))
        self.element_address(tname, tt, t.name.subscript)
        ekey = ("elem", tname, self.subscript_key(t.name.subscript))
        if ekey not in self.elem_sym:
            refuse(t, "no symbolic form for the element address of %s" % tname)
        return s_addk(self.elem_sym[ekey], fld["K"])

    STRING_FNS = ("assign_varying", "assign_fixed", "words_copy")

    def call_stmt(self, st):
        name = st.name.name
        if name in self.STRING_FNS:
            return self.string_field_stmt(st)
        base = name.partition("$")[0]
        if base in self.addrbook_by_name:
            return self.game_call(st)
        if "$" in name:
            self.rt_call(st)
            return
        refuse(st, "unknown callee %s" % name)

    def game_call(self, st):
        """R30: a game->game call.  The callee's arguments live at STATIC
        book-mode slots (quest.addrbook `alloc`); argument n is at
        `wfp - 10 - 2n`, so the slots run DOWNWARD from arg 1 and the stores
        appear in ASCENDING address order, i.e. right to left in the source
        (R18's push order).  The call itself is `call <tgt> args=<n>
        marker=<alloc + 2*argc> site=<pc> ret=<pc+4>`; an argc-0 call is not
        in the pushmap and stays an embedded `LCALL [<tgt>],0` instruction.
        DIED 701660F8 (UPDATE_SCREENS, 3 args), 70166323 (REPOSITION),
        70166341 (DISPLAY_SCREEN), 701663B6 (DISPLAY_INVENTORY);
        70166216 / 70166327 the two undecorated LCALLs."""
        name = st.name.name
        base, _, arity = name.partition("$")
        arity = int(arity) if arity else 0
        e = self.addrbook_by_name.get(base)
        if e is None:
            refuse(st, "game callee %s not in quest.addrbook" % base)
        args = st.args.exprs if st.args else []
        if len(args) != arity:
            refuse(st, "%s called with %d arguments" % (name, len(args)))
        if arity == 0:
            # not a pushmap site: the instruction stays embedded
            self.flush_elem_save()
            cont = self.new_block()
            self.terminate(("instr", "LCALL [0x%08X],0;" % e["pc"], cont.label))
            self.cur = cont
            self.regs.reset()
            return
        alloc = e["alloc"]
        # the argument VALUES first (TMP dummies get a frame temp, as R18)
        pushes = [None] * arity
        for idx, a in enumerate(args):
            if isinstance(a, c_ast.FuncCall) and a.name.name == "TMP":
                v = self.value(a.args.exprs[0], want_reg=True)
                slot = self.frame.alloc_temp(("tmp", idx))
                self.store_temp(slot, v.reg, v.width)
                pushes[idx] = "wp(ac3, %d)" % slot
            else:
                pushes[idx] = a
        # the slot stores, right to left in the source = ascending in address
        for idx in reversed(range(arity)):
            p = pushes[idx]
            text = p if isinstance(p, str) else self.address_of(p)
            self.emit("M32[%s] = %s" % (hexc(alloc + 2 * (arity - 1 - idx)), text))
        cont = self.new_block()
        self.terminate(("call", "call %08X args=%d marker=%08X site=SITE ret=RET"
                        % (e["pc"], arity, alloc + 2 * arity), cont.label, arity))
        for s, t in list(self.frame.temps.items()):
            if t is not None and t[0] == "tmp":
                self.frame.free_temp(s)
        self.cur = cont
        self.regs.reset()

    # -- branch ranges (R19) ---------------------------------------------------------
    WBR_RANGE = 127

    def words_of(self, line):
        """Estimated instruction length in words of one emitted statement."""
        if re.match(r"@", line):
            return 2
        if re.match(r"ac[0-3] = 0x", line):
            return 2 if int(line.split("= ")[1], 16) < 0x10000 or int(line.split("= ")[1], 16) >= 0xFFFF8000 else 3
        if re.match(r"ac[0-3] = M32\[0x", line) or re.search(r"= add\(ac[0-3], M32\[0x", line):
            return 3
        if re.match(r"ac[0-3] = (mul|div|cvwn)\(", line) or re.match(r"ac[0-3] = add\(ac[0-3], 1\)", line):
            return 1
        if re.match(r"ac[0-3] = ac[0-3]$", line) or re.match(r"t\d+ = ", line):
            return 1
        if line.startswith("assert("):
            return 5
        return 2

    def term_words(self, b):
        t = b.term
        if t is None or t[0] == "ret":
            return 1
        if t[0] == "fall":
            return 0
        if t[0] == "goto":
            return 1 if len(t[1]) == 1 else (0 if any(l.startswith("t") for l in b.lines[-1:]) else 2)
        if t[0] == "call":
            return 4 + 3 * (t[3] if len(t) > 3 else 0)
        if t[0] == "instr":
            return 4
        if t[0] == "rt_call":
            text = t[1]
            n = text.count(",") + 1 if "(" in text and text.split("(")[1][0] != ")" else 0
            return 4 + 2 * n + (1 if "0x7" in text else 0)
        return 0

    def block_positions(self):
        pos, at = {}, 0
        for b in self.blocks:
            pos[b.label] = at
            at += sum(self.words_of(l) for l in b.lines) + self.term_words(b)
        return pos

    def invert_far_diamonds(self):
        """R13c: `if (c) goto L` normally lowers as R13 -- skip-if-NOT-c over
        the one-word `WBR L`.  When L is beyond WBR range the goto needs an
        XJMP, which is not one word, so the compiler INVERTS the diamond:
        skip-if-c over `WBR cont`, with the XJMP following.  DIED 70166057:
        `MOV.L# 0,0,SNC; WBR 3 (0x7016605B); XJMP (0x701661AE)` for
        `if (BIT(..)) goto reincarnate;` with reincarnate 0x151 words away.
        Confidence B (one routine; three instances in DIED)."""
        if not self.if_diamonds:
            return
        pos = self.block_positions()
        end = max(pos.values()) if pos else 0
        for head, then_b, cont_b, pos_test in self.if_diamonds:
            if then_b.term is None or then_b.term[0] != "goto" or len(then_b.term[1]) != 1:
                continue
            target = then_b.term[1][0]
            if target not in pos:
                continue
            here = pos[then_b.label]
            if -self.WBR_RANGE <= pos[target] - here <= self.WBR_RANGE:
                continue
            # R19 first: a far `goto` normally reaches a one-word WBR to a stub
            # at the routine's end, which is what PICK_X_Y's three retries do.
            # Inversion is only forced when the STUB is out of WBR range too --
            # i.e. the routine is longer than a branch can span from here.
            # DIED 70166057: the target is +0x154 and the routine end +0x360.
            if end - here <= self.WBR_RANGE:
                continue
            # invert: the fall-through jumps to the continuation, the skip
            # target keeps the (now XJMP) goto to the far label
            jump_b = self.new_block("inv_" + cont_b.label)
            jump_b.term = ("goto", [cont_b.label], "0")
            i = self.blocks.index(then_b)
            self.blocks.remove(jump_b)
            self.blocks.insert(i, jump_b)
            head.term = ("goto", [jump_b.label, then_b.label], pos_test)

    def stub_branches(self):
        """R19: an unconditional `goto L` whose WBR displacement would not fit
        (|d| > 127 words) branches to a per-label long-jump stub `XJMP L`
        placed at the routine's end; the routine's own final `goto L`, when
        there is one, IS the stub (PICK_X_Y 70176269/6C/71 -> 70176275)."""
        pos, at = {}, 0
        for b in self.blocks:
            pos[b.label] = at
            at += sum(self.words_of(l) for l in b.lines) + self.term_words(b)
        last = self.blocks[-1]
        for b in list(self.blocks):
            t = b.term
            if t is None or t[0] != "goto" or len(t[1]) != 1 or t[2] != "0" or b is last:
                continue
            target = t[1][0]
            here = pos[b.label] + sum(self.words_of(l) for l in b.lines)
            d = pos[target] - here
            if -self.WBR_RANGE <= d <= self.WBR_RANGE:
                continue
            if last.term == ("goto", [target], "0") and not last.lines:
                stub = last
            else:
                stub = self.new_block("stub_" + target)
                stub.term = ("goto", [target], "0")
                last = stub
            b.term = ("goto", [stub.label], "0")

    # -- rendering ------------------------------------------------------------------
    def render(self):
        self.invert_far_diamonds()
        self.stub_branches()
        # canonical order: DFS from entry over the terminators (what ircmp does)
        by_label = {b.label: b for b in self.blocks}
        order, seen, stack = [], set(), ["entry"]
        first_body = self.blocks[1].label
        while stack:
            l = stack.pop()
            if l in seen:
                continue
            seen.add(l)
            b = by_label[l]
            order.append(b)
            if b.term is None:
                continue
            if b.term[0] == "goto":
                succ = list(b.term[1])
            elif b.term[0] == "rt_call":
                succ = [b.term[1].split("site=")[1]]
            elif b.term[0] in ("call", "instr"):
                succ = [b.term[2]]
            elif b.term[0] == "fall":
                succ = [b.term[1]]
            else:
                succ = []
            for s in reversed(succ):
                if s not in seen:
                    stack.append(s)
        for k, b in enumerate(order):
            b.pc = 0x00001000 + 0x10 * k
        pc_of = {b.label: b.pc for b in order}
        pc_of["entry"] = self.entry_pc & 0 | order[0].pc
        # the entry block gets the real entry pc? No: synthetic like the rest,
        # but the instruction line keeps the real address as audit text.
        out = ["ir 6", "mode book",
               "routine %s entry %08X lo %08X hi %08X" % (self.name, order[0].pc, order[0].pc, order[-1].pc + 0x10),
               "; translate.py (Project 35) — synthetic block pcs in canonical order; not loadable", ""]
        for b in order:
            out.append("block %08X seg 0x70000000" % b.pc)
            for l in b.lines:
                l = re.sub(r"@(B\d+|entry|[A-Za-z_]\w*)\"", lambda m: "@%08X\"" % pc_of.get(m.group(1), 0), l)
                out.append("  " + l)
            t = b.term
            if t[0] == "goto":
                out.append("  goto [%s] %s" % (", ".join("%08X" % pc_of[l] for l in t[1]), t[2]))
            elif t[0] == "rt_call":
                text, _, site = t[1].partition(" site=")
                out.append("  %s site=%08X" % (text, pc_of[site] - 4))
            elif t[0] == "call":
                text = t[1].replace("SITE", "%08X" % (pc_of[t[2]] - 4)).replace("RET", "%08X" % pc_of[t[2]])
                out.append("  " + text)
            elif t[0] == "instr":
                out.append("  @%08X %s" % (pc_of[t[2]] - 4, t[1]))
            elif t[0] == "ret":
                out.append("  ret")
            out.append("")
        out.append("blocks %d" % len(order))
        return "\n".join(out) + "\n"


# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("--routine", required=True)
    ap.add_argument("-o", "--out")
    ap.add_argument("--layout", default=os.path.join(ROOT, "game", "declarations.json"))
    ap.add_argument("--addrbook", default=os.path.join(ROOT, "emulation", "quest.addrbook"))
    ap.add_argument("--mem", default=os.path.join(ROOT, "..", "Disassembled", "quest.mem"),
                    help="memory image (hex dump) used only to resolve literal addresses")
    args = ap.parse_args()
    t0 = time.time()
    ast = parse_file(args.source, use_cpp=True,
                     cpp_args=["-I" + os.path.join(HERE, "fake_include"), "-I" + os.path.join(ROOT, "game"),
                               "-D__TRANSLATOR__", "-fdollars-in-identifiers"])
    fdef = None
    for ext in ast.ext:
        if isinstance(ext, c_ast.FuncDef) and ext.decl.name == args.routine:
            fdef = ext
    if fdef is None:
        raise SystemExit("routine %s not defined in %s" % (args.routine, args.source))
    tr = Translator(Layout(args.layout), args.addrbook, args.routine)
    tr.collect_protos(ast)
    tr.mem = MemImage(args.mem) if args.mem and os.path.exists(args.mem) else None
    try:
        text = tr.translate(fdef)
    except Refuse as e:
        sys.stderr.write(str(e) + "\n")
        sys.exit(2)
    if args.out:
        open(args.out, "w").write(text)
    else:
        sys.stdout.write(text)
    sys.stderr.write("translate %s: %d blocks, %.2fs\n" % (args.routine, text.count("\nblock "), time.time() - t0))


if __name__ == "__main__":
    main()
