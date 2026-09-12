# q001 — P45 plan gate

Worker: P45 session, Sat Sep 12 2026. Tree: `main` @ `7279d22`
(the p44-design merge). Nothing written yet except this file.

**Vintage note.** The session was started from a `Work.tgz` cut before the
P41 merge. Per the SOP, `main` is the tree of record and this gate is
against `main`. The differences that matter for P45: P41 **ran** (five
docs in `attic/Project41/`), `REWRITE_PLAN.md` is in the attic as
"superseded by absorption", R41 became R41′ with a 563-load SD_PTR census,
and `game/declarations.json` was regenerated with two new FIRE slots
(`args.2`, `locals.w8`), corrected witness PCs and corroboration counts.
§4 of this gate is where the vintage changes the picture.

---

## 1. What I read, and the reading plan

**Read in full so far:** P44 `DESIGN.md` (incl. §7.2a/§7.2b), `attic/README.md`,
`METHOD.md`, `Plan.md`, `CODEGEN_RULES.md` (all 611 lines incl. R41′),
REPORTs for P37/P38/P39/P40/P41, `FIRE_MUTUAL_CHECK.md`,
`FIRE1_ABANDONED.md`, `LIST_PLAYERS3_ABANDONED.md`, the findings sections of
P35/P36 REPORTs, `game/declarations.json`, `gen_declarations.py`'s
PARENT_FRAMES, `Census.md` header + figure lines, `INTEGRATOR.md` §10, and
every live doc that cites attic material (CURRENT_STATE, README, NextSession,
METHOD §16, game/README).

**Remaining, in order:**

1. P37's five finding docs (InadmissibleEvidence, FrameRelocation,
   GetInputFinding, ReturnMessageFinding, DIED_PREREGISTERED) — the primary
   derivations behind the seed list.
2. P41 `FIRE_DERIVATION_FROM_1/2.md`, `R41_AUDIT.md`; P40
   `QUEST1_ABANDONED.md` / `QUEST1_PREDICTIONS.md`; P35/P36 full REPORTs
   and PLANs.
3. `NextSession.pre-P44.md` **top brief only** (lines 1–283). Lines 283–829
   are P24–P34 hand-off blocks — pre-attic history already superseded by
   live docs. Spot-check for anything not carried into live `NextSession.md`,
   otherwise skip.
4. `translate.py` (3,148 lines): **grep only** for embedded program facts —
   field K values, addresses, literal texts, witness PCs in comments. Not
   the translator logic.
5. `REWRITE_PLAN.md`, P42/P43 PROMPTs: skim for carried-in facts; unrun.
6. **Verification pass** against `Disassembled/quest.dis` / `quest.mem` for
   every salvaged fact that cites a PC. Cheap: two greps this session
   already confirmed the `WMOV 0,0` at 70176FF5 and both X.CB call sites.
   This is what lets Salvage.md say *verified* rather than *reported* —
   and P41 found ~half of spot-checked witness PCs were wrong, so it is not
   optional.
7. Chase R40 / `.N@` mis-sizing into `docs/Project34/Census.md` figure by
   figure.

**Budget shortfall order:** Salvage §1/§3 and the in-place taint markers
first; §4 (void list) is mechanical and second; §5 records the rest.
First dropped: P42/P43/REWRITE_PLAN skim, the pre-P44 history tail, and
verifying void-rule PCs (no point verifying what dies).

---

## 2. Classification scheme

Each item carries:

| axis | values |
|---|---|
| **Kind** | `PROGRAM-FACT` (the binary, its data, its calling interface) · `PHENOMENON` (an observation at a named site, no rule — P44 §7.2 `keep` candidates) · `MEASUREMENT` (a census number over the book — survives as a number, not a conclusion) · `METHOD-RULING` (admissibility/procedure; live in METHOD §16 or recommended for it) · `COMPILER-CLAIM` (void) |
| **Witnesses** | count, with PCs / routine names |
| **Confidence** | `Verified` (re-checked this session against quest.dis / quest.mem / emulator source) · `High` (≥2 independent witnesses, or an independent live source such as M4aDesign.md) · `Single` (one witness) · `Reported` (asserted in a report, no checkable PC) |
| **Dependents** | every live doc / live file that cites it |
| **Live status** | `already-live` (Salvage.md cross-references and checks the live copy is complete) · `salvage` · `taint-in-place` |
| void only | `census-recoverable?` yes/no, one line |

**One axis I want ruled (§5 Q1):** calling-convention facts — link arrives
in ac1 and is saved at `wp(fp,-6)`; args at `wfp-10-2N`; slotpatch return
via the saved-ac0 image at `wp(ac3,-8)/-7`; 63/63 nested XCALL sites load
ac1 first. These describe *how the compiler builds frames*, but they are
also *what every call site in the binary does*, and DESIGN §9.2's
rendezvous contract depends on them.

---

## 3. Seed list: misclassifications and misses

**Misclassified (mildly):**
- **R39** is listed under "facts about the program". It is a method ruling
  and is already live in METHOD §16. The program fact under it —
  LOCK_FILE/UNLOCK_FILE are one hand-assembly unit at 70169B0F..70169D69,
  the only two WSAVR entries in the addrbook — is what goes in Salvage §1.
- **R40** likewise is already live in METHOD §16. The salvage work is
  checking the live copy is complete and chasing the Census figures, not
  re-recording the rule.
- **The FIRE taint** — see §4. The seed (and carried-in ruling 4) says
  "every slot rests on a SINGLE WITNESS". On `main` that is no longer what
  the file says or what P41 found.

**Missed by the seed list (first pass):**
- **R38 as a phenomenon.** PL/I BIT literals are rebuilt at run time by
  `X.CB @7017E708` at two sites (7016AA41, 701703A6) — both verified in
  quest.dis this session. Any lowering must reproduce it.
- **R35 / slotpatch.** Value return via the saved-ac0 image; three
  witnesses, both widths; DESIGN §9.2 already relies on it.
- **PL/I CHARACTER is unsigned** (`WLDB` zero-extends, then `>s 128`;
  caught by gcc, not the comparator).
- **RETURN_MESSAGE is the fatal exit** (`SYSCALL 0310`); default literal at
  0x70000CCD is "Unexpected error", 16 bytes matching `NLDAI 16`.
- **Nesting is exactly one level deep program-wide**; the five apparent
  double-nestings are addrbook misattribution of `nocall` ON-units (P39).
- **DIED's signature correction** (arg 1 = CHAR VARYING death message; arg
  2 never read) — the live `game/routines/DIED.c` carries it, but that file
  is P44-owned and the fact should not live only there.
- **Twin sizing arithmetic** derivable from the CAT chain (P36 finding 2):
  `ceil(bytes/4)` claim counts, `+3/+5; lsh -1; lsh -1`. Two groups checked.
- **The world-coordinate ruling** — no origin offset; raw 0x3Bxx space;
  PICK_X_Y's spawn box — sits only in CODEGEN_RULES' "Carried-in for P36".
  Need to check it lives anywhere live.
- **The two P41 census results** — the SD_PTR destination census (563
  loads, 302/306 ac0/ac1 loads feed the bit family) and P39's
  `M32[wp(·,-6)]` destination census (276/12/18/3) — are MEASUREMENTS about
  the binary and survive as numbers even though R41′/R44 die.
- **Census taints beyond R40.** READ_IN's row (7 stmts + embedded ON-unit
  `READ_IN.1@701766FE`, struck at the P40 gate) and DROP.1 (body runs past
  its range). METHOD §16 already says *every* `.N@` count is suspect;
  Census.md's caveat names only the two R40 pairs and is **too narrow**.
- **Five bad witness PCs** (P41): three in PARENT_FRAMES (now fixed in the
  file) and two in `bit_base_reg`'s docstring (R28: 70166283/296/046 are the
  stride constant, not a base load). The docstring ones die with
  translate.py, but the *lesson* — witness PCs must be checked against the
  listing — is a METHOD-RULING candidate and applies to Salvage.md itself.
- **Live doc dependency:** `CURRENT_STATE.md` lines 38–50 are P35–P41
  status entries citing rule numbers (R36′, R41′, R42–R45 …) as live
  results with no VOID marker. README/NextSession are correctly marked;
  CURRENT_STATE is not. `game/quest_rt.h` carries the UPLINK/UP/UPARG
  spelling and the BIT-assignment ruling — outside my write boundary,
  report only.

---

## 4. The taint, on `main` — the seed's premise is stale

`game/declarations.json` on `main` is not the file the seed describes.
After P41:

| slot | witnesses recorded in the file | single? |
|---|---|---|
| FIRE `args.1` (−12) | FIRE.2 ×3 + FIRE.3 ×12 + FIRE's own body ×10 | no |
| FIRE `args.2` (−14) | FIRE.3 ×1 + FIRE's own body ×2 | no |
| FIRE `w8` | FIRE.3 ×8 (+XPEF) + FIRE's own body ×11 | no |
| FIRE `w10` | FIRE.1 ×3 + FIRE.3 ×2 + FIRE's own body ×1 | no |
| FIRE `w12` | FIRE.2 ×2 + FIRE's own body ×10 (**no sibling** second witness) | borderline |
| FIRE `w14` | FIRE.1 ×3 + FIRE.3 ×6 (parent never touches it) | no |
| LIST_PLAYERS `w13` | LIST_PLAYERS.3 ×1 | **yes** |

P41's restated guard — "derived when ≥2 independent witnesses of
{sibling, sibling, parent's own body} agree on displacement and width" —
makes every FIRE slot derived and leaves **only LIST_PLAYERS.w13
single-witness**. The prompt asks me to propose a different confirmation
route rather than invent one; the attic already contains one, and it is
not a compiler claim — it is a count of which displacements the
instructions use, checkable against quest.dis.

**Two complications:**

1. **The file is generated.** `compiler/gen_declarations.py`'s
   `PARENT_FRAMES` table is the source of truth and `.json` regenerates
   byte-identical (P41 verified). A marker written into the `.json` alone
   is wiped by the next `gen_declarations.py` run. `compiler/**` is outside
   my write boundary.
2. **P41 is in the attic.** Its counts are `Reported` until re-verified.
   I can re-derive them myself against quest.dis (grep every
   `[ac2+0x…]` / `@[ac2+0xFFF…]` after a link load in each FIRE child, and
   every `[ac3+…]` in FIRE's own body) in well under an hour.

---

## 5. Decisions needed, options, recommendations

### Q1 — the ABI/allocation line (§2)

Options: (a) everything from the compiler line dies except what the seed
names; (b) calling-convention / frame-image / ABI facts are PROGRAM-FACTs,
intra-body allocation, scheduling and temp placement are COMPILER-CLAIMs.

**Recommendation: (b).** The rendezvous contract (DESIGN §9.2) cannot be
written without the frame image, the arg slots and the link convention, and
these are corroborated by live sources outside the attic (M4aDesign.md,
EagleStack.cpp WSAVS/WRTN, ON_ERROR_CATALOG §B). They describe what the
binary does at every boundary, which is Quest, not the machine that compiled
it. I will cite the live corroboration for each and mark any that rest on
the attic alone.

### Q2 — what "taint" means on the post-P41 file

Options: (a) mark every FIRE row tainted as ruling 4 says, ignoring P41;
(b) re-verify P41's per-slot counts against quest.dis this session, then
mark each row with its **actual** status — `derived` (≥2 independent
witnesses, verified P45) or `single` — so the file says what is true;
(c) mark only `w12` and `LIST_PLAYERS.w13`.

**Recommendation: (b).** A live file with rows marked tainted that are not
is the mirror image of the failure ruling 4 guards against. (b) also
discharges the prompt's "propose a different confirmation route": the route
is P41's family check, re-performed by me so the verification is this
session's and not the attic's. If my counts disagree with P41's, that is a
STOP-and-report, not a judgement call. `w12` gets `derived` with the note
"no sibling second witness; parent body ×10"; `LIST_PLAYERS.w13` gets
`single` unless LIST_PLAYERS' own body corroborates slot 13 (I will check).

### Q3 — where the marker lives

Options: (a) a `"_taint"` sibling key per slot in the `.json` only, and
report that regeneration wipes it; (b) same, plus a **FOR INTEGRATOR** item
asking that the marker be ported into `PARENT_FRAMES` (or that
`gen_declarations.py` learn to carry a `_taint` field) since I cannot touch
`compiler/**`; (c) ask for the boundary to be widened to
`gen_declarations.py`'s PARENT_FRAMES table only.

**Recommendation: (b).** Form of the key:

```json
"w12": { ..., "_taint": { "status": "derived", "witnesses": 12,
         "independent": ["FIRE.2", "FIRE"], "note": "no sibling second witness",
         "verified": "P45 against quest.dis, Sep 12 2026" } }
```

`_`-prefixed so nothing that reads `slot`/`width`/`witness` trips on it. No
layout value changes. (c) is cleaner but the boundary was drawn
deliberately and `gen_declarations.py` is a P44-project-3 artifact.

### Q4 — scope vs the prompt's numbers

| item | prompt | found |
|---|---|---|
| project dirs | 9 | 9 (P35–P43; P42/P43 prompts only; P41 **ran**) |
| numbered rules | ~45 | 45 base numbers (R1–R45) but **73 distinct rule tokens** with sub-letters/primes; R42–R45, R21d, R21e exist only in P39's REPORT and translate.py comments |
| ON-handler sites | 26 | consistent; lightly referenced in the attic |
| NextSession.pre-P44 | 829 lines | 829, ~280 compiler-line |

**Claims in scope: ~105** — 73 rule tokens + ~30 non-rule findings, opens
and measurements. Expected split: ~20 program facts, ~10 phenomena, ~8
measurements, ~55–60 void (perhaps 10–15 census-recoverable), taints as
per Q2. The "~45" is not badly wrong; it undercounts the void column by
about half. **No ruling needed — recorded so the estimate can be checked
against the report.**

### Q5 — one boundary question

Salvage.md §1 will want to cite `docs/Project34/PICK_X_Y_reading.md` and
`ON_ERROR_CATALOG.md` as live corroboration. Both are read-only for me and
that is fine. But `Census.md`'s caveat is too narrow (§3) and it is a live
doc I may not write. **Recommendation:** I list the exact figures and the
replacement caveat text in the REPORT's "recommended edits" section and do
not touch the file.

---

**STOP.** Waiting for `a001`.
