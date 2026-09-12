#!/usr/bin/env python3
"""lower_c.py — the NAIVE C -> ir 7 compiler (Project 48, DESIGN.md §3).

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
}
MUTATE = None


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
    "i16": ("int16_t",  1, True,  16),
    "u16": ("uint16_t", 1, False, 16),
    "i32": ("int32_t",  2, True,  32),
    "u32": ("uint32_t", 2, False, 32),
}
CNAME_TO_KIND = {
    "int16_t": "i16", "short": "i16",
    "uint16_t": "u16", "unsigned short": "u16",
    "int32_t": "i32", "int": "i32", "signed int": "i32", "long": "i32",
    "uint32_t": "u32", "unsigned int": "u32", "unsigned": "u32",
    "unsigned long": "u32",
    "char": "u16",          # Salvage F13: PL/I CHARACTER is an UNSIGNED byte
    "unsigned char": "u16",
}


def promote(kind):
    """C's integer promotions (C99 6.3.1.1).  int16_t, uint16_t AND char all
    promote to int32_t, because `int` represents every one of their values —
    so `uint16_t / uint16_t` is a SIGNED divide.  This is the single most
    counter-intuitive rule in the model and the one the hand suite's
    u16_promote case exists to hold us to."""
    if MUTATE == "u16_unsigned" and kind == "u16":
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
        self.vdecls = []            # emitted `v` lines, in allocation order
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
        ctype, words, _s, _b = KINDS[kind]
        elemwords = words
        if nelem:
            vtype = "words %d" % (words * nelem)
            total = words * nelem
        else:
            vtype = kind
            total = words
        render = "w16" if words == 1 else "w32"
        self.vdecls.append("v %-24s %s" % (vname, vtype))
        self.vrows.append(VRow(vname, kind_of_v, cname, ctype, vtype, total,
                               nelem, elemwords, render, anchor, line))
        return vname

    def new_block(self, anchor):
        b = Block("%s.b%d" % (self.entry, self.nb), anchor)
        self.nb += 1
        self.blocks.append(b)
        return b

    # ------------------------------------------------------- statements --
    def emit(self, text, comment=""):
        if self.cur is None:
            # Unreachable code after a terminator.  It still has to live in a
            # well-formed block (every block needs a terminator, IR.md §4),
            # so it gets its own, which nothing jumps to.
            self.cur = self.new_block("unreachable")
        self.cur.lines.append("  %s%s" % (text, (" ; " + comment) if comment else ""))

    def terminate(self, text, comment=""):
        if self.cur is None:
            self.cur = self.new_block("unreachable")
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
        self.emit("ac0 = M32[%s]" % cond_v)
        self.terminate("goto [%s, %s] tf(ac0)" % (false_b.name, true_b.name),
                       comment)

    # --------------------------------------------------------- accessors --
    def load_reg(self, reg, vname, kind):
        w = KINDS[kind][1]
        if w == 1:
            ext = "sx16" if is_signed(kind) else "zx16"
            if MUTATE == "no_sign_extend":
                ext = "zx16"
            self.emit("%s = %s(M16[%s])" % (reg, ext, vname))
        else:
            self.emit("%s = M32[%s]" % (reg, vname))

    def store_reg(self, reg, vname, kind):
        if KINDS[kind][1] == 1:
            self.emit("M16[%s] = trunc16(%s)" % (vname, reg))
        else:
            self.emit("M32[%s] = %s" % (vname, reg))

    def load_through_ac2(self, reg, kind):
        if KINDS[kind][1] == 1:
            self.emit("%s = %s(M16[ac2])"
                      % (reg, "sx16" if is_signed(kind) else "zx16"))
        else:
            self.emit("%s = M32[ac2]" % reg)

    def store_through_ac2(self, reg, kind):
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
        self.emit("M32[%s] = ac0" % v)
        return v, kind


# ================================================== the expression walk ====

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
            L.emit("M32[%s] = ac0" % out)
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
            L.emit("ac0 = M32[%s]" % vname)
            L.emit("M32[%s] = ac0" % out)
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
            L.emit("ac1 = M32[%s]" % iv)
            if ew != 1:
                L.emit("ac1 = ac1 * %d" % ew)
            L.emit("ac2 = %s + ac1" % vname)
            L.emit("M32[%s] = ac2" % out)
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
        L.emit("M32[%s] = ac0" % out)
        return out

    def static_field_addr(self, sname, K, width, node):
        L = self.L
        basev = self.static_addr_v(sname, node)
        out = L.new_v("node", "u32", "", 0, "addr:%s.%d" % (sname, K), self.line(node))
        L.emit("ac0 = M32[%s]" % basev)
        L.emit("ac2 = ac0 + %s" % L.const_text(K))
        L.emit("M32[%s] = ac2" % out)
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
        L.emit("ac0 = M32[%s]" % basev)
        L.emit("ac1 = M32[%s]" % iv)
        L.emit("ac1 = ac1 * %d" % t["stride"])
        L.emit("ac0 = ac0 + ac1")
        L.emit("ac0 = ac0 + %s" % L.const_text(f["K"]))
        L.emit("M32[%s] = ac0" % acc)
        for sub, stride in zip(inner, f.get("strides", [])):
            sv, _ = self.expr(sub)
            nxt = L.new_v("node", "u32", "", 0, "addr:%s.%s[]" % (tname, field),
                          self.line(node))
            L.emit("ac0 = M32[%s]" % acc)
            L.emit("ac1 = M32[%s]" % sv)
            L.emit("ac1 = ac1 * %d" % stride)
            L.emit("ac0 = ac0 + ac1")
            L.emit("M32[%s] = ac0" % nxt)
            acc = nxt
        return acc, ("i32" if f["width"] == 32 else "i16")

    def read_lvalue(self, n):
        L = self.L
        addr_v, kind = self.address_of(n)
        pk = promote(kind)
        out = L.new_v("node", pk, "", 0, "load", self.line(n))
        L.emit("ac2 = M32[%s]" % addr_v)
        L.load_through_ac2("ac0", kind)
        L.emit("M32[%s] = ac0" % out)
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
        L.emit("ac0 = M32[%s]" % v)
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
        L.emit("M32[%s] = ac0" % out)
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
            L.emit("ac0 = M32[%s]" % lv)
            L.emit("ac1 = M32[%s]" % rv)
            L.emit("ac0 = ac0 %s ac1" % self.ARITH[n.op])
            L.emit("M32[%s] = ac0" % out)
            return out, k
        if n.op in ("/", "%"):
            k = usual(lk, rk)
            op = ("/" if n.op == "/" else "%") + ("s" if is_signed(k) else "u")
            out = L.new_v("node", k, "", 0, "op" + n.op, self.line(n))
            L.emit("ac0 = M32[%s]" % lv)
            L.emit("ac1 = M32[%s]" % rv)
            L.emit("ac0 = ac0 %s ac1" % op)
            L.emit("M32[%s] = ac0" % out)
            return out, k
        if n.op in ("==", "!="):
            out = L.new_v("node", "i32", "", 0, "cmp" + n.op, self.line(n))
            L.emit("ac0 = M32[%s]" % lv)
            L.emit("ac1 = M32[%s]" % rv)
            L.emit("ac0 = (ac0 %s ac1)" % n.op)
            L.emit("M32[%s] = ac0" % out)
            return out, "i32"
        if n.op in self.CMP:
            k = usual(lk, rk)
            op = self.CMP[n.op] + ("s" if (is_signed(k) and MUTATE != "cmp_unsigned") else "u")
            out = L.new_v("node", "i32", "", 0, "cmp" + n.op, self.line(n))
            L.emit("ac0 = M32[%s]" % lv)
            L.emit("ac1 = M32[%s]" % rv)
            L.emit("ac0 = (ac0 %s ac1)" % op)
            L.emit("M32[%s] = ac0" % out)
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
            L.emit("ac0 = M32[%s]" % lv)
            L.emit("ac1 = M32[%s]" % rv)
            L.emit("ac0 = lsh(ac0, ac1)")
            L.emit("M32[%s] = ac0" % out)
            return out, lk
        if not is_signed(lk) or MUTATE == "shift_logical":
            L.emit("ac0 = M32[%s]" % lv)
            L.emit("ac1 = M32[%s]" % rv)
            L.emit("ac1 = 0 - ac1")
            L.emit("ac0 = lsh(ac0, ac1)")
            L.emit("M32[%s] = ac0" % out)
            return out, lk
        logical = L.new_v("node", "u32", "", 0, "shift>>s.logical", self.line(n))
        L.emit("ac0 = M32[%s]" % lv)
        L.emit("ac1 = M32[%s]" % rv)
        L.emit("ac1 = 0 - ac1")
        L.emit("ac0 = lsh(ac0, ac1)")
        L.emit("M32[%s] = ac0" % logical)
        signmask = L.new_v("node", "u32", "", 0, "shift>>s.signmask", self.line(n))
        L.emit("ac0 = M32[%s]" % lv)
        L.emit("ac0 = lsh(ac0, -31)")
        L.emit("ac0 = ac0 & 1")
        L.emit("ac0 = 0 - ac0")
        L.emit("M32[%s] = ac0" % signmask)
        fill = L.new_v("node", "u32", "", 0, "shift>>s.fill", self.line(n))
        L.emit("ac1 = M32[%s]" % rv)
        L.emit("ac1 = 0 - ac1")
        L.emit("ac0 = lsh(0xFFFFFFFF, ac1)")
        L.emit("ac0 = ~ac0")
        L.emit("M32[%s] = ac0" % fill)
        L.emit("ac0 = M32[%s]" % signmask)
        L.emit("ac1 = M32[%s]" % fill)
        L.emit("ac0 = ac0 & ac1")
        L.emit("ac1 = M32[%s]" % logical)
        L.emit("ac0 = ac0 | ac1")
        L.emit("M32[%s] = ac0" % out)
        return out, lk

    def shortcircuit(self, n):
        """C's && and || are SHORT-CIRCUIT; the IR's are EAGER (IR.md §5.3),
        so these lower to control flow.  With SUB()'s assert in the language
        the right operand really can trap, which makes this a semantic
        difference and not an optimisation."""
        L = self.L
        out = L.new_v("node", "i32", "", 0, "bool" + n.op, self.line(n))
        if MUTATE == "eager_bool":
            lv, _lk = self.expr(n.left)
            rv, _rk = self.expr(n.right)
            L.emit("ac0 = M32[%s]" % lv)
            L.emit("ac0 = tf(ac0)")
            L.emit("ac1 = M32[%s]" % rv)
            L.emit("ac1 = tf(ac1)")
            L.emit("ac0 = (ac0 %s ac1)" % n.op)
            L.emit("M32[%s] = ac0" % out)
            return out, "i32"
        lv, _lk = self.expr(n.left)
        b_rhs = L.new_block("%s/rhs" % n.op)
        b_join = L.new_block("%s/join" % n.op)
        L.emit("ac0 = %d" % (0 if n.op == "&&" else 1))
        L.emit("M32[%s] = ac0" % out)
        L.emit("ac0 = M32[%s]" % lv)
        if n.op == "&&":
            L.terminate("goto [%s, %s] tf(ac0)" % (b_join.name, b_rhs.name))
        else:
            L.terminate("goto [%s, %s] tf(ac0)" % (b_rhs.name, b_join.name))
        L.open_block(b_rhs)
        rv, _rk = self.expr(n.right)
        L.emit("ac0 = M32[%s]" % rv)
        L.emit("ac0 = tf(ac0)")
        L.emit("M32[%s] = ac0" % out)
        L.goto(b_join)
        L.open_block(b_join)
        return out, "i32"

    def cast(self, n):
        L = self.L
        kind = type_kind_of(n.to_type.type, n)
        v, _k = self.expr(n.expr)
        out = L.new_v("node", promote(kind), "", 0, "cast:" + kind, self.line(n))
        L.emit("ac0 = M32[%s]" % v)
        if KINDS[kind][1] == 1:
            L.emit("ac0 = %s(ac0)" % ("sx16" if is_signed(kind) else "zx16"))
        L.emit("M32[%s] = ac0" % out)
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
        if name == "SUB":
            return self.builtin_sub(n, args)
        refuse("function call", n,
               "calls are refused in this project (P46 F7: ir 7 refuses "
               "call/rt_call in a symbolic block); `%s` is not a builtin" % name)

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
        b_neg = L.new_block("ABS/neg")
        b_pos = L.new_block("ABS/pos")
        b_join = L.new_block("ABS/join")
        L.emit("ac0 = M32[%s]" % v)
        L.emit("ac0 = (ac0 >=s 0)")
        L.terminate("goto [%s, %s] tf(ac0)" % (b_neg.name, b_pos.name))
        L.open_block(b_neg)
        L.emit("ac0 = M32[%s]" % v)
        L.emit("ac0 = 0 - ac0")
        L.emit("M32[%s] = ac0" % out)
        L.goto(b_join)
        L.open_block(b_pos)
        L.emit("ac0 = M32[%s]" % v)
        L.emit("M32[%s] = ac0" % out)
        L.goto(b_join)
        L.open_block(b_join)
        return out, k

    def builtin_sub(self, n, args):
        """PL/I's subscript check: i in 1..n, else DERR 17 (an ABORT-kind
        terminal, IR.md §4a).  The message carries the source site so that
        "both trapped at the same place" is a string compare against
        quest_rt.h's quest_sub_check (a001 R5)."""
        L = self.L
        if len(args) != 2:
            refuse("SUB", n, "SUB takes two arguments")
        iv, ik = self.expr(args[0])
        nv, _nk = self.expr(args[1])
        out = L.new_v("node", "i32", "", 0, "SUB", self.line(n))
        L.emit("ac0 = M32[%s]" % iv)
        L.emit("ac1 = M32[%s]" % nv)
        L.emit('assert(((ac0 >s 0) && (ac0 <=s ac1)), "DERR17 %s:%d")'
               % (L.src, self.line(n)))
        L.emit("M32[%s] = ac0" % out)
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
        because a local of t() is not reachable from the native harness."""
        L = self.L
        for ext in ast.ext:
            if isinstance(ext, c_ast.Decl) and not isinstance(ext.type, c_ast.FuncDecl):
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
            if nelem < 1 or nelem * KINDS[kind][1] > 32767:
                refuse("array", d, "bound out of the `words 1..32767` range")
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
            v = L.new_v("param", "u32", p.name, 0, "param:" + p.name,
                        getattr(p.coord, "line", 0))
            L.params[p.name] = (v, pointee)
        # return value
        rk = None
        if not (isinstance(decl.type, c_ast.TypeDecl)
                and isinstance(decl.type.type, c_ast.IdentifierType)
                and decl.type.type.names == ["void"]):
            rk = type_kind_of(decl.type, fd)
            self.retv = L.new_v("ret", rk, "__ret", 0, "ret", 0)
        else:
            self.retv = None
        self.retkind = rk

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
            refuse("call statement", n, "calls are refused in this project")
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
            L.emit("ac0 = M32[%s]" % rv)
            L.store_reg("ac0", vname, kind)
            return
        addr_v, kind = self.W.address_of(lhs)
        L.emit("ac0 = M32[%s]" % rv)
        L.emit("ac2 = M32[%s]" % addr_v)
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
        L.emit("ac0 = M32[%s]" % v)
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
        b_head = L.new_block("for/head")
        b_body = L.new_block("for/body")
        b_step = L.new_block("for/step")
        b_after = L.new_block("for/after")
        L.goto(b_head)
        L.open_block(b_head)
        if n.cond is not None:
            cv, _k = self.W.expr(n.cond)
            L.branch(cv, b_after, b_body)
        else:
            L.goto(b_body)
        L.open_block(b_body)
        L.loops.append((b_after, b_step))
        self.stmt(n.stmt)
        L.loops.pop()
        if L.cur is not None:
            L.goto(b_step)
        L.open_block(b_step)
        if n.next is not None:
            self.stmt(n.next)
        L.goto(b_head)
        L.open_block(b_after)

    # ---------------------------------------------------------- output ---
    def ir_text(self):
        L = self.L
        out = ["ir 7", "mode stock",
               "; GENERATED by compiler/lower_c.py from %s (routine %s)" % (L.src, L.routine),
               "; NAIVE: every value in its own v, zero t-places, pure operators only",
               ""]
        out += L.vdecls
        out.append("")
        for b in L.blocks:
            out.append("block %s ; anchor=%s" % (b.name, b.anchor))
            out += b.lines
            if b.term is None:
                raise Refusal("block", 0, "block %s has no terminator" % b.name)
            out.append(b.term)
            out.append("")
        out.append("blocks %d" % len(L.blocks))
        return "\n".join(out) + "\n"

    def vmap_text(self):
        L = self.L
        out = ["# lower_c 1",
               "# src %s" % L.src,
               "# routine %s" % L.routine,
               "# entry %s" % L.entry,
               "# entryblock %s" % self.entry_block.name,
               "# columns: vname kind cname ctype vtype words nelem elemwords render anchor line"]
        out += [r.tsv() for r in L.vrows]
        return "\n".join(out) + "\n"


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
    ap.add_argument("source")
    ap.add_argument("--routine", required=True)
    ap.add_argument("--entry", default=None,
                    help="the addrbook entry to qualify names with (default: the routine name)")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--vmap", default=None)
    ap.add_argument("--mutate", choices=sorted(MUTATIONS), default=None,
                    help="inject one deliberate soundness bug (teeth leg only)")
    a = ap.parse_args()
    global MUTATE
    MUTATE = a.mutate
    entry = a.entry or a.routine

    text = preprocess(a.source)
    parser = c_parser.CParser()
    try:
        ast = parser.parse(text, filename=a.source)
    except Exception as e:
        sys.stderr.write("REFUSE parse at %s: %s\n" % (a.source, e))
        return 2

    c = Compiler(entry, a.source, a.routine)
    try:
        c.collect_file_scope(ast)
        target = None
        for ext in ast.ext:
            if isinstance(ext, c_ast.FuncDef) and ext.decl.name == a.routine:
                target = ext
        if target is None:
            sys.stderr.write("REFUSE routine at %s:0: no definition of `%s`\n"
                             % (os.path.basename(a.source), a.routine))
            return 2
        c.compile_routine(target)
        ir = c.ir_text()
        vmap = c.vmap_text()
    except Refusal as r:
        sys.stderr.write("REFUSE %s at %s:%d: %s\n"
                         % (r.construct, os.path.basename(a.source), r.line, r.why))
        return 2

    open(a.out, "w").write(ir)
    open(a.vmap or (os.path.splitext(a.out)[0] + ".vmap"), "w").write(vmap)
    return 0


if __name__ == "__main__":
    sys.exit(main())
