# Project 33-B — the 19 WMSP claim groups as arena twins (`t@<block>.<k>`)

Session Sep 6 2026, solo implementation; plan gate and landing reviewed by
the user. **TREE VINTAGE: main @ fa5b180** (= 2085231, P32 + P33-A merged, +
the P33-B prompt); uploaded Work/Disassembled byte-identical to the repo;
Provenance prefixes verified against the P32 table. Branch:
**p33b-arena-temps**. Gate: task 047 (046's 15 legs + forced, checker ON) —
committed on main; local legs in §5. Records: docs/Project33/CensusB.md,
censusB_raw.txt, p33.ledger, p33.tsv; docs/IR.md §5.9; Provenance.

## 1. Outcome — the string family is gone from the IR

- ir 6 artifacts at `--strings-slice 7`: **WCMV / WMSP / STASP embeds
  0 / 0 / 0** (both modes); embeds **557 book (from 729), 2,436 stock
  (from 2,608)**; 1,822 string statements (649 P31 + 944 P32 + 229 P33-B:
  57 `acN = t@b.k` bases, 57 `claim`, 19 `release`, 96 WCMVs as ordinary
  located statements); 19/19 groups EMIT, 0 refusals.
- The master is unchanged. The clone keeps every temp as a TWIN at a fixed
  arena address; its wsp never moves for a claim (Δ_clone ≡ 0 on every IR
  leg — `claim=0` on the clone's hook line).
- Local gates, checker ON: k1fo book K=1 **0 div / 311,581 pairs**, stock
  K=1 **0 div / 306,900**, play book K=50 0 div; derr (F2-b TERMINAL-ABORT
  with both pcs), derr-emu (readout `00000011 7015C48E`, 332 insertions
  cancelled by the claiming clone), forced (DIVERGENCE naming `UNMAPPED
  arena row t@70166144.1`); wrong-capacity refusals at load (§4).

## 2. Rulings taken (user, Sep 6, plan gate)

- **Per-claim twins, not one `p@b`** — CensusB §3: INIT_OBJ_TBL's
  intermediate residue survives to a rendezvous; the single-`p@b` model is
  exact only by a liveness argument that group breaks. `t@b.k` (57) is the
  ground; `p@b ≡ t@b.last` with the intermediate twins folded is the
  READABLE layer's rendering (StringsDesign §2 to say "ground: t@b.k;
  readable: p@b" at merge).
- `LDASP r; WADI 2,r` → `acr = t@b.k` (every slot store, reload, XLEFB,
  length-word store, WSTB and `XPEF @[slot]` push then hits the arena —
  no access path has to be found); WMSP → `claim t@b.k, acN` (4·acN ≤
  capacity, the runtime fault); STASP → `release t@b, acN` (asserts
  acN == t@b.1 − 2, then acN := wsp — the master's residue); the 96 WCMVs
  through P32's pipeline as located sites (the evaluator treats the pair's
  register as the twin's constant; no renderer change beyond naming).
- `quest.arena` (new file, wins over strhooks' provisional columns):
  `bound=exact|unbounded` per twin, 8192 B provisional at 0x2000-word
  stride (0x1000 overlapped the closed end — the loader caught it); the
  runtime claim check is the fault; the battery's per-twin max-claim
  counters size the final column (datum: 692 B).
- ir 6; the loader refuses ir 5. The checker's rows are per-claim, bound at
  each WMSP hook, no rebind. Arena pages mapped in the clone process on the
  address-book precedent.
- HELP placement: not blocking (a pre-existing driver defect); the 047 HELP
  line is informational.

## 3. Findings (design-vs-reality; the two checker corrections need a
## ruling at landing — implemented because nothing else closes the project)

1. **The wsp term subtracts the TOTAL outstanding claims, not Δ(current
   wfp).** The consuming `?WRITE_SCREEN` runs INSIDE the claim bracket and
   its blocks are listed, so at a rendezvous in the callee the master's wsp
   carries the CALLER's claims while Δ(callee) = 0. Hidden in P33-A because
   both engines claimed. `ClaimDelta::total()`; `MachineHooks::outstanding()`;
   compare_pair: `master_wsp − Σ_m == shadow_wsp + checkpoint − Σ_c`.
   Frames above the current one hold no claims (LIFO, asserted at every
   ordinary frame exit), so the total is exactly the callers' share.
2. **A claim is a STACK INSERTION the clone does not make — everything
   above it is shifted.** The callee's frame, its wfp, the pushed args, the
   pointers into the packet: all sit Δ words higher on the master, in stock
   mode as much as book mode. StringsDesign §6 treated Δ as a wsp-only
   term. The Mapper's stack leg gained a claim-insertion layer OUTSIDE the
   book/compression leg — the compression leg with the sign reversed:
   `master = book(u) + shift(book(u))`, shift = Σ w of insertions whose
   point (master no-claim coordinates = wsp_before − claims already
   outstanding) lies below the address; exact inverse for ToClone; I4
   round-trips hold. The master's WMSP/STASP hooks feed it (ClaimIns /
   ClaimRel events, keyed by the claiming frame) through the P33-A event
   queue; a clone that executes the same WMSP itself (all-emulated run, a
   refused group) CANCELS the insertion by pc, so the layer represents
   exactly the difference between the two stacks.
   - **A master address inside an outstanding claim maps to its bound
     twin** (unique while the claim is outstanding — the case the
     one-directional rule guarded against was the reuse AFTER release).
     The mediated-READ verification of the `?WRITE` buffer needs it: the
     buffer IS the temp, and the clone's bytes are in the arena. It is the
     memory oracle for free — every `?WRITE_SCREEN` of a lowered group
     verifies the twin's bytes against the master's temp. `clone_location`
     no longer shortcuts on an empty record list (stock mode has
     insertions too); `map_word` ToMaster maps a twin word through its row.
3. `release` names the STASP's register: 9 of 19 groups restore with
   `STASP 0`, 10 with `STASP 1` (the plan said ac1).
4. The landing-stub STASP and P33-A's row model: rows per claim made the
   P33-A "rebind at every WMSP" a no-op concept; `rebind` now counts a block
   re-opened in the same frame (the §6.5 loop; DISPLAY_SCREEN rebinds 126×
   on k1fo without a mismatch — the dead pointers are dead).
5. Clean legs end by SIGTERM; Launch now takes the graceful path on it so
   the verdict lines print (one line).

## 4. Implementation notes

- `hw/strings/Arena.{hpp,cpp}`: quest.arena loader (ids in order, twin
  names, disjoint closed ends, every block's claims 1..n present, exact
  bounds honoured by the capacity), env `QUEST_ARENA` (required with
  `QUEST_STRINGS_CHECK=1`, loaded before the IR — `IRExec::load_from_env`
  asks first, RTStubs again, idempotently), `map_pages` for the clone (or a
  non-lockstep QUEST client), the Mapper layout with a `claim` ordinal.
- `hw/strings/StrHooks`: per-claim binding (`Arena::by_wmsp`), master-side
  capacity check, per-twin max-claim counters, ClaimIns/ClaimRel events,
  cancellation on a claiming clone, `outstanding()`.
- `hw/Mapper`: `claim_insert / claim_release / claim_cancel / claim_total`;
  `map_word` = insertion layer around `map_word_book`; ToClone into a claim
  → the twin (else abort: a write into a temp the runtime never makes).
- `hw/IRExec`: `ir 6`; the `t@` atom (a CONST at load); `claim`/`release`
  statements; `arena`/`strings33` provenance (the arena sha must match
  QUEST_ARENA).
- `tools/string_sites.py --p33 --p33-tsv --strhooks --arena`: the evaluator
  substitutes the pair's register (only in claim blocks, only for the
  wsp+2 value, k-th pair = claim k); `classify_ptr` has an `arena` class;
  `ir_const`/`ir_bp32` name twins (`bp(t@b.k, o)`, `wp(t@b.k, o)`); the
  P33-TEMP refusal no longer fires (the class is not `temp`); a twin is
  not a literal (the arena is above the code range). A group whose rows do
  not all EMIT refuses whole. `tools/arena.py`, `tools/p33_census.py`.
- `tools/lower.py`: `--strings-sites33 --arena`, slice 7, LDASP foldable
  (site = the WADI), `ir 6` header, provenance lines, twin census.
- Refusals: an arena edited after emission → the IR's `arena` provenance
  line refuses; p33.tsv vs quest.arena sha → lower.py refuses; a capacity
  below an exact bound → the Arena loader refuses; an undersized unbounded
  twin → the clone's `claim` FAULT / the master hook's abort (self-tested).
- Tests: strhooks self-test 74 GREEN (+ the Arena loader's refusals, the
  twins' mapping including the intermediate one, the capacity abort) with
  the teeth build RED; strings and helpers GREEN.

## 5. Validation (local, 1-core container)

| leg | cfg | result |
|---|---|---|
| k1fo | book K=1, checker ON | 0 div, 311,581 pairs, clean; master claim 392 / release 133; clone claim 0; max_claim t@70166E7C.3 = 692 B |
| k1fo-st | stock K=1, checker ON | 0 div, 306,900 pairs, clean |
| play | book K=50, checker ON | 0 div, ends I.STOP; groups 5/7/8/12 live; HELP not reached (driver) |
| derr | book K=1 | `TERMINAL-ABORT at 7017ED1C … clone IR assert at 7015C48B` |
| derr-emu | emu K=1 | `TERMINAL-ABORT … (top stack wides: 00000011 7015C48E …)`; cancelled=332 |
| forced | book K=1, POKE ac2:=0x75000000:CLONE | DIVERGENCE: `hits UNMAPPED arena row t@70166144.1` |
| bad arena (exact bound violated) | — | `Arena: … capacity 16 below the exact claim of 32 bytes — refusing to launch` |
| bad arena (edited) | — | `IRExec: REFUSE: arena provenance mismatch vs QUEST_ARENA=…` |

Task 047 (main dd43b75): 046's 15 legs + forced with the checker ON; verdict
lines = ir 6 headers, embeds 557/2,436, WCMV/WMSP/STASP 0/0/0, twins 57,
bases/claims/releases 57/57/19, clone claim=0 on every IR leg, per-twin max
claim, groups exercised, derr F2-b (anchored `^IR ASSERT FAILED`), derr-emu
readout, forced twin line, self-tests. Bar: 16/16, 0 div on the 15 non-forced
legs, 0/0/0.

## 6. For the integrator

- Mapper.md §1.4: add the claim-insertion layer (finding 2) with the text
  above, and "a master address inside an outstanding claim maps to its
  bound twin (mediated dereference, unique while outstanding)".
- StringsDesign §6.3: the wsp equality is over the TOTAL outstanding claims
  (finding 1); §2.1: "ground: t@b.k; readable: p@b ≡ t@b.last"; §5.2: the
  twins, `claim`, `release`.
- Owed (NextSession): the play driver's post-auto-move keys (O/D/L/H) do not
  reach the command prompt — DIED, DISPLAY_MAGIC, LIST_PLAYERS, OBSERVE,
  HELP, GET_QUEST, OP_EDIT, STORE are unexercised; the final `cap=` column of
  quest.arena from 047's max-claim lines; §5.3's three LOCK_FILE
  by-reference constants (P33 tail item, untouched).
