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
import argparse, collections, contextlib, json, os, re, sys, time

from pycparser import c_ast, parse_file

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, HERE)
import readable as R  # noqa: E402  (addrbook loader)
import productions as PROD  # noqa: E402  (P42 production table + ledger)


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


def c_name(routine):
    """The addrbook name -> a legal C identifier.  A NESTED entry is named
    `PARENT.N@PC`; C has no nested procedures and no `.` or `@` in an
    identifier, so it becomes `PARENT_N` (P39).  Ordinary names pass through."""
    return routine.split("@", 1)[0].replace(".", "_")


class Refuse(Exception):
    pass


def refuse(node, why):
    coord = getattr(node, "coord", None)
    raise Refuse("REFUSE %s: %s (%s)" % (coord, why, type(node).__name__))


def hexc(v):
    """lower.py's constant spelling for a word an instruction carries."""
    return "0x%08X" % (v & 0xFFFFFFFF)


def _sx32(v):
    """The signed reading of a 32-bit word."""
    v &= 0xFFFFFFFF
    return v - 0x100000000 if v & 0x80000000 else v


# ---------------------------------------------------------------------------
# the world layout (declarations.json)
# ---------------------------------------------------------------------------

class Layout:
    def __init__(self, path):
        d = json.load(open(path))
        self.statics = d["statics"]
        self.direct = d["direct"]
        self.tables = d["tables"]
        self.frames = d.get("frames", {})     # P39: parent frame layouts

    def static(self, name):
        return self.statics.get(name)

    def direct_field(self, ptr, field):
        f = self.direct.get(ptr, {}).get(field)
        if f is None:
            return None
        return f["K"], f["width"]

    def parent_frame(self, name):
        """P39: the enclosing procedure's frame layout — the static link's
        target.  Every slot in it carries a width and a named witness (the
        user's ruling); gen_declarations.py refuses to emit one without."""
        return self.frames.get(name)

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

#   ("fp", None)             the frame pointer itself           cost FP_COST (R33)
#
# R33 (P38): the frame is not pinned to ac3.  It is an ordinary VALUE that ac3
# holds by default, competing for a register like any other.  Its protection
# cost is bounded BELOW by the four matched routines: at cost 0 or 1 `pick()`
# takes ac3 and all four regress (PICK_X_Y 62/64, UPDATE_SCREENS 44/72,
# REFRESH_SCREEN 59/61, OWNS 107/152 at cost 0).  At cost 2 and at cost 3 they
# are all 100 %, so those two are indistinguishable here and the UPPER bound is
# NOT witnessed: no statement in the four ever has all of ac0..ac2 live at once,
# which is the only case that separates them.  2 is taken as the weaker claim.
FP_COST = 2

COST = {"var": 0, "addr": 0, "kconst": 0, "dup": 1, "const": 2, "live": 3,
        "fp": FP_COST}

FP = ("fp", None)


class Regs:
    def __init__(self):
        self.c = {"ac0": None, "ac1": None, "ac2": None, "ac3": FP}
        self.stamp = {"ac0": 0, "ac1": 0, "ac2": 0, "ac3": 0}
        self.t = 0

    def cost(self, r):
        v = self.c[r]
        return 0 if v is None else COST[v[0]]

    def touch(self, r):
        self.t += 1
        self.stamp[r] = self.t

    def pick(self, avoid=(), only=None):
        """R7: the register with the lowest protection cost; ties to the
        lowest-numbered register.  `only` restricts the candidate set to a
        named class -- R21c's ac0/ac1 for the loop register, R41′'s ac2/ac3 for
        a base -- so that a rule stating a CLASS cannot silently fall out of it
        when every member is expensive (P40: `avoid=("ac2",)` let the loop
        register reach ac3, the frame)."""
        best = None
        for r in (only or ("ac0", "ac1", "ac2", "ac3")):
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
        """R8: register knowledge resets at a join -- but NOT the frame.

        R33 (P38): the frame is a value, but it is not *knowledge* about a
        value: `LDAFP` is a real instruction and a join does not execute one.
        Every block of all four matched routines addresses `wp(ac3, d)` with
        no preceding LDAFP, so the frame demonstrably survives a label.  If
        this cleared ac3 the frame would become cost 0 and `pick()` would take
        it immediately (measured: all four regress).
        """
        for r in self.c:
            if self.c[r] != FP:
                self.c[r] = None

    def fpreg(self):
        """The register the frame is in, or None if it is not in one."""
        for r, v in self.c.items():
            if v == FP:
                return r
        return None

    def pick_base(self):
        """R41′: a base pointer loaded FOR INDEXING, and the static link,
        allocate from the two-register class {ac2, ac3} -- ac2 preferred, ac3
        when ac2 is occupied by a live address.

        P41 narrowed the SCOPE (user ruling, Sep 9 2026).  R41 as written
        covered "an address or base load" and said ac0/ac1 were never
        candidates; that overclaims.  The class governs the ADDRESSING ROLE,
        not every load of an address -- a record base loaded for a bit
        reference (R28) is allocated by plain R7 and reaches {ac0, ac1}.  The
        SD_PTR census: 302 of the 306 ac0/ac1 loads feed the bit family
        (WSUB, then WBTO/WBTZ), against zero X-form indexing off such a base.
        This function is the addressing path; bit_base_reg is the other one
        and correctly does NOT call it.

        Witnesses (CODEGEN_RULES §9.3): INIT_OBJ_TBL 7016DF68 loads the SAME
        argument into ac2 once (ac2 free) and into ac3 later (ac2 live, ac0
        free at cost 0); FIRE.1 7016A3C7 has three ac3 address loads, each
        with ac2 live and a value register free.  Taking ac3 displaces the
        frame, which R33/R24 re-materialise by LDAFP.
        """
        if self.cost("ac2") <= self.cost("ac3"):
            return "ac2"
        return "ac3"

    def pick_fp(self):
        """R33: where an `LDAFP` puts the frame.  R7's cost ordering, but the
        tie-break is ac3 (the frame's default home) rather than the lowest
        number.  R33 says `ac3 holds it by default`; FrameRelocation.md §4
        forbids coding `the target is always ac2`, so the target is a pick.
        """
        best = None
        for r in ("ac3", "ac0", "ac1", "ac2"):
            k = (self.cost(r), 0 if r == "ac3" else 1, r)
            if best is None or k < best:
                best = k
        return best[2]

    def snapshot(self):
        """R8c: the register knowledge on one control-flow edge."""
        return dict(self.c)

    def restore(self, snap):
        """R8c: restore the register knowledge carried on one edge.

        R33 corollary: the snapshot restores *knowledge*, and where the frame
        physically is is not knowledge -- it is the record of which LDAFPs have
        been emitted.  So the frame stays where the emitter has actually left
        it and the snapshot's opinion about ac3 is discarded.  NO WITNESS: in
        all four matched routines the frame is in ac3 on both sides of every
        edge, so this choice is untested.
        """
        here = self.fpreg()
        for r in self.c:
            self.c[r] = snap.get(r)
        if here is not None:
            for r in self.c:
                if self.c[r] == FP and r != here:
                    self.c[r] = None
            self.c[here] = FP

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

    def declare(self, name, width, count=1):
        """R1/R2: declared locals take even slots 2, 4, 6, ... in declaration
        order, one wide slot each, 16-bit locals included.

        R2a (P37/GET_INPUT): an ARRAY local takes as many words as it needs,
        rounded up to a whole even slot -- a CHAR(n) buffer takes ceil(n/2)
        words.  GET_INPUT's `char buf[144]` therefore runs from slot 4 to 75
        and the frame's first temp is slot 76, which is where the book puts
        it (XLEF 2,[ac3+0x4C])."""
        slot = self.next_local
        self.locals[name] = (slot, width)
        words = (count * width + 15) // 16
        self.next_local += max(2, words + (words & 1))
        return slot

    hw = None                 # high-water mark (first never-allocated word)
    strings = None            # [(slot, words, tag or None)]

    str_died = None           # slot -> the statement index its string temp died in
    cur_stmt = -1             # the statement being generated

    def alloc_temp(self, tag, busy=()):
        """R3: the lowest free even slot above the declared locals; a slot is
        free when its temp is dead (the caller has decided liveness) and no
        LIVE string temporary occupies it (R3b as amended in P38 -- see
        in_string; the pools are NOT disjoint, which is what P35 read out of
        REFRESH_SCREEN's 4/18/20 vs 6/22/34)."""
        if self.hw is None:
            self.hw, self.strings = self.next_local, []
        if self.str_died is None:
            self.str_died = {}
        s = self.next_local
        while (s in self.temps and self.temps[s] is not None) or s in busy or self.in_string(s):
            s += 2
        self.temps[s] = tag
        self.hw = max(self.hw, s + 2)
        return s

    def in_string(self, s):
        """A LIVE string temporary occupies s.

        R3b amended (P38): the two pools are NOT disjoint, but a scalar temp
        may take a dead string temp's words only from a LATER STATEMENT on.

        HIT_ANY_CHAR 7016DEAD puts the packed CHAR VARYING at `wp(ac3, 4)` --
        the slot the 30-byte prompt's dummy occupied (4..19) until the
        ?WRITE_SCREEN call two statements earlier consumed it.  Disjointness
        would put it at 20.  REFRESH_SCREEN 70176B10 does NOT reuse slot 6 for
        the row temp although the CAT dummy there is dead by the time the temp
        is placed -- because it died in THAT SAME statement; the temp goes to
        18.  Plain overlap would put it at 6.  The statement boundary is the
        discriminator and both routines are witnesses.

        P35 read REFRESH_SCREEN's 4/18/20-vs-6/22/34 as evidence FOR disjoint
        pools; it is not.  Its string temps are live at every point where a
        scalar temp is allocated, so slot 6 was unavailable under either
        reading and the observation could not have come out the other way
        (METHOD §16).
        """
        for a, n, t in self.strings:
            if not (a <= s < a + n):
                continue
            if t is not None:
                return True
            # dead -- but only reusable by a scalar temp from a LATER statement
            if (self.str_died or {}).get(a, -1) >= self.cur_stmt:
                return True
        return False

    def alloc_string(self, tag, words):
        """R3b: a string temporary reuses a dead string temporary of sufficient
        size, else takes the frame's high-water mark (a bump allocation)."""
        if self.hw is None:
            self.hw, self.strings = self.next_local, []
        if self.str_died is None:
            self.str_died = {}
        for i, (a, n, t) in enumerate(self.strings):
            if t is None and n >= words:
                self.strings[i] = (a, n, tag)
                return a
        a = self.hw + (self.hw & 1)
        self.strings.append((a, words, tag))
        self.hw = a + words
        return a

    def free_string(self, slot, stmt=None):
        """A string temp dies.  `stmt` is the statement index it died in: a
        scalar temp may take its words only from a LATER statement on (see
        in_string)."""
        self.strings = [(a, n, None if a == slot else t) for a, n, t in self.strings]
        if stmt is not None:
            self.str_died[slot] = stmt

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
    hexc = staticmethod(hexc)

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
        self.arm_path = {}     # pre-pass: stmt index -> the branch arms enclosing it
        self.pinned = set()    # R7c': registers pinned across an if-body
        # R42 (P39): a NESTED procedure — the addrbook flags it, and its name
        # is `PARENT.N@PC`.  `parent` is None for an ordinary procedure, and
        # UP()/UPARG() then refuse.  Nesting is one level deep program-wide
        # (P39 §2), so the parent is a name, never a chain.
        self.parent = None
        if "nested" in (self.entry.get("flags") or "").split(",") and "." in routine_name:
            self.parent = routine_name.split(".", 1)[0]
        self.link_param = None
        # P42: the reduction ledger runs in PARALLEL with these emit methods.
        # `fire()` records an address and the choices taken and enforces the
        # span declaration; it emits nothing, so instrumenting a method cannot
        # change what that method produces.  Stage 3 moves templates into
        # productions.py one at a time.
        self.ledger = PROD.Ledger()

    # -- P42: productions ------------------------------------------------
    @contextlib.contextmanager
    def fire(self, name, node=None, shape=None, consumes=()):
        """Fire a production around code that already emits.

        The address is assigned on COMPLETION, so the ledger is in reduction
        order -- post-order over tile roots -- and `<production>#<n>` counts
        that production's own firings.  `consumes` names the descendant nodes
        this tile swallows: they are marked, and any later attempt to reduce
        one of them separately is a span violation.  Emitting for, or firing
        on, an undeclared node is a HARD ERROR (the user's ruling: without
        this clause "tile" collapses back into "arbitrary method")."""
        f = self.ledger.begin(name, id(node) if node is not None else None,
                              shape or (type(node).__name__ if node is not None else name),
                              self.stmt_index)
        self.ledger.consume(name, [id(n) for n in consumes if n is not None])
        try:
            yield f
        finally:
            self.ledger.end(f)

    def choice(self, kind, value, witness=None):
        """Record a choice the firing production took.  P43's choice files
        key on exactly these."""
        self.ledger.choice(kind, value, witness)

    def fired(self, name, node=None, shape=None, consumes=(), choices=()):
        """A point firing, for a production whose template does not nest
        another reduction inside it.  Placed where the emission COMPLETES, so
        it takes its address in reduction order like any other."""
        f = self.ledger.begin(name, id(node) if node is not None else None,
                              shape or (type(node).__name__ if node is not None else name),
                              self.stmt_index)
        self.ledger.consume(name, [id(n) for n in consumes if n is not None])
        for c in choices:
            self.ledger.choice(*c)
        return self.ledger.end(f)

    def consume_now(self, name, *nodes):
        """Declare a tile's span at the point the path is decided.

        P42 FINDING: element_address's span is PATH-DEPENDENT.  Five of its
        seven paths reload the element address from a temp and never evaluate
        the subscript; two compute it and genuinely reduce the subscript.  A
        single fixed span declaration is therefore wrong for that production,
        and the honest form is to declare consumption on the paths that
        consume.  Recorded rather than papered over."""
        self.ledger.consume(name, [id(n) for n in nodes if n is not None])

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
        """R24: `LDAFP` is emitted lazily, just before the next statement that
        addresses the frame (REFRESH_SCREEN 70176ABA vs 70176ADF).

        R33 (P38): the generator writes `ac3` in a frame reference to MEAN
        "the frame"; the frame's actual register is resolved here.  When the
        frame is not in a register an `LDAFP` is appended first, and when it
        is somewhere other than ac3 the reference is respelled.  Pass
        `uses_fp=False` for a text whose `ac3` is a genuine register mention
        and not a frame reference.
        """
        if uses_fp is None:
            uses_fp = "ac3" in text
        if uses_fp:
            r = self.fpr()
            if r != "ac3":
                text = text.replace("ac3", r)
        self.cur.lines.append(text)

    def fpr(self):
        """R33: the register holding the frame, materialising it if needed."""
        r = self.regs.fpreg()
        if r is None:
            r = self.regs.pick_fp()
            self.cur.lines.append("%s = wfp" % r)
            self.regs.set(r, FP)
        return r

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
            if p.name == "__up":
                # R42: the static link.  It arrives in ac1 and WSAVS saves it
                # at wp(fp, -6); it is NOT one of the procedure's arguments and
                # is not pushed, so it does not count toward argc.  That is why
                # a nested procedure can take a link AND arguments without the
                # two competing (KILL_PLAYER.4, LIST_PLAYERS.3: argc 1 + link).
                if self.parent is None:
                    refuse(p, "%s takes an UPLINK but the addrbook does not "
                              "call it nested" % self.name)
                if params.index(p) != 0:
                    refuse(p, "the UPLINK parameter must come first")
                self.link_param = p.name
                continue
            if not isinstance(p.type, c_ast.PtrDecl):
                refuse(p, "parameters are by-reference pointers (PL/I)")
            n += 1
            width = self.width_of(p.type.type)
            const = "const" in (p.type.type.quals or [])
            self.args[p.name] = (n, width, const)
        if n != self.entry["argc"]:
            raise Refuse("%s declares %d arguments; the addrbook says argc %d" % (self.name, n, self.entry["argc"]))
        if self.parent is not None and self.link_param is None:
            raise Refuse("%s is a nested procedure; its first parameter must be "
                         "`UPLINK(%s) __up` (R42)" % (self.name, self.parent))
        # R35: the declared return type; None = a PL/I procedure (returns nothing)
        rt = decl.type.type
        self.ret_width = None
        if isinstance(rt, c_ast.TypeDecl) and isinstance(rt.type, c_ast.IdentifierType) \
                and rt.type.names[-1] != "void":
            self.ret_width = self.width_of(rt)
        self.frame = Frame(self.entry["frame"])
        self.fired("prologue_epilogue", fdef, "WSAVS frame 0x%X" % self.entry["frame"],
                   choices=[("slot", self.entry["frame"])])
        body = fdef.body.block_items or []
        # declarations first (R1)
        stmts = []
        for it in body:
            if isinstance(it, c_ast.Decl):
                if it.init is not None:
                    refuse(it, "initialised locals are not in the subset")
                n = 1
                td = it.type
                if isinstance(td, c_ast.ArrayDecl):
                    if not isinstance(td.dim, c_ast.Constant):
                        refuse(it, "an array local needs a constant bound")
                    n = int(td.dim.value, 0)
                self.frame.declare(it.name, self.width_of(it.type), n)
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
            if not self.cur.lines and self.cur.label not in self.labels \
                    and not self.is_target(self.cur.label):
                self.blocks.remove(self.cur)         # nothing follows the last statement
            else:
                self.terminate(("ret",))             # PL/I END = return
        return self.render()

    def begin_stmt(self, node):
        """R8b: a value marked live for the previous statement is just a
        cached copy now (cost 0); the per-statement reference counters and
        the freed-slot set restart."""
        self.stmt_index = self.stmt_no[id(node)]
        self.frame.cur_stmt = self.stmt_index
        for r, v in self.regs.c.items():
            if v is not None and v[0] == "live":
                if r == self.keep_live:
                    continue          # R21b: the loop register stays protected through the body's first statement
                if r in self.pinned:
                    continue          # R7c': pinned across an R13b if-body
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
                    self.frame.free_string(a, self.stmt_index)

    def width_of(self, tdecl):
        while isinstance(tdecl, (c_ast.PtrDecl, c_ast.ArrayDecl)):
            tdecl = tdecl.type
        names = tdecl.type.names if isinstance(tdecl.type, c_ast.IdentifierType) else None
        if names is None:
            refuse(tdecl, "struct locals are not in the subset")
        if names[-1] in ("char",):
            return 8            # PL/I CHARACTER: a byte, addressed with bp()
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
        def visit(lst, path=()):
            for st in lst:
                i = self.stmt_no[id(st)]
                self.arm_path[i] = path
                subs = self.sub_stmts(st)
                if subs:
                    for _, ch in st.children():
                        if not isinstance(ch, c_ast.Node) or any(ch is x for x in subs):
                            continue
                        walk(ch, i)
                    # each branch ARM gets its own path component, so two uses
                    # in different arms are not "the same straight-line flow"
                    for k, sub in enumerate(subs):
                        visit([sub] if not isinstance(sub, c_ast.Compound)
                              else (sub.block_items or []), path + ((i, k),))
                else:
                    walk(st, i)
        visit(stmts)

    def subscript_vars(self, node):
        """Every identifier appearing anywhere in a subscript expression.
        `subscript_key` names the ONE variable a simple subscript reduces to
        and returns None for anything else; R36's D2 test needs to know
        whether the loop variable occurs at all, however the inner subscript
        is spelled (OWNS' is `SUB(i, 10)`)."""
        out = set()
        def scan(n):
            if isinstance(n, c_ast.ID):
                out.add(n.name)
            for _, child in n.children():
                scan(child)
        scan(node)
        return out

    def subscript_key(self, node):
        if isinstance(node, c_ast.FuncCall) and node.name.name == "SUB":
            node = node.args.exprs[0]
        if isinstance(node, c_ast.ID):
            return node.name
        # `*param` -- a by-reference argument used as a subscript (OWNS
        # subscripts PLAYER with *p in all eight of its element references)
        if isinstance(node, c_ast.UnaryOp) and node.op == "*" \
                and isinstance(node.expr, c_ast.ID) and node.expr.name in self.args:
            return node.expr.name
        # P39: an UPLEVEL variable as a subscript (FIRE.2 subscripts PLAYER
        # with both UP(FIRE, w12) and *UPARG(FIRE, 1))
        if self.is_uplevel(node, ("UP",)):
            return "UP$%s" % node.args.exprs[1].name
        if isinstance(node, c_ast.UnaryOp) and node.op == "*" \
                and self.is_uplevel(node.expr, ("UPARG",)):
            return "UPARG$%s" % node.expr.args.exprs[1].value
        return None

    def subscript_node(self, vname):
        return self._subnode.get(vname)

    def subscript_node_ok(self, node):
        """The subscript forms the codegen model knows: a variable (local or
        static) or a dereferenced by-reference parameter."""
        return isinstance(node, c_ast.ID) or (
            isinstance(node, c_ast.UnaryOp) and node.op == "*"
            and isinstance(node.expr, c_ast.ID) and node.expr.name in self.args) \
            or self.is_uplevel(node, ("UP",)) \
            or (isinstance(node, c_ast.UnaryOp) and node.op == "*"
                and self.is_uplevel(node.expr, ("UPARG",)))

    def var_changes_between(self, name, i, j):
        """Is variable `name` assigned in statements (i, j]?"""
        return any(i < k <= j for k in self.uses.get(("assign", name), []))

    def last_use_of(self, key):
        u = self.uses.get(key, [])
        return max(u) if u else -1

    def reachable_later_uses(self, key, i):
        """Uses of `key` after statement i that the flow from i can actually
        REACH.  Two statements in different ARMS of an `if` are mutually
        exclusive, so neither is "a later statement" for the other.

        This is what separates OWNS from DIED for R27.  DIED's nine WBTZs are
        consecutive siblings, so the first sees eight later uses and saves
        16*P*686 to a temp (70166296 `XWSTA 2,[ac3+8]`).  OWNS' seven bit
        references sit in seven different if-arms, so each sees NO reachable
        later use and each recomputes `*p * 686 * 16` from scratch -- which is
        exactly what the book does.  R27's trigger is narrowed accordingly.
        """
        pi = self.arm_path.get(i, ())
        out = []
        for j in self.uses.get(key, []):
            if j <= i:
                continue
            pj = self.arm_path.get(j, ())
            n = min(len(pi), len(pj))
            if pi[:n] == pj[:n]:      # one path continues the other
                out.append(j)
        return out

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
            PROD.TEMPLATES["goto"](self, self.c_label(st.name).label)
            self.fired("goto", st, "goto %s" % st.name)
            self.start(self.new_block())
            return
        if isinstance(st, c_ast.Return):
            if st.expr is not None:
                # R35: a value-returning PL/I function stores its result into
                # the SAVED-ac0 IMAGE in its own frame (`image ac0 wfp-8`,
                # quest.addrbook header), so WRTN restores it into ac0.  This
                # is exactly the addrbook's `slotpatch` flag.  A 32-bit result
                # takes the whole image word at wp(ac3, -8)
                # (DISTANCE_TO_PLAYER 701687D9 `XWSTA 0,[ac3+0x7FF8]`); a
                # 16-bit result takes its low half at wp(ac3, -7)
                # (OWNS 70175DA2 `XNSTA 2,[ac3+0x7FF9]`).
                if self.ret_width is None:
                    refuse(st, "this routine is declared void but returns a value")
                if "slotpatch" not in (self.entry.get("flags") or ""):
                    refuse(st, "%s is not marked slotpatch in the addrbook, so it "
                               "does not return a value" % self.name)
                v = self.value(st.expr, want_reg=True)
                if self.ret_width == 32:
                    self.emit("M32[wp(ac3, -8)] = %s" % v.reg)
                else:
                    self.narrow_check(st, v)
                    self.emit("M16[wp(ac3, -7)] = trunc16(%s)" % v.reg)
            if st.expr is None:
                PROD.TEMPLATES["return_void"](self)
                self.fired("return_void", st, "return")
            else:
                self.terminate(("ret",))
                self.fired("return_value", st, "return <value>",
                           consumes=(st.expr,))
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
        if isinstance(st, c_ast.Return):
            # R35: a VALUE return is not one word -- it evaluates the value,
            # stores it into the saved-ac0 image and only then WRTNs, so it
            # takes the R13b shape (skip-if-c over `goto else`), which is what
            # OWNS' seven `if (*item == K) return BIT(...)` arms do.
            return st.expr is None
        return isinstance(st, (c_ast.Goto, c_ast.Continue))

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
        # R13: the one-word THEN is CONSUMED -- the skip form has no separate
        # reduction for it.
        self.fired("if_oneword", st, "if <c> <one-word>",
                   consumes=(st.iftrue, then),
                   choices=[("spelling", "skip_if_not")])

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

        # R21d (P39): the DO control variable may be a BY-REFERENCE PARAMETER,
        # `for (*i = 1; *i <= lim; (*i)++)`.  Everything about the loop is
        # unchanged except that every reference to it is INDIRECT through the
        # argument slot -- `XNSTA 0,@[ac3+0xFFF4]` and the indirect `XNDO`
        # rather than the direct forms.  Witness: LIST_PLAYERS.3 7016F55E.
        def _ctl(e):
            """-> the control variable's name, or None."""
            if isinstance(e, c_ast.ID):
                return e.name
            if isinstance(e, c_ast.UnaryOp) and e.op == "*" and isinstance(e.expr, c_ast.ID):
                return e.expr.name
            return None

        def _indirect(e):
            return isinstance(e, c_ast.UnaryOp) and e.op == "*"

        ok = (isinstance(init, c_ast.Assignment) and _ctl(init.lvalue) is not None
              and isinstance(cond, c_ast.BinaryOp) and cond.op == "<="
              and _ctl(cond.left) == _ctl(init.lvalue)
              and _indirect(cond.left) == _indirect(init.lvalue)
              and isinstance(nxt, c_ast.UnaryOp) and nxt.op in ("p++", "++")
              and _ctl(nxt.expr) == _ctl(init.lvalue)
              and _indirect(nxt.expr) == _indirect(init.lvalue))
        if not ok:
            refuse(st, "only `for (v = a; v <= lim; v++)` (PL/I DO v = a TO lim) "
                       "is in the subset, with v a 16-bit local or a "
                       "by-reference parameter")
        vname = _ctl(init.lvalue)
        via_arg = _indirect(init.lvalue)
        if via_arg:
            if vname not in self.args:
                refuse(st, "the DO variable *%s is not a parameter" % vname)
            n_arg, awidth, aconst = self.args[vname]
            if awidth != 16:
                refuse(st, "the DO variable must be 16-bit")
            if aconst:
                refuse(st, "write through a const parameter (the DO variable)")
            vref = "R[ac3 + -%d]" % (10 + 2 * n_arg)
            vkey = ("arg", vname)
        else:
            if vname not in self.frame.locals or self.frame.locals[vname][1] != 16:
                refuse(st, "the DO variable must be a 16-bit local")
            vref = "wp(ac3, %d)" % self.frame.locals[vname][0]
            vkey = ("local", vname)
        vslot = None if via_arg else self.frame.locals[vname][0]
        a = self.value(init.rvalue, want_reg=False)
        if a.kind != "const":
            refuse(st, "DO initial value must be a constant (only form seen)")
        const_lim = isinstance(cond.right, c_ast.Constant)
        expr_lim = False
        # R36 (P37/OWNS 70175CBF..CC7; P40/QUEST.1 amends the placement): a
        # subscript in the BODY that is invariant in the loop variable is
        # evaluated at the loop head -- its bound check (R17) and stride
        # multiply BEFORE the loop's own initialisation, its temp store AFTER
        # it.
        #
        # D1 (P40/QUEST.1): "before the loop's own initialisation" means before
        # the WHOLE loop header, the limit expression included -- not merely
        # before the control variable's store.  OWNS could not witness this:
        # its limit is a CONSTANT (R21a), so there was no limit evaluation to
        # be ordered against.  QUEST.1 is the first routine with R36 and R21e
        # together, and its init block opens with the stride multiply and
        # reaches the limit load four instructions later (7015C5EA..F0).  The
        # parent QUEST @7015C337 has the same loop with the registers permuted
        # and the same order (7015c344 multiply, 7015c34d limit).
        hoisted = self.hoist_invariant_subscripts(
            st, vname,
            header_expr_limit=not const_lim and not (
                isinstance(cond.right, c_ast.ID) and cond.right.name in self.frame.locals))
        if not const_lim:
            if isinstance(cond.right, c_ast.ID) and cond.right.name in self.frame.locals:
                lim = self.value(cond.right, want_reg=True)  # the limit first: it is live for the test
                self.regs.c[lim.reg] = ("live", "lim")
                lslot = self.frame.locals[cond.right.name][0]
            else:
                # R21e AMENDED (D3, P40/QUEST.1): a DO limit that is an
                # EXPRESSION is evaluated ONCE into a frame temp -- PL/I
                # evaluates the TO expression once.  P39 added "and is reloaded
                # from that temp for the entry test", which was fitted to
                # LIST_PLAYERS.3, the only witness it had.  It is not a
                # property of the limit: there the initial constant took ac0,
                # the register the limit was in, so the value was simply gone.
                # QUEST.1 puts the constant in ac2 and ac1 carries the limit
                # from 7015C5F0 straight through the entry test at 7015C5FD --
                # no reload.  The weaker rule explains both: THE LIMIT IS AN
                # ORDINARY LIVE VALUE, reloaded only if its register was taken.
                # It also removes a reachable R41′ violation -- the old code's
                # unconditional `pick` could put this *value* in ac3, the frame
                # register, once ac0/ac1/ac2 were spoken for.
                expr_lim = True
                lv = self.value(cond.right, want_reg=True)
                lslot = self.frame.alloc_temp(("dolim", id(st)))
                self.emit("M16[wp(ac3, %d)] = trunc16(%s)" % (lslot, lv.reg))
                limkey = ("dolim", id(st))
                self.regs.set(lv.reg, ("live", limkey))
        # R21c (P37/OWNS; consistent with UPDATE_SCREENS and REFRESH_SCREEN):
        # the DO-loop register is picked from ac0/ac1 only -- ac2 stays free
        # for addressing -- and it is picked BEFORE the initial value's own
        # register.  When the two differ the constant is stored from its own
        # register and copied to the loop register (OWNS `WMOV 2,1`); when they
        # coincide, as in both P35 loops, no move appears.
        # R21c, ENFORCED (P40/QUEST.1): the loop register comes from ac0/ac1
        # ONLY -- ac2 stays free for addressing.  The old `pick(avoid=("ac2",))`
        # stated the rule but did not enforce it: with ac0 and ac1 both live it
        # fell through to ac3 and put the loop counter in the FRAME register,
        # emitting a spurious `ac3 = ac2` and an LDAFP to recover the frame.
        # QUEST.1 is the first loop that reaches that state (the hoist holds one
        # value register and the limit the other), so nothing caught it before.
        # With both live the costs tie at 3 and R7 breaks the tie on the lower
        # number -- ac0, which is the book's loop register (7015C600 `WMOV 2,0`).
        lr = self.regs.pick(only=("ac0", "ac1"))
        cr = self.regs.pick()
        self.emit("%s = %s" % (cr, hexc(a.const)))
        self.regs.set(cr, ("const", a.const))
        self.emit("M16[%s] = trunc16(%s)" % (vref, cr))
        # R21c (P40): the copy into the loop register is emitted AT THE FIRST
        # POINT THE LOOP REGISTER IS FREE.  In OWNS lr is ac1, which holds only
        # the dead stride constant once the control variable is stored, so the
        # move lands there and the hoist store follows it (70175CC7 `WMOV 2,1`
        # then `XWSTA 0,[ac3+4]`).  In QUEST.1 lr is ac0, which still holds the
        # HOISTED ELEMENT ADDRESS until its own store, and the entry test then
        # intervenes -- so the move is deferred onto the body edge (7015C600).
        # One mechanism, two placements; R36's "the temp store comes after the
        # loop's own initialisation" is unchanged in both.
        moved = False
        if cr != lr and lr not in [hr for _, hr in hoisted]:
            self.emit("%s = %s" % (lr, cr))
            self.regs.set(lr, ("dup", cr))
            moved = True
        if expr_lim:
            # D3: reload ONLY if the limit's register was taken in between.
            held = [r for r in ("ac0", "ac1", "ac2") if self.regs.c[r] == ("live", limkey)]
            if held:
                lim = Val("reg", reg=held[0], width=16)
            else:
                # R21e′/R41′, P41 audit: the limit is a VALUE, so it comes from
                # the value class {ac0, ac1} — the SAME class R21c′ enforces for
                # the loop register, minus the loop register itself.  This was
                # `pick(avoid=(lr, "ac2"))`, which stated that intent ("ac2 stays
                # free for addressing") but left ac3 — the FRAME register — in the
                # candidate set: with lr = ac0 and ac1 live at cost 3, ac3 at
                # FP_COST 2 wins and the loop limit lands in the frame pointer.
                # Identical in shape to the R21c bug P40 found forty lines above,
                # and missed by that fix.  NOT WITNESSED: none of the four matched
                # routines reaches this line (OWNS' limit is constant, the two P35
                # loops have no reload), so this closes a reachable leak by
                # applying an already-derived class, and asserts nothing new.
                lr2 = self.regs.pick(only=("ac0", "ac1"), avoid=(lr,))
                self.emit("%s = sx16(M16[wp(ac3, %d)])" % (lr2, lslot))
                self.regs.set(lr2, ("live", limkey))
                lim = Val("reg", reg=lr2, width=16)
        for skey, hr in hoisted:
            slot = self.frame.alloc_temp(skey)
            self.emit("M32[wp(ac3, %d)] = %s" % (slot, hr))
            # `hoisted`: the temp lives for the whole loop and the body reloads
            # it on EVERY iteration (R36), so it is never freed at a last use
            # the way an ordinary R9/R10 temp is.
            self.cse[skey] = dict(slot=slot, stmt=self.stmt_index, pos=0, hoisted=True)
        incr_b, body_b, after_b = self.new_block(), self.new_block(), self.new_block()
        last_stmt = self.is_last_stmt
        if const_lim:
            # R21a: constant bounds — no entry test; the init block jumps over
            # the increment block straight into the body (REFRESH_SCREEN 70176B06)
            if cr != lr and not moved:
                self.emit("%s = %s" % (lr, cr))
                self.regs.set(lr, ("dup", cr))
            self.terminate(("goto", [body_b.label], "0"))
        else:
            exit_b, skip_b = self.new_block(), self.new_block()
            # The entry test compares the INITIAL VALUE's register, not the
            # loop register: the copy into the loop register happens on the
            # body edge (below), so at the test the value is still only in cr.
            # They coincide whenever cr == lr, which is every previously
            # matched loop -- QUEST.1 is the first where they differ.
            self.terminate(("goto", [exit_b.label, skip_b.label], "(%s <=s %s)" % (cr, lim.reg)))
            self.cur = exit_b
            self.terminate(("ret",) if last_stmt else ("goto", [after_b.label], "0"))
            self.cur = skip_b
            # R21c (P40/QUEST.1): the copy into the loop register sits on the
            # EDGE INTO THE BODY.  With an entry test that edge is the skip
            # block (QUEST.1 7015C600 `WMOV 2,0`; the parent QUEST 7015c35a);
            # with a constant limit (R21a) the init block falls straight into
            # the body and the move appears at its end, which is where OWNS
            # 70175CC7 shows it.  One rule, two block shapes.
            if cr != lr and not moved:
                self.emit("%s = %s" % (lr, cr))
                self.regs.set(lr, ("dup", cr))
            self.terminate(("goto", [body_b.label], "0"))
        # increment block (continue target): XNDO with the limit in the loop register
        self.cur = incr_b
        if const_lim:
            self.emit("%s = %s" % (lr, hexc(int(cond.right.value, 0))))
        else:
            self.emit("%s = sx16(M16[wp(ac3, %d)])" % (lr, lslot))
        self.emit("t1 = nadd(M16[%s], 1)" % vref)
        self.emit("M16[%s] = t1" % vref)
        self.emit("t2 = (t1 >s %s)" % lr)
        self.emit("%s = t1" % lr)
        # R21/R22: the XNDO tile -- ONE instruction, five IR statements.  The
        # loop register comes from R21c''s class {ac0, ac1}; the limit's
        # placement is a slot choice when it is not a constant.
        self.fired("do_loop", st, "DO %s" % (self.loop_var or "?"),
                   consumes=(st.init, st.cond, st.next),
                   choices=[("reg", lr)] + ([] if const_lim else [("slot", lslot)]))
        self.terminate(("goto", [body_b.label, after_b.label], "t2"))
        # body: at its head only the loop register is known (and protected)
        self.cur = body_b
        self.regs.reset()
        self.regs.set(lr, ("live", ("local", vname)))
        self.loop_var, self.loop_reg = vname, lr
        self.loop_stack.append(dict(incr=incr_b, after=after_b))
        self.regs.set(self.regs.fpreg() or "ac3", FP)   # R33: the frame is live at a loop head
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

    def hoist_invariant_subscripts(self, st, vname, header_expr_limit=False):
        """R36 -- the loop-invariant subscript hoist.

        OWNS 70175CBF..70175CC7: the body's only element reference is
        `PLAYER(SUB(*p,10)).fm390(i)`.  Its OUTER subscript `*p` does not
        depend on the DO variable, and the compiler evaluates it once at the
        loop head: the DERR 17 check sits in the entry block and the `*686`
        multiply opens the initialisation block, with the scaled value flushed
        to its R9 temp only after `i = 1` has been set up.  The body then
        reloads the temp on every iteration (XWADD [ac3+4]).

        Returns [(skey, register)] for the caller to store after the loop init.
        Confidence C: ONE instance.  Neither P35 loop can witness it --
        UPDATE_SCREENS' body subscript IS the loop variable (R9a) and
        REFRESH_SCREEN's body references no table -- so a second witness has
        to come from a routine with two loops (DIED has two).
        """
        seen, out = [], []
        def walk(node):
            for _, child in node.children():
                if isinstance(child, c_ast.ArrayRef) and isinstance(child.name, c_ast.ID) \
                        and self.L.table(child.name.name) is not None:
                    key = self.subscript_key(child.subscript)
                    if key != vname and (child.name.name, key) not in seen:
                        seen.append((child.name.name, key))
                        out.append((child.name.name, child.subscript))
                walk(child)
        walk(st.stmt)
        # D2 (P40/QUEST.1) -- R36 AMENDED: what is hoisted is the invariant
        # PART of the reference, and how much of it is invariant falls out of
        # the reference itself.
        #
        # OWNS' body reference is `PLAYER(*p).fm390(i)`: its INNER subscript
        # varies with the loop variable, so only the outer scale can be lifted
        # and the base add must stay in the body (70175CDB `XWADD 1,[ac3+4];
        # LWADD 1,[0x70000210]`).  QUEST.1's `PLAYER(PLAYER_NUM).fm589` is
        # invariant ENTIRE, so the whole element address is loop-invariant and
        # the base is added before the store (7015C5F9 `LWADD 0,[0x70000210]`,
        # then `XWSTA 0,[ac3+0x6]`); the body reloads it straight into ac2 and
        # addresses `wp(ac2, -589)` with no base add at all (7015C60E).  The
        # parent QUEST does the same at 7015c347/7015c356/7015c36e.
        #
        # One rule, two shapes, no special case for either routine.
        def inner_subscript_uses_v(tname, key):
            """True when some reference to T[key] is a multi-dimensional field
            whose INNER subscript mentions the loop variable -- OWNS' shape."""
            found = [False]
            def scan(node):
                for _, child in node.children():
                    if isinstance(child, c_ast.ArrayRef) \
                            and isinstance(child.name, c_ast.StructRef) \
                            and isinstance(child.name.name, c_ast.ArrayRef) \
                            and isinstance(child.name.name.name, c_ast.ID) \
                            and child.name.name.name.name == tname \
                            and self.subscript_key(child.name.name.subscript) == key \
                            and vname in self.subscript_vars(child.subscript):
                        found[0] = True
                    scan(child)
            scan(st.stmt)
            return found[0]

        done = []
        for tname, sub in out:
            t = self.L.table(tname)
            bounds = None
            s = sub
            if isinstance(s, c_ast.FuncCall) and s.name.name == "SUB":
                bounds = int(s.args.exprs[1].value, 0)
                s = s.args.exprs[0]
            key = self.subscript_key(sub)
            whole = not inner_subscript_uses_v(tname, key)
            skey = ("elem", tname, key) if whole else ("scaled", tname, key)
            v = self.value(s, want_reg=True)
            r = v.reg
            if bounds is not None:
                self.bounds_check(r, bounds)
            # D4 (P40/QUEST.1 + QUEST, with OWNS as the negative control).
            # R7 alone gives ac1 here and the book gives ac2 -- but only when
            # an EXPRESSION limit still has to be evaluated in the same loop
            # header.  The stride constant is transient (dead at the WMUL);
            # the limit is not, and it needs a VALUE register.  So the constant
            # takes ac2 and leaves the value register for the limit.
            #
            # Three routines, and the third is the control:
            #   QUEST.1 7015C5EA  ac0=subscript live, limit expr  -> ac2 (R7: ac1)
            #   QUEST   7015c344  ac1=subscript live, limit expr  -> ac2 (R7: ac0)
            #   OWNS    70175CC7  ac0=subscript live, limit CONST -> ac1 (R7: ac1)
            # QUEST is the discriminator: its registers are PERMUTED against
            # QUEST.1's (subscript ac1, limit ac0), so "avoid ac1" and "prefer
            # ac2 always" both fail on it while "avoid the register the limit
            # will take" comes out right in both.  OWNS has no limit to
            # evaluate, so there is nothing to avoid and plain R7 stands --
            # which is why this was invisible until a routine had R36 and R21e
            # in the same loop.
            #
            # SCOPED DELIBERATELY to the loop header.  The general form of this
            # -- that the allocator works over a whole statement's expression
            # tree and protects a register a later operand will occupy -- also
            # fits QUEST.1 7015C621 and 7015C650, but it is a claim about WHEN
            # the allocator runs, not merely what it prefers, and it is not
            # implemented here.  See docs/Project40/QUEST1_PREDICTIONS.md D4.
            avoid = (r,)
            if header_expr_limit:
                other = [q for q in ("ac0", "ac1") if q != r]
                if other:
                    avoid = avoid + (other[0],)
            kr = self.regs.pick(avoid=avoid)
            # P42 ORPHAN: R36's hoist emits here and fires NO production.  A
            # bottom-up generator reducing `T(i).f` cannot know it is inside a
            # loop or which parts of the reference are invariant; a later loop
            # pass can.  So this emission has no reduction address, which is
            # why R36 has been read off one routine and been wrong for the
            # next.  Recorded, not given a production: inventing a 38th to
            # house it would hide the finding.
            self.ledger.orphan_emission(
                "hoist_invariant_subscripts", 3 if whole else 2,
                "R36/R36a: loop-invariant hoist -- a PLACEMENT decision, not a reduction")
            self.emit("%s = %s" % (kr, hexc(t["stride"])))
            self.regs.set(kr, ("const", t["stride"]))
            self.emit("%s = mul(%s, %s)" % (r, r, kr))
            if whole:
                base_addr = self.L.static(t["base"])["addr"]
                self.emit("%s = add(%s, M32[%s])" % (r, r, hexc(base_addr)))   # LWADD
                self.elem_sym[skey] = s_addv(self.subscript_sym(key, t["stride"]),
                                             s_load(s_const(base_addr), 32))
            self.regs.set(r, ("live", skey))
            done.append((skey, r))
        return done

    def if_multi(self, st):
        """R13b: `if (c) {body} [else {alt}]` with a body longer than one
        instruction: skip-if-c over a `goto else` block; the body ends with
        `goto after` when an else part follows (REFRESH_SCREEN 70176A93..ABA)."""
        self._if_multi_n = getattr(self, "_if_multi_n", 0) + 1
        test = self.condition(st.cond, negate=False)
        # R13b: the diamond.  Unlike if_oneword this does NOT consume its
        # arms -- the body and the else part are ordinary statements and are
        # reduced normally, so they keep their own addresses.
        self.fired("if_multi", st, "if <c> {body}%s"
                   % (" else {alt}" if st.iffalse is not None else ""),
                   choices=[("order", "body_first")])
        skip_b, body_b, after_b = self.new_block(), self.new_block(), self.new_block()
        else_b = self.new_block() if st.iffalse is not None else after_b
        self.terminate(("goto", [skip_b.label, body_b.label], test))
        # R8c: the state on the SKIP edge -- the only edge into the
        # continuation when the body does not fall through (see below)
        skip_state = self.regs.snapshot()
        self.cur = skip_b
        self.terminate(("goto", [else_b.label], "0"))
        self.cur = body_b
        # R7c' (P37/OWNS): an R13b body INHERITS the skip's register state,
        # and a register holding a variable that the if's CONTINUATION still
        # reads is PINNED (cost 3) for the body's duration.  OWNS holds
        # `*item` in ac0 across the whole seven-arm chain, so six bodies push
        # the record subscript into ac1 (`XNLDA 1,@[ac3+0xFFF4]`) while the
        # SEVENTH -- after which nothing reads `*item` -- takes ac0
        # (70175D8E `XNLDA 0`).
        # This is the narrow survivor of a falsified wider rule: see var_read.
        # It cannot disturb an R13 one-word THEN (there is no body block), which
        # is why UPDATE_SCREENS' `if (...) continue;` statements are untouched.
        self.regs.restore(skip_state)
        outer_pin = self.pinned
        self.pinned = set(self.continuation_pins(st))
        for r in self.pinned:
            v = self.regs.c.get(r)
            if v is not None and v[0] == "var":
                self.regs.c[r] = ("live", v[1])
        self.run_body(st.iftrue)
        self.pinned = outer_pin
        # after a `return`/`goto` the statement walker has already opened a
        # fresh empty block, so "the body fell through" means that block has
        # content or is itself a branch target
        fell_through = (self.cur is not None and self.cur.term is None
                        and (self.cur.lines or self.is_target(self.cur.label)))
        if not fell_through and self.cur is not None and not self.cur.lines \
                and self.cur in self.blocks:
            self.blocks.remove(self.cur)
        if fell_through:
            if st.iffalse is not None:
                self.terminate(("goto", [after_b.label], "0"))
            else:
                self.join(after_b)
                self.regs.reset()
                return
        if not fell_through and st.iffalse is None:
            # R8c: an `if (c) {body}` whose body RETURNS or GOTOes has only one
            # edge into its continuation -- the skip's own `goto` -- so nothing
            # joins there and register knowledge SURVIVES.  OWNS 7016CED..:
            # `ac0 = *item` is loaded once and all seven chain tests compare
            # ac0 directly; a reset would reload it seven times.  R8 (reset at
            # a label) still governs a genuine join, where two paths meet.
            self.cur = after_b
            self.regs.restore(skip_state)
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

    def release_pins(self):
        """R7c'': drop the if-body pin, demoting the pinned live marks back to
        ordinary cached copies (cost 0)."""
        for r in self.pinned:
            v = self.regs.c.get(r)
            if v is not None and v[0] == "live":
                self.regs.c[r] = ("var", v[1])
        self.pinned = set()

    def continuation_pins(self, st):
        """R7c': the registers holding variables that the statements AFTER this
        `if` still read -- the values that must survive the body."""
        i = self.stmt_no[id(st)]
        out = []
        for r, v in self.regs.c.items():
            if v is None or v[0] not in ("var", "live"):
                continue
            key = v[1]
            if not (isinstance(key, tuple) and len(key) == 2 and key[0] in ("arg", "local")):
                continue
            if self.reachable_later_uses(("id", key[1]), i):
                out.append(r)
        return out

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
        # C1/C2: the test's shape.  A BIT reference in a test is R29's
        # materialise-then-sign-test, never a folded skip; everything else is
        # a comparison, and whether the right operand rides as an immediate
        # or is loaded into a register is a SPELLING choice (R14).
        if isinstance(cond, c_ast.FuncCall) and cond.name.name == "BIT":
            self.fired("condition_bit", cond, "BIT in a test", consumes=(cond,))
        elif isinstance(cond, c_ast.BinaryOp):
            imm = isinstance(cond.right, c_ast.Constant)
            self.fired("condition_cmp", cond, "test %s" % cond.op,
                       consumes=(cond.right,) if imm else (),
                       choices=[("spelling", "immediate" if imm else "register")])
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
        # R7: the left operand is a value the statement STILL NEEDS (cost 3),
        # so the right operand's load must pick elsewhere.  OWNS 70175CDB:
        # the field lands in ac0 and `*item` is then forced into ac2
        # (`XNLDA 2,@[ac3+0xFFF2]`), which a cost-0 cached-copy reading of ac0
        # would have collided with.  Only a register-borne right operand can
        # collide; a constant is a wide skip-with-immediate (R15).
        self.regs.c[lhs.reg] = ("live", ("cmp-lhs", lhs.reg))
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
            kind = "assign_rt"
        elif isinstance(lv, c_ast.ArrayRef):
            r = self.indexed_store(lv, st.rvalue)
            # R23: the whole subscript chain and the value are the tile's span
            self.fired("assign_indexed", st, "T[i].f[...] = e",
                       consumes=(lv, st.rvalue), choices=[("order", "address_then_value")])
            return r
        else:
            v = self.value(st.rvalue, want_reg=True)
            kind = "assign_uplevel" if self.is_uplevel(lv, ("UP",)) or (
                isinstance(lv, c_ast.UnaryOp) and lv.op == "*"
                and self.is_uplevel(lv.expr, ("UPARG",))) else "assign_local"
        self.flush_elem_save()
        self.store(lv, v)
        ch = []
        if kind in ("assign_local", "return_value") and v.kind == "reg":
            ch = [("reg", v.reg)]
        elif kind == "assign_uplevel" and v.kind == "reg":
            ch = []          # the link's register is link_load's choice
        self.fired(kind, st, "%s = ..." % type(lv).__name__,
                   consumes=(st.rvalue,), choices=ch)

    def narrow_check(self, lv, v):
        """P36 ruling 1: no silent 32->16 narrowing.  The DG compiler emits a
        CHECKED convert (CVWN) here; the source of record must say so with
        `cvwn(e)`, so a 32-bit value reaching a 16-bit destination without one
        is a REFUSAL, not an inserted instruction."""
        if v.width == 32:
            # R16b does not reach a LITERAL that already fits in 16 bits: the
            # compiler loads it with a sign-extended NLDAI and stores it with a
            # bare trunc16, no CVWN -- there is nothing to check at run time.
            # (OWNS 70175CE7 `return -32768`; the same shape as every
            # `M16[wp(ac3,2)] = trunc16(ac1)` loop initialisation.)
            if v.const is not None and -0x8000 <= _sx32(v.const) <= 0x7FFF:
                return
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
        if self.is_uplevel(lv, ("UP",)):
            # R43: an uplevel WRITE is the SAME shape as an uplevel read — link
            # into a base, then displace.  There is no separate write form; the
            # only witness that could have shown one is FIRE.1, which reads
            # wp(link,10) and writes wp(link,14) with identical addressing.
            pname, fname, slot, width = self.uplevel_slot(lv)
            b = self.link_reg()
            r = self.to_reg(v)
            if width == 16:
                self.narrow_check(lv, v)
                self.emit("M16[wp(%s, %d)] = trunc16(%s)" % (b, slot, r), uses_fp=False)
            else:
                self.emit("M32[wp(%s, %d)] = %s" % (b, slot, r), uses_fp=False)
            self.regs.invalidate_var(("up", pname, slot))
            self.regs.set(r, ("var", ("up", pname, slot)))
            return
        if isinstance(lv, c_ast.UnaryOp) and lv.op == "*" and self.is_uplevel(lv.expr, ("UPARG",)):
            pname, k = self.uplevel_arg(lv.expr)
            width = self.uparg_width(lv, pname, k)
            b = self.link_reg()
            r = self.to_reg(v)
            d = 10 + 2 * k
            if width == 16:
                self.narrow_check(lv, v)
                self.emit("M16[R[%s + -%d]] = trunc16(%s)" % (b, d, r), uses_fp=False)
            else:
                self.emit("M32[R[%s + -%d]] = %s" % (b, d, r), uses_fp=False)
            self.regs.invalidate_var(("uparg", pname, k))
            self.regs.set(r, ("var", ("uparg", pname, k)))
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

    # -- indexed field READ (R23r; P37/OWNS 70175CDB) ------------------------
    def is_indexed_field(self, e):
        """`TABLE[i].field[j][...]` as an rvalue."""
        node = e
        while isinstance(node, c_ast.ArrayRef):
            node = node.name
        return (isinstance(node, c_ast.StructRef) and node.type == "."
                and isinstance(node.name, c_ast.ArrayRef)
                and isinstance(node.name.name, c_ast.ID)
                and self.L.table(node.name.name.name) is not None)

    def indexed_field_load(self, e, avoid):
        """R23r — the READ counterpart of R23.  The address is built exactly as
        for a store (each subscript checked, scaled, added to the running sum,
        the base added last, WMOV to ac2) and the field is then loaded from
        wp(ac2, K).  OWNS 70175CDB is the whole rule in five instructions:

            ac1 = <i>                        the subscript (the DO register)
            ac1 = add(ac1, M32[wp(ac3, 4)])  XWADD: the R9 scaled temp
            ac1 = add(ac1, M32[0x70000210])  LWADD: the base
            ac2 = ac1                        WMOV
            ac0 = sx16(M16[wp(ac2, -390)])   the field

        R23a: a stride of 1 emits NO scaling at all (stride 2 is `add(r, r)`,
        any other stride is a constant in a register and WMUL) -- OWNS is the
        first inner dimension of stride 1 seen.
        """
        subs = []
        node = e
        while isinstance(node, c_ast.ArrayRef):
            subs.append(node.subscript)
            node = node.name
        subs.reverse()
        tname = node.name.name.name
        t = self.L.table(tname)
        fld = t["fields"].get(node.field.name)
        if fld is None or "dims" not in fld:
            refuse(e, "%s.%s is not a multi-dimensional field" % (tname, node.field.name))
        if len(subs) != len(fld["dims"]):
            refuse(e, "wrong number of subscripts")
        vname = self.subscript_key(node.name.subscript)
        skey = ("scaled", tname, vname)
        i = self.stmt_index
        if not (skey in self.cse and self.cse[skey]["slot"] is not None):
            refuse(e, "an indexed field read needs the scaled subscript in a temp (only form seen)")
        partial = self.cse[skey]["slot"]
        base_addr = self.L.static(t["base"])["addr"]
        r = None
        for d, (sub, stride) in enumerate(zip(subs, fld["strides"])):
            v = self.value(sub, want_reg=True, avoid=avoid)
            r = v.reg
            if stride == 2:
                self.emit("%s = add(%s, %s)" % (r, r, r))                   # WADD r,r
            elif stride != 1:                                              # R23a: stride 1 scales by nothing
                kr = self.regs.pick(avoid=(r,))
                self.emit("%s = %s" % (kr, hexc(stride)))
                self.regs.set(kr, ("const", stride))
                self.emit("%s = mul(%s, %s)" % (r, r, kr))
            self.emit("%s = add(%s, M32[wp(ac3, %d)])" % (r, r, partial))   # XWADD
            self.regs.set(r, ("live", "partial"))
            if self.last_use_of(skey) <= i and self.frame.temps.get(partial) == skey:
                self.frame.free_temp(partial)
                self._freed.add(partial)
                self.cse[skey]["slot"] = None
            if d < len(subs) - 1:
                slot = self.frame.alloc_temp(("partial", d))
                self.emit("M32[wp(ac3, %d)] = %s" % (slot, r))
                partial = slot
            elif self.frame.temps.get(partial, None) is not None \
                    and self.frame.temps[partial][0] == "partial":
                self.frame.free_temp(partial)
        self.emit("%s = add(%s, M32[%s])" % (r, r, hexc(base_addr)))        # LWADD
        if r != "ac2":
            self.emit("ac2 = %s" % r)
            self.regs.set("ac2", ("dup", r))
        K, width = fld["K"], fld["width"]
        self.regs.c["ac2"] = ("addr", ("elem", tname, vname))
        text = "M32[wp(ac2, %d)]" % K if width == 32 else "sx16(M16[wp(ac2, %d)])" % K
        return self.load(text, width, ("elem", tname, K), True, avoid)

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
        if not self.subscript_node_ok(sub):
            refuse(sub, "BIT(): subscript must be a variable or a dereferenced "
                        "parameter (optionally SUB(v, n))")
        vname = self.subscript_key(sub)
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
            # PATH 1 (R27): the 16*scaled temp is live; subscript not evaluated
            self.fired("bit_address", sub, "BIT %s(%s) 16*scaled-temp" % (tname, vname),
                       consumes=(sub,), choices=[("reg", r), ("slot", slot)])
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
        # R7c'' : the enclosing if's pinned condition value is released once
        # the SUBSCRIPT EXPRESSION is done -- the stride multiply is its last
        # instruction.  The bit-address arithmetic that follows may take the
        # register.  OWNS' six pinned bodies avoid ac0 for `*p` and for the
        # stride constant 686 and then take it for the constant 16
        # (70175CF8 `NLDAI 686,2` then `NLDAI 16,0`), which is exactly this
        # boundary; the seventh, unpinned, uses ac0/ac1/ac2 in order.
        self.release_pins()

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
        if self.reachable_later_uses(bkey, i):
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
            # R28, and the R41' NEGATIVE case: this is the same physical
            # operation as field_direct's base load -- loading a record base --
            # but its legal set is all four registers, because the production
            # is FOR something else (a bit reference, not indexing).  The
            # SD_PTR census: {ac0, ac1} 302 times out of 306.
            self.fired("bit_base", None, "bit-reference record base",
                       choices=[("reg", r)])
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
            self.fired("bit_stmt", call, "BIT_SET", consumes=(args[0], args[1]))
            return
        if op == "BIT_CLR":
            self.emit("%s = %s & ~%s" % (mem, mem, mask))
            self.fired("bit_stmt", call, "BIT_CLR", consumes=(args[0], args[1]))
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
        self.fired("bit_stmt", call, "BIT_PUT", consumes=(args[0], args[1]),
                   choices=[("reg", vr)])

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
        if isinstance(e, c_ast.UnaryOp) and e.op == "-" and isinstance(e.expr, c_ast.Constant):
            # a negated literal IS a literal: the compiler loads the
            # sign-extended immediate in one NLDAI (OWNS 70175CE7 `-32768` ->
            # `ac0 = 0xFFFF8000 ; NLDAI 32768 (0x8000),0`)
            e = c_ast.Constant(e.expr.type, str(-int(e.expr.value, 0)), e.coord)
        if isinstance(e, c_ast.Constant):
            k = int(e.value, 0) & 0xFFFFFFFF
            v = Val("const", text=hexc(k), width=32, const=k)
            if want_reg:
                r = self.regs.find(("const", k))
                if r is None:
                    r = self.regs.pick(avoid)
                    # PORTED (P42 Stage 3): productions.t_const_materialise
                    sp = PROD.TEMPLATES["const_materialise"](self, r, k)
                    self.fired("const_materialise", e, "Constant %s" % hexc(k),
                               choices=[("reg", r), ("spelling", sp)])
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
                _v = self.load("sx16(M16[wp(ac3, -9)])", 16, key, want_reg, avoid)
                self.fired("marker_ref", e, "arity marker wp(ac3,-9)")
                return _v
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
        if self.is_uplevel(e, ("UP",)):
            # R43: an uplevel READ — load the link into a base, then displace.
            # Structurally identical to the WRITE in store(); the compiler has
            # no separate form for the two (FIRE.1 wp(link,10) read vs
            # wp(link,14) written).
            pname, fname, slot, width = self.uplevel_slot(e)
            key = ("up", pname, slot)
            r = self.regs.find(("var", key))
            if r is not None:
                return Val("reg", reg=r, width=width)
            b = self.link_reg()
            text = "M32[wp(%s, %d)]" % (b, slot) if width == 32 \
                else "sx16(M16[wp(%s, %d)])" % (b, slot)
            _v = self.load(text, width, key, want_reg, avoid, name=fname)
            self.fired("uplevel_ref", e, "UP(%s, %s)" % (pname, fname),
                       choices=[("reg", b)])
            return _v
        if isinstance(e, c_ast.UnaryOp) and e.op == "*" and self.is_uplevel(e.expr, ("UPARG",)):
            # the parent's k'th argument: link, then INDIRECT through the
            # parent's argument slot (FIRE.2 7016A479 `XNLDA 1,@[ac2+0xFFF4]`)
            pname, k = self.uplevel_arg(e.expr)
            width = self.uparg_width(e, pname, k)
            key = ("uparg", pname, k)
            r = self.regs.find(("var", key))
            if r is not None:
                return Val("reg", reg=r, width=width)
            b = self.link_reg()
            d = 10 + 2 * k
            text = "M32[R[%s + -%d]]" % (b, d) if width == 32 \
                else "sx16(M16[R[%s + -%d]])" % (b, d)
            _v = self.load(text, width, key, want_reg, avoid)
            self.fired("uplevel_arg_ref", e, "UPARG(%s, %d)" % (pname, k),
                       choices=[("reg", b)])
            return _v
        if isinstance(e, c_ast.StructRef):
            return self.field(e, want_reg, avoid)
        if isinstance(e, c_ast.ArrayRef) and self.is_indexed_field(e):
            return self.indexed_field_load(e, avoid)
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
            self.fired("abs_builtin", e, "ABS()", choices=[("reg", r)])
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
            self.fired("convert", e, "%s()" % op)
            return Val("reg", reg=v.reg, width=16)
        if isinstance(e, c_ast.FuncCall) and e.name.name == "BIT":
            # R29: a bit reference used as a value, outside a condition
            # (OWNS returns one directly from seven of its arms)
            _bvr = self.bit_value_reg(e)
            self.fired("bit_value", e, "BIT() as a value", choices=[("reg", _bvr)])
            return Val("reg", reg=_bvr, width=16)
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
        column expression, so *x takes ac2).

        FALSIFIED HERE: P37 first tried R7c -- "the protection runs to the
        variable's LAST REACHABLE use, anywhere" -- to explain OWNS' bit-body
        registers.  It did not fix OWNS and it regressed UPDATE_SCREENS from
        72/72 to 47/72 (25 DIFF), because `*x` and `*y` are read by later
        SIBLING statements there and the compiler plainly does not protect
        them.  The surviving rule is the narrower R7c' in if_multi."""
        self._var_done[name] = self._var_done.get(name, 0) + 1
        total = self.var_refs.get(self.stmt_index, {}).get(name, 0)
        self.regs.c[r] = ("live", key) if self._var_done[name] < total else ("var", key)

    def load(self, text, width, key, want_reg, avoid, name=None):
        if not want_reg:
            return Val("mem", text=text, width=width)
        r = self.regs.pick(avoid)
        self.emit("%s = %s" % (r, text))
        self.regs.set(r, ("var", key))
        self.fired("scalar_ref", None, "load %s" % (name or key[0]),
                   choices=[("reg", r)])
        if name is not None:
            self.var_read(r, key, name)
        return Val("reg", reg=r, width=width)

    # -- record fields ---------------------------------------------------------
    def base_reg(self, ptr_name):
        """R5 as amended by R41′: a base pointer used for indexing is loaded
        into the base-register class {ac2, ac3} -- ac2 (LWLDA 2) whenever it
        is available, ac3 when ac2 is holding a live address."""
        s = self.L.static(ptr_name)
        key = ("base", ptr_name)
        for r in ("ac2", "ac3"):
            if self.regs.c[r] == ("addr", key):
                return r
        r = self.regs.pick_base()
        if self.regs.cost(r) >= COST["live"]:
            raise Refuse("R41′: both base registers hold live addresses at a "
                         "base load of %s -- not witnessed, no rule" % ptr_name)
        self.emit("%s = M32[%s]" % (r, hexc(s["addr"])), uses_fp=False)
        self.regs.set(r, ("addr", key))
        return r

    def link_reg(self):
        """R42/R44 (P39): the static link — the enclosing procedure's frame
        pointer, saved by WSAVS at wp(fp, -6).  It is loaded like any other
        BASE (R41′: the class {ac2, ac3}, ac2 preferred), and once loaded it is
        an ordinary cached address: a second uplevel reference in the same
        block reuses it (FIRE.2 7016A477 -> 7016A47F, where ac2 survives the
        DERR continuation).  R8 flushes it at a real block boundary, which is
        why the book re-loads it in every block that needs it.

        Evidence for the register class being the SAME class as an ordinary
        base, rather than a rule of its own: program-wide, the 288 base loads
        of the link go to ac2 (276) or ac3 (12) and NEVER to ac0/ac1, though
        those are free at cost 0 at many of the sites; the only ac0/ac1 loads
        of the link (21) are value loads immediately before a call, which is a
        different role.  See docs/Project39/REPORT.md §2.
        """
        key = ("link",)
        for r in ("ac2", "ac3"):
            if self.regs.c[r] == ("addr", key):
                return r
        r = self.regs.pick_base()
        if self.regs.cost(r) >= COST["live"]:
            raise Refuse("R41′: both base registers hold live addresses at a "
                         "static-link load — not witnessed, no rule")
        # the link lives at wp(fp, -6); resolve the frame's register EXPLICITLY
        # (uses_fp=False) because the destination may itself be ac3, and then a
        # blanket 'ac3' -> frame-register rewrite would corrupt the source.
        f = self.fpr()
        self.emit("%s = M32[wp(%s, -6)]" % (r, f), uses_fp=False)
        self.regs.set(r, ("addr", key))
        # R41' governs the static link as well as the indexing base
        self.fired("link_load", None, "static link", choices=[("reg", r)])
        return r

    def uplevel_slot(self, e):
        """`UP(PARENT, name)` -> (parent, name, slot, width).  The parent must
        be the one the routine is nested in, and the slot must be in
        declarations.json's frames table — which gen_declarations.py will only
        emit with a width and a named witness (the P39 user ruling)."""
        args = e.args.exprs if e.args else []
        if len(args) != 2 or not isinstance(args[0], c_ast.ID) or not isinstance(args[1], c_ast.ID):
            refuse(e, "UP(PARENT, name) takes two identifiers")
        pname, fname = args[0].name, args[1].name
        if self.parent is None:
            refuse(e, "UP() in %s, which the addrbook does not call a nested "
                      "procedure" % self.name)
        if pname != self.parent:
            refuse(e, "UP(%s, ...) in a procedure nested in %s — a procedure "
                      "reaches its OWN enclosing frame only (nesting is one "
                      "level deep program-wide, P39 §2)" % (pname, self.parent))
        pf = self.L.parent_frame(pname)
        if pf is None:
            refuse(e, "no frame layout for %s in declarations.json" % pname)
        f = pf["locals"].get(fname)
        if f is None:
            refuse(e, "%s has no slot %s in declarations.json — add it to "
                      "gen_declarations.py PARENT_FRAMES with its width and a "
                      "named witness" % (pname, fname))
        return pname, fname, f["slot"], f["width"]

    def uplevel_arg(self, e):
        """`UPARG(PARENT, k)` -> (parent, k).  The parent's own parameters are
        by-reference, so the value is `*UPARG(P, k)`: link, then the parent's
        argument slot at wfp-10-2k, then the datum (`XNLDA 1,@[ac2+0xFFF4]`)."""
        args = e.args.exprs if e.args else []
        if len(args) != 2 or not isinstance(args[0], c_ast.ID) \
                or not isinstance(args[1], c_ast.Constant):
            refuse(e, "UPARG(PARENT, k) takes an identifier and a constant")
        pname, k = args[0].name, int(args[1].value, 0)
        if self.parent is None or pname != self.parent:
            refuse(e, "UPARG(%s, ...) in %s" % (pname, self.name))
        pf = self.L.parent_frame(pname)
        if pf is None:
            refuse(e, "no frame layout for %s in declarations.json" % pname)
        if not 1 <= k <= pf["argc"]:
            refuse(e, "%s takes %d arguments; UPARG(%s, %d)" % (pname, pf["argc"], pname, k))
        return pname, k

    def uparg_width(self, e, pname, k):
        a = self.L.parent_frame(pname)["args"].get(str(k))
        if a is None:
            refuse(e, "the width of %s's argument %d is not witnessed — add it "
                      "to gen_declarations.py PARENT_FRAMES args with a named "
                      "witness" % (pname, k))
        return a["width"]

    @staticmethod
    def is_uplevel(e, which=("UP", "UPARG")):
        return isinstance(e, c_ast.FuncCall) and isinstance(e.name, c_ast.ID) \
            and e.name.name in which

    def field(self, e, want_reg, avoid):
        if e.type == "->" and isinstance(e.name, c_ast.ID):
            ptr = e.name.name
            f = self.L.direct_field(ptr, e.field.name)
            if f is None:
                refuse(e, "%s->%s not in declarations" % (ptr, e.field.name))
            K, width = f
            b = self.base_reg(ptr)
            # R41': the ADDRESSING role -- legal set {ac2, ac3}.  The ledger
            # rejects any other choice against the production's declared class.
            self.fired("field_direct", e, "StructRef %s->%s" % (ptr, e.field.name),
                       choices=[("reg", b)])
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
            self.fired("field_element", e, "StructRef %s.%s" % (tname, e.field.name),
                       consumes=(e.name,))
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
        if not self.subscript_node_ok(sub):
            refuse(sub, "subscript must be a variable or a dereferenced "
                        "parameter (optionally SUB(v, n))")
        vname = self.subscript_key(sub)
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
        # R36/D2: a HOISTED element-address temp -- the whole reference was
        # loop-invariant, so the temp already holds base + i*stride and the
        # body reloads it straight into ac2 with no base add (QUEST.1 7015C60E
        # `XWLDA 2,[ac3+0x6]` then `XNLDA 1,[ac2+0x7DB3]`).  It is reloaded on
        # every iteration and outlives every use, so it is not freed here.
        if ekey in self.cse and self.cse[ekey].get("hoisted") \
                and self.cse[ekey].get("slot") is not None:
            slot = self.cse[ekey]["slot"]
            self.emit("ac2 = M32[wp(ac3, %d)]" % slot)
            self.regs.set("ac2", ("addr", ekey))
            # PATH 1 (R36/D2, hoisted): the subscript is NEVER evaluated.
            self.fired("element_address", sub, "%s(%s) hoisted-temp" % (tname, vname),
                       consumes=(sub,), choices=[("reg", "ac2"), ("slot", slot)])
            self.note_ref(tname, vname)
            return "ac2"
        if ekey in self.cse and self.cse[ekey].get("slot") is not None:
            slot = self.cse[ekey]["slot"]
            if pos >= self.cse[ekey]["pos"] + 2:
                self.emit("ac2 = M32[wp(ac3, %d)]" % slot)
                self.regs.set("ac2", ("addr", ekey))
                self.frame.free_temp(slot)
                self._freed.add(slot)
                self.cse[ekey]["slot"] = None
                # PATH 2 (R10): element temp reloaded; subscript not evaluated.
                self.fired("element_address", sub, "%s(%s) elem-temp" % (tname, vname),
                           consumes=(sub,), choices=[("reg", "ac2"), ("slot", slot)])
                self.note_ref(tname, vname)
                return "ac2"
        if self.regs.c["ac2"] in (("addr", ekey), ("live", ekey)):
            # PATH 3: already in ac2 -- ZERO instructions, subscript unevaluated.
            self.fired("element_address", sub, "%s(%s) already-in-ac2" % (tname, vname),
                       consumes=(sub,), choices=[("reg", "ac2")])
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
            # PATH 4 (R31): base already in ac2, add the temp TO it -- an ORDER
            # choice, and the two orders are visible in the folded address.
            self.fired("element_address", sub, "%s(%s) R31 temp-to-base" % (tname, vname),
                       consumes=(sub,), choices=[("reg", "ac2"), ("slot", slot),
                                                 ("order", "temp_to_base")])
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
            # PATH 5: scaled subscript in a temp; subscript not re-evaluated.
            self.fired("element_address", sub, "%s(%s) scaled-temp" % (tname, vname),
                       consumes=(sub,), choices=[("reg", "ac2"), ("slot", slot),
                                                 ("order", "base_to_temp")])
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
        # PATHS 6/7: the first computation.  The subscript IS reduced here --
        # this production's span is PATH-DEPENDENT, which a single fixed span
        # declaration cannot express.  Recorded as a P42 finding.
        #
        # P42 FINDING (the span check caught this on its first run): this
        # production makes TWO register decisions, and only one is under
        # R41''s class.
        #   * `r` is the WORKING register -- the subscript value's register,
        #     an ordinary R7 pick over all four, in which the stride multiply
        #     and the base add proceed IN PLACE (R5/R6).  It is not a base
        #     register and R41' does not govern it; it is already recorded by
        #     whatever production produced the subscript value.
        #   * the INDEXING BASE is what this production yields, and it is
        #     always ac2 here -- the `WMOV r,2` below is R5 moving the
        #     finished address into the class.
        # Recording `r` conflated the two and tripped the class check.  The
        # class is NOT widened to admit it: {ac2, ac3} stands, and the second
        # member is simply unreached by these four routines (CODEGEN_RULES
        # SS9.3 says exactly that -- ac2 is free at every base load in all 349
        # statements).
        self.fired("element_address", None, "%s(%s) computed" % (tname, vname),
                   choices=[("reg", "ac2"), ("order", "in_place_then_move")])
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
        elif vname.startswith("UP$") or vname.startswith("UPARG$"):
            # P39: an UPLEVEL variable as the subscript.  The symbolic form is
            # the double indirection itself: the link at wp(fp, -6), then the
            # parent's slot -- and for UPARG one more level, through the
            # parent's argument slot.
            link = s_load(s_addv(Sym("fp"), s_const(-6)), 32)
            if vname.startswith("UP$"):
                f = self.L.parent_frame(self.parent)["locals"][vname[3:]]
                atom = s_load(s_addv(link, s_const(f["slot"])), f["width"])
            else:
                k = int(vname[6:], 0)
                w = self.L.parent_frame(self.parent)["args"][str(k)]["width"]
                atom = s_load(s_load(s_addv(link, s_const(-(10 + 2 * k))),
                                     32, ind=True), w)
        elif vname in self.args:
            # a BY-REFERENCE parameter as the subscript: the datum is at the
            # address in the argument slot, so the symbolic form is a load
            # THROUGH it -- `sx16(M16[R[ac3 + -12]])` (LIST_PLAYERS.3 7016F583).
            n_arg, width, _ = self.args[vname]
            atom = s_load(s_load(s_addv(Sym("fp"), s_const(-(10 + 2 * n_arg))),
                                 32, ind=True), width)
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
            # R20: the more complex operand went FIRST -- an order choice
            self.fired("binop_reg_reg", e, "BinaryOp %s (rhs first)" % e.op,
                       choices=[("reg", r), ("order", "rhs_first")])
            return Val("reg", reg=r, width=32)
        lhs = self.value(e.left, want_reg=True, avoid=avoid)
        r = lhs.reg
        self.regs.c[r] = ("live", "lhs")
        if isinstance(rhs, c_ast.Constant):
            k = int(rhs.value, 0)
            if e.op == "+" and k == 1:
                sp = PROD.TEMPLATES["binop_const_inc"](self, r)
                self.fired("binop_const_inc", e, "BinaryOp + 1",
                           consumes=(rhs,), choices=[("spelling", sp)])
                return Val("reg", reg=r, width=32)
            if e.op in ("+", "-"):
                kk = k if e.op == "+" else -k
                if -0x8000 <= kk <= 0x7FFF:
                    sp = PROD.TEMPLATES["binop_const_addi"](self, r, kk)
                    self.fired("binop_const_addi", e, "BinaryOp %s %s" % (e.op, hexc(k)),
                               consumes=(rhs,), choices=[("spelling", sp)])
                    return Val("reg", reg=r, width=32)
                refuse(e, "constant beyond 16 bits")
            kr = self.regs.pick(avoid=(r,))
            op = "mul" if e.op == "*" else "div"
            # PORTED. D4 census target: the constant's register is an R7 pick
            # with an exclusion -- synthesised. If D4 answers "down", a target.
            PROD.TEMPLATES["binop_const_scale"](self, r, kr, k, op)
            self.fired("binop_const_scale", e, "BinaryOp %s const" % e.op,
                       consumes=(rhs,), choices=[("reg", kr)])
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
                self.fired("binop_reg_mem", e, "BinaryOp %s mem32" % e.op,
                           consumes=(rhs,), choices=[("spelling", "memory_operand")])
                return Val("reg", reg=r, width=32)
            rv = self.value(rhs, want_reg=True, avoid=(r,))
        op = {"+": "add", "-": "sub", "*": "mul", "/": "div"}[e.op]
        self.emit("%s = %s(%s, %s)" % (r, op, r, rv.reg))
        self.regs.set(r, ("live", "res"))
        self.fired("binop_reg_reg", e, "BinaryOp %s (lhs first)" % e.op,
                   choices=[("reg", r), ("order", "lhs_first")])
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
        if "ac3" in ", ".join(texts):
            r = self.fpr()                       # R33: may append the LDAFP
            if r != "ac3":
                texts = [t.replace("ac3", r) for t in texts]
        # R18: TMP arguments left to right, each stored to its temp slot at
        # once UNLESS a slot collides, in which case ALL stores defer -- an
        # ORDER choice, and the slots are SLOT choices.
        self.fired("rt_call_stmt", call, "%s(%d args)" % (base, arity),
                   consumes=tuple(args),
                   choices=[("order", "deferred" if defer else "immediate")]
                           + [("slot", int(str(t).split(", ")[1].rstrip(")")))
                              for t in pushes
                              if isinstance(t, str) and t.startswith("wp(ac3, ")])
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
        self.fired("temp_place", None, "temp store slot %d" % slot,
                   choices=[("slot", slot)])
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
        # R24 + R33: ac3 points into the source, so the frame is no longer in a
        # register.  The next frame reference materialises it (emit -> fpr()).
        self.regs.set("ac3", ("addr", ("wcmv-src-end",)))

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
                self.frame.free_string(sub_slot, self.stmt_index)
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
                    slot, width = self.frame.locals[t.name]
                    if width == 8:
                        # a CHAR(1) local's address is a BYTE address: XPEFB,
                        # `bp(ac3, 2*slot)`.  HIT_ANY_CHAR 7016DEA7
                        # `M32[0x74003F1C] = bp(ac3, 4)` for the local at slot 2.
                        self.fired("address_of", a, "&local (byte)",
                                   consumes=(t,), choices=[("spelling", "bp")])
                        return "bp(ac3, %d)" % (2 * slot)
                    self.fired("address_of", a, "&local",
                               consumes=(t,), choices=[("spelling", "wp")])
                    return "wp(ac3, %d)" % slot
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
        self.fired("game_call_stmt", st, "call %s" % st.name.name,
                   consumes=tuple(st.args.exprs if st.args else ()),
                   choices=[("order", "right_to_left")])
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
    ap.add_argument("--ledger", help="P42: write the reduction ledger here "
                                     "(addresses in reduction order + the choices taken)")
    ap.add_argument("--productions", action="store_true",
                    help="P42 Stage 3: fire ported production templates instead of "
                         "the legacy emit path (no production is ported yet)")
    args = ap.parse_args()
    t0 = time.time()
    ast = parse_file(args.source, use_cpp=True,
                     cpp_args=["-I" + os.path.join(HERE, "fake_include"), "-I" + os.path.join(ROOT, "game"),
                               "-D__TRANSLATOR__", "-fdollars-in-identifiers"])
    fdef = None
    want = c_name(args.routine)
    for ext in ast.ext:
        if isinstance(ext, c_ast.FuncDef) and ext.decl.name in (args.routine, want):
            fdef = ext
    if fdef is None:
        raise SystemExit("routine %s (C name %s) not defined in %s"
                         % (args.routine, want, args.source))
    tr = Translator(Layout(args.layout), args.addrbook, args.routine)
    tr.collect_protos(ast)
    tr.mem = MemImage(args.mem) if args.mem and os.path.exists(args.mem) else None
    try:
        text = tr.translate(fdef)
    except Refuse as e:
        sys.stderr.write(str(e) + "\n")
        sys.exit(2)
    # P42: the span check has TEETH -- a consumed descendant that is
    # separately reduced, a choice of a kind the production does not declare,
    # or a register outside a production's declared class, is a HARD ERROR.
    # Without this the tile discipline collapses back into arbitrary methods.
    bad = tr.ledger.check()
    if bad:
        sys.stderr.write("P42 SPAN/CHOICE VIOLATIONS (%d):\n  %s\n"
                         % (len(bad), "\n  ".join(bad)))
        sys.exit(3)
    if args.out:
        open(args.out, "w").write(text)
    else:
        sys.stdout.write(text)
    if args.ledger:
        open(args.ledger, "w").write(tr.ledger.render())
    nch = sum(len(f.choices) for f in tr.ledger.firings)
    sys.stderr.write("translate %s: %d blocks, %d reductions, %d choices, %.2fs\n"
                     % (args.routine, text.count("\nblock "),
                        len(tr.ledger.firings), nch, time.time() - t0))


if __name__ == "__main__":
    main()
