#!/usr/bin/env python3
"""cgen.py — the differential tester's program generator (Project 48, Stage A).

Emits a random C program IN THE v1 SUBSET plus the native harness that makes
its final state observable.  Nothing here knows anything about the compiler:
it produces C text, and gcc supplies the semantics (a001's correction to
q001 §2.5 item 10 — the generator is not a second implementation of C, so a
shared misunderstanding cannot hide a bug, it can only fail to generate one).

Contract with the rest of the tester
------------------------------------
  <dir>/prog.c        the subset routine + its file-scope storage.  This is
                      the ONLY file compiler/lower_c.py ever sees.
  <dir>/prog_main.c   #includes prog.c and prints every observable.  Not in
                      the subset; the compiler never reads it.
  <dir>/meta.json     the seed, the class, the observable list, and the
                      CONSTRUCT CENSUS for this program (a001: the census is
                      the central instrument, so every program carries one).

Observables are FILE-SCOPE variables, declared without initialisers and
assigned explicitly in the routine's first statements.  File scope because a
local of t() is not reachable from the harness; without initialisers because
a C file-scope initialiser runs once at load while an IR `v` is just memory
(0x76 pages read zero, IR.md §5.10.6) — making the initialisation ordinary
statements removes the question instead of answering it.

Rendering: every observable prints as its RAW BIT PATTERN zero-extended to
32 bits, `name=%08X` (arrays `name[i]=%08X`), so the two sides compare as
text with no width or sign convention to agree on separately.

UB is engineered out at generation time (q001 §2.2), because where a program
is UB gcc is not an oracle:
  signed overflow      -fwrapv on the native side (the driver's business)
  / or % by zero       every divisor is ((e & 0x7FFFFFFF) | 1)
  INT_MIN / -1         the same guard: a divisor in 1..0x7FFFFFFF is never -1
  shift count >= 32    every count is (e & 31)
  signed <<            the left operand of << is always cast to uint32_t
  uninitialised read   every variable is assigned before any read
  eval order           the subset has no side-effecting subexpressions at all
"""

import argparse
import json
import os
import random

# ---------------------------------------------------------------- types ----

# (C spelling, words, how the harness renders one element)
TYPES = {
    "i16": ("int16_t",  1, "w16"),
    "u16": ("uint16_t", 1, "w16"),
    "i32": ("int32_t",  2, "w32"),
    "u32": ("uint32_t", 2, "w32"),
}
SCALAR_KINDS = ["i16", "u16", "i32", "u32"]

# The constant pool is BIASED, not uniform (q001 §2.2): uniform 32-bit values
# almost never land on a 16-bit boundary, and the narrowing rules are where
# the bugs are.  Written so every entry has an unambiguous C type: no bare
# 2147483648, no unsuffixed value that silently becomes long.
CONSTANTS = (
    ["0", "1", "-1", "2", "-2", "4", "5", "-5", "6", "9", "10", "11", "22"] * 3
    + ["127", "128", "255", "256", "-128", "-255"]
    + ["32766", "32767", "-32767", "-32768", "65534", "65535", "65536"] * 2
    + ["2147483647", "(-2147483647-1)", "-1073741824", "1073741824"]
    + ["1000003", "305419896", "-305419896", "686", "-611", "-589", "-588"]
    + ["4294967295u", "2863311530u", "65535u", "32768u", "0u", "1u"]
)

# Constants the loader refuses as literals anywhere (IR.md §5.1): the
# synthetic 0x76/0x77 spaces are loader-assigned and never authorable.  None
# of the pool above lands there; assert it rather than trust it.
for _c in CONSTANTS:
    _t = _c.strip("u()").lstrip("-")
    if _t.isdigit() and 0x76000000 <= int(_t) < 0x78000000:
        raise AssertionError("constant pool enters the refused 0x76/0x77 range: " + _c)

ARITH_OPS = ["+", "-", "*", "&", "|", "^"]
CMP_OPS = ["<", "<=", ">", ">=", "==", "!="]


class Gen:
    def __init__(self, seed, klass):
        self.rng = random.Random(seed)
        self.seed = seed
        self.klass = klass            # "A" flattened, "B" nested, "C" subscript-heavy
        self.decls = []               # (cname, kind, nelem)  nelem 0 == scalar
        self.scalars = []             # cnames assignable and readable
        self.arrays = []              # (cname, kind, nelem)
        self.loop_counters = []       # cnames that loops own; never assigned by a body
        self.lines = []
        self.indent = 1
        self.label_n = 0
        self.census = {}
        self.depth_limit = 1 if klass == "A" else 3

    # ------------------------------------------------------------ census --
    def note(self, feature):
        self.census[feature] = self.census.get(feature, 0) + 1

    def emit(self, text):
        self.lines.append("    " * self.indent + text)

    # ------------------------------------------------------- declarations --
    def build_storage(self):
        r = self.rng
        # chk is always first and always u32: the trace (q001 §2.3)
        self.decls.append(("chk", "u32", 0))
        self.scalars.append("chk")
        n_scalar = r.randint(4, 9)
        for i in range(n_scalar):
            kind = r.choice(SCALAR_KINDS)
            name = "s%d" % i
            self.decls.append((name, kind, 0))
            self.scalars.append(name)
        n_array = r.randint(1, 3) if self.klass != "A" else r.randint(0, 2)
        for i in range(n_array):
            kind = r.choice(SCALAR_KINDS)
            nelem = r.choice([4, 8, 16])     # powers of two: `& (n-1)` is in range
            name = "a%d" % i
            self.decls.append((name, kind, nelem))
            self.arrays.append((name, kind, nelem))
        for i in range(4):                   # loop counters and goto budgets
            name = "lc%d" % i
            self.decls.append((name, "i32", 0))
            self.loop_counters.append(name)

    def kind_of(self, cname):
        for n, k, ne in self.decls:
            if n == cname:
                return k
        raise KeyError(cname)

    # -------------------------------------------------------- expressions --
    def leaf(self):
        r = self.rng
        pick = r.random()
        if pick < 0.42:
            self.note("leaf.var")
            return r.choice(self.scalars)
        if pick < 0.55 and self.arrays:
            name, kind, nelem = r.choice(self.arrays)
            self.note("leaf.array_read")
            return "%s[%s]" % (name, self.index_expr(nelem))
        if pick < 0.60 and self.loop_counters:
            self.note("leaf.loopvar")
            return r.choice(self.loop_counters)
        self.note("leaf.const")
        return r.choice(CONSTANTS)

    def index_expr(self, nelem):
        """An array index that is ALWAYS in range: masked to the power-of-two
        size, or a SUB() check on a 1-based index (class C's speciality)."""
        r = self.rng
        if self.klass == "C" and r.random() < 0.45:
            if r.random() < 0.75:
                self.note("index.sub_inrange")
                return "SUB(1 + (%s & %d), %d) - 1" % (self.leaf(), nelem - 1, nelem)
            self.note("index.sub_unguarded")     # MAY trap: that is the point
            return "SUB(1 + ((%s) & 15), %d) - 1" % (self.leaf(), nelem)
        self.note("index.masked")
        return "(%s) & %d" % (self.expr(0), nelem - 1)

    def divisor(self):
        """Never 0 and never -1, so neither /0 nor INT_MIN/-1 is reachable."""
        return "(((%s) & 0x7FFFFFFF) | 1)" % self.expr(max(0, self.depth_limit - 2))

    def expr(self, depth):
        r = self.rng
        if depth <= 0:
            return self.leaf()
        pick = r.random()
        sub = depth - 1
        if pick < 0.34:
            op = r.choice(ARITH_OPS)
            self.note("op." + op)
            return "(%s %s %s)" % (self.expr(sub), op, self.expr(sub))
        if pick < 0.42:
            op = r.choice(["/", "%"])
            signed = r.random() < 0.5
            self.note("op." + op + (".s" if signed else ".u"))
            lhs = "(int32_t)(%s)" % self.expr(sub) if signed else "(uint32_t)(%s)" % self.expr(sub)
            return "(%s %s %s)" % (lhs, op, self.divisor())
        if pick < 0.50:
            form = r.choice(["shl", "shr_u", "shr_s"])
            self.note("op." + form)
            cast = "int32_t" if form == "shr_s" else "uint32_t"
            sym = "<<" if form == "shl" else ">>"
            return "((%s)(%s) %s ((%s) & 31))" % (cast, self.expr(sub), sym, self.expr(sub))
        if pick < 0.62:
            t = r.choice(["int16_t", "uint16_t", "int32_t", "uint32_t"])
            self.note("cast." + t)
            return "((%s)(%s))" % (t, self.expr(sub))
        if pick < 0.70:
            op = r.choice(["-", "~"])
            self.note("unary." + op)
            return "(%s(%s))" % (op, self.expr(sub))
        if pick < 0.80:
            op = r.choice(CMP_OPS)
            self.note("cmp." + op)
            return "(%s %s %s)" % (self.expr(sub), op, self.expr(sub))
        if pick < 0.87:
            op = r.choice(["&&", "||"])
            self.note("bool." + op)
            return "(%s %s %s)" % (self.expr(sub), op, self.expr(sub))
        if pick < 0.92:
            self.note("unary.not")
            return "(!(%s))" % self.expr(sub)
        self.note("builtin.ABS")
        return "ABS(%s)" % self.expr(sub)

    def cond(self):
        r = self.rng
        if r.random() < 0.7:
            self.note("cond.cmp")
            return "(%s %s %s)" % (self.expr(self.depth_limit - 1),
                                   r.choice(CMP_OPS),
                                   self.expr(self.depth_limit - 1))
        self.note("cond.value")
        return self.expr(self.depth_limit)

    # --------------------------------------------------------- statements --
    def assign(self):
        r = self.rng
        rhs = self.expr(self.depth_limit)
        if self.arrays and r.random() < 0.3:
            name, kind, nelem = r.choice(self.arrays)
            self.note("stmt.assign_array")
            target_kind = kind
            lhs = "%s[%s]" % (name, self.index_expr(nelem))
        else:
            name = r.choice([s for s in self.scalars if s != "chk"])
            target_kind = self.kind_of(name)
            self.note("stmt.assign_scalar")
            lhs = name
        # Half the narrowing assignments carry an explicit cast and half do
        # not, so BOTH spellings are in the corpus: quest_rt.h's P36 prose
        # made a bare `int16 = int32` a refusal, and this compiler follows C
        # instead (trunc16 on the store).  Generating both is how that choice
        # gets tested rather than assumed.
        if TYPES[target_kind][1] == 1 and r.random() < 0.5:
            self.note("narrow.explicit")
            rhs = "(%s)(%s)" % (TYPES[target_kind][0], rhs)
        elif TYPES[target_kind][1] == 1:
            self.note("narrow.implicit")
        self.emit("%s = %s;" % (lhs, rhs))

    def chk_update(self):
        self.note("stmt.chk")
        self.emit("chk = chk * 1000003u + (uint32_t)(%s);" % self.leaf())

    def incdec(self):
        r = self.rng
        name = r.choice([s for s in self.scalars if s != "chk"])
        op = r.choice(["++", "--"])
        self.note("stmt.incdec")
        self.emit("%s%s;" % (name, op))

    def stmt_if(self, budget, in_loop, depth):
        """`depth` MUST be threaded through: a loop nested inside an `if`
        inside a loop is still nested, and giving it the enclosing loop's
        counter resets that counter and the outer loop never ends.  Found by
        the driver's own timeout at seed 29 class A."""
        r = self.rng
        self.note("stmt.if")
        self.emit("if (%s) {" % self.cond())
        self.indent += 1
        self.chk_update()
        self.block(max(1, budget // 2), in_loop, depth)
        self.indent -= 1
        if r.random() < 0.6:
            self.note("stmt.else")
            self.emit("} else {")
            self.indent += 1
            self.chk_update()
            self.block(max(1, budget // 2), in_loop, depth)
            self.indent -= 1
        self.emit("}")

    def stmt_loop(self, budget, depth):
        """Termination is STRUCTURAL, not probabilistic: the loop owns a
        counter that no body may assign (loop counters are not in
        self.scalars), the trip count is a literal, and the `while` form
        increments at the TOP of the body so that a `continue` in the body
        cannot skip the increment.  A generator that can emit an infinite
        loop turns every timeout into a question."""
        r = self.rng
        lc = self.loop_counters[min(depth, len(self.loop_counters) - 2)]
        trips = r.randint(0, 5)
        is_for = r.random() < 0.6
        if is_for:
            self.note("stmt.for")
            self.emit("for (%s = 0; %s < %d; %s++) {" % (lc, lc, trips, lc))
            self.indent += 1
        else:
            self.note("stmt.while")
            self.emit("%s = 0;" % lc)
            self.emit("while (%s < %d) {" % (lc, trips))
            self.indent += 1
            self.emit("%s++;" % lc)      # before the body: continue-safe
        self.chk_update()
        self.block(max(1, budget // 2), in_loop=True, depth=depth + 1)
        self.indent -= 1
        self.emit("}")

    def stmt_goto_forward(self):
        self.note("stmt.goto_forward")
        n = self.label_n
        self.label_n += 1
        self.emit("if (%s) goto L%d;" % (self.cond(), n))
        self.chk_update()
        self.emit("L%d: ;" % n)

    def stmt_goto_backward(self, depth):
        """A BOUNDED backward goto: the counter makes termination structural."""
        self.note("stmt.goto_backward")
        n = self.label_n
        self.label_n += 1
        lc = self.loop_counters[len(self.loop_counters) - 1]
        self.emit("%s = 0;" % lc)
        self.emit("L%d: ;" % n)
        self.chk_update()
        self.emit("%s++;" % lc)
        self.emit("if (%s < 3) goto L%d;" % (lc, n))

    def block(self, budget, in_loop=False, depth=0):
        r = self.rng
        n = max(1, r.randint(1, budget))
        for _ in range(n):
            pick = r.random()
            if pick < 0.44:
                self.assign()
            elif pick < 0.52:
                self.chk_update()
            elif pick < 0.58:
                self.incdec()
            elif pick < 0.72 and budget > 1:
                self.stmt_if(budget, in_loop, depth)
            elif pick < 0.84 and budget > 2 and depth < 2:
                self.stmt_loop(budget, depth)
            elif pick < 0.88 and in_loop:
                kw = r.choice(["break", "continue"])
                self.note("stmt." + kw)
                self.emit("if (%s) %s;" % (self.cond(), kw))
            elif pick < 0.94 and budget > 1:
                self.stmt_goto_forward()
            elif depth < 2 and budget > 2:
                self.stmt_goto_backward(depth)
            else:
                self.assign()

    # ------------------------------------------------------------ output --
    def observables(self):
        out = []
        for name, kind, nelem in self.decls:
            ctype, words, render = TYPES[kind]
            out.append({"name": name, "kind": kind, "ctype": ctype,
                        "nelem": nelem, "render": render})
        return out

    def prog_c(self):
        head = ['/* GENERATED by compiler/difftest/cgen.py — seed %d, class %s.',
                ' * In the P48 v1 C subset.  Do not edit; regenerate from the seed. */',
                '#include "quest_rt.h"',
                '']
        head[0] = head[0] % (self.seed, self.klass)
        body = []
        for name, kind, nelem in self.decls:
            ctype = TYPES[kind][0]
            if nelem:
                body.append("%-9s %s[%d];" % (ctype, name, nelem))
            else:
                body.append("%-9s %s;" % (ctype, name))
        body.append("")
        body.append("void t(void)")
        body.append("{")
        return "\n".join(head + body + self.lines + ["}", ""])

    def init_lines(self):
        """Explicit assignment of every observable, before any read."""
        r = self.rng
        for name, kind, nelem in self.decls:
            if nelem:
                for i in range(nelem):
                    self.emit("%s[%d] = %s;" % (name, i, r.choice(CONSTANTS)))
            else:
                self.emit("%s = %s;" % (name, r.choice(CONSTANTS)))

    def prog_main_c(self):
        out = ['/* GENERATED by compiler/difftest/cgen.py — the NATIVE harness.',
               ' * Not in the subset; compiler/lower_c.py never reads this file. */',
               '#include <stdio.h>',
               '#include <stdint.h>',
               '#include "prog.c"',
               '',
               'int main(void)',
               '{',
               '    t();']
        for name, kind, nelem in self.decls:
            render = TYPES[kind][2]
            cast = "(uint32_t)(uint16_t)" if render == "w16" else "(uint32_t)"
            if nelem:
                for i in range(nelem):
                    out.append('    printf("%s[%d]=%%08X\\n", %s%s[%d]);'
                               % (name, i, cast, name, i))
            else:
                out.append('    printf("%s=%%08X\\n", %s%s);' % (name, cast, name))
        out += ['    fflush(stdout);', '    return 0;', '}', '']
        return "\n".join(out)


def generate(seed, klass, outdir):
    g = Gen(seed, klass)
    g.build_storage()
    g.init_lines()
    g.emit("")
    budget = {"A": 14, "B": 9, "C": 9}[klass]
    g.block(budget)
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "prog.c"), "w") as f:
        f.write(g.prog_c())
    with open(os.path.join(outdir, "prog_main.c"), "w") as f:
        f.write(g.prog_main_c())
    meta = {"seed": seed, "class": klass,
            "observables": g.observables(), "census": g.census}
    with open(os.path.join(outdir, "meta.json"), "w") as f:
        json.dump(meta, f, indent=1)
    return meta


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, required=True)
    ap.add_argument("--class", dest="klass", choices=["A", "B", "C"], default="B")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    meta = generate(a.seed, a.klass, a.out)
    print("%s seed=%d class=%s observables=%d features=%d"
          % (a.out, a.seed, a.klass, len(meta["observables"]), len(meta["census"])))


if __name__ == "__main__":
    main()
