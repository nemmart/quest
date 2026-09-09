# Project 40 — routines that need nothing new

GOAL: **finish routines.** Not build a construct — there is no new
construct in this project. A census of the book (Sep 9, integrator)
found four routines whose statements use *only* machinery the model
already has, and three more needing a single `rt_call`. P38 and P39
each finished zero routines while building real things; the question
this project answers is whether the model, as it now stands, can close
routines when nothing is in the way.

If these close, the compiler model is in good shape and the remaining
work is construct-by-construct. If they do not, the model is wrong
about ordinary expressions and that is the most important thing we
could learn — P39's parting diagnosis was exactly this, and QUEST.1 is
designed to test it with no construct to hide behind.

> **Stage nothing.** A routine is finished (100 % MATCH, primary and
> `--folded`, native-clean) or abandoned with a reason. Unchanged from
> P38/P39.

Hi Claude! Read docs/METHOD.md first, **including §16** — which now
carries four cautions, two of them added from P39: `crossings.py`
cannot see intra-family mis-sizing; every `.N@` statement count is
suspect; the addrbook's nested flag conflates called procedures with
`nocall` ON-units; and **marker-based sweeps are blind to
expression-level gaps**. That last one applies directly to the census
below: it is statement-level, so it can tell you a routine has no
floats, but not that its expressions are all modelled.

Foundation: **compiler/CODEGEN_RULES.md** (the model — read §9 and the
P39 additions R42–R45, R21d, R21e), **compiler/translate.py**,
**compiler/ircmp.py** (seven equivalences, FIXED unless a construct
forces an eighth — a ruling), and the P39 output: docs/Project39/
REPORT.md (the static link as built; the material gap below),
game/quest_rt.h (`UPLINK`/`UP`/`UPARG`), compiler/gen_declarations.py
(`check_frames()` — the witness rule, a hard error). Prior: docs/
Project38/{REPORT,FIRE1_ABANDONED}.md, docs/Project37/REPORT.md and
DIED_PREREGISTERED.md, docs/Project35/REPORT.md. TREE VINTAGE: main
after the P39 merge (7e8f9cc or later) — state it; verify
docs/Provenance.md. Nothing here touches emulation/ or Disassembled/;
no battery. **Run `compiler/crossings.py` first**, and note §16's
caveat on what it cannot see.

## The routines, in order

Censused on the book by construct usage, not by size. Counts are
statements; "blocked" means float / twin / divide / syscall / string /
call of any kind.

| # | routine | stmts | uses | why this one |
|---|---|---|---|---|
| 1 | **QUEST.1@7015C5E1** | 70 | bits ×4; **nothing else** — not even nested | **The diagnostic.** No construct at all: arithmetic, control flow, bit tests. If this does not close, the failure is pure expression-level modelling and that is the project's finding. Do it first for that reason, not because it is easiest |
| 2 | **FIRE.2@7016A461** | 70 | link ×4, 1 divide, 1 string, 2 rt_call | P39 refused at a `cvwn`/divide width question. Start where it stopped |
| 3 | **FIRE.1@7016A3BD** | 89 | link ×5; `COM.# 1,1,SZR`, `WADC 0,0` | P38 abandoned it for the static link — **that reason is gone**. Its two small forms are the only known gap (WADC now has five witnesses per P39, so it need not stay at C). **Run 2 and 3 back to back: they are the FIRE mutual check P39 owes** — the parent-frame layout in declarations.json rests on single witnesses per slot and is FITTING until the siblings agree independently. If they disagree, that disagreement is the finding |
| 4 | **MOVE_PLAYER.1@701729E2** | 73 | link ×2, bits ×4; nothing blocked | second clean link witness |
| 5 | **RANDOM** (17) and **READ_IN** (17) | 17 each | one `rt_call` each | cheap; two finished routines for little budget if the model holds |
| 6 | **LIST_PLAYERS.3@7016F556** | 62 | link ×1, bits ×3 | P39 abandoned it on an R9/R10 temp-placement divergence (temp created at the second reference, slot 6 not 4). Return to it **only if** #1 has already exposed and fixed the expression-level problem — otherwise it will fail the same way |

Take them in that order. Abandon-with-a-reason and move on; do not let
one routine consume the project.

## What to do if QUEST.1 does not close

This is the interesting branch, so plan for it. The diffs will be
expression-level: temp placement (R3b′, R9, R10), register choice
(R7/R8/R41), or a spelling. For each:

1. State the divergence as a *prediction that could fail* before
   changing anything (P39's DO-limit test is the model: prediction
   written, three parts named, then run).
2. Prefer amending a rule with its instruction pair over adding one.
3. If two readings fit and nothing separates them, record both and
   abandon — do not fit (P38's HIT_ANY_CHAR is the precedent).
4. **The three predicted arm-path defects are still unfixed** (R9's
   scaled-subscript temp, R10's element-address temp, R3's
   temp-free-from-last-use — all count uses in mutually exclusive
   branches). P37 predicted one would trip; LIST_PLAYERS.3's R9/R10
   divergence may be it. If so, say that it was predicted.

## Part 1 — plan gate (short)

There is no design to rule on. Report: the re-baseline (four routines
349/349 primary, 242/242 folded, `--selftest` PASS, `crossings.py`),
your own construct check of the six routines against the census above
(the census is statement-level — say what it misses), and any ruling
request. **If you have none, say so and start**; do not spend a turn on
ceremony.

## Part 3 — report

Per routine: the C, the match table, the rules exercised, pragma count.
The FIRE mutual-check verdict — agree or disagree, and what it means
for declarations.json. Any rule promoted, amended or voided with its
instruction pair, and whether a predicted arm-path defect tripped.
Then the number: **routines finished this project, running total out of
~130**, and — given what the diffs actually showed — your honest read on
whether the model closes ordinary routines or needs an expression-level
overhaul.

## Boundaries — BINDING

1. Files: compiler/, game/, docs/Project40/. Nothing in emulation/,
   Disassembled/, no artifact, no battery.
2. **Stage nothing.** Finish or abandon-with-a-reason.
3. The four matched routines must never regress; `--selftest` green
   after every rule change; never leave the tree regressed and unpushed.
4. Derive before asserting; one witness is confidence C and named as
   such; a correlation with no mechanism is not a rule (§16); a pragma
   is an admission, recorded and counted.
5. No match number for a partial translation; "not measured" is valid.
6. Commit and push at every routine boundary. Deliver a Work.tgz AND
   push the branch (`p40-routines`).
