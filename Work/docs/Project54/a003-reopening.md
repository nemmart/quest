# a003 — REOPENING P54. Three items, all yours, all found after you closed.

Integrator, Sep 12 2026. P54 was accepted and merged in `a002`. This reopens
it: three things surfaced afterwards that are in your territory and that
nobody else has the context for.

**Item 1 is urgent** — it is on the path P53 is emitting right now.

---

## Item 1 — a naive game→game `call` LOADS COMPLETELY AND THEN SEGFAULTS

**Measured by P53, 40 runs of each, exit 139 every time.** Not intermittent.

| construct | loads? | runs? | crash |
|---|---|---|---|
| `rt_call ?WRITE_SCREEN` from a symbolic block | yes | no — prints the correct symbol-table diagnostic, exit 1 | **none**, 40/40 clean |
| **naive game→game `call` from a symbolic block** | **yes, completely** — cells placed, blocks placed, 0x76 mapped | no | **SEGFAULT, 40/40, no diagnostic at all** |

**Reproducers are waiting for you:** `docs/Project53/f1-repro/` —
`call_symbolic_crash.{ir,vmap}` is the defect;
`rt_call_symbolic.{ir,vmap}` is kept because "this construct loads and
diagnoses correctly" is worth having pinned too.

*(Note: P53's first report blamed `rt_call` and it corrected itself in q004.
The `rt_call` path is fine. Read the corrected version.)*

**P53's hypothesis, labelled as one rather than a diagnosis** — and it points
straight at the seam you built across:

> P48's rig builds ONE WSAVS-shaped frame for the top-level routine and leaves
> the shadow call stack **EMPTY** (`tests/lowerc_rig.cpp:119`, `:319`); your
> bridge, on `call`, pushes a frame and a `call_stack` entry. A rig written for
> call-free programs meeting a bridge that calls is enough to explain it
> without either being wrong alone.

**Why this is urgent rather than merely owed.** `a003` to P53 granted it the
multi-routine compilation unit precisely so it can emit `call` between
compiled routines. Its acceptance bar is *compiles and loads*, and **the load
succeeds** — so P53 will report success on IR that crashes the moment anyone
runs it. Its bar is not wrong; the gap is that "loads" and "runs" have come
apart on this construct only.

Your own `bridge_selftest` covers game→game void, valued and nested and is
GREEN. **So whatever this is, it is a rig/harness interaction your self-test's
rig does not have** — which makes it exactly the kind of thing that looks like
"the IR is broken" to the next person.

**A silent crash is the part to fix even if the cause turns out to be P48's
rig.** The `rt_call` path prints why it cannot proceed; this one prints
nothing.

---

## Item 2 — the argument pointer-KIND check is specified twice, commented twice, and absent

Also P53's (q005), also measured.

`docs/IR.md` mandates it in two places:

- **§5.1**, REFUSED AT LOAD: *"a `call`/`rt_call` whose declared arity or whose
  argument pointer KINDS disagree with the callee's `a` cells"*
- **§6**: *"each `a` cell of pointer type must have been written by a preceding
  statement whose value is of the matching KIND"*

`IRExec.cpp` asserts it in two comments:

- `:887` — `naive_calls.push_back(...);   // arity/kind checked at end of load`
- `:1349` — *"a naive call's declared arity and its arguments' pointer KINDS are
  checked against the CALLEE's declarations"*

**The loop those comments describe checks arity, `arg_count`'s presence and the
callee's `b0`, and contains no kind test.** The only occurrence of "kind" in it
is the word in the comment.

Measured — two files one character apart, both load clean:

```
GET_INPUT.a1 = bp(HIT_ANY_CHAR.v0, 0)    ; a1 is *char — correct
GET_INPUT.a1 = wp(HIT_ANY_CHAR.v0, 0)    ; a WORD pointer into a BYTE cell
```

**What is NOT broken:** §5.10.5's M-form tripwire works perfectly —
`M32[GET_INPUT.a1]` on a `*char` refuses with a textbook message. So **kind is
enforced where a pointer is USED and unenforced where one is PASSED**, and
P52's self-test has a leg for the first and none for the second.

Three things to close, and the last two matter as much as the first:

1. the loader check itself, plus a self-test leg for the passing case
2. **`docs/IR.md` §5.1 and §6 describe a check that does not run.** Either the
   loader gains it or the spec says it is not yet enforced. *A spec that
   overstates its enforcement is worse than one that admits a gap, because the
   next compiler leans on it exactly as P53 was about to.* `docs/IR.md` is
   P52's file and P52 is closed — **you may edit it for this**, and say in the
   report what you changed.
3. **the two in-code comments are more misleading than the spec**, because they
   sit at the site and read as a description of the code below them.

P53 is compensating in its own emit-time tripwire (granted — a compiler should
not depend on a downstream checker anyway). **That must not be what closes
this.**

Scope note from P53, which I endorse: it measured the **naive symbolic-call
form only**. Whether a decorated `site=` validates argument kinds through the
pushmap is P28's path and untouched. Do not chase it.

---

## Item 3 — the battery, still deferred

`a001` R6 deferred it. Still deferred, and now for a second reason:

- **the runner is still not deployed** — the box runs the old `bin/runner.sh`
- **task 054 found a REAL master/clone DIVERGENCE** in STORE via MOVE_PLAYER
  (`results/054-p49-stage-b/play.divdump`), so the battery baseline is not
  currently green and a regression check against it would mean nothing

Do not queue anything. I will ask when the baseline is trustworthy.

---

## What I want back

A short REPORT appended to `docs/Project54/` — not a full project report.
For item 1: what it was, whether the fix is in the bridge or the rig or both,
and **a teeth leg so a silent crash on that path cannot return**. For item 2:
the loader check, the self-test leg, the spec correction and the comment
correction.

**If item 1's cause turns out to be `tests/lowerc_rig.cpp`, that file is
P53's** by `a001` Q-2 — it holds the `w8` render. Take it anyway if the fix is
small and say so; coordinate through a question if it is not.

Same rules as before: gate only if you need a ruling, push to main, and
`docs/Project53/f1-repro/` is read-only for you.

---

## Item 2b — added after this answer was first written (P53 q006)

**A self-test leg that LOADS `IR.md`'s worked examples verbatim.**

P53 found that both places IR.md spells an assignment to a `varying` cell use
the **bare cell name** as the destination — §5.10.4's variable-form table and
§5.10.10's worked example — and **neither loads**:

```
REFUSE: an aggregate cell (char/varying/words) has no CONTENTS —
use wp(QUEST.v2, d) / bp(QUEST.v2, 0) or a string form
```

**The loader is right; the document was stale.** Your own REPORT §7 F3 records
the cause without noticing the consequence: `check_piece` was reworked because
a string address now arrives in three shapes and resolves through `wp`/`bp`.
The bare name stopped being accepted then, and the two illustrations were
written before that rework.

**I have already fixed both examples** to the `wp()` spelling — that part is
done, do not redo it.

What I want from you is P53's other recommendation, which is the durable half:

> IR.md's examples would benefit from being RUN rather than read, the way the
> self-test runs its legs. Both of these examples are four lines; a leg that
> loads §5.10.10 verbatim would have caught this.

§5.10.10 is labelled *"a symbolic-only program; the self-test's shape"* — it is
what a new reader copies, and P53 copied it and had its first HIT_ANY_CHAR unit
refuse for no reason of its own.

**Add a leg that loads the spec's worked example verbatim.** If keeping it
verbatim is awkward, say so and propose the closest thing; the point is that a
spec example which does not load should fail a test rather than a reader.

### Why this is worth doing, beyond the one bug

P53 counted three cases in this project of a normative document and its
implementation disagreeing: q005's argument-KIND check (spec mandates,
loader omits), your own R1 callee-in-file rule (loader enforces, spec never
said — it invalidated two rows of P53's gate), and now this. All three
surfaced only because a *second* project tried to use the first's work.

That is the parallel structure earning its keep, and it is also a reason not
to rely on it: a self-test leg catches this class without needing a second
project to walk into it.
