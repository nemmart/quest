# Project 39 — the static link: uplevel variable access in nested procedures

GOAL: build the **static link** — PL/I's nested-procedure mechanism, by
which an inner procedure reads and writes variables of its enclosing
one — and finish the routines it unblocks. P38's diagnosis: throughput
is no longer bounded by the register model (that work is done) but by
unmodelled constructs, and the static link is the largest of them.
**23 nested entries across 13 parents** carry it; five are small enough
to finish inside this project.

This is a **construct project with routine validators**, not a matching
project. Success = the construct built and derived (not fitted), plus
routines finished with it. P38's binding constraint stands:

> **Stage nothing.** A routine is either finished (100 % MATCH, primary
> and `--folded`, native-clean) or explicitly abandoned with a reason.

Hi Claude! Read docs/METHOD.md first, **including §16** (R39/R40, the
detector, "consistent is not evidence"). Foundation: **compiler/
CODEGEN_RULES.md** (the model — P38 added R8d, FP_COST, R41, and VOIDED
R3b in favour of R3b′; read §9 closely), **compiler/translate.py**,
**compiler/ircmp.py** (seven equivalences, FIXED unless a construct
forces an eighth — a ruling), and the P38 companions: **docs/Project38/
FIRE1_ABANDONED.md** (the static link's first evidence, and the routine
this project should return to), docs/Project38/REPORT.md,
docs/Project37/{REPORT,DIED_PREREGISTERED,InadmissibleEvidence,
FrameRelocation}.md, docs/Project35/REPORT.md, docs/Project36/REPORT.md.
Also game/routines/*.c, game/quest_rt.h, game/declarations.h,
compiler/gen_declarations.py, docs/IR.md (ir 6), docs/M4aDesign.md (the
frame layout — it documents where the static link arrives). TREE
VINTAGE: main after the P38 merge (57d6481 or later) — state it; verify
docs/Provenance.md. Nothing here touches emulation/ or Disassembled/;
no battery.

**Run `compiler/crossings.py` first** (METHOD §16, now at that path).

## What is known about the construct

From FIRE1_ABANDONED.md, corroborated independently by M4aDesign.md's
frame layout and ON_ERROR_CATALOG — *not* fitted from one routine:

- A nested procedure receives the **enclosing frame pointer** as a
  static link; it arrives in **ac1** and `WSAVS` saves it at
  **`wp(fp, -6)`**.
- An uplevel reference is therefore a double indirection: load the link
  from `wp(fp, -6)`, then displace off it — FIRE.1 has four such
  references.
- The addrbook's nested entries are named `PARENT.N@addr`; their `argc`
  is the *inner* procedure's own argument count (many are 0), and their
  frames are their own.

What is NOT known and must be derived, not assumed: how the compiler
allocates the register holding the link (R41's {ac2, ac3} base class is
the obvious hypothesis and the obvious trap — check whether the link is
a "base" in R41's sense or an ordinary value); whether an uplevel write
differs from an uplevel read; whether a doubly-nested procedure exists
(the addrbook's names suggest not — verify) and what it would imply;
and how the parent *calls* the nested procedure (an ordinary decorated
call, or something else — the call site is evidence the inner routine
cannot give you).

## The C spelling — a ruling request for the gate

There is no spelling today for "a variable of the enclosing procedure"
in quest_rt.h or declarations.json. Design one and bring it to the gate
with alternatives considered. The obvious candidates:

- an `UPLEVEL(name)` accessor macro naming the parent's slot;
- declaring the parent's frame as a struct type and passing it, which
  is closer to what C would do but further from what the compiler did;
- a `__enclosing` qualifier on the declaration.

Pick on this criterion: the source must make the double indirection
*visible* (it is a real cost the compiler pays) without inventing
machinery the IR does not have. Note that the C++ native build must
also compile it, and that a nested procedure in C is not a thing — the
`.N@` routines will be ordinary C functions taking the parent's frame
somehow.

## The routines (validators, in order)

Small nested entries, from the addrbook (statement counts are per the
R40 caveat — verify with `crossings.py`):

| routine | argc | frame | stmts | note |
|---|---|---|---|---|
| **DROP.1@70169683** | 0 | 0x11 | 58 | smallest nested entry with a plain frame |
| **KILL_PLAYER.4@7016E5CB** | 1 | 0x05 | 58 | takes an argument *and* a static link — the two mechanisms together |
| **LIST_PLAYERS.3@7016F556** | 1 | 0x03 | 62 | smallest frame; a second witness for the argument+link shape |
| **FIRE.2@7016A461** | 0 | 0x0A | 70 | sibling of the abandoned FIRE.1, same parent |
| **FIRE.1@7016A3BD** | 0 | 0x04 | 89 | **return to it**: P38 abandoned it for the static link plus `COM.# 1,1,SZR` (test against −1) and `WADC 0,0` (−1 constant). If the link closes, those two are small and this routine becomes the proof the abandonment was construct-shaped, not routine-shaped |

Take them in that order; each finished before the next. If one needs a
construct beyond the static link and the two small forms above, abandon
it with a reason and move on — but say what it needs, because that is
the next project's input.

## Part 1 — plan gate

1. Re-baseline: four routines 349/349 primary, 242/242 folded,
   `--selftest` PASS, `crossings.py` clean. State it.
2. The construct as you read it from the book across **all five**
   validators plus FIRE.1 — not from one — with the register question
   answered by evidence (is the link a base under R41 or not?) and the
   parent-side call form.
3. The C spelling, with alternatives and the reason for your choice.
   **This is the ruling request.**
4. What you expect to add to CODEGEN_RULES, with confidences.
5. Anything in the carried-over rules you believe is wrong.

STOP AND REPORT. Keep it short — a gate with one ruling request should
not cost a turn beyond the reading.

## Part 3 — report

Per routine: the C, the match table, the rules exercised, pragma count.
The construct's rules with their witnesses and confidences. Any rule
promoted, amended or voided, with the instruction pair. Then: **routines
finished this project, and the running total out of ~130** — and the
next blocking construct, with the evidence for calling it next
(twins/arena and floats are the current candidates; if the static link
turns out to unblock more than five routines, say how many).

## Boundaries — BINDING

1. Files: compiler/, game/, docs/Project39/. Nothing in emulation/,
   Disassembled/, no artifact, no battery.
2. **Stage nothing.** Finish or abandon-with-a-reason.
3. The four matched routines must never regress; `--selftest` green
   after every rule change.
4. Derive before asserting. A rule with one witness is recorded at C
   and named as such; a correlation with no mechanism is not a rule
   (§16). A pragma is an admission, recorded and counted.
5. No match number for a partial translation; no verdict for an
   untested rule; "not measured" is a valid line.
6. Commit and push at every routine boundary; never leave the tree in a
   regressed state unpushed (P38's near-miss). Deliver a Work.tgz AND
   push the branch (`p39-static-link`).
