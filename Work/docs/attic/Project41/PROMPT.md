# Project 41 — the FIRE mutual check, and an audit

GOAL: two narrow, overdue items first, then routines with whatever
budget is left. This project is deliberately smaller in scope than
P38–P40, each of which planned six or seven routines and reached one or
two. **Item 1 is the deliverable**; items 3–4 are what you do if it goes
well.

> **Stage nothing.** A routine is finished (100 % MATCH, primary and
> `--folded`, native-clean) or abandoned with a reason. Unchanged.

Hi Claude! Read docs/METHOD.md first, **including §16** (four cautions —
the last two matter here: every `.N@` statement count is suspect, and
marker-based sweeps are blind to expression-level gaps). Foundation:
**compiler/CODEGEN_RULES.md** (read §9 and the P40 amendments R36′,
R36a, R21e′, R21c′), **compiler/translate.py**, **compiler/ircmp.py**
(seven equivalences, FIXED unless a construct forces an eighth — a
ruling), **game/quest_rt.h** (`UPLINK`/`UP`/`UPARG` and the bit-spelling
notes), **compiler/gen_declarations.py** (`check_frames()` — the witness
rule, a hard error). Reports in order of relevance: docs/Project40/
REPORT.md and QUEST1_PREDICTIONS.md, docs/Project39/REPORT.md (written
by the integrator — the static link as built, and the gap below),
docs/Project38/{REPORT,FIRE1_ABANDONED}.md, docs/Project37/REPORT.md.
TREE VINTAGE: main after the P40 merge (d8e47f7 or later) — state it;
verify docs/Provenance.md. Nothing here touches emulation/ or
Disassembled/; no battery.

**Before anything**: run `compiler/crossings.py`, `ircmp.py --selftest`,
AND **translate at least one existing routine end to end**. The selftest
compares the book to itself and never invokes the translator — P40
shipped a `quest_rt.h` that did not parse and the selftest passed
anyway. Do not trust a green selftest as a re-baseline.

## Item 1 — the FIRE mutual check (the deliverable)

**Two projects overdue.** P39 built the static link and put FIRE's
parent-frame layout into `declarations.json` from **single witnesses per
slot**. The guard attached to that ruling was that FIRE.1 and FIRE.2 —
siblings sharing one parent — must agree on FIRE's layout
*independently*, or the layout is fitting rather than derivation.
Neither P39 nor P40 performed it. It is the only outstanding item that
makes an existing artifact untrustworthy rather than merely incomplete.

Do it in this order and say what you find at each step:

1. **Derive FIRE's frame layout from FIRE.2 alone**, from the book —
   every slot it reaches through `UP`/`UPARG`, with width and witness
   PC. Write it down *before* looking at what `declarations.json`
   already contains.
2. **Derive it from FIRE.1 alone**, the same way, without reference to
   step 1.
3. **Compare the two derivations, and both against
   `declarations.json`.** Agreement is the confirmation. **Disagreement
   is the finding** — report it as such, do not reconcile by picking the
   one that makes a routine match.
4. Only then continue with the routines themselves: FIRE.2 stopped at a
   `cvwn`/divide width question, FIRE.1 at `COM.# 1,1,SZR` (test against
   −1) and `WADC 0,0` (−1). P39 judged both to be −1 counterparts of
   existing families (R14 compare-against-0, R37 zero-is-`WSUB r,r`), so
   they are amendments, not new rules. If either routine closes, good;
   **the verdict in step 3 is the deliverable either way.**

## Item 2 — the R41 exclusion audit (small, mechanical)

P40 found that R21c named a register *class* but implemented it as an
exclusion (`avoid=("ac2",)`), so it could fall through to ac3 — the
frame register — a reachable bug. R41 names the class {ac2, ac3} for
base loads. Audit it the same way: read the implementation, decide
whether it *enforces the class* or *excludes its complement*, and fix it
if it is the latter. Then sweep the other rules that name a class or a
preference and say which are enforced and which are exclusions. This is
a code audit, not a derivation; report what you found and changed.

## Item 3 — routines, if budget remains

MOVE_PLAYER.1@701729E2 (73 stmts, link ×2, bits ×4, nothing blocked —
the second clean link witness), then RANDOM (17, one `rt_call`,
slotpatch return). Do not add more; if these two close, the project has
delivered.

## Standing instruction (from P40's parting judgement)

> The emission order of a statement's operands has **no witness at all**.
> It should be established **on the book** — with statement boundaries,
> across many routines — and the expression emitter rebuilt once, not
> patched against the comparator one diff at a time.

If you find yourself changing operand-emission order to make a diff go
away, **stop and record it instead**. That measurement is a project of
its own and P42's likely subject; contaminating it with per-diff patches
would cost more than the routine is worth. (P40 measured a related
hypothesis at 163/242 and correctly declined to build on it.)

## Part 1 — plan gate (very short)

No design ruling is expected. Report: the re-baseline including the
end-to-end translation, and anything in the carried rules you believe is
wrong. **If you have no ruling request, say so and start.**

## Part 3 — report

The FIRE verdict (agree / disagree / partial, with the two derivations
side by side and what it means for `declarations.json`). The R41 audit
result and any other rule found to be an exclusion. Per routine
attempted: the C, the match table, rules exercised, pragma count. Then:
**routines finished, running total out of ~130**, and anything you
recorded rather than fixed under the standing instruction.

## Boundaries — BINDING

1. Files: compiler/, game/, docs/Project41/. Nothing in emulation/,
   Disassembled/, no artifact, no battery.
2. **Stage nothing.** Finish or abandon-with-a-reason.
3. The four matched routines must never regress. Verify by TRANSLATING
   them, not by the selftest alone. Never leave the tree regressed and
   unpushed.
4. Derive before asserting; one witness is confidence C and named as
   such; a correlation with no mechanism is not a rule (§16); a pragma
   is an admission, recorded and counted.
5. No match number for a partial translation; "not measured" is valid.
6. Commit and push at every item boundary. Deliver a Work.tgz AND push
   the branch (`p41-fire`).
