#!/usr/bin/env python3
"""lower_c.py — the NAIVE C -> ir 8 compiler (Project 48; ir 8 in Project 53).

Choice-free, deliberately uniform, deliberately stupid.  It emits `v`
declarations and symbolic blocks and places nothing; the loader assigns
0x76/0x77 (IR.md §5.10).

The rules it is built to, from docs/Project48/{PROMPT,q001,a001}.md:

  * Every value lives in its own `v`.  No reuse, no hoisting, no register
    cost model, no lookahead, no peepholes.  Two occurrences of `i + 1` are
    two nodes and two `v`s.  Cleverness here is a defect: every load/store
    pair NOT emitted is a rewrite that cannot be credited later.
  * ZERO `t`-places (a001 R3).  A `t` falls out of lifting one machine
    instruction; this compiler lifts none.
  * PURE OPERATORS ONLY (a001 R2).  No add/sub/mul/div/cvwn/ash — the
    effectful family owns c/ovr, and the C source says nothing about carry.
    Converting `a + b` to `add(a, b)` where the flags are dead is a REWRITE
    with a flag-liveness precondition, not this compiler's business.
  * Statement shape: operand 1 -> ac0, operand 2 -> ac1, result in ac0,
    addresses in ac2 (DESIGN §7.1's house style).
  * Types compile away.  The type model is C's: every node carries a
    PROMOTED type, which is always 32-bit, and masks appear at exactly the
    three places C says a conversion happens (see promote()/narrow_store()).
  * REFUSE, never approximate.  Anything not understood is a loud refusal
    naming the construct and the source line, exit 2.

Output: OUT.ir (ir 7, mode stock, symbolic blocks only) and OUT.vmap, the
manifest the test rig and the differential driver read.
"""

import argparse
import json
import re
import os
import subprocess
import sys

from pycparser import c_ast, c_parser

HERE = os.path.dirname(os.path.abspath(__file__))
FAKE_INCLUDE = os.path.join(HERE, "fake_include")
GAME = os.path.join(os.path.dirname(HERE), "game")
DECLS_JSON = os.path.join(GAME, "declarations.json")


# ---------------------------------------------------------------- teeth ----
#
# A differential tester that has never gone red proves nothing (the P46
# -DP46_BROKEN_ALLOC precedent).  --mutate injects ONE deliberate, plausible
# soundness bug; run_lowerc_difftest.sh requires the tester to CATCH each.
# Every mutation below is a lowering a reasonable person could write.
MUTATIONS = {
    "u16_unsigned":   "uint16_t promotes to uint32_t instead of int32_t",
    "no_sign_extend": "read an int16_t with zx16 instead of sx16",
    "eager_bool":     "lower C's && / || to the IR's EAGER && / ||",
    "shift_logical":  "signed >> as a plain logical shift",
    "cmp_unsigned":   "ordering comparisons always take the u suffix",
    "abs_argtype":    "ABS returns the argument's type (the bug seed 2 found)",
    # P53, and both are the byte bug classes the prompt names.  The first is
    # P51 §2 item 5 itself: `char` as a one-word cell, which is what this
    # compiler did until stage C.  The second is IR.md §5.2's byte/word
    # asymmetry — bp()'s displacement is ALREADY IN BYTES, so scaling a byte
    # subscript by the element width is wrong by a factor of two AND
    # self-consistent between the store and the load, which is why
    # cases/byte_index.c reads every byte back at a different index.
    "byte_as_word":   "lower `char` to a one-word cell (P51 §2 item 5)",
    "bp_scaled":      "scale a byte subscript by the element width",
}
MUTATE = None


# ===================================== the runtime calling conventions ====
#
# Carried-in ruling 6: `TMP(e)`'s width comes from the CALLEE'S PARAMETER, not
# from the expression.  That is not derivable from the C — it is a lookup in
# docs/Project28/RTConventions.md — so it lives here as DATA, with the row that
# justifies each entry.  A call with no row REFUSES (ruling 7 applied to a
# table): the compiler must not guess a width.
#
#   argc    the arity the `$N` name asserts
#   valued  the result arrives in ac0 (RTConventions); the valued-call split of
#           IR.md §5.10.6 makes `r = f(...)` a call plus `r = ac0` in the next
#           block, so a call can never sit inside a larger expression
#   dummy   1-based argument index -> the vtype a TMP() at that position takes
RT_CALLS = {
    # ?RANDOM_NUMBER @7017DE33.  Value in ac0 (RTConventions row).  P51 §3: at
    # PICK_X_Y's three sites all lo/hi arguments are 32-bit temps (XWSTA),
    # including the constant 1 and OBJ_PTR->region_count — the parameters are
    # FIXED BIN(31).  The seed is passed directly by reference.
    "RANDOM_NUMBER$3": dict(callee="?RANDOM_NUMBER", argc=3, valued=True,
                            dummy={1: "i32", 2: "i32", 3: "i32"}),
    # ?WRITE_SCREEN @7017E27A: channel, text.  A CHAR-constant text argument is
    # a CHAR VARYING dummy built in the caller's frame, its WORD address pushed
    # (P51 §3, HIT_ANY_CHAR 7016DE93..DEA3).
    "WRITE_SCREEN$2": dict(callee="?WRITE_SCREEN", argc=2, valued=False,
                           dummy={}),
    "WRITE_SCREEN$5": dict(callee="?WRITE_SCREEN", argc=5, valued=False,
                           dummy={5: "i16"}),
    # ?READ @7017DE5F.  Argument roles from its BODY (RTConventions, P51 §3):
    # arg 2's datum is a POINTER (a dummy holding the buffer's byte pointer);
    # arg 3 is the byte count, 16-bit, IN/OUT — the callee writes it back at
    # 7017DEE4; arg 5 is the 16-bit options word.
    "READ$6": dict(callee="?READ", argc=6, valued=False,
                   dummy={2: "*char", 3: "i16", 5: "i16"}),
    "UNSIGNED_TO_CHAR$1": dict(callee="?UNSIGNED_TO_CHAR", argc=1, valued=False,
                               dummy={}),
}

# X.CB @7017E708 (Salvage F12, RTConventions): builds a BIT literal at run
# time.  ac2 = destination WORD address, ac0 = byte pointer to the character
# form, ac1 = its length, then an undecorated LCALL with NO stack arguments —
# which is why IR.md §6 widened the `rt_call` callee rule to accept a non-`?`
# callee with an EMPTY argument list rather than adding a second production.
X_CB = "X.CB"


class Refusal(Exception):
    def __init__(self, construct, line, why):
        self.construct, self.line, self.why = construct, line, why
        Exception.__init__(self, "%s: %s" % (construct, why))


def refuse(construct, node, why):
    line = getattr(getattr(node, "coord", None), "line", 0) or 0
    raise Refusal(construct, line, why)


# ============================================================ the types ====
#
# Four declared kinds; two sizes.  IR.md §5.1 puts width and sign in the
# OPERATOR, never in the storage, so these exist to drive mask generation and
# then are gone.

KINDS = {            # kind -> (C spelling, words, signed, bits)
    "u8":  ("unsigned char", 0, False, 8),   # see byte_words(): 0 is deliberate
    "i16": ("int16_t",  1, True,  16),
    "u16": ("uint16_t", 1, False, 16),
    "i32": ("int32_t",  2, True,  32),
    "u32": ("uint32_t", 2, False, 32),
}

# A BYTE IS HALF A WORD, and it is the only kind in this table that is.  Every
# other size is `words * nelem`; a byte array of n elements is the IR's
# `char <n>` and occupies ceil(n/2) words (IR.md §5.10.1).  KINDS["u8"][1] is 0
# precisely so that a forgotten `words * nelem` yields 0 and refuses loudly
# instead of silently sizing a 144-byte buffer as 0 or 144 words — P51 §2 item
# 5 is exactly the second of those, and it is the bug this project exists to
# not re-introduce.
def byte_words(nbytes):
    return (nbytes + 1) // 2
CNAME_TO_KIND = {
    "int16_t": "i16", "short": "i16",
    "uint16_t": "u16", "unsigned short": "u16",
    "int32_t": "i32", "int": "i32", "signed int": "i32", "long": "i32",
    "uint32_t": "u32", "unsigned int": "u32", "unsigned": "u32",
    "unsigned long": "u32",
    # Salvage F13: PL/I CHARACTER is an UNSIGNED byte.  Until P53 these mapped
    # to u16, a one-word cell — so GET_INPUT's `unsigned char buf[144]` would
    # have been 144 WORDS and every access a word access that agrees with gcc
    # on small values.  `signed char` is deliberately absent: no routine of the
    # seven has one, and refusing is better than guessing its extension.
    "char": "u8",
    "unsigned char": "u8",
}


def promote(kind):
    """C's integer promotions (C99 6.3.1.1).  int16_t, uint16_t AND char all
    promote to int32_t, because `int` represents every one of their values —
    so `uint16_t / uint16_t` is a SIGNED divide.  This is the single most
    counter-intuitive rule in the model and the one the hand suite's
    u16_promote case exists to hold us to."""
    if MUTATE == "u16_unsigned" and kind == "u16":
        return "u32"
    if MUTATE == "byte_as_word" and kind == "u8":
        return "u32"
    return "u32" if kind == "u32" else "i32"


def usual(a, b):
    """C's usual arithmetic conversions (C99 6.3.1.8), over promoted types."""
    return "u32" if (a == "u32" or b == "u32") else "i32"


def is_signed(kind):
    return KINDS[kind][2]


# ======================================================= the IR emitter ====

class Block:
    def __init__(self, name, anchor):
        self.name, self.anchor = name, anchor
        self.lines = []
        self.term = None


class VRow:
    def __init__(self, vname, kind_of_v, cname, ctype, vtype, words,
                 nelem, elemwords, render, anchor, line):
        self.f = [vname, kind_of_v, cname, ctype, vtype, str(words),
                  str(nelem), str(elemwords), render, anchor, str(line)]

    def tsv(self):
        return "\t".join(self.f)


class Lowerer:
    def __init__(self, entry, src, routine):
        self.entry = entry
        self.src = os.path.basename(src)
        self.routine = routine
        self.vrows = []
        self.vdecls = []            # emitted `v` / `a` lines, in allocation order
        self.vtype = {}             # cell name -> its declared vtype (the tripwire).
                                    # SHARED across a unit (set_unit_vtype): a
                                    # caller writes its CALLEE's `a` cells, so
                                    # the kind check needs the callee's types.
        self.game_routines = {}     # callable game routines IN THIS UNIT
        self.nv = 0
        self.blocks = []
        self.nb = 0
        self.cur = None
        self.scope = {}             # cname -> (vname, kind, nelem, elemwords)
        self.params = {}            # cname -> (vname, pointee_kind)
        self.labels = {}            # C label -> Block
        self.loops = []             # (break_block, continue_block)
        self.statics = {}
        self.tables = {}
        self.direct = {}
        self.load_declarations()

    # ------------------------------------------------ game declarations --
    def load_declarations(self):
        """R1a: the statics and record tables of game/declarations.json,
        accessed by the declarations.h spellings.  The compiler READS the
        geometry; it never invents one."""
        try:
            d = json.load(open(DECLS_JSON))
        except Exception:
            return
        self.statics = d.get("statics", {})
        self.tables = d.get("tables", {})
        self.direct = d.get("direct", {})

    # ------------------------------------------------------------ names --
    def new_v(self, kind_of_v, kind, cname="", nelem=0, anchor="", line=0):
        vname = "%s.v%d" % (self.entry, self.nv)
        self.nv += 1
        return self.declare_cell("v", vname, kind_of_v, kind, cname, nelem,
                                 anchor, line)

    def new_a(self, local, kind_of_v, kind, cname="", nelem=0, anchor="", line=0,
              ptr_to=None):
        """An ARGUMENT CELL (IR.md §5.10.1c).  Same type system, same
        placement, same cursor as a `v` — what it adds is that the routine's
        SIGNATURE is declared, so a caller's arity and argument pointer KINDS
        are checked against the callee rather than guessed."""
        return self.declare_cell("a", "%s.%s" % (self.entry, local), kind_of_v,
                                 kind, cname, nelem, anchor, line, ptr_to)

    def declare_cell(self, lead, name, kind_of_v, kind, cname, nelem,
                     anchor, line, ptr_to=None):
        if MUTATE == "byte_as_word" and kind == "u8":
            kind = "u16"                      # the pre-P53 lowering, injected
        ctype, words, _s, _b = KINDS[kind]
        elemwords = words
        if ptr_to is not None:
            # A by-reference parameter: two words, one level, KIND enforced
            # (§5.10.1a).  The pointee width is advisory and carried for ircmp.
            # The IR spells a BYTE pointer `*char`, not `*u8` — the pointee
            # forms of §5.10.1a are i16|u16|i32|u32|char|varying n|words n, and
            # `char` is the byte one.  KIND is enforced; this is that kind.
            vtype = "*char" if ptr_to == "u8" else "*" + ptr_to
            total, elemwords, render = 2, 2, "w32"
        elif kind == "u8" and MUTATE != "byte_as_word":
            # A byte cell is the IR's `char <n>` — an AGGREGATE, n BYTES,
            # ceil(n/2) words.  A scalar is `char 1`: §5.10.4 has no one-byte
            # value vtype, so even a lone byte names a region and is reached
            # through bp().  That costs one statement per access, which is
            # what naive costs.
            nbytes = nelem if nelem else 1
            vtype = "char %d" % nbytes
            total = byte_words(nbytes)
            elemwords = 0        # a byte is half a word: nothing may multiply
            render = "w8"
        elif nelem:
            vtype = "words %d" % (words * nelem)
            total = words * nelem
            render = "w16" if words == 1 else "w32"
        else:
            # THE MUTATION MOVED HERE (a001 asked why, so it is written down):
            # in ir 7 `no_sign_extend` corrupted the READ (`zx16` for `sx16`).
            # In ir 8 there is no read to corrupt — §5.10.4 takes width and
            # sign from the declaration — so the same soundness bug is now
            # spelled as declaring `u16` where `i16` belongs.  Same bug, one
            # level down, and the corpus must still catch it.
            vtype = kind
            if MUTATE == "no_sign_extend" and kind == "i16":
                vtype = "u16"
            total = words
            render = "w16" if words == 1 else "w32"
        self.vtype[name] = vtype
        self.vdecls.append("%s %-24s %s" % (lead, name, vtype))
        self.vrows.append(VRow(name, kind_of_v, cname, ctype, vtype, total,
                               nelem, elemwords, render, anchor, line))
        return name

    def new_block(self, anchor):
        b = Block("%s.b%d" % (self.entry, self.nb), anchor)
        self.nb += 1
        self.blocks.append(b)
        return b

    # --------------------------------------------------------- tripwire --
    #
    # ir 8 REVERSED what a `v` name means (IR.md §5.10.9): in ir 7 it was the
    # cell's ADDRESS, so `M32[V]` was how you read it; in ir 8 it is the cell's
    # CONTENTS and `M32[V]` means "read through V as a POINTER".  72 emit sites
    # in this file spelled the ir 7 form.
    #
    # The danger in a migration that size is NOT the site you convert wrongly —
    # that fails loudly.  It is the site you FORGET, because `M32[V]` is still
    # grammatical and, for a `v` that happens to be declared `u32`, still
    # LOADS: it just reads through a number as if it were an address.  So this
    # mirrors the loader's KIND tripwire (§5.10.5) at EMIT time, where the
    # message can name the Python call site instead of an IR line number.
    #
    # It stays after the migration.  It costs one regex per emitted line and it
    # is the only thing standing between "the compiler emits a pointer
    # dereference of an integer" and a debugging session three projects later.
    MEMFORM = re.compile(r"\bM(8|16|32)\[\s*([A-Za-z_$][\w.$]*)\s*\]")
    POINTER_VTYPE = re.compile(r"^\*")

    def tripwire(self, text):
        for m in self.MEMFORM.finditer(text):
            width, index = m.group(1), m.group(2)
            if index not in self.vtype:
                continue                      # a register or a t-place: fine
            vt = self.vtype[index]
            if not self.POINTER_VTYPE.match(vt):
                raise Refusal(
                    "ir8-tripwire", 0,
                    "emitted `M%s[%s]` but %s is declared `%s`, not a pointer — "
                    "in ir 8 a cell name is its CONTENTS (IR.md §5.10.4), so "
                    "this is an unconverted ir 7 address form, not a "
                    "dereference" % (width, index, index, vt))
            if width == "8" and vt != "*char":
                raise Refusal("ir8-tripwire", 0,
                              "M8 through the word pointer %s (%s)" % (index, vt))
            if width in ("16", "32") and vt == "*char":
                raise Refusal("ir8-tripwire", 0,
                              "M%s through the byte pointer %s" % (width, index))
        # q005 / a004: THE ARGUMENT POINTER-KIND CHECK.  IR.md §5.1 and §6 both
        # say a call whose argument pointer KINDS disagree with the callee's
        # `a` cells refuses at load, and IRExec.cpp:887/:1349 say so too — but
        # the loader does not implement it (measured: a `wp()` into a `*char`
        # cell loads clean).  The integrator has scheduled the loader fix; this
        # is the compiler not depending on a downstream checker, which is the
        # same reason the rest of this tripwire exists.
        #
        # `wp()` yields a WORD pointer, `bp()` a BYTE pointer — statically, by
        # which builder was used (§5.2's kind overload).  This is the exact
        # spelling HIT_ANY_CHAR gets wrong if anything does: it hands GET_INPUT
        # a `*char` built with bp() while every other argument in the seven is
        # a word pointer built with wp(), one letter apart.
        m = re.match(r"\s*([A-Za-z_$][\w.$]*)\s*=\s*(wp|bp)\(", text)
        if m:
            cell, builder = m.group(1), m.group(2)
            vt = self.vtype.get(cell)
            if vt and vt.startswith("*"):
                want = "bp" if vt == "*char" else "wp"
                if builder != want:
                    raise Refusal(
                        "ir8-tripwire", 0,
                        "assigning a %s pointer (%s(...)) to `%s`, declared "
                        "`%s` — argument pointer KIND must match the callee's "
                        "`a` cell (IR.md §6)"
                        % ("BYTE" if builder == "bp" else "WORD",
                           builder, cell, vt))

        # A bare AGGREGATE cell as a value or an lvalue names a region, not a
        # value, and REFUSES at load (§5.10.4).  Catch it here too.
        for name, vt in self.vtype.items():
            if vt.split()[0] in ("char", "varying", "words"):
                if re.search(r"(?<![\w.$])%s\s*=" % re.escape(name), text) or \
                   re.search(r"=\s*%s\s*$" % re.escape(name), text):
                    raise Refusal("ir8-tripwire", 0,
                                  "bare aggregate `%s` (%s) used as a value or "
                                  "an lvalue" % (name, vt))

    # ------------------------------------------------------- statements --
    def emit(self, text, comment=""):
        if self.cur is None:
            # Unreachable code after a terminator.  It still has to live in a
            # well-formed block (every block needs a terminator, IR.md §4),
            # so it gets its own, which nothing jumps to.
            self.cur = self.new_block("unreachable")
        self.tripwire(text)
        self.cur.lines.append("  %s%s" % (text, (" ; " + comment) if comment else ""))

    def terminate(self, text, comment=""):
        if self.cur is None:
            self.cur = self.new_block("unreachable")
        self.tripwire(text)
        self.cur.term = "  %s%s" % (text, (" ; " + comment) if comment else "")
        self.cur = None

    def goto(self, block, comment=""):
        self.terminate("goto [%s] 0" % block.name, comment)

    def open_block(self, b):
        self.cur = b

    def branch(self, cond_v, false_b, true_b, comment=""):
        """Always `tf(...)`, even when the value is already 0/1: a `goto`
        index outside [0, count) is a loud executor fault (IR.md §5.1) and I
        would rather never be able to produce one."""
        self.emit("ac0 = %s" % cond_v)
        self.terminate("goto [%s, %s] tf(ac0)" % (false_b.name, true_b.name),
                       comment)

    # --------------------------------------------------------- accessors --
    def load_reg(self, reg, vname, kind):
        """ir 8: a cell name IS its contents, read at the DECLARED width and
        signedness (IR.md §5.10.4).  There is no extension to spell here —
        `v X.v3 i16` is what makes this read sign-extend.  That is why the
        `no_sign_extend` mutation moved into new_v(): after ir 8 there is no
        read site left to corrupt, only a declaration."""
        if self.is_byte(vname):
            self.emit("%s = zx8(M8[bp(%s, 0)])" % (reg, vname))
        else:
            self.emit("%s = %s" % (reg, vname))

    def store_reg(self, reg, vname, kind):
        """Likewise: the declaration owns the truncation, not a trunc16()."""
        if self.is_byte(vname):
            self.emit("M8[bp(%s, 0)] = trunc8(%s)" % (vname, reg))
        else:
            self.emit("%s = %s" % (vname, reg))

    def is_byte(self, vname):
        """A `char <n>` cell is an AGGREGATE: it names a region, not a value,
        so the variable form of §5.10.4 does not apply and every access goes
        through bp()."""
        return self.vtype.get(vname, "").startswith("char ")

    def load_through_ac2(self, reg, kind):
        if kind == "u8":
            self.emit("%s = zx8(M8[ac2])" % reg)
            return
        if KINDS[kind][1] == 1:
            ext = "sx16" if is_signed(kind) else "zx16"
            if MUTATE == "no_sign_extend":
                ext = "zx16"
            self.emit("%s = %s(M16[ac2])" % (reg, ext))
        else:
            self.emit("%s = M32[ac2]" % reg)

    def store_through_ac2(self, reg, kind):
        if kind == "u8":
            self.emit("M8[ac2] = trunc8(%s)" % reg)
            return
        if KINDS[kind][1] == 1:
            self.emit("M16[ac2] = trunc16(%s)" % reg)
        else:
            self.emit("M32[ac2] = %s" % reg)

    # ------------------------------------------------------- constants ---
    @staticmethod
    def const_text(value):
        v = value & 0xFFFFFFFF
        if 0x76000000 <= v < 0x78000000:
            # IR.md §5.1: a literal in the synthetic spaces is REFUSED
            # anywhere — only a name may denote one.  Split it so the text
            # contains no such literal; the value is identical.
            return "(0x40000000 + 0x%08X)" % (v - 0x40000000)
        if v & 0x80000000:
            return "0x%08X" % v
        return str(v)

    def const_v(self, value, kind, anchor, line):
        v = self.new_v("node", kind, "", 0, anchor, line)
        self.emit("ac0 = %s" % self.const_text(value))
        self.emit("%s = ac0" % v)
        return v, kind


# ================================================== the expression walk ====

C_ESCAPES = {"n": 10, "t": 9, "r": 13, "v": 11, "b": 8, "f": 12, "a": 7,
             "0": 0, "\\": 92, "'": 39, '"': 34, "?": 63}


def decode_c_string(tok, node):
    """The BYTES of a C string literal, as pycparser hands it over (quoted,
    escapes unexpanded).  No terminating NUL: PL/I CHAR(n) is a counted region,
    not a C string, and the length is the declared capacity."""
    t = tok
    if t.startswith('"') and t.endswith('"'):
        t = t[1:-1]
    out, i = [], 0
    while i < len(t):
        ch = t[i]
        if ch != "\\":
            out.append(ord(ch)); i += 1; continue
        i += 1
        if i >= len(t):
            refuse("string constant", node, "trailing backslash")
        e = t[i]
        if e == "x":
            j = i + 1
            while j < len(t) and t[j] in "0123456789abcdefABCDEF":
                j += 1
            out.append(int(t[i + 1:j], 16) & 0xFF); i = j; continue
        if e in C_ESCAPES:
            out.append(C_ESCAPES[e]); i += 1; continue
        refuse("string constant", node, "unsupported escape \\%s" % e)
    for b in out:
        if b > 0xFF:
            refuse("string constant", node, "byte out of range")
    return bytes(out)


def ir_escape(data):
    """IR.md 5.8's literal escaping: printable 0x20..0x7E stay literal except
    the quote, the backslash and the semicolon; everything else becomes a hex
    escape."""
    out = []
    for b in data:
        if 0x20 <= b <= 0x7E and b not in (0x22, 0x5C, 0x3B):
            out.append(chr(b))
        else:
            out.append("\\x%02X" % b)
    return "".join(out)


def parse_int_literal(tok, node):
    """A C integer constant, with its TYPE, in our 32-bit model."""
    t = tok.strip()
    suffix = ""
    while t and t[-1] in "uUlL":
        suffix += t[-1].lower()
        t = t[:-1]
    unsigned = "u" in suffix
    if t.lower().startswith("0x"):
        val, base_hex = int(t, 16), True
    elif t.startswith("0") and len(t) > 1 and t[1:].isdigit():
        val, base_hex = int(t, 8), True
    else:
        val, base_hex = int(t, 10), False
    if unsigned:
        if val > 0xFFFFFFFF:
            refuse("integer constant", node, "%s does not fit 32 bits" % tok)
        return val & 0xFFFFFFFF, "u32"
    if val <= 0x7FFFFFFF:
        return val, "i32"
    if base_hex and val <= 0xFFFFFFFF:
        return val, "u32"          # hex that does not fit int becomes unsigned
    refuse("integer constant", node,
           "%s is wider than int in this model (C would make it long)" % tok)


class Walker:
    """Lowers expressions.  Every method returns (vname, promoted_kind); the
    invariant is that the 32-bit pattern in that `v` IS the C value of the
    node under its promoted type, exactly (q001 §1.3)."""

    def __init__(self, L):
        self.L = L

    def anchor(self, node):
        return "%s:%d" % (self.L.src, getattr(getattr(node, "coord", None), "line", 0) or 0)

    def line(self, node):
        return getattr(getattr(node, "coord", None), "line", 0) or 0

    # --------------------------------------------------------- dispatch --
    def expr(self, n):
        L = self.L
        if isinstance(n, c_ast.Constant):
            if any(bad in n.type for bad in ("float", "double", "char", "string")):
                refuse("constant", n,
                       "only integer constants are in the subset (got %s)" % n.type)
            val, kind = parse_int_literal(n.value, n)
            return L.const_v(val, kind, "const", self.line(n))
        if isinstance(n, c_ast.ID):
            return self.read_id(n)
        if isinstance(n, c_ast.ArrayRef):
            return self.read_lvalue(n)
        if isinstance(n, c_ast.StructRef):
            return self.read_lvalue(n)
        if isinstance(n, c_ast.UnaryOp):
            return self.unary(n)
        if isinstance(n, c_ast.BinaryOp):
            return self.binary(n)
        if isinstance(n, c_ast.Cast):
            return self.cast(n)
        if isinstance(n, c_ast.FuncCall):
            return self.call(n)
        refuse(type(n).__name__, n, "construct not in the v1 C subset")

    # ------------------------------------------------------------ reads --
    def read_id(self, n):
        L = self.L
        if n.name in L.scope:
            vname, kind, nelem, _ew = L.scope[n.name]
            if nelem:
                refuse("array value", n, "an array may only be subscripted")
            pk = promote(kind)
            out = L.new_v("node", pk, "", 0, "read:" + n.name, self.line(n))
            L.load_reg("ac0", vname, kind)
            L.emit("%s = ac0" % out)
            return out, pk
        refuse("identifier", n, "undeclared name `%s`" % n.name)

    def address_of(self, n):
        """The word address of an lvalue, materialised into its own `v`.
        Returns (addr_v, element_kind)."""
        L = self.L
        if isinstance(n, c_ast.ArrayRef):
            return self.address_of_arrayref(n)
        if isinstance(n, c_ast.StructRef):
            return self.address_of_structref(n)
        if isinstance(n, c_ast.UnaryOp) and n.op == "*":
            if not (isinstance(n.expr, c_ast.ID) and n.expr.name in L.params):
                refuse("dereference", n,
                       "only a by-reference PARAMETER may be dereferenced")
            vname, pointee = L.params[n.expr.name]
            out = L.new_v("node", "u32", "", 0, "addr:*" + n.expr.name, self.line(n))
            L.emit("ac0 = %s" % vname)
            L.emit("%s = ac0" % out)
            return out, pointee
        refuse("lvalue", n, "not an addressable form in the subset")

    def address_of_arrayref(self, n):
        L = self.L
        # a declared local array: base v + index * element words
        if isinstance(n.name, c_ast.ID) and n.name.name in L.scope:
            vname, kind, nelem, ew = L.scope[n.name.name]
            if not nelem:
                refuse("subscript", n, "`%s` is not an array" % n.name.name)
            iv, _ik = self.expr(n.subscript)
            out = L.new_v("node", "u32", "", 0, "addr:" + n.name.name, self.line(n))
            L.emit("ac1 = %s" % iv)
            byte = L.is_byte(vname)
            if byte:
                # bp(base, d) scales the BASE to bytes and takes d ALREADY IN
                # BYTES (IR.md §5.2's recorded asymmetry, the hardware's).  So
                # a byte subscript is the displacement unscaled; multiplying it
                # by an element width would be wrong by two AND self-consistent
                # between the store and the load.  That is `bp_scaled`.
                if MUTATE == "bp_scaled":
                    L.emit("ac1 = ac1 * 2")
                L.emit("ac2 = bp(%s, ac1)" % vname)
            else:
                if ew != 1:
                    L.emit("ac1 = ac1 * %d" % ew)
                # ir 8: the ADDRESS of a cell is wp(cell, d) (§5.2, the kind
                # overload).  The element scaling stays its own statement —
                # folding it into the displacement would be a rewrite.
                L.emit("ac2 = wp(%s, ac1)" % vname)
            L.emit("%s = ac2" % out)
            return out, kind
        # a record table element, or a further subscript of a record field
        return self.address_of_table(n)

    def table_of(self, node):
        """Unwrap `TABLE[i]` / `TABLE[i].field` / `TABLE[i].field[a][b]`."""
        subs = []
        cur = node
        while isinstance(cur, c_ast.ArrayRef):
            subs.append(cur.subscript)
            cur = cur.name
        subs.reverse()
        return cur, subs

    def address_of_structref(self, n):
        L = self.L
        if n.type != "->" and n.type != ".":
            refuse("field access", n, "unknown field operator")
        field = n.field.name
        base = n.name
        # SD_PTR->field  (a static pointer plus a direct field displacement)
        if isinstance(base, c_ast.ID) and base.name in L.statics:
            sname = base.name
            info = L.direct.get(sname, {}).get(field)
            if info is None:
                refuse("field", n, "%s has no field %s in declarations.json"
                       % (sname, field))
            return self.static_field_addr(sname, info["K"], info["width"], n)
        # TABLE[i].field
        head, subs = self.table_of(base)
        if isinstance(head, c_ast.ID) and head.name in L.tables:
            return self.table_field_addr(head.name, subs, field, [], n)
        refuse("field access", n, "base is not a declared static or table")

    def address_of_table(self, n):
        """`TABLE[i].field[a][b]` — the ArrayRef chain OUTSIDE the field."""
        L = self.L
        outer = []
        cur = n
        while isinstance(cur, c_ast.ArrayRef):
            outer.append(cur.subscript)
            cur = cur.name
        outer.reverse()
        if not isinstance(cur, c_ast.StructRef):
            refuse("subscript", n, "not a declared array or record field")
        head, subs = self.table_of(cur.name)
        if not (isinstance(head, c_ast.ID) and head.name in L.tables):
            refuse("subscript", n, "base is not a declared record table")
        return self.table_field_addr(head.name, subs, cur.field.name, outer, n)

    def static_addr_v(self, sname, node):
        L = self.L
        addr = L.statics[sname]["addr"]
        out = L.new_v("node", "u32", "", 0, "base:" + sname, self.line(node))
        L.emit("ac2 = %s" % L.const_text(addr), "the static's own address")
        L.emit("ac0 = M32[ac2]", sname)
        L.emit("%s = ac0" % out)
        return out

    def static_field_addr(self, sname, K, width, node):
        L = self.L
        basev = self.static_addr_v(sname, node)
        out = L.new_v("node", "u32", "", 0, "addr:%s.%d" % (sname, K), self.line(node))
        L.emit("ac0 = %s" % basev)
        L.emit("ac2 = ac0 + %s" % L.const_text(K))
        L.emit("%s = ac2" % out)
        return out, ("i32" if width == 32 else "i16")

    def table_field_addr(self, tname, subs, field, inner, node):
        """addr = M32[base_static] + i*stride + sum(idx_k * stride_k) + K.
        K already carries the 1-based origin (declarations.json); nothing is
        folded or simplified here — each term is its own `v`."""
        L = self.L
        t = L.tables[tname]
        f = t["fields"].get(field)
        if f is None:
            refuse("field", node, "%s has no field %s in declarations.json"
                   % (tname, field))
        if len(subs) != 1:
            refuse("subscript", node, "a record table takes exactly one subscript")
        dims = f.get("dims", [])
        if len(inner) != len(dims):
            refuse("subscript", node,
                   "%s.%s has %d dimension(s), %d given"
                   % (tname, field, len(dims), len(inner)))
        basev = self.static_addr_v(t["base"], node)
        iv, _ = self.expr(subs[0])
        acc = L.new_v("node", "u32", "", 0, "addr:%s[]" % tname, self.line(node))
        L.emit("ac0 = %s" % basev)
        L.emit("ac1 = %s" % iv)
        L.emit("ac1 = ac1 * %d" % t["stride"])
        L.emit("ac0 = ac0 + ac1")
        L.emit("ac0 = ac0 + %s" % L.const_text(f["K"]))
        L.emit("%s = ac0" % acc)
        for sub, stride in zip(inner, f.get("strides", [])):
            sv, _ = self.expr(sub)
            nxt = L.new_v("node", "u32", "", 0, "addr:%s.%s[]" % (tname, field),
                          self.line(node))
            L.emit("ac0 = %s" % acc)
            L.emit("ac1 = %s" % sv)
            L.emit("ac1 = ac1 * %d" % stride)
            L.emit("ac0 = ac0 + ac1")
            L.emit("%s = ac0" % nxt)
            acc = nxt
        return acc, ("i32" if f["width"] == 32 else "i16")

    def read_lvalue(self, n):
        L = self.L
        addr_v, kind = self.address_of(n)
        pk = promote(kind)
        out = L.new_v("node", pk, "", 0, "load", self.line(n))
        L.emit("ac2 = %s" % addr_v)
        L.load_through_ac2("ac0", kind)
        L.emit("%s = ac0" % out)
        return out, pk

    # ----------------------------------------------------------- unary ---
    def unary(self, n):
        L = self.L
        if n.op == "*":
            return self.read_lvalue(n)
        if n.op in ("p++", "p--", "++", "--"):
            refuse("increment", n, "++/-- is a STATEMENT in this subset, not an expression")
        v, k = self.expr(n.expr)
        out_kind = k
        out = L.new_v("node", out_kind, "", 0, "unary" + n.op, self.line(n))
        L.emit("ac0 = %s" % v)
        if n.op == "-":
            L.emit("ac0 = 0 - ac0")
        elif n.op == "~":
            L.emit("ac0 = ~ac0")
        elif n.op == "+":
            pass
        elif n.op == "!":
            L.emit("ac0 = (ac0 == 0)")
            out_kind = "i32"
        else:
            refuse("unary " + n.op, n, "operator not in the subset")
        L.emit("%s = ac0" % out)
        return out, out_kind

    # ---------------------------------------------------------- binary ---
    ARITH = {"+": "+", "-": "-", "*": "*", "&": "&", "|": "|", "^": "^"}
    CMP = {"<": "<", "<=": "<=", ">": ">", ">=": ">="}

    def binary(self, n):
        L = self.L
        if n.op in ("&&", "||"):
            return self.shortcircuit(n)
        if n.op in ("<<", ">>"):
            return self.shift(n)
        lv, lk = self.expr(n.left)
        rv, rk = self.expr(n.right)
        if n.op in self.ARITH:
            k = usual(lk, rk)
            out = L.new_v("node", k, "", 0, "op" + n.op, self.line(n))
            L.emit("ac0 = %s" % lv)
            L.emit("ac1 = %s" % rv)
            L.emit("ac0 = ac0 %s ac1" % self.ARITH[n.op])
            L.emit("%s = ac0" % out)
            return out, k
        if n.op in ("/", "%"):
            k = usual(lk, rk)
            op = ("/" if n.op == "/" else "%") + ("s" if is_signed(k) else "u")
            out = L.new_v("node", k, "", 0, "op" + n.op, self.line(n))
            L.emit("ac0 = %s" % lv)
            L.emit("ac1 = %s" % rv)
            L.emit("ac0 = ac0 %s ac1" % op)
            L.emit("%s = ac0" % out)
            return out, k
        if n.op in ("==", "!="):
            out = L.new_v("node", "i32", "", 0, "cmp" + n.op, self.line(n))
            L.emit("ac0 = %s" % lv)
            L.emit("ac1 = %s" % rv)
            L.emit("ac0 = (ac0 %s ac1)" % n.op)
            L.emit("%s = ac0" % out)
            return out, "i32"
        if n.op in self.CMP:
            k = usual(lk, rk)
            op = self.CMP[n.op] + ("s" if (is_signed(k) and MUTATE != "cmp_unsigned") else "u")
            out = L.new_v("node", "i32", "", 0, "cmp" + n.op, self.line(n))
            L.emit("ac0 = %s" % lv)
            L.emit("ac1 = %s" % rv)
            L.emit("ac0 = (ac0 %s ac1)" % op)
            L.emit("%s = ac0" % out)
            return out, "i32"
        refuse("binary " + n.op, n, "operator not in the subset")

    def shift(self, n):
        """The result type is the PROMOTED LEFT operand's; the right operand
        promotes independently (C99 6.5.7).  `<<` is one `lsh` in both
        signedness; signed `>>` has no pure primary in ir 7 (IR.md §8 parks
        it, and `ash` writes ovr), so it is the six-statement decomposition
        of q001 §1.4a."""
        L = self.L
        lv, lk = self.expr(n.left)
        rv, _rk = self.expr(n.right)
        out = L.new_v("node", lk, "", 0, "shift" + n.op, self.line(n))
        if n.op == "<<":
            L.emit("ac0 = %s" % lv)
            L.emit("ac1 = %s" % rv)
            L.emit("ac0 = lsh(ac0, ac1)")
            L.emit("%s = ac0" % out)
            return out, lk
        if not is_signed(lk) or MUTATE == "shift_logical":
            L.emit("ac0 = %s" % lv)
            L.emit("ac1 = %s" % rv)
            L.emit("ac1 = 0 - ac1")
            L.emit("ac0 = lsh(ac0, ac1)")
            L.emit("%s = ac0" % out)
            return out, lk
        logical = L.new_v("node", "u32", "", 0, "shift>>s.logical", self.line(n))
        L.emit("ac0 = %s" % lv)
        L.emit("ac1 = %s" % rv)
        L.emit("ac1 = 0 - ac1")
        L.emit("ac0 = lsh(ac0, ac1)")
        L.emit("%s = ac0" % logical)
        signmask = L.new_v("node", "u32", "", 0, "shift>>s.signmask", self.line(n))
        L.emit("ac0 = %s" % lv)
        L.emit("ac0 = lsh(ac0, -31)")
        L.emit("ac0 = ac0 & 1")
        L.emit("ac0 = 0 - ac0")
        L.emit("%s = ac0" % signmask)
        fill = L.new_v("node", "u32", "", 0, "shift>>s.fill", self.line(n))
        L.emit("ac1 = %s" % rv)
        L.emit("ac1 = 0 - ac1")
        L.emit("ac0 = lsh(0xFFFFFFFF, ac1)")
        L.emit("ac0 = ~ac0")
        L.emit("%s = ac0" % fill)
        L.emit("ac0 = %s" % signmask)
        L.emit("ac1 = %s" % fill)
        L.emit("ac0 = ac0 & ac1")
        L.emit("ac1 = %s" % logical)
        L.emit("ac0 = ac0 | ac1")
        L.emit("%s = ac0" % out)
        return out, lk

    def shortcircuit(self, n):
        """C's && and || are SHORT-CIRCUIT; the IR's are EAGER (IR.md §5.3),
        so these lower to control flow.  With RANGE_CHECK()'s assert in the language
        the right operand really can trap, which makes this a semantic
        difference and not an optimisation."""
        L = self.L
        out = L.new_v("node", "i32", "", 0, "bool" + n.op, self.line(n))
        if MUTATE == "eager_bool":
            lv, _lk = self.expr(n.left)
            rv, _rk = self.expr(n.right)
            L.emit("ac0 = %s" % lv)
            L.emit("ac0 = tf(ac0)")
            L.emit("ac1 = %s" % rv)
            L.emit("ac1 = tf(ac1)")
            L.emit("ac0 = (ac0 %s ac1)" % n.op)
            L.emit("%s = ac0" % out)
            return out, "i32"
        lv, _lk = self.expr(n.left)
        b_rhs = L.new_block("%s/rhs" % n.op)
        b_join = L.new_block("%s/join" % n.op)
        L.emit("ac0 = %d" % (0 if n.op == "&&" else 1))
        L.emit("%s = ac0" % out)
        L.emit("ac0 = %s" % lv)
        if n.op == "&&":
            L.terminate("goto [%s, %s] tf(ac0)" % (b_join.name, b_rhs.name))
        else:
            L.terminate("goto [%s, %s] tf(ac0)" % (b_rhs.name, b_join.name))
        L.open_block(b_rhs)
        rv, _rk = self.expr(n.right)
        L.emit("ac0 = %s" % rv)
        L.emit("ac0 = tf(ac0)")
        L.emit("%s = ac0" % out)
        L.goto(b_join)
        L.open_block(b_join)
        return out, "i32"

    def cast(self, n):
        L = self.L
        kind = type_kind_of(n.to_type.type, n)
        v, _k = self.expr(n.expr)
        out = L.new_v("node", promote(kind), "", 0, "cast:" + kind, self.line(n))
        L.emit("ac0 = %s" % v)
        if kind == "u8":
            L.emit("ac0 = trunc8(ac0)")
        elif KINDS[kind][1] == 1:
            L.emit("ac0 = %s(ac0)" % ("sx16" if is_signed(kind) else "zx16"))
        L.emit("%s = ac0" % out)
        return out, promote(kind)

    # -------------------------------------------------------- builtins ---
    def call(self, n):
        L = self.L
        if not isinstance(n.name, c_ast.ID):
            refuse("call", n, "indirect calls are not in the subset")
        name = n.name.name
        args = n.args.exprs if n.args else []
        if name == "ABS":
            return self.builtin_abs(n, args)
        if name == "RANGE_CHECK":
            return self.builtin_sub(n, args)
        if name in ("MIN", "MAX"):
            return self.builtin_minmax(n, name, args)
        if name in RT_CALLS:
            return self.rt_call(n, name, args)
        if name in L.game_routines:
            return self.game_call(n, name, args)
        if name == "BITS":
            refuse("BITS", n, "BITS() is only an ARGUMENT of a call, never a "
                              "value: it builds its literal into a temp through "
                              "X.CB (IR.md §6)")
        refuse("function call", n,
               "`%s` is not a builtin, not a runtime routine in RT_CALLS, and "
               "not another routine of this compilation unit.  A naive `call` "
               "enters the callee's b0, which the loader requires to be a block "
               "of THIS file (IR.md §6, P54 a001 R1) — so compile the callee "
               "into the same unit: --routine %s --routine %s"
               % (name, self.L.routine, name))

    def builtin_abs(self, n, args):
        """a001 R1b: a lowered PL/I builtin, not a call.  The naive form is
        the compare/negate diamond, three blocks.

        SOUNDNESS BUG, found by the differential tester (seed 2 class B, and
        the reason abs_edges alone did not catch it): the result type is
        ABS's DECLARED type, `int32_t`, not the argument's.  Propagating the
        argument's type made `ABS(chk)` unsigned for a `uint32_t` chk, which
        then poisoned the enclosing divide into `/u` and the comparison above
        it into `<=u` — a wrong answer three operators away from the cause.
        A builtin's type comes from its declaration, exactly as a call's
        would."""
        L = self.L
        if len(args) != 1:
            refuse("ABS", n, "ABS takes one argument")
        v, argk = self.expr(args[0])
        k = argk if MUTATE == "abs_argtype" else "i32"
        out = L.new_v("node", k, "", 0, "ABS", self.line(n))
        # P55 CHANGE 2 — ONE-ARMED, on a book-wide census: all 36
        # compare-against-zero sites (`WSGE r,r`, IR.md §5.6) skip a
        # ONE-INSTRUCTION arm — 14 `WNEG` and 8 `NNEG` (this idiom, narrow and
        # wide) and 8 `WSUB` (MAX against 0).  NONE is a two-armed diamond.
        # The positive path is the skip's fall-through and needs no block: the
        # value is already in place and only the negative path acts.
        b_neg = L.new_block("ABS/neg")
        b_join = L.new_block("ABS/join")
        L.emit("ac0 = %s" % v)
        L.emit("%s = ac0" % out, "in place; only the negative arm acts")
        L.emit("ac0 = (ac0 >=s 0)")
        L.terminate("goto [%s, %s] tf(ac0)" % (b_neg.name, b_join.name))
        L.open_block(b_neg)
        L.emit("ac0 = %s" % v)
        L.emit("ac0 = 0 - ac0")
        L.emit("%s = ac0" % out)
        L.goto(b_join)
        L.open_block(b_join)
        return out, k

    def builtin_minmax(self, n, which, args):
        """P51 §2 item 7: the `WSGE a,b; WMOV a,b` diamond, exactly as ABS is
        the WSGE/WNEG diamond.  A builtin's result type is its DECLARED type
        (P48 §2.1's bug), and MIN/MAX are declared int32_t."""
        L = self.L
        if len(args) != 2:
            refuse(which, n, "%s takes two arguments" % which)
        av, _ak = self.expr(args[0])
        bv, _bk = self.expr(args[1])
        out = L.new_v("node", "i32", "", 0, which, self.line(n))
        # P55 CHANGE 2 — ONE-ARMED.  This diamond is register-REGISTER, so the
        # 36/36 compare-against-zero census does NOT cover it and it must not
        # be changed by pattern-match from ABS (a005 confirms: the aggregate
        # 807:199 does not cover it either).  Its own census:
        # 403 of 416 register-register compare-skip sites have a
        # ONE-INSTRUCTION arm, of which 150 are exactly this lone-`MOV`
        # diamond (P55 q002; a005 accepts it as the warrant).
        #
        # `a` is the in-place default, so only the `b` arm is emitted.
        b_b = L.new_block("%s/b" % which)
        b_join = L.new_block("%s/join" % which)
        L.emit("ac0 = %s" % av)
        L.emit("ac1 = %s" % bv)
        L.emit("%s = ac0" % out, "a in place; only the b arm acts")
        L.emit("ac0 = (ac0 %s ac1)" % ("<s" if which == "MIN" else ">s"))
        L.terminate("goto [%s, %s] tf(ac0)" % (b_b.name, b_join.name))
        L.open_block(b_b)
        L.emit("ac0 = %s" % bv)
        L.emit("%s = ac0" % out)
        L.goto(b_join)
        L.open_block(b_join)
        return out, "i32"

    # ------------------------------------------------------------- calls --
    def string_literal(self, node, text, anchor):
        """A compiled routine's string literal is NOWHERE (IR.md §5.10.1b): a
        §5.8 literal piece names a byte address IN THE IMAGE and the executor
        faults if the bytes disagree.  So it becomes an INITIALISED `v`, whose
        bytes the loader writes at placement."""
        L = self.L
        data = decode_c_string(text, node)
        if not data:
            refuse("string constant", node, "an empty string has no capacity")
        if len(data) > 32767:
            refuse("string constant", node, "longer than `char 32767`")
        v = L.new_v("literal", "u8", "", len(data), anchor, self.line(node))
        L.vdecls[-1] = "v %-24s char %d = \"%s\"" % (v, len(data), ir_escape(data))
        L.vtype[v] = "char %d" % len(data)
        return v, len(data)

    def varying_dummy(self, node, v, nbytes, anchor):
        """A CHAR constant reaching a CHAR VARYING parameter: the caller builds
        the dummy — length word then data — in its own frame and pushes its
        WORD address (P51 §2 item 4, §3; HIT_ANY_CHAR 7016DE93..DEA3).  Naive
        means one dummy per site: the original reuses one temp for both of its
        literals, and reusing it here would be a rewrite nobody could be
        credited with later."""
        L = self.L
        d = L.new_v("dummy", "u16", "", 0, anchor, self.line(node))
        L.vdecls[-1] = "v %-24s varying %d" % (d, nbytes)
        L.vtype[d] = "varying %d" % nbytes
        # The destination is `wp(cell, 0)`, NOT the bare cell name.  IR.md
        # §5.10.4 and §5.10.10 both spell this `[@QUEST.v2, 8 varying]`, and
        # that form does not load — measured against the spec's own worked
        # example.  P52 §7 F3 is why: `check_piece` was reworked to resolve
        # through wp/bp and use which wrapper it found as the word/byte
        # evidence, and the examples were not updated with it.  Reported as
        # q006 and FIXED: IR.md §5.10.4 and §5.10.10 now spell it this way, and
        # P54 added a self-test leg that loads the spec's examples verbatim.
        L.emit("[@wp(%s, 0), %d varying] = [@bp(%s, 0), %d]"
               % (d, nbytes, v, nbytes))
        return d

    def arg_address(self, n, callee, idx, spec):
        """One by-reference argument, as the WORD ADDRESS the callee receives.
        Returns a pure expr for the rt_call/call argument list."""
        L = self.L
        dummies = spec.get("dummy", {})
        # &lvalue — the address of a static, a field, an element or a local
        if isinstance(n, c_ast.UnaryOp) and n.op == "&":
            av = self.address_of_any(n.expr)
            return av
        # a string constant reaching a CHAR VARYING parameter
        if isinstance(n, c_ast.Constant) and n.type == "string":
            v, nbytes = self.string_literal(n, n.value, "lit:%s#%d" % (callee, idx))
            d = self.varying_dummy(n, v, nbytes, "dummy:%s#%d" % (callee, idx))
            return "wp(%s, 0)" % d
        # BITS("001") — built at run time by X.CB into a temp (Salvage F12)
        if isinstance(n, c_ast.FuncCall) and isinstance(n.name, c_ast.ID) \
                and n.name.name == "BITS":
            return self.bits_literal(n, callee, idx)
        # TMP(e) — a dummy of the CALLEE PARAMETER's width (ruling 6)
        if isinstance(n, c_ast.FuncCall) and isinstance(n.name, c_ast.ID) \
                and n.name.name == "TMP":
            if idx not in dummies:
                refuse("TMP", n,
                       "no dummy width recorded for %s argument %d — the width "
                       "comes from the CALLEE's parameter (carried-in ruling 6), "
                       "which is a row in docs/Project28/RTConventions.md, not "
                       "something to infer from the expression"
                       % (callee, idx))
            return self.tmp_dummy(n, callee, idx, dummies[idx])
        refuse("argument", n,
               "argument %d of %s is not a by-reference form: PL/I passes by "
               "reference, so write `&lvalue`, `TMP(expr)`, a CHAR constant or "
               "BITS(...)" % (idx, callee))

    def tmp_dummy(self, n, callee, idx, vtype):
        L = self.L
        inner = n.args.exprs[0] if n.args and n.args.exprs else None
        if inner is None:
            refuse("TMP", n, "TMP takes one argument")
        anchor = "tmp:%s#%d" % (callee, idx)
        if vtype == "*char":
            # arg 2 of ?READ: the datum IS a pointer — a dummy holding the
            # buffer's BYTE pointer, whose address is pushed (P51 §2 item 3).
            if not (isinstance(inner, c_ast.ID) and inner.name in L.scope):
                refuse("TMP", n, "a pointer-valued dummy takes an array name")
            base, _k, nelem, _ew = L.scope[inner.name]
            if not nelem or not L.is_byte(base):
                refuse("TMP", n, "a *char dummy takes a byte array")
            d = L.new_v("dummy", "u32", "", 0, anchor, self.line(n))
            L.vdecls[-1] = "v %-24s *char" % d
            L.vtype[d] = "*char"
            L.emit("%s = bp(%s, 0)" % (d, base))
            return "wp(%s, 0)" % d
        v, _k = self.expr(inner)
        d = L.new_v("dummy", vtype, "", 0, anchor, self.line(n))
        L.emit("ac0 = %s" % v)
        L.emit("%s = ac0" % d)
        return "wp(%s, 0)" % d

    def bits_literal(self, n, callee, idx):
        """`'001'B` is NOT constant-folded by the 1986 compiler: it is built at
        run time, at every evaluation, by X.CB (Salvage F12).  So this is a
        SECOND call, and because a call is a terminator it takes its own block
        pair — two calls and three blocks for one C argument."""
        L = self.L
        if not (n.args and len(n.args.exprs) == 1
                and isinstance(n.args.exprs[0], c_ast.Constant)
                and n.args.exprs[0].type == "string"):
            refuse("BITS", n, "BITS takes one string literal")
        text = decode_c_string(n.args.exprs[0].value, n)
        if any(b not in (0x30, 0x31) for b in text):
            refuse("BITS", n, "a BIT literal is '0's and '1's only")
        lit, nbytes = self.string_literal(n, n.args.exprs[0].value,
                                          "bits:%s#%d" % (callee, idx))
        dest = L.new_v("dummy", "u32", "", 0, "bits:dest", self.line(n))
        nxt = L.new_block("X.CB/ret")
        L.emit("ac2 = wp(%s, 0)" % dest, "X.CB: destination word address")
        L.emit("ac0 = bp(%s, 0)" % lit, "X.CB: byte pointer to the character form")
        L.emit("ac1 = %d" % nbytes, "X.CB: its length")
        L.terminate("rt_call %s() ret=%s" % (X_CB, nxt.name),
                    "undecorated, no stack arguments (IR.md §6)")
        L.open_block(nxt)
        return "wp(%s, 0)" % dest

    def address_of_any(self, n):
        """`&x` for the forms a call argument takes.  Returns a pure expr."""
        L = self.L
        if isinstance(n, c_ast.ID):
            if n.name in L.statics:
                return L.const_text(L.statics[n.name]["addr"])
            if n.name in L.scope:
                vname, _k, nelem, _ew = L.scope[n.name]
                if L.is_byte(vname):
                    return "bp(%s, 0)" % vname
                return "wp(%s, 0)" % vname
            if n.name in L.params:
                # passing our own incoming pointer straight through
                return L.params[n.name][0]
            refuse("address-of", n, "undeclared name `%s`" % n.name)
        addr_v, _kind = self.address_of(n)
        return addr_v

    def rt_call(self, n, name, args):
        L = self.L
        spec = RT_CALLS[name]
        if len(args) != spec["argc"]:
            refuse("rt_call", n, "%s takes %d arguments, %d given"
                   % (name, spec["argc"], len(args)))
        exprs = [self.arg_address(a, name, i + 1, spec)
                 for i, a in enumerate(args)]
        nxt = L.new_block("%s/ret" % name)
        L.terminate("rt_call %s(%s) ret=%s"
                    % (spec["callee"], ", ".join(exprs), nxt.name))
        L.open_block(nxt)
        if not spec["valued"]:
            return None, None
        # THE VALUED-CALL SPLIT (IR.md §5.10.6): the call ended a block and the
        # result is read in the NEXT one, which is why a call can never sit
        # inside a larger expression.
        out = L.new_v("node", "i32", "", 0, "ret:" + name, self.line(n))
        L.emit("ac0 = ac0", "the runtime returns in ac0 (RTConventions)")
        L.emit("%s = ac0" % out)
        return out, "i32"

    def game_call(self, n, name, args):
        """game->game: the arguments go in the CALLEE's `a` cells, which is
        legal only because the game is non-reentrant (IR.md §5.10.6).  We
        control both ends, so the original's push-and-prologue machinery is a
        rewrite to reintroduce at L2, not something the compiler emits."""
        L = self.L
        sig = L.game_routines[name]
        if len(args) != len(sig):
            refuse("call", n, "%s takes %d arguments, %d given"
                   % (name, len(sig), len(args)))
        exprs = [self.address_of_any(a.expr if isinstance(a, c_ast.UnaryOp)
                                     and a.op == "&" else a)
                 for a in args]
        for i, (e, pointee) in enumerate(zip(exprs, sig), 1):
            L.emit("%s.a%d = %s" % (name, i, e))
        L.emit("%s.arg_count = %d" % (name, len(args)))
        nxt = L.new_block("%s/ret" % name)
        L.terminate("call %s args=%d ret=%s" % (name, len(args), nxt.name))
        L.open_block(nxt)
        return None, None

    def builtin_sub(self, n, args):
        """PL/I's subscript check: i in 1..n, else DERR 17 (an ABORT-kind
        terminal, IR.md §4a).  The message carries the source site so that
        "both trapped at the same place" is a string compare against
        quest_rt.h's quest_sub_check (a001 R5)."""
        L = self.L
        if len(args) != 2:
            refuse("RANGE_CHECK", n, "RANGE_CHECK takes two arguments")
        iv, ik = self.expr(args[0])
        nv, _nk = self.expr(args[1])
        out = L.new_v("node", "i32", "", 0, "RANGE_CHECK", self.line(n))
        L.emit("ac0 = %s" % iv)
        L.emit("ac1 = %s" % nv)
        L.emit('assert(((ac0 >s 0) && (ac0 <=s ac1)), "DERR17 %s:%d")'
               % (L.src, self.line(n)))
        L.emit("%s = ac0" % out)
        return out, "i32"


# ========================================================== declarations ===

def type_kind_of(td, node):
    """The declared kind of a TypeDecl / Typename."""
    t = td
    while not isinstance(t, c_ast.IdentifierType):
        if isinstance(t, (c_ast.TypeDecl,)):
            t = t.type
        elif isinstance(t, c_ast.PtrDecl):
            refuse("pointer type", node, "pointers beyond by-reference parameters are out")
        else:
            refuse("type", node, "unsupported declarator %s" % type(t).__name__)
    name = " ".join(t.names)
    if name not in CNAME_TO_KIND:
        refuse("type", node, "`%s` is not in the v1 subset" % name)
    return CNAME_TO_KIND[name]


# =============================================================== driver ====

class Compiler:
    def __init__(self, entry, src, routine):
        self.L = Lowerer(entry, src, routine)
        self.W = Walker(self.L)

    # ------------------------------------------------------ file scope --
    def collect_file_scope(self, ast):
        """DESIGN §4.1: a `v` is globally scoped and qualified by addrbook
        entry, so a C file-scope object and a C local are the SAME thing
        here.  The generated test programs put observables at file scope
        because a local of t() is not reachable from the native harness.

        Two kinds of file-scope Decl are NOT storage and must not become a `v`.
        Both arrive the moment a routine includes `declarations.h`, which
        UPDATE_SCREENS did not until P53 fixed its native view — so this path
        had never been exercised by a real routine, only by generated programs
        that include `quest_rt.h` alone:

          * a BARE STRUCT definition (`struct region_rec { ... };`) has no
            declarator at all — it defines a type and declares nothing.
          * an `extern` declares storage that lives SOMEWHERE ELSE.  Here that
            somewhere is the game image, whose geometry the compiler already
            reads from declarations.json; allocating a 0x76 cell for `PLAYER`
            would be inventing a second, wrong home for it.

        Skipping an `extern` does NOT make an unknown name silently acceptable:
        a name with no declarations.json entry still refuses at its USE site
        (`read_id`, `address_of_structref`), where the message names the field.
        """
        L = self.L
        for ext in ast.ext:
            if not isinstance(ext, c_ast.Decl):
                continue
            if isinstance(ext.type, c_ast.FuncDecl):
                continue
            if ext.name is None:
                continue                       # a bare struct/union/enum type
            if "extern" in (ext.storage or []):
                continue                       # lives in the image, not in 0x76
            self.declare(ext)

    def declare(self, d):
        L = self.L
        if d.init is not None:
            refuse("initialiser", d,
                   "declare and then assign: a C initialiser runs once at "
                   "load while a `v` is only memory")
        if d.name in L.scope:
            refuse("declaration", d, "`%s` declared twice" % d.name)
        if isinstance(d.type, c_ast.ArrayDecl):
            kind = type_kind_of(d.type.type, d)
            if d.type.dim is None or not isinstance(d.type.dim, c_ast.Constant):
                refuse("array", d, "an array needs a constant bound")
            nelem, _ = parse_int_literal(d.type.dim.value, d)
            size = nelem if kind == "u8" else nelem * KINDS[kind][1]
            if nelem < 1 or size > 32767:
                refuse("array", d,
                       "bound out of the `%s 1..32767` range"
                       % ("char" if kind == "u8" else "words"))
            v = L.new_v("local", kind, d.name, nelem, "decl:" + d.name,
                        getattr(d.coord, "line", 0))
            L.scope[d.name] = (v, kind, nelem, KINDS[kind][1])
        elif isinstance(d.type, c_ast.TypeDecl):
            kind = type_kind_of(d.type, d)
            v = L.new_v("local", kind, d.name, 0, "decl:" + d.name,
                        getattr(d.coord, "line", 0))
            L.scope[d.name] = (v, kind, 0, KINDS[kind][1])
        else:
            refuse("declaration", d, "unsupported declarator")

    # -------------------------------------------------------- the routine --
    def compile_routine(self, fd):
        L = self.L
        decl = fd.decl.type
        # parameters: by reference (PL/I).  a001 R4: a `u32` v holding the
        # argument's WORD ADDRESS; the rig or the calling bridge seeds it.
        params = decl.args.params if decl.args else []
        self.nargs = 0
        for p in params:
            if isinstance(p, c_ast.Typename) and isinstance(p.type, c_ast.TypeDecl) \
                    and isinstance(p.type.type, c_ast.IdentifierType) \
                    and p.type.type.names == ["void"]:
                continue
            if not isinstance(p, c_ast.Decl):
                refuse("parameter", p, "unsupported parameter form")
            if not isinstance(p.type, c_ast.PtrDecl):
                refuse("parameter", p,
                       "PL/I passes by reference: every parameter is a pointer")
            pointee = type_kind_of(p.type.type, p)
            self.nargs += 1
            v = L.new_a("a%d" % self.nargs, "param", "u32", p.name, 0,
                        "param:" + p.name, getattr(p.coord, "line", 0),
                        ptr_to=pointee)
            L.params[p.name] = (v, pointee)
        # return value
        rk = None
        if not (isinstance(decl.type, c_ast.TypeDecl)
                and isinstance(decl.type.type, c_ast.IdentifierType)
                and decl.type.type.names == ["void"]):
            rk = type_kind_of(decl.type, fd)
            self.retv = L.new_a("ret", "ret", rk, "__ret", 0, "ret", 0)
        else:
            self.retv = None
        self.retkind = rk
        # The supplied-argument count (§5.10.1c).  Declared even when the
        # routine takes none: it is half the signature, and a caller cannot
        # emit `call` at all unless the callee declares it (IRExec.cpp:1352).
        # The CALLER writes it; the callee only declares it.
        L.new_a("arg_count", "argc", "u16", "__arg_count", 0, "argc", 0)

        self.prescan_labels(fd.body)
        entry = L.new_block("fn/entry")
        L.open_block(entry)
        self.entry_block = entry
        self.stmt(fd.body)
        if L.cur is not None:
            L.terminate("ret", "fall off the end")

    def prescan_labels(self, node):
        """Blocks for C labels are allocated up front so a forward `goto`
        has a name to jump to (ir 7 allows forward symbolic references)."""
        L = self.L
        for _, child in node.children():
            if isinstance(child, c_ast.Label):
                if child.name not in L.labels:
                    L.labels[child.name] = L.new_block("label:" + child.name)
            self.prescan_labels(child)

    # ----------------------------------------------------- statements ----
    def stmt(self, n):
        L = self.L
        if n is None:
            return
        if isinstance(n, c_ast.Compound):
            for s in (n.block_items or []):
                self.stmt(s)
            return
        if isinstance(n, c_ast.Decl):
            self.declare(n)
            return
        if isinstance(n, c_ast.EmptyStatement):
            return
        if isinstance(n, c_ast.Assignment):
            self.assign(n)
            return
        if isinstance(n, c_ast.UnaryOp) and n.op in ("p++", "p--", "++", "--"):
            self.incdec(n)
            return
        if isinstance(n, c_ast.If):
            self.if_stmt(n)
            return
        if isinstance(n, c_ast.While):
            self.while_stmt(n)
            return
        if isinstance(n, c_ast.For):
            self.for_stmt(n)
            return
        if isinstance(n, c_ast.Return):
            self.return_stmt(n)
            return
        if isinstance(n, c_ast.Goto):
            if n.name not in L.labels:
                refuse("goto", n, "no label `%s` in this routine" % n.name)
            L.goto(L.labels[n.name], "goto " + n.name)
            return
        if isinstance(n, c_ast.Label):
            b = L.labels[n.name]
            if L.cur is not None:
                L.goto(b, "fall into " + n.name)
            L.open_block(b)
            self.stmt(n.stmt)
            return
        if isinstance(n, c_ast.Break):
            if not L.loops:
                refuse("break", n, "break outside a loop")
            L.goto(L.loops[-1][0], "break")
            return
        if isinstance(n, c_ast.Continue):
            if not L.loops:
                refuse("continue", n, "continue outside a loop")
            L.goto(L.loops[-1][1], "continue")
            return
        if isinstance(n, c_ast.FuncCall):
            self.W.call(n)          # a void call in statement position
            return
        refuse(type(n).__name__, n, "statement not in the v1 C subset")

    def assign(self, n):
        L = self.L
        if n.op != "=":
            refuse("compound assignment " + n.op, n,
                   "write it out: `x = x %s e`" % n.op[:-1])
        rv, _rk = self.W.expr(n.rvalue)
        lhs = n.lvalue
        if isinstance(lhs, c_ast.ID) and lhs.name in L.scope:
            vname, kind, nelem, _ew = L.scope[lhs.name]
            if nelem:
                refuse("assignment", n, "cannot assign to a whole array")
            L.emit("ac0 = %s" % rv)
            L.store_reg("ac0", vname, kind)
            return
        addr_v, kind = self.W.address_of(lhs)
        L.emit("ac0 = %s" % rv)
        L.emit("ac2 = %s" % addr_v)
        L.store_through_ac2("ac0", kind)

    def incdec(self, n):
        L = self.L
        if not (isinstance(n.expr, c_ast.ID) and n.expr.name in L.scope):
            refuse("increment", n, "++/-- applies to a declared scalar only")
        vname, kind, nelem, _ew = L.scope[n.expr.name]
        if nelem:
            refuse("increment", n, "++/-- applies to a scalar")
        L.load_reg("ac0", vname, kind)
        L.emit("ac0 = ac0 %s 1" % ("+" if n.op in ("p++", "++") else "-"))
        L.store_reg("ac0", vname, kind)

    def return_stmt(self, n):
        L = self.L
        if n.expr is None:
            if self.retkind is not None:
                refuse("return", n, "this routine returns a value")
            L.terminate("ret")
            return
        if self.retkind is None:
            refuse("return", n, "a void routine cannot return a value")
        v, _k = self.W.expr(n.expr)
        L.emit("ac0 = %s" % v)
        L.store_reg("ac0", self.retv, self.retkind)
        L.terminate("ret")

    def if_stmt(self, n):
        L = self.L
        cv, _k = self.W.expr(n.cond)
        b_then = L.new_block("if/then")
        b_else = L.new_block("if/else") if n.iffalse is not None else None
        b_join = L.new_block("if/join")
        L.branch(cv, b_else or b_join, b_then)
        L.open_block(b_then)
        self.stmt(n.iftrue)
        if L.cur is not None:
            L.goto(b_join)
        if b_else is not None:
            L.open_block(b_else)
            self.stmt(n.iffalse)
            if L.cur is not None:
                L.goto(b_join)
        L.open_block(b_join)

    def while_stmt(self, n):
        L = self.L
        b_head = L.new_block("while/head")
        b_body = L.new_block("while/body")
        b_after = L.new_block("while/after")
        L.goto(b_head)
        L.open_block(b_head)
        cv, _k = self.W.expr(n.cond)
        L.branch(cv, b_after, b_body)
        L.open_block(b_body)
        L.loops.append((b_after, b_head))
        self.stmt(n.stmt)
        L.loops.pop()
        if L.cur is not None:
            L.goto(b_head)
        L.open_block(b_after)

    def for_stmt(self, n):
        L = self.L
        if n.init is not None:
            if isinstance(n.init, c_ast.DeclList):
                for d in n.init.decls:
                    self.stmt(d)
            else:
                self.stmt(n.init)
        # P55 CHANGE 1 — the canonical DG PL/I `DO` shape, on a BOOK-WIDE
        # CENSUS of 206 XNDO/XWDO sites with no exceptions (P55 BlockCensus
        # §2.2; ruled in P55 a001 Q2(a), landed under a004/a005):
        #
        #   <entry test>:  cond       -> [after, preheader]   (OUTSIDE the loop)
        #   preheader:     goto body                (unconditional, OVER step)
        #   step:          next; cond -> [body, after]   (the DO block:
        #                                  increment AND test, in ONE block,
        #                                  entered only on the back edge)
        #   body:          ... ; goto step               (back edge)
        #
        # This is ordinary loop rotation, so the gcc differential oracle still
        # checks it — which was the deciding argument for changing the LOWERING
        # rather than adding a `DO()` macro or a rewrite (a001 Q2).
        #
        # DESIGN §6 permits this ONLY on a book-wide census and never to close
        # one routine's diff.  `while_stmt` is deliberately NOT rotated: the
        # 206 sites are PL/I DO loops, nothing censused covers a `while`, and
        # widening on a pattern-match is the move the clause forbids.  The
        # resulting for/while asymmetry is intended (q002 §exclusions, a005).
        #
        # COST, reported rather than hidden: `cond` is emitted TWICE — once as
        # the entry test, once in the step block — so each loop's condition and
        # its `v`s are duplicated.  That is faithful (the book loads the limit
        # in the header and RELOADS it in the DO block, 206/206) but it raises
        # the `v` count, a headline metric under DESIGN §5.1.
        #
        # Blocks are created in the book's address order.  §6: numeric order
        # carries no meaning, so this is legibility, not semantics.
        b_pre = L.new_block("for/preheader")
        b_step = L.new_block("for/step")
        b_body = L.new_block("for/body")
        b_after = L.new_block("for/after")

        # The entry test stays in the CURRENT block: the book's guard is
        # outside the loop and its not-entered leg lands on its own block.
        if n.cond is not None:
            cv, _k = self.W.expr(n.cond)
            L.branch(cv, b_after, b_pre)
        else:
            L.goto(b_pre)

        L.open_block(b_pre)
        L.goto(b_body, "preheader: jumps OVER the step block")

        L.open_block(b_step)
        if n.next is not None:
            self.stmt(n.next)
        if n.cond is not None:
            cv2, _k2 = self.W.expr(n.cond)
            L.branch(cv2, b_after, b_body)
        else:
            L.goto(b_body)

        L.open_block(b_body)
        L.loops.append((b_after, b_step))   # break -> after, continue -> step
        self.stmt(n.stmt)
        L.loops.pop()
        if L.cur is not None:
            L.goto(b_step, "back edge")
        L.open_block(b_after)


def _compiler_ir_text(self):
    return unit_ir_text([self])


def _compiler_vmap_text(self):
    return unit_vmap_text([self])


def unit_ir_text(compilers):
    """A COMPILATION UNIT: one or more routines in one ir 8 file (P53 a003).

    A naive `call` enters the callee's `b0`, which the loader requires to be a
    block of THIS file (P54 a001 R1) — so a caller and its callees compile
    together or the caller does not load at all.

    THE ORDERING CONSTRAINT, and it is the one that would break silently: a
    cell must be declared before its first reference IN FILE ORDER (§5.10.1),
    and a caller references its CALLEE's `a` cells.  So ALL declarations of
    ALL routines precede ALL blocks of ALL routines — not each routine's
    declarations before its own blocks.  `--check-unit-order` asserts it.
    """
    first = compilers[0].L
    out = ["ir 8", "mode stock",
           "; GENERATED by compiler/lower_c.py from %s" % first.src,
           "; unit: %s" % ", ".join(c.L.routine for c in compilers),
           "; NAIVE: every value in its own v, zero t-places, pure operators only",
           ""]
    for c in compilers:                      # every declaration, every routine
        out += c.L.vdecls
    out.append("")
    nblocks = 0
    for c in compilers:                      # then every block
        for b in c.L.blocks:
            out.append("block %s ; anchor=%s" % (b.name, b.anchor))
            out += b.lines
            if b.term is None:
                raise Refusal("block", 0, "block %s has no terminator" % b.name)
            out.append(b.term)
            out.append("")
            nblocks += 1
    out.append("blocks %d" % nblocks)
    return "\n".join(out) + "\n"


def unit_vmap_text(compilers):
    first = compilers[0]
    out = ["# lower_c 1",
           "# src %s" % first.L.src,
           "# routine %s" % first.L.routine,
           "# entry %s" % first.L.entry,
           "# entryblock %s" % first.entry_block.name,
           "# unit %s" % " ".join(c.L.routine for c in compilers),
           "# columns: vname kind cname ctype vtype words nelem elemwords render anchor line"]
    for c in compilers:
        out += [r.tsv() for r in c.L.vrows]
    return "\n".join(out) + "\n"




def game_signatures(ast, routines):
    """The by-reference signatures of the unit's routines, from their C
    prototypes: name -> [pointee kind per parameter]."""
    out = {}
    for ext in ast.ext:
        decl = None
        if isinstance(ext, c_ast.FuncDef):
            decl = ext.decl
        elif isinstance(ext, c_ast.Decl) and isinstance(ext.type, c_ast.FuncDecl):
            decl = ext
        if decl is None or decl.name not in routines or decl.name in out:
            continue
        params = decl.type.args.params if decl.type.args else []
        sig, ok = [], True
        for prm in params:
            if isinstance(prm, c_ast.Typename) and isinstance(prm.type, c_ast.TypeDecl) \
                    and isinstance(prm.type.type, c_ast.IdentifierType) \
                    and prm.type.type.names == ["void"]:
                continue
            if not isinstance(prm, c_ast.Decl) or not isinstance(prm.type, c_ast.PtrDecl):
                ok = False
                break
            sig.append(type_kind_of(prm.type.type, prm))
        if ok:
            out[decl.name] = sig
    return out


def check_unit_kinds(compilers):
    """The argument pointer-KIND check, as a UNIT POST-PASS.

    The per-statement tripwire cannot do this one.  A caller writes its
    CALLEE's `a` cells, and the callee is compiled AFTER the caller — so at the
    moment HIT_ANY_CHAR emits `GET_INPUT.a1 = bp(...)`, `GET_INPUT.a1` has no
    declared type yet and the check silently passes.  Measured: flipping bp to
    wp produced a clean compile and an IR the loader then refused.

    A tripwire with a hole in exactly the construct it was added for is worse
    than none, because it is trusted.  So the check runs again here, over the
    finished text, when every declaration exists — which is the same shape
    P54's loader post-pass settled on, for the same reason.
    """
    vtype = {}
    for c in compilers:
        vtype.update(c.L.vtype)
    pat = re.compile(r"^\s*([A-Za-z_$][\w.$]*\.a\d+)\s*=\s*(wp|bp)\(")
    for c in compilers:
        for b in c.L.blocks:
            for line in b.lines:
                m = pat.match(line)
                if not m:
                    continue
                cell, builder = m.group(1), m.group(2)
                vt = vtype.get(cell)
                if not vt or not vt.startswith("*"):
                    continue
                want = "bp" if vt == "*char" else "wp"
                if builder != want:
                    raise Refusal(
                        "ir8-kind", 0,
                        "in block %s: `%s` is declared `%s` but is written a %s "
                        "pointer (%s(...)) — argument pointer KIND must match "
                        "the callee's `a` cell (IR.md §6)"
                        % (b.name, cell, vt,
                           "BYTE" if builder == "bp" else "WORD", builder))


def check_unit_order(ir):
    """a003 asked for a teeth leg on the ordering constraint, because it is the
    kind that breaks silently: a caller writes its CALLEE's `a` cells, so if a
    routine's blocks were emitted before a later routine's declarations the
    loader would refuse with `reference before declaration` — or worse, a
    future loader might not.  Assert the shape directly on the text."""
    seen_block = False
    for n, line in enumerate(ir.splitlines(), 1):
        t = line.strip()
        if t.startswith("block "):
            seen_block = True
        elif seen_block and (t.startswith("v ") or t.startswith("a ")) \
                and not line.startswith("  "):
            raise Refusal("unit-order", n,
                          "declaration `%s` follows a block: every routine's "
                          "declarations must precede EVERY routine's blocks "
                          "(IR.md §5.10.1, declared before first reference in "
                          "FILE order)" % t)


Compiler.ir_text = _compiler_ir_text        # a one-routine unit is still a unit
Compiler.vmap_text = _compiler_vmap_text


def preprocess(path):
    cmd = ["cpp", "-D__TRANSLATOR__", "-nostdinc",
           "-I" + FAKE_INCLUDE, "-I" + GAME, "-I" + os.path.dirname(os.path.abspath(path)),
           path]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if p.returncode != 0:
        sys.stderr.write(p.stderr.decode())
        sys.exit(1)
    return p.stdout.decode()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source", nargs="+",
                    help="one or more .c files.  A compilation unit spans "
                         "FILES as well as routines: HIT_ANY_CHAR calls "
                         "GET_INPUT and they are separate files, but a naive "
                         "call needs the callee's b0 in the same IR file.")
    ap.add_argument("--routine", required=True, action="append",
                    help="repeatable: a COMPILATION UNIT of several routines "
                         "(P53 a003).  A naive `call` enters the callee's b0, "
                         "which must be a block of this file, so a caller and "
                         "its callees compile together.  The FIRST --routine "
                         "is the unit's entry.")
    ap.add_argument("--entry", default=None, action="append",
                    help="the addrbook entry to qualify names with, one per "
                         "--routine, in the same order (default: the routine name)")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--vmap", default=None)
    ap.add_argument("--mutate", choices=sorted(MUTATIONS), default=None,
                    help="inject one deliberate soundness bug (teeth leg only)")
    a = ap.parse_args()
    global MUTATE
    MUTATE = a.mutate
    routines = a.routine
    entries = a.entry or []
    if entries and len(entries) != len(routines):
        sys.stderr.write("REFUSE unit at %s:0: %d --entry for %d --routine; "
                         "give one per routine, in order\n"
                         % (os.path.basename(a.source), len(entries), len(routines)))
        return 2
    if not entries:
        entries = list(routines)

    parser = c_parser.CParser()
    asts = []
    for src in a.source:
        try:
            asts.append((src, parser.parse(preprocess(src), filename=src)))
        except Exception as e:
            sys.stderr.write("REFUSE parse at %s: %s\n" % (src, e))
            return 2
    ast = asts[0][1]

    compilers = []
    try:
        # ONE vtype map for the whole unit: a caller writes its CALLEE's `a`
        # cells, so the q005 kind check has to see the callee's declarations,
        # which belong to a different Lowerer.
        unit_vtype = {}
        # Callable game routines are the OTHER routines of this unit and
        # nothing else — the loader requires a callee's b0 to be a block of
        # this file (P54 a001 R1), so a call to anything outside the unit could
        # not load, and must refuse here where the message is better.
        sigs = {}
        for _src, one in asts:
            sigs.update(game_signatures(one, routines))
        for routine, entry in zip(routines, entries):
            src, ast_for = None, None
            for cand_src, one in asts:
                for ext in one.ext:
                    if isinstance(ext, c_ast.FuncDef) and ext.decl.name == routine:
                        src, ast_for = cand_src, one
            if ast_for is None:
                sys.stderr.write("REFUSE routine at %s:0: no definition of `%s` "
                                 "in any given source\n"
                                 % (os.path.basename(a.source[0]), routine))
                return 2
            c = Compiler(entry, src, routine)
            c.L.vtype = unit_vtype
            c.L.game_routines = {k: v for k, v in sigs.items() if k != routine}
            # File-scope STORAGE in a multi-routine unit would be declared once
            # per routine and become two independent cells for one C object.
            # Rather than invent a sharing rule, refuse: no routine of the seven
            # has file-scope storage (statics come from declarations.json), and
            # the generated corpus is always a single routine.
            if len(routines) > 1:
                for ext in ast_for.ext:
                    if isinstance(ext, c_ast.Decl) \
                            and not isinstance(ext.type, c_ast.FuncDecl) \
                            and ext.name is not None \
                            and "extern" not in (ext.storage or []):
                        refuse("file-scope storage", ext,
                               "`%s` is file-scope storage in a multi-routine "
                               "unit: one C object would become one cell per "
                               "routine" % ext.name)
            else:
                c.collect_file_scope(ast_for)
            target = None
            for ext in ast_for.ext:
                if isinstance(ext, c_ast.FuncDef) and ext.decl.name == routine:
                    target = ext
            if target is None:
                sys.stderr.write("REFUSE routine at %s:0: no definition of `%s`\n"
                                 % (os.path.basename(a.source), routine))
                return 2
            c.compile_routine(target)
            compilers.append(c)
        ir = unit_ir_text(compilers)
        vmap = unit_vmap_text(compilers)
        check_unit_order(ir)
        check_unit_kinds(compilers)
    except Refusal as r:
        sys.stderr.write("REFUSE %s at %s:%d: %s\n"
                         % (r.construct,
                            ", ".join(os.path.basename(x) for x in a.source),
                            r.line, r.why))
        return 2

    open(a.out, "w").write(ir)
    open(a.vmap or (os.path.splitext(a.out)[0] + ".vmap"), "w").write(vmap)
    return 0


if __name__ == "__main__":
    sys.exit(main())
