# Project 45 — REPORT

Sep 12 2026. Branch `p45-salvage` from `main` @ `7279d22` (post-P44, post-P41,
post-a001). Gate: `q001-plan-gate.md` / `a001-plan-gate.md`.

## Deliverables

| path | what |
|---|---|
| `docs/Salvage.md` | the salvage of record: 22 program facts, 7 measurements, 14 phenomena, the tainted-file table, the void list by rule token, what was not read |
| `compiler/gen_declarations.py` | `_taint` dict added to `PARENT_FRAMES` for FIRE and LIST_PLAYERS (a001 Q3, boundary widened); **one expression** added to the table emission so the markers reach the `.json`; nothing else touched |
| `game/declarations.json`, `game/declarations.h` | regenerated; `.json` byte-identical to the prior file except the `_taint` keys (checked by parse-and-compare); `.h` differs only in its table digest line |
| `docs/Project45/verify_frames.py` | the re-derivation of every parent-frame slot witness from `quest.dis`; prints every PC it counts |
| `docs/attic/README.md` | one section added pointing at Salvage.md and correcting the two stale seed items |

Nothing in `emulation/`, `Disassembled/`, `docs/IR.md`, `docs/Provenance.md`,
`game/routines/`, `compiler/readable.py`, `compiler/ircmp.py` was touched. No
artifact regenerated. `crossings.py` was *run* (read-only), not edited.

## What I read, and what I did not

Read in full: P44 DESIGN, attic README, METHOD, Plan, `CODEGEN_RULES.md`
(611 lines), REPORTs P37–P41, `FIRE_MUTUAL_CHECK.md`, `FIRE_DERIVATION_FROM_1.md`,
`FIRE1_ABANDONED.md`, `LIST_PLAYERS3_ABANDONED.md`, `InadmissibleEvidence.md`,
`ReturnMessageFinding.md`, `GetInputFinding.md`, `FrameRelocation.md`,
`DIED_PREREGISTERED.md` (first half), P35/P36 findings sections, the pre-P44
brief's top 150 lines, `Census.md` header and figure lines, `PARENT_FRAMES`,
INTEGRATOR §10. Not read: see Salvage §5 (translate.py body, P35/P36 PLANs
and report bodies, comparator outputs, P40 predictions/abandonment bodies,
`R41_AUDIT.md`, `FIRE_DERIVATION_FROM_2.md`, REWRITE_PLAN, P42/43, the
pre-P44 history tail).

## The counts

| | |
|---|---|
| rule tokens examined | 73 (R1–R45 with sub-letters/primes, incl. the six that only exist in P39's REPORT/translate.py) |
| non-rule claims examined | ~35 (findings, opens, censuses, corrections) |
| **program facts salvaged** | **22** (F1–F22), of which 19 Verified, 2 High, 1 Single |
| measurements recorded | 7 (M1–M7), 5 Verified |
| phenomena recorded | 14 (P1–P14), 9 with at least one Verified site |
| void compiler claims | ~60 rule tokens; 8 tokens re-classed as *not void* because they are instruction semantics, live spellings or METHOD rulings (R4, R14/14b/15, R17, R39, R40, R30's slot layout, R31/32's port) |
| census-recoverable | 22 rows of the void table |
| taints marked | 7 slots (6 FIRE + 1 LIST_PLAYERS) — **all `derived`**, none `single` |

## The verification scoreboard (a001 asked for this)

Of the citations I checked against the listing/mem/addrbook/crossings.py:

| result | count | cases |
|---|---|---|
| exact | 34 | the FIRE-family counts (all 7 slots, every PC), M1 (all four registers and the 302/306), M3 (63/63), M4, M5, RETURN_MESSAGE's five call sites + tail + literal + `NLDAI 16`, X.CB ×2 + literal bytes, GET_INPUT frame/temps/?READ args/unsigned-byte test, DIED bit sequence 70166046..57, DIED 70166096, twin constants ×4, R40 prologues ×2, LOCK/UNLOCK edges, the self-move, WPSH/WPOP sites, `WLDAI 1` at 7016DEAD, P8 |
| **wrong PC, right fact** | 6 | FIRE.3's call site (P41 wrote 7016A309 = FIRE.2's; FIRE.3 is 7016A1E5); R37's FIRE.1 `WSUB 1,1` (cited 7016A3D2, mid-instruction; it is 7016A3D0); R37's INIT_OBJ_TBL `WSUB 1,1` (no such instruction in the routine at all); P40's three QUEST.1 hoist PCs (7015C5F9/FB/60E → 7015C5ED/FC/611); PICK_X_Y's y-low gate constant (attic 0x3B77, listing **0x3B73**); R28's three PCs (P41 already found them) |
| not checkable / not chased | 4 | M2's ac2 count (288 in the listing vs 276 in the book), M6, M7, P2 |

**Wrong-citation rate: 6 of 40 ≈ 15 %** — lower than P41's "roughly half of
those spot-checked", but P41 checked witness PCs specifically (the class most
likely to be computed rather than read), and I checked mostly block labels
and instruction sequences. Two patterns account for all six: PCs computed
from assumed instruction lengths (off by 2–3 words), and a constant
transcribed by memory (0x3B77). **Every one of the six is a citation error
on a true claim; no salvaged fact turned out false.** That is the finding:
the attic's *facts* about the program are reliable, its *addresses* are
not, and anything that carries a PC forward from it should re-check the PC.

## Did the measurement/conclusion split hold?

Yes, and it did work: M1–M3 are exactly the numbers that would otherwise
have died with R41′/R44/R45, and they are the evidence P44 §13 project 8
would need. Two claims resisted the four-way classification:

- **F5** (two optional-argument spellings). It is a program fact (there are
  exactly two `mixed:` routines and they differ) *and* the observation that
  the discriminator is unfalsifiable is a conclusion. Recorded as a fact
  with the conclusion attached.
- **F3** (nesting one level deep). A fact about the program established only
  by the absence of a counter-instance in a census; it has no positive
  witness. Recorded as Verified-by-consistency, which is a fifth status I
  used once.

The ABI/allocation line (Q1) held without a hard case: every calling-
convention fact had a live corroborator outside the attic, and no
allocation claim did.

## Live docs and live figures that depend on a voided or tainted item

| where | what | recommendation |
|---|---|---|
| `docs/Project34/Census.md` header caveat | names only the two R40 pairs | **replace** with the text below |
| `docs/Project34/Census.md` per-routine rows | READ_IN (17 = union with its ON-unit), LIST_PLAYERS/.1, DROP/.1/.2, the four R40 entries | as caveat; re-count only if a live consumer needs them |
| `docs/CURRENT_STATE.md` P35–P41 entries | cite R-numbers | banner-marked by integrator (a001) — done |
| `docs/README.md` row for `attic/README.md` | "Lists what a salvage project should mine" | change to "salvaged: see `docs/Salvage.md`"; add a `Salvage.md` row, status LIVE |
| `docs/NextSession.md` §"Next work" item 1 | P45 pending | mark done; point at Salvage.md; note the FIRE taint is **discharged** (all slots derived) so nobody re-opens it |
| `docs/METHOD.md` §16 | correct as written | optional one-line addendum: "P45: LIST_PLAYERS.1 is a third verified instance of ON-unit interleaving; and the attic's PCs are ~15 % wrong — re-check any PC carried out of it" |
| `game/README.md` | "declarations.json carries single-witness data" | now false; replace with "every slot is `derived` (P45 re-verification); see `_taint` markers and Salvage §3.1" |
| `game/quest_rt.h` | P44-owned | no action from P45; Salvage §3.4 lists what it encodes |
| `Work/emulation/quest.addrbook` | RETURN_MESSAGE `mixed:3/6`; `nested` on `#` lines | not in boundary; Salvage §3.5 — for the addrbook owner / P46 |

**Replacement caveat for `Census.md`:**

> CAVEAT (P37/R40; widened P39, P45): per-routine statement counts here are
> derived from addrbook ranges. They MIS-SIZE (a) PL/I multiple-ENTRY pairs
> — CREATE_MAP 7 vs DISPLAY_MAP 902, TRANSPORT_TERRAK 2 vs TRANSPORT_SUNDAR
> 138 — and (b) EVERY parent whose range contains a `nocall` ON-unit, because
> the unit's body is branched over and the parent's body resumes inside the
> unit's range: verified instances READ_IN/READ_IN.1, DROP.1/DROP.2,
> LIST_PLAYERS/LIST_PLAYERS.1 (`docs/Salvage.md` F10). Any per-routine,
> size-dependent figure below inherits the error; the whole-book totals do
> not. See METHOD §16.

## FOR P46

Nothing that requires action, two things worth knowing:

1. **`quest.addrbook` flags that are misleading** (Salvage §3.5): RETURN_MESSAGE
   `mixed:3/6` records a hand-assembly caller; the `nested` flag on `#`
   (nocall) lines describes ON-units, not called procedures. If P46's loader
   or `readable.py` keys on either, it is keying on a wrong description.
2. **Block-label citations vs instruction PCs.** The attic cites IR block
   labels (e.g. `701687D9`, `7016DF68`) as if they were instruction
   addresses. If P46's ir 7 work renames or re-splits blocks, any tool that
   resolves an attic-style citation will need to know which kind it holds.

## Anything the design got wrong

- **Carried-in ruling 4 was stale on `main`**, as the gate said; a001
  superseded it. Worth noting for the SOP: a prompt written against a
  tarball should state the commit it was written against so a worker can
  diff, as this one could.
- **The prompt's "~45 numbered rules" undercounted by half** (73 tokens);
  the void table is the part that scaled, and it was the part a001 said to
  shrink under pressure. It did not need shrinking.
- **The prompt says the mutual check "was never performed"** and `game/README.md`
  says `declarations.json` "carries single-witness data". Both are now
  false in the tree and should be edited (table above).
- **One thing the design has right that the attic almost lost:** P41's
  family-check reformulation — "a pair with no shared slot is not a check"
  — is a method lesson of the same rank as METHOD §16's "consistent is not
  evidence". It is in Salvage §3.1 and worth a line in METHOD.

## Verification at landing

- `python3 compiler/gen_declarations.py` → `wrote declarations.h /
  declarations.json (table c98e825630c90c4c)`; `check_frames()` passes (all
  witness PCs still resolve to the instruction their comment cites).
- `.json` parse-and-compare with `_taint` removed: identical to the committed
  pre-P45 file.
- `python3 compiler/crossings.py` → the three known pairs, one mutual; no
  new crossings.
- `python3 docs/Project45/verify_frames.py Disassembled/quest.dis` → the
  counts in Salvage §3.1.
