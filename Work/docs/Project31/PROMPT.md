# Project 31 — located strings (ir 5): assignment, compare, block moves

GOAL: give the IR the located-string type of StringsDesign.md §2 and
lower the simplest population with it — `v = 'lit'` (528), `v = t` in
every pad/truncate form (≈190), WCMP equality (40), WBLM copy and fill
(12): ≈780 sites — as `[@a, n varying] = "lit"`, `[@a, n] = piece`,
`a == b`, `words()/fill()`, each setting the residues of §3. NO arena,
NO checker change: every one of these writes master-identical bytes at
master-identical addresses. Expected: embeds 2,322 → ≈1,540 (book).

Hi Claude! Solo implementation session; the user reviews at the plan
gate and at the landing. Read docs/METHOD.md first. Design of record:
**docs/Project29/StringsDesign.md** (all of it; you implement §2, §3, §4,
§7's first block of idioms; §5–§6 are P32/P33 and you must not need
them). Also: docs/Project29/Census.md (the census: §2 operand classes,
§3 idiom catalogue, §4 WCMP, §5 WBLM, §7 destinations, §9 findings —
F7 is yours to verify), docs/Project29/{sites.txt, quest.strings},
tools/string_sites.py (the census tool you extend, not replace),
docs/IR.md (ir 4 — you write ir 5), Project28/{PROMPT,REPORT}.md (the
freshest lowering + executor precedent), docs/Provenance.md (verify your
tree FIRST). TREE VINTAGE: main after P28 (ir 4) — state the commit.

**Dependency**: Phase A (below) may run before P30 lands. Phase B calls
P30's library (`hw/strings/EagleString`, the residue functions) and
WAITS for P30 on main. Do not write your own copy of the string
semantics in IRExec.

## Phase A — census + grammar (plan gate)

1. **Site list for P31**, from string_sites.py extended with a `--p31`
   selection: every WCMV whose idiom is ASSIGN-LIT-VARYING,
   ASSIGN-STR-VARYING(-PAD), ASSIGN-STR-FIXED, ASSIGN-LIT-FIXED,
   ASSIGN-STR-MIN, ASSIGN-FROM-TEMP-with-located-source; every WCMP;
   every WBLM. For each: block, the window, the four operands rendered
   as IR expressions (`[@wp(ac3,4), 30 varying]`, `"Cannot set shared
   partition"`, …), the capacity `n` and where it came from (the min
   shape's constant, the literal length, the layout spacing), the
   destination class (a/b/c/d/p from Census §7), and REFUSE with reason
   when any operand is `computed` in a way the grammar cannot spell.
   Reproduce the census counts or explain the delta. Runtime line.
2. **The min shape**: `WSLE/WMOV` and `WSGE/WMOV` split the block;
   `[@a, n varying] = piece` absorbs the min (§2.4) — so the interior
   block(s) of the min diamond are DELISTED, P27-style (translations
   ship their sync list; the assumed-foldable discipline applies:
   predecessor census, listed in an artifact). Count them; state the
   synclist delta.
3. **Grammar draft for IR.md ir 5**: located strings `[@a, n]` /
   `[@a, n varying]`; pieces (literal — with the quest.strings address
   and length recorded so the loader can verify the bytes against the
   image —, located read, `substr`, `char`); the two assignment
   statements; `==` and `len()`; `words()/fill()`; the refuse list
   (a piece longer than 32K, a capacity not a constant, a literal not
   in quest.strings, `substr` with a byte offset the grammar cannot
   spell — bring the `bp + expr` question to the gate); executor faults
   (segment crossing, as EagleSpecial throws). State the ir 4 → ir 5
   loader rule (refuses older files, as P28 did).
4. **Residue plan**: which residue function each statement calls; the
   WCMP residue B-4 reproduced; the carry after copies set even though
   never read (F1).
5. **F7**: verify or refute the census's SYSCALL-preserves-ac3
   assumption against the OS dispatcher source; a refutation is a
   finding that may change sites' windows.
6. **Liveness plan**: which idioms the standing legs execute (the
   `v = 'lit'` sites are the game's every message setup — they will be
   live; WBLM's fill is INIT_OBJ_TBL, driver-reached? say). Battery
   plan: 034 template via bin/task_source.sh, JOBS=3, verdict lines for
   sites lowered / refused, embeds, synclist entries, string-statement
   coverage from IRExec first-execution lines. Landing bar: 15/15 green
   (040's legs), 0 div, embeds ≤ prediction.

STOP AND REPORT at the plan gate. Phase B after the user's rulings AND
P30 on main.

## Phase B — implementation

- lower.py: the emitter for the §7 first-block idioms, behind
  `--strings-slice 1` (lit → varying) `2` (str → varying/fixed, min
  shapes) `3` (WCMP, WBLM); `--strings-census` writes the per-site
  ledger. Slice 0 reproduces the P28 artifacts byte for byte.
- IRExec: parse ir 5; evaluate pieces; call the P30 library for the
  copy, the compare, the block move and the residues; loader validation
  per the grammar draft, including verifying every literal's bytes
  against the loaded image (a mismatch = refuse to load).
- IR.md: ir 5 (version history; §2–§4 of StringsDesign transcribed as
  spec; the delisting note). Provenance.md updated; synclist shipped.
- Land in slices behind K=1 book + stock gates.

## Phase C — validation

Task 044: 040's 15 legs + the string verdict lines. Bar as in A.6. Red
= stop and report.

## Boundaries — BINDING

1. Scope = located assignment, compare, block moves. NOT: append chains
   (P32), `p@b`/arena/hooks (P33), WSTB, CALLRESULT pieces, tail
   splits, any checker change.
2. Phase A before Phase B; Phase B after P30. No string semantics
   implemented in IRExec — the library only.
3. Semantics per StringsDesign §2.4/§3; refuse-don't-guess; every
   refused site listed with its window.
4. The strict surface is untouched: residues set on every statement; no
   drops, no masks.
5. Design-vs-reality: STOP AND REPORT — a site whose residues the §3
   table gets wrong, a literal whose bytes differ from the image, a min
   shape with an outside predecessor, F7 refuted.
6. Deliverables: the extended census tool + ledger, docs/Project31/
   {Census,REPORT,REPORT_worklog}.md, the emitter + executor + IR.md ir
   5, regenerated artifacts + synclist + Provenance.md, task 044 result,
   CURRENT_STATE/NextSession, TREE VINTAGE, tool runtimes.
