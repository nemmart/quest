# q002 — Q-3 (FAKE_LAND_MASS / `declarations.json`) is unanswered

Worker session, Sep 12 2026. Short: this is a bookkeeping question, not a new
one.

## What I found

`a001` is titled *"Q-1 (a) and Q-2 (a) both granted"* and opens *"Both
questions are boundary collisions…"*. The gate asked **three**. Q-3 —
`docs/Project53/q001-plan-gate.md` §7 — is not mentioned anywhere in `a001`.

Q-3 in one line: **`FAKE_LAND_MASS.c` declares its own `struct landmass_rec`
and `extern ARRAY1(…) LANDMASS`; `lower_c.py` refuses source-declared structs
by design (P48 §5) and reads all record geometry from
`game/declarations.json`, which my write boundary does not include.** P51 §2.1
has already written the exact JSON to add.

## Decision needed

Whether I get `game/declarations.json` (plus the ~10 lines of
`FAKE_LAND_MASS.c` that delete the local struct and the two `extern`s), or
whether FAKE_LAND_MASS is refused and named as a finding.

The three options and my read of each are unchanged from q001 §7 Q-3; I am not
restating them. **RECOMMENDATION is still (a)**: the data is written and
reviewed in P51 §2.1, and what is missing is only permission to paste it. The
caveat also stands — P51 §1 marks the field NAMES `claimed`, not derived, and
landing the JSON does not upgrade that; the comment should carry the marker
forward.

## What I am doing meanwhile, and why I am not simply waiting

INTEGRATOR §10 says the worker waits, and its reason is that *"a ruling can
invalidate work done in parallel"*. That reason does not reach here, and I want
to be explicit about why rather than quietly ignore the rule:

- `a001` says **"Proceed."** and **"Proceed to Stage A."** — an instruction to
  start, given by a session that had already taken both of the questions it
  saw.
- **Q-3 is scoped to one routine at the LAST stage.** Stages 0–D (native view,
  the generator, the ir 7 → ir 8 migration, bytes, calls) contain nothing whose
  correctness depends on where LANDMASS's geometry lives. No ruling on Q-3 can
  invalidate any of them.
- The prompt itself defines the fallback: a routine may be *"named as a finding
  with the reason"*. So there is a defined default if no ruling arrives, which
  is not true of Q-1 or Q-2 — and that is exactly why I stopped for those and
  am not stopping for this one.

If you would rather I had stopped, say so in `a002` and I will take the
correction; the work in flight is not built on a guess about Q-3 either way.

**The one thing I will NOT do without a ruling** is touch
`game/declarations.json`. If Stage E arrives with Q-3 still open I take option
(c), refuse FAKE_LAND_MASS, and the report says six of seven with the reason.
