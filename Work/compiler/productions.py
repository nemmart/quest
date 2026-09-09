"""P42 — the production table, the reduction ledger, and the span check.

Stage 2 lands this in PARALLEL with the existing emit methods.  A production
fires (`Translator.fire`) around the code that already emits; firing records a
reduction address and the choices taken, and enforces the span declaration.
It emits nothing itself, so Stage 2 is behaviour-preserving by construction
rather than by argument -- the four matched routines cannot move.

Stage 3 flips `ported` per production and moves the template in here.

The design of record is docs/Project42/PRODUCTIONS.md, ruled on Sep 9 2026.
Four things it fixed, and this module is where three of them are enforced:

  TILES WITH DECLARED SPANS.  A production declares its root node type and the
  exact descendant positions it consumes.  Consumed descendants are NOT
  separately reduced and receive NO reduction address.  Reduction order is
  post-order over TILE ROOTS.  **Emitting for an undeclared node is a hard
  error** -- the user's ruling was explicit that without this clause "tile"
  collapses back into "arbitrary method", so it is a check with teeth and not
  a convention.

  CHOICE KINDS are P43's four and no others: reg, slot, order, spelling.  A
  production may only record a choice of a kind it declares.

  INHERITED ATTRIBUTES ARE DECLARED, NOT INHERITED SILENTLY.  D4 (did the DG
  generator pass a target register DOWN, or only combine what came UP?) is
  open.  The current translator has already answered it by construction --
  want_reg and avoid are threaded down through value()/binop()/
  element_address()/bit_base_reg() -- so porting them as-is would encode
  "down" structurally and leave P44's census nothing to decide.  Every
  production therefore names its inherited attributes, and `census_targets()`
  returns the seven that would need one if D4 answers "down".
"""

CHOICE_KINDS = ("reg", "slot", "order", "spelling")


class SpanViolation(Exception):
    """A production emitted for, or fired on, a node it did not declare."""


class Production:
    def __init__(self, name, group, root, span=(), inherited=(), choices=(),
                 rules=(), classes=None, ported=False, note=""):
        bad = [c for c in choices if c not in CHOICE_KINDS]
        if bad:
            raise ValueError("%s: choice kind(s) %r outside P43's four" % (name, bad))
        self.name = name
        self.group = group            # stmt | addr | expr | cond | plumb
        self.root = root              # the AST shape it roots (documentation)
        self.span = tuple(span)       # descendant POSITIONS consumed
        self.inherited = tuple(inherited)
        self.choices = tuple(choices)
        self.rules = tuple(rules)
        self.classes = classes        # legal register set, where a class applies
        self.ported = ported
        self.note = note

    def __repr__(self):
        return "<production %s>" % self.name


# --------------------------------------------------------------------------
# The table.  37 productions, as ruled at the P42 gate.
#
# `classes` is the legal set a `reg` choice may draw from.  R41' (user ruling,
# Sep 9 2026) is published HERE rather than as a rule about register loads:
# three addressing productions restrict to {ac2, ac3}; bit_base DENIES the
# constraint and takes all four.  Same physical operation -- load a record
# base -- two legal sets, because the productions are FOR different things.
# A FIFTH class appearing is evidence AGAINST the production framing and is
# reportable; it must not be introduced quietly to make something match.
# --------------------------------------------------------------------------

BASE_CLASS = ("ac2", "ac3")          # R41' -- the addressing role
ALL_REGS = ("ac0", "ac1", "ac2", "ac3")
VALUE_CLASS = ("ac0", "ac1")         # R21c' -- the loop/value register class

P = []


def _p(*a, **k):
    P.append(Production(*a, **k))


# --- statements (13) -------------------------------------------------------
_p("assign_local", "stmt", "Assignment '=' with a scalar lvalue",
   span=("rvalue",), inherited=(), choices=("reg",), rules=("R16", "R16b"))
_p("assign_indexed", "stmt", "Assignment to T[i].f[r][c]",
   span=("lvalue.subscripts", "rvalue"), choices=("slot", "order"),
   rules=("R23",), note="address first, then value, then the store")
_p("assign_rt", "stmt", "Assignment whose rvalue is a runtime call",
   span=("rvalue",), rules=("RTConventions",), note="result in ac0")
_p("assign_uplevel", "stmt", "Assignment to UP()/UPARG()",
   span=("lvalue", "rvalue"), choices=("reg",), rules=("R43", "R44"),
   classes=BASE_CLASS)
_p("if_oneword", "stmt", "If with a one-instruction consequent",
   span=("iftrue",), choices=("spelling",), rules=("R13", "R13c", "R8a"))
_p("if_multi", "stmt", "If with a multi-statement or two-armed body",
   choices=("order",), rules=("R13b", "R19"))
_p("do_loop", "stmt", "For -- the XNDO idiom",
   span=("init", "cond", "next"), choices=("reg", "slot"),
   rules=("R21", "R21c'", "R21e'", "R22", "R36"), classes=VALUE_CLASS,
   note="one instruction, five IR statements")
_p("goto", "stmt", "Goto")
_p("return_void", "stmt", "Return with no value", rules=("R13",))
_p("return_value", "stmt", "Return with a value",
   span=("expr",), choices=("reg",), rules=("R35",))
_p("rt_call_stmt", "stmt", "A runtime call statement NAME$n(...)",
   span=("args",), choices=("slot", "order"), rules=("R18",))
_p("game_call_stmt", "stmt", "A game-routine call",
   span=("args",), choices=("order",), rules=("R40",))
_p("bit_stmt", "stmt", "BIT_SET / BIT_CLR / BIT_PUT",
   span=("word", "nbit"), choices=("reg",), rules=("R29",))

# --- addressing and references (11) ---------------------------------------
_p("element_address", "addr", "ArrayRef -- the element address base + i*stride",
   span=("subscript", "stride", "base"),
   inherited=("want_reg",),
   choices=("reg", "slot", "order", "spelling"),
   rules=("R4", "R5", "R6", "R9", "R9a", "R10", "R11", "R31", "R36", "R36a"),
   classes=BASE_CLASS,
   note="SEVEN paths; FIVE never evaluate the subscript at all -- the tile "
        "that settled the per-node question (QUEST.1 7015c611)")
_p("field_direct", "addr", "StructRef '->'",
   inherited=("avoid",), choices=("reg",), rules=("R7", "R41'"),
   classes=BASE_CLASS)
_p("field_element", "addr", "StructRef '.' over an ArrayRef",
   span=("name",), rules=("R4",), note="delegates the address to element_address")
_p("bit_address", "addr", "the bit address 16*scaled + (16*K + n)",
   span=("subscript", "stride", "disp"), choices=("reg", "slot"),
   rules=("R26", "R27"))
_p("bit_base", "addr", "the record base of a BIT reference",
   inherited=("avoid",), choices=("reg",), rules=("R28",), classes=ALL_REGS,
   note="DENIES R41'. Plain R7 over all four; the SD_PTR census puts it in "
        "{ac0, ac1} 302 times out of 306. The falsification test: if a FIFTH "
        "class is ever needed, the production framing is wrong -- report it, "
        "do not introduce it quietly.")
_p("link_load", "addr", "the static link",
   choices=("reg",), rules=("R42", "R43", "R44", "R45"), classes=BASE_CLASS)
_p("uplevel_ref", "addr", "UP() read", span=("link",), choices=("reg",),
   rules=("R43",))
_p("uplevel_arg_ref", "addr", "UPARG() dereference", span=("link",),
   choices=("reg",), rules=("R44",))
_p("scalar_ref", "addr", "ID / *param -- local, static, argument",
   inherited=("want_reg", "avoid"), choices=("reg",), rules=("R7", "R8"))
_p("marker_ref", "addr", "the two-arity marker word", rules=("R12",),
   note="fixed wp(ac3,-9); no choice")
_p("address_of", "addr", "UnaryOp '&'", span=("expr",), choices=("spelling",),
   rules=("R32",), note="wp vs bp")

# --- expressions (9) -------------------------------------------------------
_p("binop_reg_reg", "expr", "BinaryOp, both operands in registers",
   inherited=("avoid",), choices=("reg", "order"), rules=("R20",))
_p("binop_reg_mem", "expr", "BinaryOp with a 32-bit memory right operand",
   inherited=("avoid",), choices=("spelling",), rules=("R15",),
   note="direct memory operand vs load-first")
_p("binop_const_inc", "expr", "BinaryOp + 1", choices=("spelling",),
   rules=("R15",), note="WINC vs WNADI -- P43's worked example")
_p("binop_const_addi", "expr", "BinaryOp +/- a 16-bit constant",
   choices=("spelling",), rules=("R15",))
_p("binop_const_scale", "expr", "BinaryOp * or / a constant",
   inherited=("avoid",), choices=("reg",), rules=("R11", "R16"),
   note="D4 census target: the constant's register")
_p("const_materialise", "expr", "Constant",
   inherited=("avoid",), choices=("reg", "spelling"), rules=("R37", "R29"),
   note="D4's ACTUAL site -- 686 to ac2 twice, ac1 once. Expected to "
        "accumulate the most choices in P43.")
_p("convert", "expr", "cvwn / sx16 / trunc16", span=("expr",),
   rules=("R16",), note="explicit in the source (P36 ruling 1); no choice")
_p("abs_builtin", "expr", "ABS()", span=("expr",), choices=("reg",))
_p("bit_value", "expr", "BIT() used as a value", choices=("reg",), rules=("R29",))

# --- conditions and plumbing (4) ------------------------------------------
_p("condition_cmp", "cond", "a comparison in a test",
   span=("left", "right"), choices=("order", "spelling"),
   rules=("R14", "R14b"))
_p("condition_bit", "cond", "a BIT reference in a test", span=("expr",),
   choices=("reg",), rules=("R29",))
_p("temp_place", "plumb", "a CSE temp store or reload", choices=("slot",),
   rules=("R3", "R3b'"))
_p("prologue_epilogue", "plumb", "the function's frame", choices=("slot",),
   rules=("R1", "R2", "R34"))

TABLE = {p.name: p for p in P}
assert len(TABLE) == len(P), "duplicate production name"


def census_targets():
    """The productions that would need an inherited attribute if D4 answers
    'down'.  P44's census decides it on data; P42 only names them."""
    return [p.name for p in P if p.inherited]


# --------------------------------------------------------------------------
# The ledger
# --------------------------------------------------------------------------

class Firing:
    __slots__ = ("prod", "node_id", "shape", "choices", "addr", "stmt")

    def __init__(self, prod, node_id, shape, stmt):
        self.prod = prod
        self.node_id = node_id
        self.shape = shape
        self.stmt = stmt
        self.choices = []
        self.addr = None


class Ledger:
    """Records every firing in REDUCTION order -- that is, in order of
    COMPLETION, post-order over tile roots.  The address `<production>#<n>`
    counts that production's own firings, so it survives edits elsewhere in
    the routine.  This is what P43's choice files key on and what P44
    censuses."""

    def __init__(self):
        self.firings = []
        self.counts = {}
        self.stack = []
        self.consumed = {}        # id(node) -> the production that consumed it
        self.seen_roots = set()
        self.violations = []
        self.orphans = []         # emissions belonging to NO production

    # -- firing ------------------------------------------------------------
    def begin(self, name, node_id, shape, stmt):
        prod = TABLE.get(name)
        if prod is None:
            raise SpanViolation("no such production: %s" % name)
        owner = self.consumed.get(node_id)
        if owner is not None and owner != name:
            self.violations.append(
                "%s fired on a node already consumed by %s -- a consumed "
                "descendant must not be separately reduced" % (name, owner))
        f = Firing(name, node_id, shape, stmt)
        self.stack.append(f)
        return f

    def end(self, f):
        assert self.stack and self.stack[-1] is f
        self.stack.pop()
        n = self.counts.get(f.prod, 0)
        self.counts[f.prod] = n + 1
        f.addr = "%s#%d" % (f.prod, n)
        self.firings.append(f)
        return f.addr

    def orphan_emission(self, site, n, why):
        """An emission that belongs to no production in the table.

        P42 expected one (pre-registered: R36 would be multi-homed or an
        orphan).  Recording them is what keeps the table honest -- an
        unattributed emission is invisible to both the reduction ledger and
        P43's choice files, so a rule living there cannot be addressed, cannot
        be overridden, and cannot be censused.  This is NOT a violation: an
        orphan is a FINDING about the production set, and Stage 4's rule map
        is where it is reported."""
        self.orphans.append((site, n, why))

    def consume(self, name, node_ids):
        for nid in node_ids:
            if nid is None:
                continue
            self.consumed[nid] = name

    # -- choices -----------------------------------------------------------
    def choice(self, kind, value, witness=None):
        """Record a choice the firing production took.  A production may only
        record a kind it DECLARES -- that is what keeps the choice file an
        honest measure of freedom rather than a place to write anything."""
        if not self.stack:
            return
        f = self.stack[-1]
        prod = TABLE[f.prod]
        if kind not in CHOICE_KINDS:
            self.violations.append("%s: choice kind %r outside P43's four"
                                   % (f.prod, kind))
            return
        if kind not in prod.choices:
            self.violations.append(
                "%s recorded a %s choice it does not declare" % (f.prod, kind))
            return
        if kind == "reg" and prod.classes and value not in prod.classes:
            self.violations.append(
                "%s chose %s, outside its declared class %s"
                % (f.prod, value, "{%s}" % ", ".join(prod.classes)))
            return
        f.choices.append((kind, value, witness))

    # -- reporting ---------------------------------------------------------
    def check(self):
        """The teeth.  Returns the list of span violations; the caller makes
        it fatal."""
        if self.stack:
            self.violations.append(
                "%d production(s) began and never completed" % len(self.stack))
        return list(self.violations)

    def by_production(self):
        out = {}
        for f in self.firings:
            out.setdefault(f.prod, []).append(f)
        return out

    def render(self):
        L = ["# P42 reduction ledger -- firings in reduction order",
             "# <address>  <group>  <choices>   # shape",
             ""]
        for f in self.firings:
            ch = " ".join("%s=%s" % (k, v) for k, v, _ in f.choices) or "-"
            L.append("%-28s %-6s %-28s # %s"
                     % (f.addr, TABLE[f.prod].group, ch, f.shape))
        L.append("")
        L.append("# totals by production (reduction count, choices)")
        for name, fs in sorted(self.by_production().items(),
                               key=lambda kv: -len(kv[1])):
            nch = sum(len(f.choices) for f in fs)
            L.append("#   %-24s %4d firings %4d choices" % (name, len(fs), nch))
        if self.orphans:
            L.append("#")
            L.append("# ORPHAN EMISSIONS -- code that belongs to no production")
            for site, n, why in self.orphans:
                L.append("#   %-32s %2d instruction(s)  %s" % (site, n, why))
        unfired = [p.name for p in P if p.name not in self.counts]
        L.append("#")
        L.append("# productions never fired by this routine: %d" % len(unfired))
        for n in sorted(unfired):
            L.append("#   %s" % n)
        return "\n".join(L) + "\n"


# --------------------------------------------------------------------------
# Self-test with teeth.  Each case below MUST be rejected; if any is
# accepted the test goes RED.  The ruling was explicit that the span check
# is a check and not a convention, so it gets tested like one.
# --------------------------------------------------------------------------

def _selftest():
    fails = []

    def expect_reject(what, fn):
        L = Ledger()
        try:
            fn(L)
        except SpanViolation:
            return
        if not L.check():
            fails.append("ACCEPTED (should have been rejected): %s" % what)

    def expect_accept(what, fn):
        L = Ledger()
        try:
            fn(L)
        except SpanViolation as e:
            fails.append("REJECTED (should have been accepted): %s -- %s" % (what, e))
            return
        bad = L.check()
        if bad:
            fails.append("REJECTED (should have been accepted): %s -- %s" % (what, bad))

    # 1. a consumed descendant separately reduced
    def case1(L):
        f = L.begin("element_address", 100, "T(i)", 0)
        L.consume("element_address", [200])
        L.end(f)
        g = L.begin("scalar_ref", 200, "i", 0); L.end(g)
    expect_reject("a consumed descendant reduced separately", case1)

    # 2. a register outside the production's declared class (R41')
    def case2(L):
        f = L.begin("field_direct", 1, "p->f", 0)
        L.choice("reg", "ac0")          # R41' class is {ac2, ac3}
        L.end(f)
    expect_reject("field_direct choosing ac0, outside {ac2, ac3}", case2)

    # 3. a choice of a kind the production does not declare
    def case3(L):
        f = L.begin("marker_ref", 1, "marker", 0)
        L.choice("reg", "ac0")          # marker_ref declares no choices at all
        L.end(f)
    expect_reject("a choice of an undeclared kind", case3)

    # 4. a choice kind outside P43's four
    def case4(L):
        f = L.begin("element_address", 1, "T(i)", 0)
        L.choice("width", "32")
        L.end(f)
    expect_reject("a choice kind outside reg/slot/order/spelling", case4)

    # 5. a production that began and never completed
    def case5(L):
        L.begin("element_address", 1, "T(i)", 0)
    expect_reject("a production that never completed", case5)

    # 6. an unknown production name
    def case6(L):
        L.begin("no_such_production", 1, "x", 0)
    expect_reject("an unknown production name", case6)

    # 7. bit_base DENIES R41' -- ac1 must be ACCEPTED here, and this is the
    #    case that proves the class is per-production and not global
    def case7(L):
        f = L.begin("bit_base", 1, "record base of a bit ref", 0)
        L.choice("reg", "ac1")
        L.end(f)
    expect_accept("bit_base choosing ac1 (it denies R41')", case7)

    # 8. the clean case, and the address is in reduction order
    def case8(L):
        for nid in (1, 2, 3):
            f = L.begin("const_materialise", nid, "Constant", 0)
            L.choice("reg", "ac0")
            L.end(f)
        got = [f.addr for f in L.firings]
        if got != ["const_materialise#0", "const_materialise#1", "const_materialise#2"]:
            raise SpanViolation("addresses not in reduction order: %s" % got)
    expect_accept("three clean firings, addressed in reduction order", case8)

    # 9. the table's own shape
    if len(TABLE) != 37:
        fails.append("the table has %d productions, the gate ruled 37" % len(TABLE))
    classes = {tuple(p.classes) for p in P if p.classes}
    if len(classes) != 3:
        fails.append("%d distinct register classes; the gate ruled 3 "
                     "(R41' {ac2,ac3}, bit_base all-four, R21c' {ac0,ac1}). "
                     "A FOURTH is evidence against the production framing and "
                     "is REPORTABLE, not to be introduced quietly." % len(classes))
    if sorted(census_targets()) != sorted([
            "element_address", "field_direct", "bit_base", "scalar_ref",
            "binop_reg_reg", "binop_reg_mem", "binop_const_scale",
            "const_materialise"]):
        fails.append("the D4 census targets moved: %s" % sorted(census_targets()))

    for f in fails:
        print("FAIL: %s" % f)
    print("productions selftest %s (%d cases)" % ("FAIL" if fails else "PASS", 9))
    return 1 if fails else 0


if __name__ == "__main__":
    import sys
    sys.exit(_selftest())


# --------------------------------------------------------------------------
# Stage 3 -- the templates.
#
# A template receives the Translator and uses its Regs / Frame / Layout state
# UNCHANGED (no allocator rewrite in this project).  It emits exactly what the
# method it replaces emitted; the four matched routines are re-translated after
# each port, and a template that cannot be written without an `if` on which
# routine it is in is a FINDING -- name it and leave the production on the old
# path rather than encoding a routine-specific hack.
# --------------------------------------------------------------------------

def t_const_materialise(tr, r, k):
    """E6.  R37: the compiler materialises ZERO by subtracting a register from
    itself, never by an immediate load.  Everything else is NLDAI/WLDAI."""
    if k == 0:
        tr.emit("%s = sub(%s, %s)" % (r, r, r))
        spelling = "WSUB"
    else:
        tr.emit("%s = %s" % (r, tr.hexc(k)))
        spelling = "NLDAI"
    tr.regs.set(r, ("const", k))
    return spelling


def t_binop_const_inc(tr, r):
    """E3.  `x + 1` is WINC, not WNADI with an immediate 1.  P43's worked
    example of a spelling choice."""
    tr.emit("%s = add(%s, 1)" % (r, r))
    tr.regs.set(r, ("live", "sum"))
    return "WINC"


def t_binop_const_addi(tr, r, kk):
    """E4.  A 16-bit signed immediate add (subtraction is an add of -k)."""
    tr.emit("%s = add(%s, %s)" % (r, r, tr.hexc(kk)))
    tr.regs.set(r, ("live", "sum"))
    return "WNADI"


def t_binop_const_scale(tr, r, kr, k, op):
    """E5.  R11: the stride constant goes into a REGISTER first; there is no
    multiply-immediate.  The register is D4's site."""
    tr.emit("%s = %s" % (kr, tr.hexc(k)))
    tr.regs.set(kr, ("const", k))
    tr.emit("%s = %s(%s, %s)" % (r, op, r, kr))
    return kr


def t_goto(tr, label):
    """S8."""
    tr.terminate(("goto", [label], "0"))


def t_return_void(tr):
    """S9."""
    tr.terminate(("ret",))


def t_scalar_ref(tr, r, text, key):
    """A9.  A local / static / argument read into a register (R7/R8)."""
    tr.emit("%s = %s" % (r, text))
    tr.regs.set(r, ("var", key))


def t_field_direct(tr, r, addr, key):
    """A2.  The indexing base load.  R41' governs `r`: the legal set is
    {ac2, ac3}, and the ledger rejects anything else against the production's
    declared class."""
    tr.emit("%s = M32[%s]" % (r, tr.hexc(addr)), uses_fp=False)
    tr.regs.set(r, ("addr", key))


def t_link_load(tr, r, fp, key):
    """A6.  R42/R44: the static link, saved by WSAVS at wp(fp, -6).  Under
    R41' this is an ordinary member of the addressing class."""
    tr.emit("%s = M32[wp(%s, -6)]" % (r, fp), uses_fp=False)
    tr.regs.set(r, ("addr", key))


def t_bit_base(tr, r, addr):
    """A5.  R28, and the R41' NEGATIVE case: the SAME physical operation as
    t_field_direct -- loading a record base -- but the legal set is all four
    registers, because this production is for a bit reference rather than for
    indexing.  The two templates are deliberately separate: merging them is
    what would force a fifth class."""
    tr.emit("%s = M32[%s]" % (r, tr.hexc(addr)))


def t_temp_place(tr, slot, r, w):
    """P1.  R3/R3b': a CSE temp store."""
    tr.emit("M%d[wp(ac3, %d)] = %s" % (w, slot, r) if w == 32
            else "M16[wp(ac3, %d)] = trunc16(%s)" % (slot, r))


TEMPLATES = {
    "scalar_ref": t_scalar_ref,
    "field_direct": t_field_direct,
    "link_load": t_link_load,
    "bit_base": t_bit_base,
    "const_materialise": t_const_materialise,
    "binop_const_inc": t_binop_const_inc,
    "binop_const_addi": t_binop_const_addi,
    "binop_const_scale": t_binop_const_scale,
    "goto": t_goto,
    "return_void": t_return_void,
}

for _n, _t in TEMPLATES.items():
    TABLE[_n].ported = True
    TABLE[_n].template = _t


def ported_names():
    return sorted(n for n, p in TABLE.items() if p.ported)
