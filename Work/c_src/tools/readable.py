#!/usr/bin/env python3
"""readable.py — Project 34: the readable layer (RESEARCH PROTOTYPE).

A pass over an ir 5 book (docs/IR.md) that RENDERS chosen routines with
mechanical naming and folding, each rewrite switchable and each reporting
what it resolved and what it could not.  The output is a rendering for
humans; nothing executes it.  Every rewrite renames or folds — it never
changes an address or a value:

  frame    wp(ac3, d)/R[ac3 + -k] -> local_d / arg_N from quest.addrbook
  static   M32[0x7000xxxx] -> symbol from quest.symbols, else static_<addr>,
           plus a usage census over the WHOLE book
  callargs book-mode arg-slot stores + `call` -> call NAME(args) (slot -> arg N
           through the callee's addrbook layout)
  record   base + i*stride + K -> <BASE>_rec<stride>[i].f<K>
  literal  [@local, n varying] = "text"; rt_call F(&local) -> rt_call F("text")
           when the local is provably dead after the call
  regfold  single-assignment, single-use register values consumed inside the
           block fold into their use; registers live at the terminator stay
  simplify a short list of provable identities (listed in the header)
  sketch   loop / if / bounds-check marks on terminators

Inputs are read only.  Provenance (book sha256, branch, commit) is stamped in
every rendering's header.  See docs/Project34/Census.md.
"""
import argparse, collections, hashlib, os, re, sys, time

# ----------------------------------------------------------------------------
# tokenizer / parser for the ir 5 expression grammar (IR.md §5.1, §5.8)
# ----------------------------------------------------------------------------

TOKEN_RE = re.compile(r"""
    (?P<ws>\s+)
  | (?P<bp>0x[0-9A-Fa-f]+:[01])
  | (?P<hex>0x[0-9A-Fa-f]+)
  | (?P<dec>-?\d+)
  | (?P<str>"(?:[^"\\]|\\x[0-9A-Fa-f]{2}|\\.)*")
  | (?P<id>[A-Za-z_?][A-Za-z0-9_?.]*)
  | (?P<op><=s|<=u|>=s|>=u|<s|<u|>s|>u|==|!=|&&|\|\||/s|/u|%s|%u|[-+*&|^~!()\[\],=@])
""", re.X)

BINOPS = {"+", "-", "*", "&", "|", "^", "/s", "/u", "%s", "%u",
          "==", "!=", "<s", "<=s", ">s", ">=s", "<u", "<=u", ">u", ">=u",
          "&&", "||"}
CMPOPS = {"==", "!=", "<s", "<=s", ">s", ">=s", "<u", "<=u", ">u", ">=u"}
FUNCS = {"ind", "wp", "bp", "lsh", "tf", "sx16", "zx16", "zx8", "trunc16"}
EFFOPS = {"add", "sub", "mul", "div", "cvwn", "ash", "nadd", "nsub", "nmul"}
REGS = {"ac0", "ac1", "ac2", "ac3", "c", "ovr", "wfp", "wsp", "wsb", "wsl"}


class ParseError(Exception):
    pass


def tokenize(s):
    out, pos = [], 0
    while pos < len(s):
        m = TOKEN_RE.match(s, pos)
        if not m:
            raise ParseError("bad token at %d: %r" % (pos, s[pos:pos + 20]))
        pos = m.end()
        kind = m.lastgroup
        if kind == "ws":
            continue
        out.append((kind, m.group(kind)))
    return out


class Parser:
    def __init__(self, s):
        self.toks = tokenize(s)
        self.i = 0

    def peek(self, k=0):
        j = self.i + k
        return self.toks[j] if j < len(self.toks) else (None, None)

    def next(self):
        t = self.peek()
        self.i += 1
        return t

    def expect(self, val):
        k, v = self.next()
        if v != val:
            raise ParseError("expected %r got %r" % (val, v))

    def at_end(self):
        return self.i >= len(self.toks)

    # expr := primary (op primary)*  (one class per chain; sub-chains are
    # parenthesized by the emitter, so a flat loop is exact)
    def expr(self):
        first = self.unary()
        rest = []
        while self.peek()[1] in BINOPS:
            op = self.next()[1]
            rest.append((op, self.unary()))
        if not rest:
            return first
        return ("chain", first, rest)

    def unary(self):
        k, v = self.peek()
        if v == "~":
            self.next()
            return ("not", self.unary())
        if v == "!":
            self.next()
            return ("lnot", self.unary())
        return self.primary()

    def primary(self):
        k, v = self.next()
        if k == "bp":
            w, b = v.split(":")
            return ("bp", int(w, 16), int(b))
        if k == "hex":
            return ("num", int(v, 16), "hex")
        if k == "dec":
            return ("num", int(v) & 0xFFFFFFFF, "dec")
        if k == "str":
            return ("str", v[1:-1])
        if v == "(":
            e = self.expr()
            self.expect(")")
            return ("paren", e)
        if v == "[":
            # located / piece: [@addr, n] [@addr, n varying] [@addr, varying] [@0xW:b, "text"]
            self.expect("@")
            addr = self.expr()
            self.expect(",")
            if self.peek()[0] == "str":
                text = self.next()[1][1:-1]
                self.expect("]")
                return ("lit", addr, text)
            if self.peek()[1] == "varying":
                self.next()
                self.expect("]")
                return ("located", addr, None, True)
            n = self.expr()
            varying = False
            if self.peek()[1] == "varying":
                self.next()
                varying = True
            self.expect("]")
            return ("located", addr, n, varying)
        if k == "id":
            if v in ("M8", "M16", "M32", "R") and self.peek()[1] == "[":
                self.next()
                e = self.expr()
                self.expect("]")
                return ("mem", v, e)
            if v in FUNCS or v in EFFOPS or v in ("cmp", "words"):
                self.expect("(")
                args = []
                if v == "words":
                    self.expect("@")
                if self.peek()[1] != ")":
                    args.append(self.expr())
                    while self.peek()[1] == ",":
                        self.next()
                        args.append(self.expr())
                self.expect(")")
                tag = "effop" if v in EFFOPS else ("call" if v in FUNCS else v)
                return (tag, v, args)
            if v in REGS:
                return ("reg", v)
            if re.fullmatch(r"t\d+", v):
                return ("t", v)
            raise ParseError("unknown identifier %r" % v)
        raise ParseError("unexpected token %r" % (v,))


GOTO_RE = re.compile(r"^goto \[([0-9A-Fa-f ,]+)\] (.*)$")
RT_RE = re.compile(r"^rt_call ([^\s(]+)\((.*)\) site=([0-9A-Fa-f]{8})$")
CALL_RE = re.compile(r"^call ([0-9A-Fa-f]{8}) args=(\d+) marker=([0-9A-Fa-f]+) site=([0-9A-Fa-f]{8}) ret=([0-9A-Fa-f]{8})$")
INSTR_RE = re.compile(r"^@([0-9A-Fa-f]{8}) (.*)$")


def parse_stmt(text):
    """One IR line (comment already stripped) -> a statement tuple.
    kinds: instr(pc, text) assign(lhs, rhs) assert(expr, msg) goto(labels, expr)
           rt_call(callee, args, site) call(tgt, argc, marker, site, ret) ret
           sassign(dst_located, piece) cmp(p1, p2) words(dst, src, k)"""
    t = text.strip()
    m = INSTR_RE.match(t)
    if m:
        return ("instr", int(m.group(1), 16), m.group(2))
    if t == "ret":
        return ("ret",)
    m = GOTO_RE.match(t)
    if m:
        labels = [int(x, 16) for x in m.group(1).replace(",", " ").split()]
        p = Parser(m.group(2))
        e = p.expr()
        if not p.at_end():
            raise ParseError("goto trailing")
        return ("goto", labels, e)
    m = RT_RE.match(t)
    if m:
        p = Parser(m.group(2))
        args = []
        if not p.at_end():
            args.append(p.expr())
            while p.peek()[1] == ",":
                p.next()
                args.append(p.expr())
        if not p.at_end():
            raise ParseError("rt_call trailing")
        return ("rt_call", m.group(1), args, int(m.group(3), 16))
    m = CALL_RE.match(t)
    if m:
        return ("call", int(m.group(1), 16), int(m.group(2)), int(m.group(3), 16),
                int(m.group(4), 16), int(m.group(5), 16))
    if t.startswith("assert("):
        p = Parser(t[len("assert"):])
        p.expect("(")
        e = p.expr()
        msg = None
        if p.peek()[1] == ",":
            p.next()
            msg = p.next()[1][1:-1]
        p.expect(")")
        return ("assert", e, msg)
    p = Parser(t)
    lhs = p.expr()
    p.expect("=")
    rhs = p.expr()
    if not p.at_end():
        raise ParseError("trailing tokens: %r" % (p.toks[p.i:],))
    if lhs[0] == "located":
        return ("sassign", lhs, rhs)
    if rhs[0] == "cmp":
        return ("cmp", rhs[2][0], rhs[2][1])
    if lhs[0] == "words":
        return ("words", lhs[2][0], rhs[2][0], lhs[2][1])
    return ("assign", lhs, rhs)


# ----------------------------------------------------------------------------
# loaders
# ----------------------------------------------------------------------------

def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


class Block:
    __slots__ = ("pc", "seg", "stmts", "raws", "comments")

    def __init__(self, pc, seg):
        self.pc, self.seg = pc, seg
        self.stmts, self.raws, self.comments = [], [], []


def load_book(path):
    header, blocks, cur = [], {}, None
    with open(path, encoding="ascii", errors="replace") as f:
        for raw in f:
            line = raw.rstrip("\r\n")
            if cur is None and not line.startswith("block "):
                if line.strip():
                    header.append(line)
                continue
            if line.startswith("block "):
                parts = line.split()
                cur = Block(int(parts[1], 16), int(parts[3], 16))
                blocks[cur.pc] = cur
                continue
            if not line.strip():
                cur = None
                continue
            if line.startswith("blocks "):
                break
            body, _, comment = line.partition(";")
            body = body.strip()
            if not body:
                continue
            try:
                st = parse_stmt(body)
            except ParseError as e:
                raise ParseError("block %08X: %s: %s" % (cur.pc, e, body))
            cur.stmts.append(st)
            cur.raws.append(body)
            cur.comments.append(comment.strip())
    return header, blocks


def load_addrbook(path):
    """entry pc -> dict(name, alloc, wfp, argc, frame, variant, flags, stacked)"""
    entries = {}
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            stacked = False
            if line.startswith("#"):
                if not re.match(r"^#[0-9A-Fa-f]{8}\s", line):
                    continue
                stacked = True
                line = line[1:]
            parts = line.split()
            if len(parts) < 7 or parts[0] == "borrow_slots":
                continue
            pc = int(parts[0], 16)
            entries[pc] = dict(pc=pc, name=parts[1], alloc=int(parts[2], 16),
                               wfp=int(parts[3], 16), argc=int(parts[4]),
                               frame=int(parts[5], 16), variant=parts[6],
                               flags=parts[7] if len(parts) > 7 else "",
                               stacked=stacked)
    return entries


def load_symbols(path):
    """addr -> first name (statics and routines share the table)."""
    syms = {}
    with open(path) as f:
        for line in f:
            parts = line.strip().split(None, 1)
            if len(parts) != 2:
                continue
            addr = int(parts[0], 16)
            name = parts[1].split(" / ")[0].strip()
            syms.setdefault(addr, name)
    return syms


def load_block_succs(path):
    """block pc -> successor pcs (from quest.blocks.split's n/c lines)."""
    succs, cur = {}, None
    with open(path, encoding="ascii", errors="replace") as f:
        for raw in f:
            line = raw.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            m = re.match(r"^([0-9A-Fa-f]{8}):$", line)
            if m:
                cur = int(m.group(1), 16)
                continue
            m = re.match(r"^(?:c [0-9A-Fa-f]{8} )?n((?: [0-9A-Fa-f]{8})+)$", line)
            if m and cur is not None:
                succs[cur] = [int(x, 16) for x in m.group(1).split()]
    return succs


def block_succs(block, file_succs):
    """Successors from the IR terminator; the blocks file for instruction exits."""
    last = block.stmts[-1] if block.stmts else None
    if last is None:
        return file_succs.get(block.pc, [])
    k = last[0]
    if k == "goto":
        return list(last[1])
    if k == "rt_call":
        return [last[3] + 4]
    if k == "call":
        return [last[5]]
    if k == "ret":
        return []
    return file_succs.get(block.pc, [])


def attribute_routines(blocks, file_succs, entries):
    """block pc -> owning addrbook entry pc, by reachability from each entry
    over intra-procedural edges, not entering another entry.  Unreached
    blocks fall back to the nearest preceding entry (reported)."""
    owner, order = {}, {}
    entry_pcs = sorted(entries)
    for e in entry_pcs:
        if e not in blocks:
            continue
        stack, seen = [e], {e}
        while stack:
            pc = stack.pop()
            if pc in owner and owner[pc] != e:
                continue
            owner[pc] = e
            b = blocks.get(pc)
            if b is None:
                continue
            for s in block_succs(b, file_succs):
                if s in entries and s != e:
                    continue
                if s in blocks and s not in seen:
                    seen.add(s)
                    stack.append(s)
    fallback = []
    import bisect
    for pc in blocks:
        if pc not in owner:
            i = bisect.bisect_right(entry_pcs, pc) - 1
            if i >= 0:
                owner[pc] = entry_pcs[i]
                fallback.append(pc)
    return owner, fallback


# ----------------------------------------------------------------------------
# AST helpers
# ----------------------------------------------------------------------------

def children(n):
    k = n[0]
    if k in ("num", "bp", "str", "reg", "t", "regin"):
        return []
    if k == "chain":
        return [n[1]] + [x[1] for x in n[2]]
    if k in ("not", "lnot", "paren"):
        return [n[1]]
    if k == "mem":
        return [n[2]]
    if k in ("call", "effop", "cmp", "words"):
        return list(n[2])
    if k == "lit":
        return [n[1]]
    if k == "located":
        return [n[1]] + ([n[2]] if n[2] is not None else [])
    if k == "ann":               # ("ann", node, note) — rendering annotation
        return [n[1]]
    if k == "effnest":           # an effectful op folded into an adjacent use
        return [n[1]]
    raise ValueError(k)


def walk(n):
    yield n
    for c in children(n):
        yield from walk(c)


def map_children(n, f):
    k = n[0]
    if k in ("num", "bp", "str", "reg", "t", "regin"):
        return n
    if k == "chain":
        return ("chain", f(n[1]), [(op, f(x)) for op, x in n[2]])
    if k in ("not", "lnot", "paren"):
        return (k, f(n[1]))
    if k == "mem":
        return ("mem", n[1], f(n[2]))
    if k in ("call", "effop", "cmp", "words"):
        return (k, n[1], [f(a) for a in n[2]])
    if k == "lit":
        return ("lit", f(n[1]), n[2])
    if k == "located":
        return ("located", f(n[1]), f(n[2]) if n[2] is not None else None, n[3])
    if k == "ann":
        return ("ann", f(n[1]), n[2])
    if k == "effnest":
        return ("effnest", f(n[1]))
    raise ValueError(k)


def reg_reads(n):
    return {x[1] for x in walk(n) if x[0] in ("reg", "t")}


def reads_mem(n):
    return any(x[0] == "mem" for x in walk(n))


_HE, _RF = {}, {}


def has_effop(n):
    key = id(n)
    hit = _HE.get(key)
    if hit is not None and hit[0] is n:
        return hit[1]
    r = any(x[0] == "effop" for x in walk(n))
    _HE[key] = (n, r)
    return r


def reads_flags(n):
    key = id(n)
    hit = _RF.get(key)
    if hit is not None and hit[0] is n:
        return hit[1]
    r = any(x[0] == "reg" and x[1] in ("c", "ovr") for x in walk(n))
    _RF[key] = (n, r)
    return r


def subst_reg(n, name, repl):
    """Replace every read of register/t `name` by repl (parenthesised when
    the replacement is a chain and the context is a chain)."""
    if n[0] in ("reg", "t") and n[1] == name:
        return repl
    if n[0] == "chain":
        def wrap(x):
            r = subst_reg(x, name, repl)
            if r is not x and r[0] == "chain":
                r = ("paren", r)
            return r
        return ("chain", wrap(n[1]), [(op, wrap(x)) for op, x in n[2]])
    return map_children(n, lambda c: subst_reg(c, name, repl))


def count_reg(n, name):
    return sum(1 for x in walk(n) if x[0] in ("reg", "t") and x[1] == name)


def unparen(n):
    while n[0] in ("paren", "effnest"):
        n = n[1]
    return n


def is_num(n, v=None):
    n = unparen(n)
    return n[0] == "num" and (v is None or n[1] == v)


def num_val(n):
    return unparen(n)[1]


def signed(v):
    return v - (1 << 32) if v & 0x80000000 else v




# instruction lines left in the book (ir 5): declared beliefs about which
# registers they read/write, from the emulator's arms (EagleSpecial/EagleStack/
# NovaCompute).  Unknown mnemonics are treated as reading and writing everything.
AC = {"ac0", "ac1", "ac2", "ac3"}
INSTR_WRITES = {
    "WCMV": AC | {"c"}, "WCMP": AC, "WBLM": {"ac1", "ac2", "ac3"},
    "WSAVS": {"ac3"}, "WSAVR": {"ac3"}, "WMSP": set(), "STASP": set(), "WPSH": set(),
    "DIVX": {"ac0", "ac1", "ovr"}, "WDIVS": {"ac0", "ac1", "ovr"},
    "SYSCALL": {"ac0", "ac1", "ac2"}, "DERR": set(), "WLOB": {"ac1"},
    "FAS": set(), "FRDS": set(), "FMS": set(), "FRH": set(), "FEXP": set(), "FHLV": set(),
    "FDS": set(), "XFAMS": set(), "WFFAD": set(), "WFLAD": set(), "FMOV": set(), "XFLDS": set(),
    "FSS": set(), "FSGT": set(), "FCMP": set(),
    "LCALL": {"ac0", "ac1", "ac2"}, "XCALL": {"ac0", "ac1", "ac2"}, "LJSR": {"ac0", "ac1", "ac2", "ac3"},
}
INSTR_READS = {
    "WCMV": AC, "WCMP": AC, "WBLM": AC, "WSAVS": set(), "WSAVR": set(),
    "DIVX": {"ac0", "ac1", "ac2"}, "WDIVS": {"ac0", "ac1", "ac2"}, "SYSCALL": AC, "DERR": set(),
    "FAS": set(), "FRDS": set(), "FMS": set(), "FRH": set(), "FEXP": set(), "FHLV": set(),
    "FDS": set(), "XFAMS": set(), "WFFAD": set(), "WFLAD": set(), "FMOV": set(), "XFLDS": set(),
    "FSS": set(), "FSGT": set(), "FCMP": set(),
}
INSTR_MEMWRITE = {"WCMV", "WBLM", "WPSH", "STASP", "SYSCALL", "LCALL", "XCALL", "LJSR", "WFLAD", "WFFAD"}

# runtime callees (docs/Project28/RTConventions.md, "Registers"): no callee
# reads ac0/ac1 on entry, one reads ac2, four return a value in ac0, none
# writes ac1/ac2 back (WSAVS/WRTN preserve them across the call).
RT_ENTRY_READS = {"?UNSIGNED_TO_CHAR": {"ac2"}}
RT_RETURNS = {"?RANDOM_NUMBER", "?CHAR_TO_UNSIGNED", "?LIB_ERROR_CODE", "?CURRENT_PID"}


def instr_mnemonic(text):
    return text.split()[0].rstrip(";") if text.split() else "?"


def instr_writes(text):
    m = instr_mnemonic(text)
    if m in INSTR_WRITES:
        return set(INSTR_WRITES[m]) | {"c", "ovr"}
    return set(REGS) | {"*"}


def instr_reads(text):
    m = instr_mnemonic(text)
    if m in INSTR_READS:
        return set(INSTR_READS[m])
    if m in ("WMSP", "STASP"):
        r = re.search(r"[A-Z]+ (\d)", text)
        return {"ac" + r.group(1)} if r else set(AC)
    if m == "WPSH":
        r = re.search(r"WPSH (\d),(\d)", text)
        if r:
            a, b = int(r.group(1)), int(r.group(2))
            regs = set()
            i = a
            while True:
                regs.add("ac%d" % i)
                if i == b:
                    break
                i = (i + 1) % 4
            return regs
        return set(AC)
    return set(REGS)


def instr_sets_fp(text):
    return instr_mnemonic(text) in ("WSAVS", "WSAVR", "LDAFP")


# statement-level reads / writes -----------------------------------------------

def stmt_exprs(st):
    """All expression nodes a statement READS (lhs addresses included)."""
    k = st[0]
    if k == "assign":
        lhs, rhs = st[1], st[2]
        out = [rhs]
        if lhs[0] == "mem":
            out.append(lhs[2])
        return out
    if k == "assert":
        return [st[1]]
    if k == "goto":
        return [st[2]]
    if k == "rt_call":
        return list(st[2])
    if k == "sassign":
        return [st[1], st[2]]
    if k == "cmp":
        return [st[1], st[2]]
    if k == "words":
        return [st[1], st[2], st[3]]
    return []


_SWR = {}


def stmt_writes_reg(st):
    """The register/t/flag a statement writes, if any (plus implicit flag writes)."""
    key = id(st)
    hit = _SWR.get(key)
    if hit is not None and hit[0] is st:
        return hit[1]
    out = _stmt_writes_reg(st)
    _SWR[key] = (st, out)
    return out


def _stmt_writes_reg(st):
    k = st[0]
    out = set()
    if k == "assign":
        lhs = st[1]
        if lhs[0] in ("reg", "t"):
            out.add(lhs[1])
        if has_effop(st[2]):
            out |= {"c", "ovr"}
    elif k in ("sassign", "cmp", "words"):
        out |= {"ac0", "ac1", "ac2", "ac3", "c"}      # library residues (§5.8)
    elif k == "instr":
        out |= instr_writes(st[2])
    return out


def stmt_mem_write(st):
    """('mem', width, addr) if the statement stores to memory; 'string' for a
    string statement (unknown extent); '*' for an instruction; None else."""
    k = st[0]
    if k == "assign" and st[1][0] == "mem":
        return st[1]
    if k in ("sassign", "words"):
        return "string"
    if k == "instr":
        return "*" if instr_mnemonic(st[2]) in INSTR_MEMWRITE else None
    return None


# ----------------------------------------------------------------------------
# rendering context
# ----------------------------------------------------------------------------

DATA_LO, DATA_HI = 0x70000000, 0x7015BD20      # statics live below the first code symbol
SLOT_LO, SLOT_HI = 0x74000000, 0x74010000      # book-mode frame area (addrbook base)


class Switches:
    def __init__(self, **kw):
        self.frame = kw.get("frame", True)
        self.static = kw.get("static", True)
        self.callargs = kw.get("callargs", True)
        self.record = kw.get("record", True)
        self.literal = kw.get("literal", True)
        self.regfold = kw.get("regfold", True)
        self.simplify = kw.get("simplify", True)
        self.sketch = kw.get("sketch", True)
        self.heapdisjoint = kw.get("heapdisjoint", True)
        self.argdisjoint = kw.get("argdisjoint", True)
        self.envinherit = kw.get("envinherit", True)

    def text(self):
        return " ".join(("+" if getattr(self, k) else "-") + k for k in
                        ("frame", "static", "callargs", "record", "literal",
                         "regfold", "simplify", "sketch", "heapdisjoint", "argdisjoint", "envinherit"))


class Report:
    """Per-rewrite tallies and examples (the census)."""
    def __init__(self):
        self.counts = collections.Counter()
        self.examples = collections.defaultdict(list)

    def hit(self, key, example=None, cap=12):
        self.counts[key] += 1
        if example is not None and len(self.examples[key]) < cap:
            self.examples[key].append(example)


class World:
    """Everything routine-independent: the book, the books, the census."""
    def __init__(self, blocks, entries, syms, file_succs, sw):
        self.blocks, self.entries, self.syms, self.sw = blocks, entries, syms, sw
        self.file_succs = file_succs
        self.owner, self.fallback = attribute_routines(blocks, file_succs, entries)
        self.static_census = collections.defaultdict(lambda: dict(
            routines=set(), reads=collections.Counter(), writes=collections.Counter(),
            cmp=set(), addr_taken=0))
        self.tables = collections.defaultdict(lambda: dict(
            fields=collections.defaultdict(lambda: collections.Counter()),
            routines=set(), sites=0, guarded=0, index_forms=collections.Counter()))
        self.report = Report()
        self.slot_index = {}
        for e in entries.values():
            for n in range(1, e["argc"] + 1):
                self.slot_index[e["wfp"] - 10 - 2 * n] = (e, n)
        self.routine_blocks = collections.defaultdict(list)
        for pc, o in self.owner.items():
            self.routine_blocks[o].append(pc)
        for v in self.routine_blocks.values():
            v.sort()

    def static_name(self, addr):
        s = self.syms.get(addr)
        if s:
            return s
        return "static_%X" % addr

    def routine_name(self, pc):
        e = self.entries.get(pc)
        if e:
            return e["name"]
        return self.syms.get(pc, "routine_%08X" % pc)


# ----------------------------------------------------------------------------
# in-block analysis: register folding (rewrite 5) + symbolic env
# ----------------------------------------------------------------------------

def classify_addr(idx, width, ctx):
    """Where does a memory access land?  ('local', lo, hi) word range in the
    frame, ('static', lo, hi), ('slot', lo, hi); None if unknown.
    hi is exclusive; widths in words (M8/M16 -> 1 word, M32 -> 2)."""
    words = 2 if width == "M32" else 1
    idx = unparen(idx)
    if ctx.sw.frame and idx[0] == "call" and idx[1] == "wp":
        b, d = idx[2]
        b = unparen(b)
        if is_num(d) and ((b == ("reg", "ac3") and ctx.fp) or
                          (b[0] == "regin" and b[1] == "ac3" and getattr(ctx, "fp_in", False))):
            dv = signed(num_val(d))
            return ("local", dv, dv + words)
    # R[ac3 + k] / M32[ac3 + k]: the frame word k itself (arg slots, images)
    if ctx.sw.frame and idx[0] == "chain" and len(idx[2]) == 1 and idx[2][0][0] == "+" and is_num(idx[2][0][1]):
        b = unparen(idx[1])
        if (b == ("reg", "ac3") and ctx.fp) or (b[0] == "regin" and b[1] == "ac3" and getattr(ctx, "fp_in", False)):
            dv = signed(num_val(idx[2][0][1]))
            return ("local", dv, dv + (2 if width in ("M32", "R") else 1))
    # M8[bp(ac3, d) (+ k)]: byte d(+k) of the frame = word (d+k)//2
    if ctx.sw.frame and width == "M8":
        bl = byte_local(idx, ctx)
        if bl is not None:
            return ("local", bl[0], bl[0] + 1)
    if idx[0] == "num":
        a = idx[1]
        if DATA_LO <= a < DATA_HI:
            return ("static", a, a + words)
        if SLOT_LO <= a < SLOT_HI:
            return ("slot", a, a + words)
    # DECLARED BELIEF (switch argdisjoint): a by-reference argument pointer
    # (R[ac3 + -k], k >= 12) was computed by the caller before this frame
    # existed, so what it points at is not one of this frame's locals.
    if ctx.sw.frame and ctx.sw.argdisjoint and idx[0] == "mem" and idx[1] == "R":
        i2 = unparen(idx[2])
        if i2[0] == "chain" and len(i2[2]) == 1 and i2[2][0][0] == "+" and is_num(i2[2][0][1]) \
                and unparen(i2[1])[0] in ("reg", "regin") and unparen(i2[1])[1] == "ac3" \
                and ((unparen(i2[1])[0] == "reg" and ctx.fp) or (unparen(i2[1])[0] == "regin" and getattr(ctx, "fp_in", False))):
            k = signed(num_val(i2[2][0][1]))
            if k <= -12 and k % 2 == 0:
                return ("argderef", k, k + words)
    # DECLARED BELIEF (switch heapdisjoint): an address that decomposes to a
    # record (a pointer static + i*stride + K) lands in the shared data pages,
    # never in the frame, the statics or the arg-slot area.  Every alias
    # decision that rests on it is counted (world.report heap.disjoint_used).
    if ctx.sw.record and ctx.sw.heapdisjoint and getattr(ctx, "env", None) is not None:
        r = decompose(idx, ctx)
        if r is not None:
            return ("heap", r.base, r.stride, r.K, r.K + words, repr(r.idx), r.elem_local)
    return None


def byte_local(idx, ctx):
    """M8 index bp(ac3, d) [+ k] -> (word, byte) in the frame, or None."""
    idx = unparen(idx)
    k = 0
    if idx[0] == "chain" and {op for op, _ in idx[2]} == {"+"} and all(is_num(x) for _, x in idx[2]):
        k = sum(signed(num_val(x)) for _, x in idx[2])
        idx = unparen(idx[1])
    if idx[0] == "call" and idx[1] == "bp":
        b, d = idx[2]
        b = unparen(b)
        if is_num(d) and ((b == ("reg", "ac3") and ctx.fp) or
                          (b[0] == "regin" and b[1] == "ac3" and getattr(ctx, "fp_in", False))):
            byte = signed(num_val(d)) + k
            return (byte // 2, byte % 2)
    return None


def may_alias(a, b, world=None):
    if a is None or b is None:
        return True
    if a[0] != b[0]:
        if world is not None and ("heap" in (a[0], b[0])):
            world.report.hit("heap.disjoint_used")
        if world is not None and ("argderef" in (a[0], b[0])):
            world.report.hit("arg.disjoint_used")
        return False
    if a[0] == "argderef":
        return True
    if a[0] == "heap":
        # same table, same index text, non-overlapping field ranges -> disjoint
        if a[1] == b[1] and a[2] == b[2] and a[5] == b[5] and a[6] == b[6] and a[2] is not None:
            return not (a[4] <= b[3] or b[4] <= a[3])
        return True
    return not (a[2] <= b[1] or b[2] <= a[1])


class BlockCtx:
    def __init__(self, world, entry, block):
        self.world, self.entry, self.block, self.sw = world, entry, block, world.sw
        self.fp = True                 # ac3 == fp at block entry (assumption; checked)
        self.env = {}                  # reg -> closed symbolic value
        self.notes = []
        self.fp_in = True              # ac3 == fp at block ENTRY (regin ac3 is fp)
        self.shape_now = {}            # local d -> shape set by a def earlier in this block
        self.reach_in = {}             # local d -> set of shapes reaching the block entry
        self.shapes = {}

    def shape_at(self, d):
        """The one shape every reaching def of local d has, or None."""
        if d in self.shape_now:
            shs = self.shape_now[d]
        else:
            shs = self.reach_in.get(d)
        if not shs or len(shs) != 1:
            return None
        sh = next(iter(shs))
        return sh if sh[0] in ("scaled", "elemptr", "baseptr") else None


def close_value(rhs, env, pc=None):
    """rhs with every register read replaced by its env value (closed) or by
    ('regin', r, pc) — the value at the entry of block pc — when unknown."""
    def f(n):
        if n[0] == "reg":
            if n[1] in env:
                v = env[n[1]]
                return ("paren", v) if v[0] == "chain" else v
            return ("regin", n[1], pc)
        if n[0] == "t":
            return env.get(n[1], n)
        return map_children(n, f)
    return f(rhs)


def env_deps(v):
    return {x[1] for x in walk(v) if x[0] == "regin"}


def env_deps_any(v, regs):
    return False   # closed values reference entry values only (immutable)


def env_after(st, ctx):
    """Update ctx.env / ctx.fp for a statement (in the ORIGINAL sequence)."""
    env = ctx.env
    k = st[0]
    written = stmt_writes_reg(st)
    memw = stmt_mem_write(st)
    if k == "instr":
        if "*" in written:
            env.clear()
            ctx.fp = instr_sets_fp(st[2])
            return
        for r in list(env):
            if reads_flags(env[r]) or env_deps_any(env[r], written):
                del env[r]
        for r in written:
            env.pop(r, None)
        if memw == "*":
            for r in list(env):
                if reads_mem(env[r]):
                    del env[r]
        if instr_sets_fp(st[2]):
            ctx.fp = True
        elif "ac3" in written:
            ctx.fp = False
        return
    if k == "assign" and st[1][0] in ("reg", "t"):
        name = st[1][1]
        val = close_value(st[2], env, ctx.block.pc)
        if name == "ac3":
            ctx.fp = (unparen(st[2]) == ("reg", "wfp"))
        # drop entries depending on the written register (via regin) — the
        # closed form only references entry values, so only a write to a
        # register whose ENTRY value is referenced... cannot happen: entry
        # values are immutable.  Memory-dependent entries are dropped below.
        if name in ("c", "ovr"):
            val = None
        for r in list(env):
            if reads_flags(env[r]) and (name in ("c", "ovr") or has_effop(st[2])):
                del env[r]
        if val is not None:
            env[name] = val
        else:
            env.pop(name, None)
    elif k in ("sassign", "cmp", "words"):
        for r in ("ac0", "ac1", "ac2", "ac3"):
            env.pop(r, None)
        for r in list(env):
            if reads_flags(env[r]):
                del env[r]
        ctx.fp = False if k in ("sassign", "cmp", "words") else ctx.fp
        # the library writes ac3: fp is lost until LDAFP
    elif k == "assign" and has_effop(st[2]):
        for r in list(env):
            if reads_flags(env[r]):
                del env[r]
    if k == "assign" and st[1][0] == "mem":
        loc = local_of(st[1], ctx)
        if loc:
            d, wdt = loc
            if wdt == "M32":
                ctx.shape_now[d] = {shape_of(close_value(st[2], env, ctx.block.pc), ctx)}
            else:
                ctx.shape_now[d] = {("narrow",)}
                ctx.shape_now[d - 1] = {("narrow",)} if d - 1 in ctx.shape_now or d - 1 in ctx.reach_in else ctx.shape_now.get(d - 1, {("narrow",)})
    if memw is not None:
        w = classify_addr(memw[2], memw[1], ctx) if memw not in ("string", "*") else None
        for r in list(env):
            v = env[r]
            if not reads_mem(v):
                continue
            # drop unless every memory read in v is provably disjoint
            ok = memw not in ("string", "*")
            if ok:
                for x in walk(v):
                    if x[0] == "mem" and may_alias(classify_addr(x[2], x[1], ctx), w, ctx.world):
                        ok = False
                        break
            if not ok:
                del env[r]


def fold_block(block, ctx, rep):
    """Rewrite 5.  Returns (stmts, folded_flags, live_regs) where stmts is the
    folded statement list (dropped defs removed), and live_regs the registers
    written in the block whose final value reaches the terminator explicitly."""
    stmts = list(block.stmts)
    dropped = [False] * len(stmts)
    n = len(stmts)
    for phase, i in [(ph, i) for ph in (0, 1) for i in range(n)]:
        st = stmts[i]
        if dropped[i] or st[0] != "assign" or st[1][0] not in ("reg", "t"):
            continue
        name, rhs = st[1][1], st[2]
        if has_effop(rhs) != (phase == 1):
            continue
        if name in ("ac3", "c", "ovr", "wfp", "wsp", "wsb", "wsl"):
            continue
        if not ctx.sw.regfold:
            continue
        live_out = ctx.routine.live_out.get(block.pc, set())
        if name in live_out and not any(
                (not dropped[j]) and name in stmt_writes_reg(stmts[j]) for j in range(i + 1, n)):
            rep.hit("regfold.kept_live_at_exit")
            continue
        # 1. every use of `name` until its next write (an instruction line
        #    counts as an opaque use + write)
        uses, opaque = [], False
        for j in range(i + 1, n):
            if dropped[j]:
                continue
            sj = stmts[j]
            if sj[0] == "instr":
                if name in instr_reads(sj[2]) or "*" in instr_writes(sj[2]):
                    opaque = True
                    break
                if name in instr_writes(sj[2]):
                    break
                continue
            c = sum(count_reg(e, name) for e in stmt_exprs(sj))
            if c:
                uses.append((j, c))
            if name in stmt_writes_reg(sj):
                break
        if opaque:
            rep.hit("regfold.blocked", "%08X %s = … : instruction line reads it" % (block.pc, name))
            continue
        if not uses:
            rep.hit("regfold.no_use_in_block")
            continue
        if len(uses) != 1 or uses[0][1] != 1:
            rep.hit("regfold.multi_use", "%08X %s (%d uses)" % (block.pc, name, sum(c for _, c in uses)))
            continue
        j = uses[0][0]
        # 2. may rhs cross every statement strictly between i and j?
        blocked, reason = False, None
        rhs_regs = reg_reads(rhs)
        rhs_mem = reads_mem(rhs)
        rhs_eff = has_effop(rhs)
        rhs_flags = reads_flags(rhs)
        for k in range(i + 1, j):
            if dropped[k]:
                continue
            sk = stmts[k]
            wr = stmt_writes_reg(sk)
            if rhs_regs & wr:
                blocked, reason = True, "input register rewritten before the use"
                break
            if rhs_eff and (reads_flags_stmt(sk) or has_effop_stmt(sk)):
                blocked, reason = True, "effectful op cannot cross a flag reader/writer"
                break
            if rhs_flags and (wr & {"c", "ovr"}):
                blocked, reason = True, "flag read cannot cross a flag write"
                break
            mw = stmt_mem_write(sk)
            if mw is not None and rhs_mem:
                if mw in ("string", "*"):
                    blocked, reason = True, "memory read cannot cross a string/instruction store"
                    break
                w = classify_addr(mw[2], mw[1], ctx)
                for x in walk(rhs):
                    if x[0] == "mem" and may_alias(classify_addr(x[2], x[1], ctx), w, ctx.world):
                        blocked, reason = True, "memory read cannot cross a possibly-aliasing store"
                        break
                if blocked:
                    break
        if blocked:
            rep.hit("regfold.blocked", "%08X %s = … : %s" % (block.pc, name, reason))
            continue
        sj = stmts[j]
        adjacent = all(dropped[k] for k in range(i + 1, j))
        if rhs_eff:
            # root rule: an effectful op may replace a bare-register rhs; or,
            # when the use is the very next statement (no statement between,
            # so the order of every side effect is unchanged), nest into it
            if (sj[0] == "assign" and unparen(sj[2]) == (st[1][0], name)
                    and (sj[1][0] in ("reg", "t") or (sj[1][0] == "mem" and not count_reg(sj[1][2], name)))):
                newst = ("assign", sj[1], rhs)
            elif adjacent:
                newst = subst_stmt(sj, name, ("effnest", rhs))
                rep.hit("regfold.effop_nested")
            else:
                rep.hit("regfold.effop_not_root", "%08X %s" % (block.pc, name))
                continue
        else:
            newst = subst_stmt(sj, name, rhs)
        stmts[j] = newst
        dropped[i] = True
        rep.hit("regfold.folded")
    # live registers at the terminator: registers written by surviving defs
    # whose value is not overwritten later in the block
    live, last_def = set(), {}
    for i, st in enumerate(stmts):
        if dropped[i]:
            continue
        for r in stmt_writes_reg(st):
            if r in ("ac0", "ac1", "ac2", "ac3") and st[0] == "assign" and st[1][0] == "reg":
                last_def[r] = i
    live_out = ctx.routine.live_out.get(block.pc, set())
    live = {r for r in last_def if r in live_out}
    rep.hit("live.explicit_defs_at_exit.%d" % len(last_def))
    out = [st for i, st in enumerate(stmts) if not dropped[i]]
    return out, live


def reads_flags_stmt(st):
    return any(reads_flags(e) for e in stmt_exprs(st))


def has_effop_stmt(st):
    return any(has_effop(e) for e in stmt_exprs(st))


def subst_stmt(st, name, repl):
    k = st[0]
    if k == "assign":
        lhs = st[1]
        if lhs[0] == "mem":
            lhs = ("mem", lhs[1], subst_reg(lhs[2], name, repl))
        return ("assign", lhs, subst_reg(st[2], name, repl))
    if k == "assert":
        return ("assert", subst_reg(st[1], name, repl), st[2])
    if k == "goto":
        return ("goto", st[1], subst_reg(st[2], name, repl))
    if k == "rt_call":
        return ("rt_call", st[1], [subst_reg(a, name, repl) for a in st[2]], st[3])
    if k == "sassign":
        return ("sassign", subst_reg(st[1], name, repl), subst_reg(st[2], name, repl))
    if k == "cmp":
        return ("cmp", subst_reg(st[1], name, repl), subst_reg(st[2], name, repl))
    if k == "words":
        return ("words", subst_reg(st[1], name, repl), subst_reg(st[2], name, repl),
                subst_reg(st[3], name, repl))
    return st


# ----------------------------------------------------------------------------
# simplification (rewrite `simplify`): provable identities only
# ----------------------------------------------------------------------------

def nova17_body(n):
    """((e & 0xFFFF) | lsh(c, 16)) -> e ; else None."""
    n = unparen(n)
    if n[0] == "chain" and {op for op, _ in n[2]} == {"|"} and len(n[2]) == 1:
        a, b = unparen(n[1]), unparen(n[2][0][1])
        if b == ("call", "lsh", [("reg", "c"), ("num", 16, "dec")]):
            if a[0] == "chain" and {op for op, _ in a[2]} == {"&"} and len(a[2]) == 1 and is_num(a[2][0][1], 0xFFFF):
                return a[1]
    return None


def simplify(n, rep):
    n = map_children(n, lambda c: simplify(c, rep))
    k = n[0]
    if k == "chain":
        ops = {op for op, _ in n[2]}
        first = unparen(n[1])
        # ((e & 0xFFFF) | lsh(c, 16)) & 0xFFFF  ->  e & 0xFFFF   (c is 0/1)
        if ops == {"&"} and len(n[2]) == 1 and is_num(n[2][0][1], 0xFFFF):
            inner = first
            if inner[0] == "chain" and {op for op, _ in inner[2]} == {"|"} and len(inner[2]) == 1:
                a, b = unparen(inner[1]), unparen(inner[2][0][1])
                if b == ("call", "lsh", [("reg", "c"), ("num", 16, "dec")]):
                    if a[0] == "chain" and {op for op, _ in a[2]} == {"&"} and is_num(a[2][0][1], 0xFFFF):
                        rep.hit("simplify.nova16")
                        return a
        # lsh(((e & 0xFFFF) | lsh(c,16)), -16) & 1 -> c ;  -k (1..15) -> lsh(e, -k) & 1
        if ops == {"&"} and len(n[2]) == 1 and is_num(n[2][0][1], 1):
            if first[0] == "call" and first[1] == "lsh" and is_num(first[2][1]):
                k = -signed(num_val(first[2][1]))
                e = nova17_body(first[2][0])
                if e is not None and k == 16:
                    rep.hit("simplify.nova_carry")
                    return ("reg", "c")
                if e is not None and 1 <= k <= 15:
                    rep.hit("simplify.nova_bit")
                    return ("chain", ("call", "lsh", [e, ("num", (-k) & 0xFFFFFFFF, "dec")]), [("&", ("num", 1, "dec"))])
        # (((e & 0xFFFF) | lsh(c,16)) ^ 0xFFFF) & 0xFFFF -> (e ^ 0xFFFF) & 0xFFFF
        if ops == {"&"} and len(n[2]) == 1 and is_num(n[2][0][1], 0xFFFF):
            if first[0] == "chain" and {op for op, _ in first[2]} == {"^"} and len(first[2]) == 1 and is_num(first[2][0][1], 0xFFFF):
                e = nova17_body(first[1])
                if e is not None:
                    rep.hit("simplify.nova_com")
                    return ("chain", ("paren", ("chain", e, [("^", ("num", 0xFFFF, "hex"))])), [("&", ("num", 0xFFFF, "hex"))])
    if k == "paren" and n[1][0] != "chain":
        return n[1]
    return n


# ----------------------------------------------------------------------------
# record recognition (rewrite 3): base + i*stride + K
# ----------------------------------------------------------------------------

def flat_terms(n):
    """Flatten +/- chains and add()/sub() effops into [(sign, node)]."""
    n = unparen(n)
    if n[0] == "chain" and {op for op, _ in n[2]} <= {"+", "-"}:
        out = flat_terms(n[1])
        for op, x in n[2]:
            sub = flat_terms(x)
            out += [((s if op == "+" else -s), t) for s, t in sub]
        return out
    if n[0] == "effop" and n[1] in ("add", "sub") and len(n[2]) == 2:
        a = flat_terms(n[2][0])
        b = flat_terms(n[2][1])
        return a + ([(s, t) for s, t in b] if n[1] == "add" else [(-s, t) for s, t in b])
    return [(1, n)]


def scaled_term(n):
    """mul(I, s) / (I * s) / (s * I) -> (I, s) or None."""
    n = unparen(n)
    if n[0] == "effop" and n[1] == "mul" and len(n[2]) == 2:
        a, b = n[2]
        if is_num(b):
            return (a, signed(num_val(b)))
        if is_num(a):
            return (b, signed(num_val(a)))
    if n[0] == "chain" and {op for op, _ in n[2]} == {"*"} and len(n[2]) == 1:
        a, b = n[1], n[2][0][1]
        if is_num(b):
            return (a, signed(num_val(b)))
        if is_num(a):
            return (b, signed(num_val(a)))
    return None


def expand_regs(n, env, via):
    """Substitute every register read that the in-block env knows (exact:
    the register's current value); records the registers used in `via`."""
    if n[0] == "reg" and n[1] in env:
        via.append(n[1])
        v = env[n[1]]
        return ("paren", v) if v[0] == "chain" else v
    if n[0] in ("num", "bp", "str", "reg", "t", "regin"):
        return n
    return map_children(n, lambda c: expand_regs(c, env, via))


class Rec:
    __slots__ = ("base", "idx", "stride", "K", "via", "elem_local", "extra")

    def __init__(self, base, idx=None, stride=None, K=0):
        self.base, self.idx, self.stride, self.K = base, idx, stride, K
        self.via, self.elem_local, self.extra = [], None, []


def local_of(n, ctx):
    """('local', d, width) if n is a frame local read; else None."""
    n = unparen(n)
    if not ctx.sw.frame:
        return None
    if n[0] == "mem" and n[2][0] == "call" and n[2][1] == "wp":
        b, d = n[2][2]
        b = unparen(b)
        if is_num(d) and ((b == ("reg", "ac3") and ctx.fp) or
                          (b[0] == "regin" and b[1] == "ac3" and ctx.fp_in)):
            return (signed(num_val(d)), n[1])
    return None


def decompose(n, ctx, depth=0):
    """A record address -> Rec, or None.  Uses the in-block env for registers
    (exact: the register's current value) and the routine's local shapes for
    frame locals (exact: every def of the local has that shape)."""
    if depth > 6 or not ctx.sw.record:
        return None
    n = unparen(n)
    if depth == 0:
        via = []
        n = expand_regs(n, ctx.env, via)
        r = decompose(n, ctx, 1)
        if r is not None:
            r.via = list(dict.fromkeys(r.via + via))
        return r
    if n[0] == "reg":
        v = ctx.env.get(n[1])
        if v is None:
            return None
        r = decompose(v, ctx, depth + 1)
        if r:
            r.via.append(n[1])
        return r
    if n[0] == "call" and n[1] == "wp":
        b, d = n[2]
        if not is_num(d):
            return None
        r = decompose(b, ctx, depth + 1)
        if r:
            r.K += signed(num_val(d))
        return r
    if n[0] == "mem" and n[1] == "M32" and is_num(n[2]):
        a = num_val(n[2])
        if DATA_LO <= a < DATA_HI:
            return Rec(a)
        return None
    if n[0] == "call" and n[1] == "ind":
        r = decompose(n[2][0], ctx, depth + 1)
        if r is not None and r.stride is None and r.K == 0 and not r.extra:
            r.via.append("ind")
            return r
        return None
    loc = local_of(n, ctx)
    if loc and loc[1] == "M32":
        sh = ctx.shape_at(loc[0])
        if sh and sh[0] == "elemptr":
            r = Rec(sh[1], None, sh[2], sh[3])
            r.elem_local = loc[0]
            return r
        if sh and sh[0] == "baseptr":
            r = Rec(sh[1], None, None, sh[2])
            r.via.append("local_%d" % loc[0])
            return r
        return None
    terms = flat_terms(n)
    if len(terms) == 1:
        return None
    base, scaled, K, extra = None, None, 0, []
    for sign, t in terms:
        t = unparen(t)
        if is_num(t):
            K += sign * signed(num_val(t))
            continue
        st = scaled_term(t)
        if st and sign == 1:
            if scaled is not None:
                return None
            scaled = st
            continue
        loc = local_of(t, ctx)
        if loc and loc[1] == "M32" and sign == 1:
            sh = ctx.shape_at(loc[0])
            if sh and sh[0] == "scaled":
                if scaled is not None:
                    return None
                scaled = (("scaledlocal", loc[0], sh[1]), sh[1])
                continue
        r = decompose(t, ctx, depth + 1) if sign == 1 else None
        if r is not None and base is None:
            base = r
            continue
        extra.append((sign, t))          # an unknown term: base + ?
    if base is None:
        return None
    if scaled is not None:
        if base.stride is not None:
            return None
        base.idx, base.stride = scaled
    base.K += K
    base.extra += extra
    return base


# ----------------------------------------------------------------------------
# routine analysis: ownership, fp state, local shapes, liveness
# ----------------------------------------------------------------------------

class Routine:
    def __init__(self, world, entry_pc):
        self.world, self.entry = world, world.entries[entry_pc]
        self.pcs = world.routine_blocks.get(entry_pc, [])
        self.blocks = [world.blocks[pc] for pc in self.pcs]
        self.succs = {pc: [s for s in block_succs(world.blocks[pc], world.file_succs)]
                      for pc in self.pcs}
        self.preds = collections.defaultdict(list)
        for pc, ss in self.succs.items():
            for s in ss:
                self.preds[s].append(pc)
        self.shapes = {}
        self.guards = set()
        self.report = Report()
        self.fp_in = {pc: True for pc in self.pcs}
        self.fp_at = {}
        self.fp_out = {}

    def ctx_for(self, block):
        ctx = BlockCtx(self.world, self.entry, block)
        ctx.reach_in = getattr(self, "reach_in", {}).get(block.pc, {})
        ctx.routine = self
        ctx.fp = ctx.fp_in = self.fp_in.get(block.pc, True)
        ctx.env = dict(getattr(self, "env_in", {}).get(block.pc, {}))
        return ctx

    # -- fp state: is ac3 the frame pointer at each statement? ----------------
    def analyse_fp(self):
        self.env_in = {}
        for _ in range(4):
            changed = False
            for b in self.blocks:
                ctx = self.ctx_for(b)
                at = []
                for st in b.stmts:
                    at.append(ctx.fp)
                    env_after(st, ctx)
                self.fp_at[b.pc] = at
                self.fp_out[b.pc] = ctx.fp
                for s in self.succs[b.pc]:
                    if s in self.fp_in and not ctx.fp and self.fp_in[s]:
                        self.fp_in[s] = False
                        changed = True
                    # a block with exactly one predecessor starts with the
                    # predecessor's exit values (only those closed over memory,
                    # constants and fp — no other entry-register markers)
                    if self.world.sw.envinherit and s in self.fp_in and self.preds.get(s) == [b.pc] \
                            and b.stmts and b.stmts[-1][0] == "goto" and self.fp_in.get(s) == self.fp_in.get(b.pc):
                        inh = {r: v for r, v in ctx.env.items() if r in ("ac0", "ac1", "ac2")}
                        if inh != self.env_in.get(s, {}):
                            self.env_in[s] = inh
                            changed = True
            if not changed:
                break
        self.report.counts["env.inherited_blocks"] = sum(1 for v in self.env_in.values() if v)
        for b in self.blocks:
            if not self.fp_out[b.pc] and self.succs[b.pc]:
                self.report.hit("frame.ac3_not_fp_at_exit", "%08X -> %s" % (
                    b.pc, ",".join("%08X" % s for s in self.succs[b.pc])))

    # -- local shapes: reaching definitions of each frame local, by shape ----
    def analyse_shapes(self):
        """reach_in[pc][d] = the set of shapes of the defs of local d that
        reach the block entry (forward may-reach; a def kills earlier defs
        of the same word; a string/unknown/instruction store to an unknown
        address kills every local into ('other',))."""
        self.reach_in = {pc: {} for pc in self.pcs}
        self.shape_detail = {}
        dirty = set(self.pcs)
        rounds = 0
        while dirty and rounds < 12:
            rounds += 1
            changed = False
            todo, dirty = dirty, set()
            for b in self.blocks:
                if b.pc not in todo:
                    continue
                ctx = self.ctx_for(b)
                for st in b.stmts:
                    mw = stmt_mem_write(st)
                    if mw == "string" or mw == "*":
                        c = ptr_class(st[1][1], st[1], ctx.fp, self.world.sw) if st[0] == "sassign" else None
                        if c is None:
                            for d in set(ctx.reach_in) | set(ctx.shape_now):
                                ctx.shape_now[d] = {("other",)}
                        else:
                            for d in range(c[1], c[2]):
                                ctx.shape_now[d] = {("other",)}
                    elif mw is not None and classify_addr(mw[2], mw[1], ctx) is None:
                        for d in set(ctx.reach_in) | set(ctx.shape_now):
                            ctx.shape_now[d] = {("other",)}
                    env_after(st, ctx)
                out = dict(ctx.reach_in)
                out.update(ctx.shape_now)
                for s_ in self.succs[b.pc]:
                    if s_ not in self.reach_in:
                        continue
                    tgt = self.reach_in[s_]
                    for d, shs in out.items():
                        cur = tgt.get(d, set())
                        if not shs <= cur:
                            tgt[d] = cur | shs
                            changed = True
                            dirty.add(s_)
            if not changed:
                break
        # detail for the header: per local, the union of shapes over the routine
        allsh = collections.defaultdict(set)
        for pc, m in self.reach_in.items():
            for d, shs in m.items():
                allsh[d] |= shs
        for d, shs in allsh.items():
            good = {x for x in shs if x[0] in ("scaled", "elemptr", "baseptr")}
            if not good:
                continue
            if len(shs) == 1:
                self.shape_detail[d] = next(iter(shs))
            else:
                self.shape_detail[d] = ("mixed", tuple(sorted(str(x) for x in shs)))

    # -- cross-block register liveness (the strict surface at each terminator) --
    def analyse_reg_liveness(self):
        """live_out[pc] = registers some successor may read before writing.
        A call/rt_call/instruction/ret exit reads everything (callee entry,
        return value, unknown exits); goto exits read what the target's
        upward-exposed uses read."""
        regs = ("ac0", "ac1", "ac2", "ac3")
        ue, kill = {}, {}
        for b in self.blocks:
            u, kl = set(), set()
            for st in b.stmts:
                if st[0] == "instr":
                    u |= (instr_reads(st[2]) - kl)
                    if "*" in instr_writes(st[2]):
                        u |= set(regs) - kl
                    kl |= instr_writes(st[2])
                    continue
                for e in stmt_exprs(st):
                    u |= (reg_reads(e) - kl)
                kl |= stmt_writes_reg(st)
            ue[b.pc] = u & set(regs)
            kill[b.pc] = kl & set(regs)
        live_in = {pc: set(ue[pc]) for pc in self.pcs}
        self.live_out = {}
        changed = True
        while changed:
            changed = False
            for b in self.blocks:
                last = b.stmts[-1] if b.stmts else None
                if last is None or last[0] == "instr":
                    lo = set(regs)
                elif last[0] == "ret":
                    # WRTN restores ac0..ac2 from the frame image and ac3 from
                    # wfp (EagleStack.cpp:471-495): register values at `ret`
                    # are discarded; a return value is the frame.ac0_img write
                    lo = set()
                elif last[0] == "rt_call":
                    # the callee reads only its declared entry registers; the
                    # continuation sees the caller's ac0..ac2 preserved except a
                    # value returned in ac0 (RTConventions.md)
                    cont = live_in.get(last[3] + 4, set(regs))
                    lo = set(RT_ENTRY_READS.get(last[1], set())) | (cont - ({"ac0"} if last[1] in RT_RETURNS else set())) | {"ac3"}
                elif last[0] == "call":
                    # a game callee: registers preserved by WSAVS/WRTN, so the
                    # continuation's reads are the caller's values (conservative:
                    # a return value in ac0 is not assumed)
                    lo = set(live_in.get(last[5], set(regs))) | {"ac3"}
                else:
                    lo = set()
                    for s_ in self.succs[b.pc]:
                        lo |= live_in.get(s_, set(regs))
                self.live_out[b.pc] = lo
                li = ue[b.pc] | (lo - kill[b.pc])
                if li != live_in[b.pc]:
                    live_in[b.pc] = li
                    changed = True

    def analyse_guards(self):
        for b in self.blocks:
            ctx = self.ctx_for(b)
            for st in b.stmts:
                if st[0] == "assert":
                    for x in walk(st[1]):
                        if x[0] == "chain" and len(x[2]) == 1 and x[2][0][0] == ">s" and is_num(x[2][0][1], 0):
                            self.guards.add(repr(close_value(x[1], ctx.env, b.pc)))
                env_after(st, ctx)


def shape_of(closed, ctx):
    st = scaled_term(closed)
    if st:
        return ("scaled", st[1])
    r = decompose(closed, ctx)
    if r and r.stride is not None:
        return ("elemptr", r.base, r.stride, r.K)
    if r:
        return ("baseptr", r.base, r.K)
    return ("other",)


# -- liveness of a frame-local word range (rewrite 4's proof) -----------------

def range_access(st, lo, hi, fp, sw, folded_arg=None, ctx=None):
    """First-order classification of one statement's effect on the local word
    range [lo, hi): 'read' | 'write' (covers whole range) | None."""
    if ctx is None:
        ctx = DummyCtx(fp, sw)
    else:
        ctx.fp = fp
    if st[0] == "instr":
        m = instr_mnemonic(st[2])
        if m in INSTR_READS and m not in INSTR_MEMWRITE and m in INSTR_WRITES:
            return None            # register-only instruction: touches no frame word
        return "read"
    def alias_read(e):
        for x in walk(e):
            if x[0] == "mem":
                c = classify_addr(x[2], x[1], ctx)
                if may_alias(c, ("local", lo, hi), ctx.world):
                    return True
            if x[0] in ("located", "lit"):
                addr = unparen(x[1])
                if x[0] == "located":
                    c = ptr_class(addr, x, fp, sw)
                    if may_alias(c, ("local", lo, hi)):
                        return True
            if x[0] == "call" and x[1] == "wp":
                b, d = x[2]
                if fp and unparen(b) == ("reg", "ac3") and is_num(d):
                    dv = signed(num_val(d))
                    if lo <= dv < hi and x is not folded_arg:
                        return True        # address of the range escapes
                elif unparen(b) == ("reg", "ac3") and not fp:
                    return True
            if x[0] == "mem" and x[1] == "R":
                i2 = unparen(x[2])
                if not (i2[0] == "chain" and len(i2[2]) == 1 and unparen(i2[1])[0] in ("reg", "regin")
                        and unparen(i2[1])[1] == "ac3" and is_num(i2[2][0][1]) and signed(num_val(i2[2][0][1])) <= -12):
                    return True
        return False
    k = st[0]
    if k == "assign":
        lhs, rhs = st[1], st[2]
        if alias_read(rhs):
            return "read"
        if lhs[0] == "mem":
            if alias_read(lhs[2]):
                return "read"
            c = classify_addr(lhs[2], lhs[1], ctx)
            if c is None:
                return "read"          # unknown store: might partially write
            if c[0] == "local" and c[1] <= lo and c[2] >= hi:
                return "write"
            if may_alias(c, ("local", lo, hi), ctx.world):
                return "read"          # partial write
        return None
    if k == "sassign":
        dst, src = st[1], st[2]
        if alias_read(src):
            return "read"
        c = ptr_class(dst[1], dst, fp, sw)
        if c is None:
            return "read"
        if c[0] == "local" and c[1] <= lo and c[2] >= hi:
            return "write"
        if c[0] == "local" and c[1] == lo and dst[3]:
            # a varying assignment to the same base rewrites the length word;
            # every later varying read is bounded by it, so the range is dead
            # from here (a fixed-length or M16 read of the tail would say 'read')
            return "write"
        if may_alias(c, ("local", lo, hi)):
            return "read"
        return None
    if k in ("cmp", "words"):
        return "read" if any(alias_read(e) for e in stmt_exprs(st)) else None
    if k == "rt_call":
        for a in st[2]:
            if a is folded_arg:
                continue
            if alias_read(a):
                return "read"
        return None
    if k in ("assert", "goto"):
        return "read" if any(alias_read(e) for e in stmt_exprs(st)) else None
    return None


class DummyCtx:
    """A context with no in-block knowledge (used by the liveness scans)."""
    def __init__(self, fp, sw, world=None):
        self.fp, self.fp_in, self.sw, self.world = fp, fp, sw, world
        self.env = {}

    def shape_at(self, d):
        return None


def ptr_class(addr, located, fp, sw):
    """Word range a located string occupies when its address is a frame local."""
    addr = unparen(addr)
    if addr[0] == "call" and addr[1] == "wp" and fp and sw.frame:
        b, d = addr[2]
        if unparen(b) == ("reg", "ac3") and is_num(d):
            dv = signed(num_val(d))
            n = located[2]
            if n is None or not is_num(n):
                return None
            nb = num_val(n)
            words = (nb + 1) // 2 + (1 if located[3] else 0)
            return ("local", dv, dv + words)
    return None


def local_dead_after(rt, block, idx, lo, hi, stmts_by_pc, folded_arg=None):
    """True if the frame words [lo, hi) are provably not read on any path
    after statement idx of block (first access on every path is a write)."""
    sw = rt.world.sw
    def first_access(pc, start):
        stmts, fpat = stmts_by_pc[pc]
        ctx = rt.ctx_for(rt.world.blocks[pc])
        for i in range(0, len(stmts)):
            fp = fpat[i] if i < len(fpat) else False
            if i >= start:
                a = range_access(stmts[i], lo, hi, fp, sw, folded_arg if (pc == block.pc and i == idx) else None, ctx)
                if a:
                    return a
            env_after(stmts[i], ctx)
        return None
    a = first_access(block.pc, idx + 1)
    if a == "write":
        return True, None
    if a == "read":
        return False, "read later in the same block"
    # iterative may-live fixpoint over the routine's blocks (least fixpoint
    # from False: live iff some path reaches a read before a write)
    cache = rt.__dict__.setdefault("fa_cache", {})
    fa = cache.get((lo, hi))
    if fa is None:
        fa = {pc: first_access(pc, 0) for pc in stmts_by_pc}
        cache[(lo, hi)] = fa
    live_in = {pc: False for pc in stmts_by_pc}
    changed = True
    while changed:
        changed = False
        for pc in stmts_by_pc:
            if fa[pc] == "read":
                r = True
            elif fa[pc] == "write":
                r = False
            else:
                ss = rt.succs.get(pc)
                b = rt.world.blocks[pc]
                last = b.stmts[-1] if b.stmts else None
                if ss:
                    r = any(live_in.get(x, True) for x in ss)
                elif last and last[0] == "ret":
                    r = False
                else:
                    r = True                 # unknown exit: conservative
            if r != live_in[pc]:
                live_in[pc] = r
                changed = True
    def live(pc, _seen=None):
        return live_in.get(pc, True)
    ss = rt.succs.get(block.pc, [])
    for s in ss:
        if live(s, frozenset()):
            return False, "live in successor %08X" % s
    return True, None


# ----------------------------------------------------------------------------
# rendering (rewrites 1, 2, 3 apply here — naming never changes a value)
# ----------------------------------------------------------------------------

WSUF = {"M32": "w", "M16": "h", "M8": "b", "R": "ptr"}
FRAME_IMG = {0: "frame.ac3_c", -2: "frame.prev_wfp", -4: "frame.ac2_img",
             -6: "frame.ac1_img", -8: "frame.ac0_img", -10: "frame.word"}


def fmt_num(v, ctx=None, pos="value"):
    sv = signed(v)
    if DATA_LO <= v < DATA_HI and ctx is not None and ctx.sw.static:
        s = ctx.world.syms.get(v)
        if s:
            ctx.world.static_census[v]["addr_taken"] += 1
            ctx.world.static_census[v]["routines"].add(ctx.entry["pc"])
            return "&" + s
        return "0x%08X" % v
    if -65536 <= sv <= 65535:
        return str(sv)
    return "0x%08X" % v


class Renderer:
    def __init__(self, ctx):
        self.ctx = ctx
        self.w = ctx.world
        self.sw = ctx.sw
        self.rep = ctx.routine.report

    # -- frame ---------------------------------------------------------------
    def frame_name(self, d, width=None):
        e = self.ctx.entry
        top = 2 * e["frame"]
        if d >= 1:
            name = "local_%d" % d
            if d > top:
                self.rep.hit("frame.beyond_frame", "%s d=%d frame=%d (%s)" % (e["name"], d, e["frame"], self.ctx.block and "%08X" % self.ctx.block.pc))
                name += "?"
            else:
                self.rep.hit("frame.local")
            return name
        if d in FRAME_IMG:
            self.rep.hit("frame.image_word")
            return FRAME_IMG[d]
        if d < -10 and (-10 - d) % 2 == 0:
            n = (-10 - d) // 2
            if n <= e["argc"]:
                self.rep.hit("frame.arg")
                return "arg_%d" % n
            self.rep.hit("frame.beyond_argc", "%s d=%d argc=%d" % (e["name"], d, e["argc"]))
            return "arg_%d?" % n
        self.rep.hit("frame.odd_offset", "%s d=%d" % (e["name"], d))
        return "fpword_%d" % d

    def arg_of_R(self, idx):
        """R[ac3 + -k] -> arg N (by-reference pointer) or None."""
        idx = unparen(idx)
        if idx[0] == "chain" and len(idx[2]) == 1 and idx[2][0][0] == "+" \
                and unparen(idx[1]) == ("reg", "ac3") and is_num(idx[2][0][1]):
            d = signed(num_val(idx[2][0][1]))
            return d
        return None

    # -- record --------------------------------------------------------------
    def rec_name(self, r, width, kind):
        base = self.w.static_name(r.base)
        via = (" /*via %s*/" % ",".join(r.via)) if r.via else ""
        if r.extra:
            # base pointer plus a term that is neither i*stride nor a constant
            self.rep.hit("record.unknown_term", "%08X %s + %s" % (self.ctx.block.pc, base, " ".join(
                ("+" if sg == 1 else "-") + " " + self.expr(t) for sg, t in r.extra)))
            t = self.w.tables[(r.base, "?")]
            t["fields"][r.K][kind + ":" + (width or "?")] += 1
            t["routines"].add(self.ctx.entry["pc"]); t["sites"] += 1
            terms = "".join(" %s %s" % ("+" if sg == 1 else "-", self.expr(t_)) for sg, t_ in r.extra)
            if r.stride is not None:
                terms = " + %s*%d" % (self.expr(r.idx) if not (isinstance(r.idx, tuple) and r.idx[0] == "scaledlocal") else "local_%d.w/%d" % (r.idx[1], r.idx[2]), r.stride) + terms
            return "(%s%s)->f%s" % (base, terms, fmt_K(r.K)), via
        if r.stride is None:
            key = (r.base, None)
            t = self.w.tables[key]
            t["fields"][r.K][kind + ":" + (width or "?")] += 1
            t["routines"].add(self.ctx.entry["pc"]); t["sites"] += 1
            self.rep.hit("record.direct")
            return "%s->f%s" % (base, fmt_K(r.K)), via
        key = (r.base, r.stride)
        t = self.w.tables[key]
        t["fields"][r.K][kind + ":" + (width or "?")] += 1
        t["routines"].add(self.ctx.entry["pc"]); t["sites"] += 1
        if r.elem_local is not None:
            self.rep.hit("record.via_local")
            t["index_forms"]["elemptr local"] += 1
            return "local_%d->f%s" % (r.elem_local, fmt_K(r.K)), via
        idx = r.idx
        if isinstance(idx, tuple) and idx[0] == "scaledlocal":
            istr = "local_%d.w/%d" % (idx[1], idx[2])
            t["index_forms"]["scaled local"] += 1
        else:
            istr = self.expr(idx)
            t["index_forms"]["expr"] += 1
        closed = close_value(idx, self.ctx.env, self.ctx.block.pc) if not (isinstance(idx, tuple) and idx[0] == "scaledlocal") else None
        if closed is not None and repr(closed) in self.ctx.routine.guards:
            t["guarded"] += 1
        self.rep.hit("record.indexed")
        return "%s_rec%d[%s].f%s" % (base, r.stride, istr, fmt_K(r.K)), via

    # -- memory ---------------------------------------------------------------
    def mem(self, width, idx, kind="read"):
        ctx = self.ctx
        i = unparen(idx)
        suf = WSUF[width]
        # frame
        if self.sw.frame:
            if i[0] == "call" and i[1] == "wp":
                b, d = i[2]
                if unparen(b) == ("reg", "ac3") and is_num(d):
                    if ctx.fp:
                        return "%s.%s" % (self.frame_name(signed(num_val(d)), width), suf)
                    self.rep.hit("frame.ac3_not_fp", "%08X wp(ac3, %d)" % (ctx.block.pc, signed(num_val(d))))
            if width == "R" or (i[0] == "mem" and i[1] == "R"):
                pass
        if i[0] == "mem" and i[1] == "R" and self.sw.frame and ctx.fp:
            d = self.arg_of_R(i[2])
            if d is not None:
                nm = self.frame_name(d)
                return "%s->%s" % (nm, suf)
        if width == "M8" and self.sw.frame:
            bl = byte_local(i, ctx)
            if bl is not None:
                return "%s.b%d" % (self.frame_name(bl[0]), bl[1])
        # static
        if i[0] == "num":
            a = i[1]
            if DATA_LO <= a < DATA_HI and self.sw.static:
                c = self.w.static_census[a]
                c["routines"].add(ctx.entry["pc"])
                (c["reads"] if kind == "read" else c["writes"])[width] += 1
                self.rep.hit("static.named" if a in self.w.syms else "static.generated")
                return "%s.%s" % (self.w.static_name(a), suf)
            if SLOT_LO <= a < SLOT_HI:
                si = self.w.slot_index.get(a)
                if si:
                    self.rep.hit("callargs.slot_unfolded", "%08X slot %08X" % (ctx.block.pc, a))
                    return "%s.arg_%d.slot" % (si[0]["name"], si[1])
                self.rep.hit("callargs.slot_unknown", "%08X %08X" % (ctx.block.pc, a))
                return "slot_%08X" % a
        # record
        if self.sw.record:
            r = decompose(idx, ctx)
            if r is not None:
                name, via = self.rec_name(r, width, kind)
                return "%s.%s%s" % (name, suf, via)
        self.rep.hit("mem.unresolved", "%08X %s[%s]" % (ctx.block.pc, width, self.expr(idx)))
        return "%s[%s]" % (width, self.expr(idx))

    # -- pointer-valued expressions (args, string addresses) -----------------
    def ptr_named(self, n):
        """A readable name for a pointer-valued expression, or None."""
        ctx = self.ctx
        i = unparen(n)
        if i[0] == "call" and i[1] == "wp" and self.sw.frame:
            b, d = i[2]
            if unparen(b) == ("reg", "ac3") and is_num(d) and ctx.fp:
                return "&" + self.frame_name(signed(num_val(d)))
        if i[0] == "mem" and i[1] == "R" and self.sw.frame and ctx.fp:
            d = self.arg_of_R(i[2])
            if d is not None:
                return self.frame_name(d)
        if self.sw.record and i[0] in ("call", "chain", "effop", "mem"):
            r = decompose(i, ctx)
            if r is not None:
                name, via = self.rec_name(r, None, "addr")
                return "&%s%s" % (name, via)
        return None

    def ptr(self, n):
        p = self.ptr_named(n)
        if p is not None:
            return p
        i = unparen(n)
        if i[0] == "num":
            return fmt_num(i[1], self.ctx)
        return self.expr(n)

    # -- expressions ----------------------------------------------------------
    def expr(self, n, top=False):
        if top:
            n = unparen(n)
        k = n[0]
        if k == "num":
            return fmt_num(n[1], self.ctx)
        if k == "bp":
            return "0x%08X:%d" % (n[1], n[2])
        if k == "str":
            return '"%s"' % n[1]
        if k == "reg":
            return n[1]
        if k == "regin":
            if len(n) > 2 and n[2] is not None and n[2] != self.ctx.block.pc:
                return "%s#in@%08X" % (n[1], n[2])
            return n[1] + "#in"
        if k == "t":
            return n[1]
        if k == "paren":
            inner = self.expr(n[1])
            return "(" + inner + ")" if unparen(n[1])[0] == "chain" else inner
        if k == "chain":
            s = self.expr(n[1])
            for op, x in n[2]:
                s += " %s %s" % (op, self.expr(x))
            return s
        if k == "not":
            return "~" + self.expr(n[1])
        if k == "lnot":
            return "!" + self.expr(n[1])
        if k == "mem":
            if n[1] == "R":
                if self.sw.frame and self.ctx.fp:
                    d = self.arg_of_R(n[2])
                    if d is not None:
                        return self.frame_name(d)
                return "R[%s]" % self.expr(n[2])
            return self.mem(n[1], n[2])
        if k == "call":
            if n[1] == "wp":
                p = self.ptr_named(n)
                if p is not None:
                    return p
                return "wp(%s, %s)" % (self.expr(n[2][0]), self.expr(n[2][1]))
            return "%s(%s)" % (n[1], ", ".join(self.expr(a) for a in n[2]))
        if k == "effop":
            return "%s(%s)" % (n[1], ", ".join(self.expr(a) for a in n[2]))
        if k == "lit":
            self.rep.hit("literal.piece")
            return '"%s"' % n[2]
        if k == "located":
            addr = self.ptr(n[1])
            if n[2] is None:
                return "vstr(%s)" % addr
            return "%s(%s, %s)" % ("vstr" if n[3] else "str", addr, self.expr(n[2]))
        if k == "ann":
            return self.expr(n[1]) + " /*%s*/" % n[2]
        if k == "effnest":
            return self.expr(n[1])
        raise ValueError(k)

    def lhs(self, n):
        if n[0] in ("reg", "t"):
            return n[1]
        if n[0] == "mem":
            return self.mem(n[1], n[2], "write")
        return self.expr(n)


def fmt_K(K):
    return str(K) if K >= 0 else "m%d" % -K


# ----------------------------------------------------------------------------
# per-routine rendering
# ----------------------------------------------------------------------------

def label(pc):
    return "L_%08X" % pc


def render_routine(world, entry_pc, header_lines):
    rt = Routine(world, entry_pc)
    rt.analyse_fp()
    rt.analyse_shapes()
    rt.analyse_reg_liveness()
    rt.analyse_guards()
    sw = world.sw
    rep = rt.report
    e = rt.entry
    out = []
    out += header_lines
    out.append("; routine %s @%08X  argc %d  frame 0x%02X (%d local words)  variant %s  flags %s  blocks %d" % (
        e["name"], e["pc"], e["argc"], e["frame"], 2 * e["frame"], e["variant"], e["flags"] or "-", len(rt.blocks)))
    if rt.shape_detail:
        for d in sorted(rt.shape_detail):
            sh = rt.shape_detail[d]
            if sh[0] == "scaled":
                out.append(";   local_%d holds index*%d (every def is mul(i, %d))" % (d, sh[1], sh[1]))
            elif sh[0] == "elemptr":
                out.append(";   local_%d holds an element pointer into %s_rec%d (+%d)" % (d, world.static_name(sh[1]), sh[2], sh[3]))
            elif sh[0] == "baseptr":
                out.append(";   local_%d holds %s + %d" % (d, world.static_name(sh[1]), sh[2]))
            elif sh[0] == "mixed":
                out.append(";   local_%d is reused: defs of shape %s (record naming applies where one shape reaches)" % (d, " | ".join(sh[1])))
                rep.hit("record.local_reused", "%s local_%d" % (e["name"], d))
    out.append("")
    out.append("%s(%s):" % (e["name"], ", ".join("arg_%d" % n for n in range(1, e["argc"] + 1))))

    # pass A: fold every block, keep the env at each surviving statement
    folded = {}
    for b in rt.blocks:
        ctx = rt.ctx_for(b)
        stmts, live = fold_block(b, ctx, rep)
        if sw.simplify:
            stmts = [simplify_stmt(st, rep) for st in stmts]
        folded[b.pc] = (stmts, live)
        rep.hit("live.hist.%d" % len(live))

    # pass B: literal folding (needs liveness over the folded routine)
    fp_tables = {}
    for b in rt.blocks:
        stmts, live = folded[b.pc]
        # recompute fp per surviving statement
        ctx = rt.ctx_for(b)
        fpat = []
        for st in stmts:
            fpat.append(ctx.fp)
            env_after(st, ctx)
        fp_tables[b.pc] = (stmts, fpat)
    literal_drops = {}
    if sw.literal:
        for b in rt.blocks:
            stmts, fpat = fp_tables[b.pc]
            for i, st in enumerate(stmts):
                if st[0] != "sassign" or st[2][0] != "lit":
                    continue
                dst = st[1]
                if not dst[3] or dst[2] is None:
                    rep.hit("literal.dst_not_varying")
                    continue
                c = ptr_class(dst[1], dst, fpat[i], sw)
                if c is None:
                    rep.hit("literal.dst_not_local", "%08X %s" % (b.pc, "?"))
                    continue
                lo, hi = c[1], c[2]
                d = signed(num_val(unparen(dst[1])[2][1]))
                # find the consuming rt_call in this block or through unconditional single-pred successors
                path = [(b.pc, i + 1)]
                found, reason = None, None
                pc, start = b.pc, i + 1
                for hop in range(4):
                    s2, fp2 = fp_tables[pc]
                    for j in range(start, len(s2)):
                        sj = s2[j]
                        if sj[0] == "rt_call":
                            hits = [a for a in sj[2] if unparen(a) == ("call", "wp", [("reg", "ac3"), ("num", d & 0xFFFFFFFF, "dec")]) or
                                    (unparen(a)[0] == "call" and unparen(a)[1] == "wp" and unparen(unparen(a)[2][0]) == ("reg", "ac3") and is_num(unparen(a)[2][1]) and signed(num_val(unparen(a)[2][1])) == d)]
                            if len(hits) == 1 and fp2[j]:
                                found = (pc, j, hits[0])
                            elif hits:
                                reason = "argument appears %d times / fp unknown" % len(hits)
                            else:
                                reason = "next rt_call does not take the local"
                            break
                        a = range_access(sj, lo, hi, fp2[j], sw)
                        if a:
                            reason = "%s of the local before the call (%08X stmt %d)" % (a, pc, j)
                            break
                    if found or reason:
                        break
                    last = s2[-1] if s2 else None
                    if last and last[0] == "goto" and len(last[1]) == 1 and len(rt.preds.get(last[1][0], [])) == 1 and last[1][0] in fp_tables:
                        pc, start = last[1][0], 0
                        continue
                    reason = "no consuming rt_call before the block exit"
                    break
                if not found:
                    rep.hit("literal.unproven", "%08X %s: %s" % (b.pc, '"%s"' % st[2][2][:30], reason))
                    continue
                cpc, j, arg = found
                dead, why = local_dead_after(rt, world.blocks[cpc], j, lo, hi, fp_tables, folded_arg=arg)
                if not dead:
                    rep.hit("literal.unproven", "%08X %s: live after the call (%s)" % (b.pc, '"%s"' % st[2][2][:30], why))
                    continue
                # fold: replace the argument by the literal, drop the assignment
                s2 = fp_tables[cpc][0]
                call = s2[j]
                newargs = [("lit", st[2][1], st[2][2]) if a is arg else a for a in call[2]]
                s2[j] = ("rt_call", call[1], newargs, call[3])
                literal_drops[(b.pc, i)] = "vstr(&local_%d, %s) = \"%s\"" % (d, num_val(dst[2]), st[2][2])
                rep.hit("literal.folded")

    # pass C: render
    order = rt.pcs
    for bi, b in enumerate(rt.blocks):
        stmts, fpat = fp_tables[b.pc]
        live = folded[b.pc][1]
        ctx = rt.ctx_for(b)
        rd = Renderer(ctx)
        hdr = label(b.pc) + ":"
        if not rt.fp_in.get(b.pc, True):
            hdr += "        ; ac3 is not fp on entry (predecessor left it repurposed)"
        out.append(hdr)
        # callargs: collect slot stores for the terminator
        call_args, call_drop = None, set()
        last = stmts[-1] if stmts else None
        if sw.callargs and last and last[0] == "call":
            callee = world.entries.get(last[1])
            if callee and callee["argc"] > 0:
                slots = {}
                okfold = True
                for i, st in enumerate(stmts[:-1]):
                    if st[0] == "assign" and st[1][0] == "mem" and st[1][1] == "M32" and is_num(st[1][2]):
                        a = num_val(st[1][2])
                        si = world.slot_index.get(a)
                        if si and si[0]["pc"] == callee["pc"]:
                            if si[1] in slots:
                                okfold = False
                            slots[si[1]] = i
                if okfold and set(slots) == set(range(1, callee["argc"] + 1)):
                    # each stored value must be renderable at the store point and
                    # its registers untouched until the call
                    rendered = {}
                    ctx2 = rt.ctx_for(b)
                    rd2 = Renderer(ctx2)
                    for i, st in enumerate(stmts[:-1]):
                        for n, si in slots.items():
                            if si == i:
                                val = st[2]
                                regs = reg_reads(val)
                                clean = True
                                for k in range(i + 1, len(stmts) - 1):
                                    if regs & stmt_writes_reg(stmts[k]):
                                        clean = False
                                    mw = stmt_mem_write(stmts[k])
                                    if mw is not None and (mw in ("string", "*") or may_alias(classify_addr(mw[2], mw[1], ctx2), ("slot", num_val(st[1][2]), num_val(st[1][2]) + 2))):
                                        clean = False
                                if not clean:
                                    okfold = False
                                rendered[n] = rd2.ptr(val)
                        env_after(st, ctx2)
                    if okfold:
                        call_args = [rendered[n] for n in range(1, callee["argc"] + 1)]
                        call_drop = set(slots.values())
                        rep.hit("callargs.folded")
                    else:
                        rep.hit("callargs.unfolded", "%08X %s: value not stable to the call" % (b.pc, callee["name"]))
                else:
                    rep.hit("callargs.unfolded", "%08X %s: slots %s of %d" % (b.pc, callee["name"], sorted(slots), callee["argc"]))
        for i, st in enumerate(stmts):
            ctx.fp = fpat[i]
            line = None
            if (b.pc, i) in literal_drops:
                line = "; folded into the call below: " + literal_drops[(b.pc, i)]
            elif i in call_drop:
                continue
            else:
                line = render_stmt(st, rd, rt, b, bi, order, call_args)
            out.append("  " + line)
            env_after(st, ctx)
        if live and sw.regfold:
            out.append("  ; live at exit: " + ", ".join(sorted(live)))
        out.append("")
    return rt, out


def simplify_stmt(st, rep):
    k = st[0]
    f = lambda e: simplify(e, rep)
    if k == "assign":
        lhs = st[1]
        if lhs[0] == "mem":
            lhs = ("mem", lhs[1], f(lhs[2]))
        return ("assign", lhs, f(st[2]))
    if k == "assert":
        return ("assert", f(st[1]), st[2])
    if k == "goto":
        return ("goto", st[1], f(st[2]))
    if k == "rt_call":
        return ("rt_call", st[1], [f(a) for a in st[2]], st[3])
    return st


def render_stmt(st, rd, rt, b, bi, order, call_args):
    sw = rd.sw
    k = st[0]
    if k == "instr":
        rd.rep.hit("instr.kept", "%08X %s" % (st[1], st[2][:24]))
        return "@%08X %s" % (st[1], st[2])
    if k == "ret":
        return "return"
    if k == "assign":
        lhs, rhs = st[1], st[2]
        if lhs == ("reg", "ac3") and unparen(rhs) == ("reg", "wfp"):
            return "ac3 = wfp        ; fp restored"
        return "%s = %s" % (rd.lhs(lhs), rd.expr(rhs, top=True))
    if k == "assert":
        cond = rd.expr(st[1], top=True)
        if sw.sketch and st[2] and st[2].startswith("DERR"):
            return "bounds_check %s   ; %s" % (cond, st[2])
        return "assert(%s%s)" % (cond, (', "%s"' % st[2]) if st[2] else "")
    if k == "goto":
        labels, e = st[1], st[2]
        if len(labels) == 1:
            t = labels[0]
            nxt = order[bi + 1] if bi + 1 < len(order) else None
            if sw.sketch:
                if t == nxt:
                    return "; fall through to %s" % label(t)
                if t <= b.pc:
                    return "goto %s        ; loop back" % label(t)
            return "goto %s" % label(t)
        if len(labels) == 2 and sw.sketch:
            return "if (%s) goto %s else goto %s" % (rd.expr(e, top=True), label(labels[1]), label(labels[0]))
        return "goto [%s] %s" % (", ".join(label(x) for x in labels), rd.expr(e, top=True))
    if k == "rt_call":
        args = ", ".join(rd.ptr(a) for a in st[2])
        return "%s(%s)        ; site %08X" % (st[1], args, st[3])
    if k == "call":
        name = rd.w.routine_name(st[1])
        if call_args is not None:
            return "call %s(%s)        ; site %08X" % (name, ", ".join(call_args), st[4])
        return "call %s args=%d        ; site %08X (arg slots not folded)" % (name, st[2], st[4])
    if k == "sassign":
        return "%s = %s" % (rd.expr(st[1]), rd.expr(st[2]))
    if k == "cmp":
        return "ac1 = cmp(%s, %s)" % (rd.expr(st[1]), rd.expr(st[2]))
    if k == "words":
        return "words(%s, %s) = words(%s, %s)" % (rd.ptr(st[1]), rd.expr(st[3]), rd.ptr(st[2]), rd.expr(st[3]))
    return "?? " + repr(st)


# ----------------------------------------------------------------------------
# main
# ----------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--book", required=True)
    ap.add_argument("--addrbook", required=True)
    ap.add_argument("--symbols", required=True)
    ap.add_argument("--blocks", required=True)
    ap.add_argument("--out", help="directory for <ROUTINE>.txt renderings")
    ap.add_argument("--routines", default="", help="comma-separated routine names (addrbook names)")
    ap.add_argument("--top-statics", type=int, default=0, help="also render the N routines with the most distinct static references")
    ap.add_argument("--census", help="write the machine census here")
    ap.add_argument("--provenance", default="", help="free text stamped in every header (branch, commit)")
    for k in ("frame", "static", "callargs", "record", "literal", "regfold", "simplify", "sketch", "heapdisjoint", "argdisjoint", "envinherit"):
        ap.add_argument("--no-" + k, dest=k, action="store_false")
    args = ap.parse_args()
    t0 = time.time()

    sw = Switches(**{k: getattr(args, k) for k in ("frame", "static", "callargs", "record", "literal", "regfold", "simplify", "sketch", "heapdisjoint", "argdisjoint", "envinherit")})
    header, blocks = load_book(args.book)
    entries = load_addrbook(args.addrbook)
    syms = load_symbols(args.symbols)
    file_succs = load_block_succs(args.blocks)
    world = World(blocks, entries, syms, file_succs, sw)
    book_sha = sha256(args.book)
    hdr = ["; readable.py (Project 34 prototype) — rendering, not an artifact anything executes",
           "; book %s sha256=%s (%s)" % (os.path.basename(args.book), book_sha, header[0] if header else "?"),
           "; " + args.provenance if args.provenance else "; provenance: (none given)",
           "; addrbook sha256=%s  symbols sha256=%s  blocks sha256=%s" % (
               sha256(args.addrbook)[:16], sha256(args.symbols)[:16], sha256(args.blocks)[:16]),
           "; switches: " + sw.text(),
           "; naming: local_<d> = frame word d (ac3+d); arg_<N> = by-reference argument N (R[ac3-10-2N]);",
           ";   .w/.h/.b = 32/16/8-bit access; &x = address of x; p->w = *p; X_rec<s>[i].f<K> = record i (1-based),",
           ";   stride s words, field at word offset K from X (K is the raw displacement — see Census.md for origins);",
           ";   r#in = the value register r had at block entry; vstr(p, n) = CHAR(n) VARYING at p; str(p, n) = CHAR(n).",
           ""]

    # whole-book census: render every routine (output discarded unless chosen)
    per_routine = {}
    chosen = set(x for x in args.routines.split(",") if x)
    name_to_pc = {e["name"]: pc for pc, e in entries.items()}
    static_refs = collections.Counter()
    renderings = {}
    live_hist = collections.Counter()
    all_rep = Report()
    for pc in sorted(entries):
        if pc not in world.routine_blocks:
            continue
        before = {a: (dict(c["reads"]), dict(c["writes"])) for a, c in world.static_census.items()}
        rt, text = render_routine(world, pc, hdr)
        per_routine[pc] = rt
        renderings[pc] = text
        distinct = {a for a, c in world.static_census.items() if pc in c["routines"]}
        static_refs[pc] = len(distinct)
        for k, v in rt.report.counts.items():
            all_rep.counts[k] += v
            for ex in rt.report.examples.get(k, []):
                if len(all_rep.examples[k]) < 12:
                    all_rep.examples[k].append(ex)
    if args.top_statics:
        for pc, n in static_refs.most_common(args.top_statics):
            chosen.add(entries[pc]["name"])
    written = []
    if args.out:
        os.makedirs(args.out, exist_ok=True)
        for name in sorted(chosen):
            pc = name_to_pc.get(name)
            if pc is None or pc not in renderings:
                print("no such routine: %s" % name, file=sys.stderr)
                continue
            path = os.path.join(args.out, name.replace("/", "_") + ".txt")
            rt = per_routine[pc]
            text = list(renderings[pc])
            text.append("; ---- this routine's rewrite report ----")
            for k in sorted(rt.report.counts):
                text.append("; %-32s %5d" % (k, rt.report.counts[k]))
                for ex in rt.report.examples.get(k, [])[:6]:
                    text.append(";     e.g. " + ex)
            with open(path, "w") as f:
                f.write("\n".join(text) + "\n")
            written.append(path)
    if args.census:
        with open(args.census, "w") as f:
            w = lambda s="": f.write(s + "\n")
            w("# readable.py machine census")
            w("# book sha256=%s  blocks=%d  routines=%d  unreached-blocks(fallback attribution)=%d" % (
                book_sha, len(blocks), len(per_routine), len(world.fallback)))
            w("# provenance: " + args.provenance)
            w("# switches: " + sw.text())
            w()
            w("## rewrite counts (whole book)")
            for k in sorted(all_rep.counts):
                w("%-36s %6d" % (k, all_rep.counts[k]))
                for ex in all_rep.examples.get(k, []):
                    w("    e.g. " + ex)
            w()
            w("## registers left live at the terminator (blocks by count)")
            w("  live = defined in the block, reaching its exit explicitly, and read by some successor before being written")
            for k in sorted(all_rep.counts):
                if k.startswith("live.hist."):
                    w("  %s live: %d blocks" % (k.split(".")[-1], all_rep.counts[k]))
            w("  explicit register defs reaching the exit (live or not):")
            for k in sorted(all_rep.counts):
                if k.startswith("live.explicit_defs_at_exit."):
                    w("  %s defs: %d blocks" % (k.split(".")[-1], all_rep.counts[k]))
            w()
            w("## routines by distinct static references")
            for pc, n in static_refs.most_common(25):
                w("  %-28s %3d" % (entries[pc]["name"], n))
            w()
            w("## static usage census (addr  name  routines  reads[width]  writes[width]  addr-taken  compared-with)")
            for a in sorted(world.static_census):
                c = world.static_census[a]
                w("  %08X %-24s r=%-3d %-20s %-20s &=%-3d cmp=%s" % (
                    a, world.static_name(a), len(c["routines"]),
                    ",".join("%s:%d" % kv for kv in sorted(c["reads"].items())) or "-",
                    ",".join("%s:%d" % kv for kv in sorted(c["writes"].items())) or "-",
                    c["addr_taken"], ",".join(sorted(c["cmp"])) or "-"))
            w()
            w("## record tables (base, stride) -> fields seen (raw K: access kinds)")
            for key in sorted(world.tables, key=lambda k: (k[0], -1 if k[1] == "?" else (k[1] or 0))):
                t = world.tables[key]
                base, stride = key
                ks = sorted(t["fields"])
                w("  %s stride=%s sites=%d routines=%d guarded-index-sites=%d index-forms=%s" % (
                    world.static_name(base), stride, t["sites"], len(t["routines"]), t["guarded"],
                    dict(t["index_forms"]) or "-"))
                if stride and stride != "?":
                    w("    min K %d -> if the first field seen is field 0, origin = base + %d (1-based)" % (ks[0], ks[0] + stride))
                for K in ks:
                    w("    f%-7s %s" % (fmt_K(K), ", ".join("%s:%d" % kv for kv in sorted(t["fields"][K].items()))))
            w()
            w("## local shapes per routine (record recognition through frame locals)")
            for pc, rt in sorted(per_routine.items()):
                if rt.shape_detail:
                    w("  %s: %s" % (entries[pc]["name"], "; ".join(
                        "local_%d=%s" % (d, s if s[0] != 'mixed' else 'mixed') for d, s in sorted(rt.shape_detail.items()))))
    dt = time.time() - t0
    print("readable.py: %d blocks, %d routines, %d renderings written, %.1f s%s" % (
        len(blocks), len(per_routine), len(written), dt, "  ** > 10 s **" if dt > 10 else ""))


if __name__ == "__main__":
    main()
