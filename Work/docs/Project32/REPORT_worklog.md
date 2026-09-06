# Project 32 — worklog

## Sep 6 2026 — Phase A (census) + the P31 correction commit

- Tree 6c3c28c verified against Provenance (12/12 prefixes). Read METHOD,
  StringsDesign, P31 Census/ledger, IR.md §5.8, P30 REPORT, string_sites.py.
- Baseline: string_sites.py regenerates every P31 file byte-identical.
- Exploring the P32 population (932 WCMV + 9 WCMP): found that the
  evaluator kept frame slots across ?UNSIGNED_TO_CHAR (which writes its
  varying there) → 17 wrong P31 EMITs. STOP-AND-REPORT; rulings taken
  (correction (a)/(b)/(c), see Census §1.1).
- Evaluator: Src provenance on every loaded value; passed slots dropped at
  calls; stale rule; Merged carries provenance; unk tagged with its
  register; precall register snapshot for the chain check.
- P31 regenerated: 649/127; artifacts 649 statements, 1,673/3,552.
  09f6593 (Follow.java fix) landed meanwhile: blocks.split/tags changed →
  assumed-foldable.txt header regenerated (content identical); IR headers
  regenerated with it.
- Local K=1 gates (1 core, sequential, /tmp/leg.sh): book k1fo 0 div,
  308,923 pairs, 0 blk mismatch, clean, 49 string first-execs, 0 literal
  mismatch; stock k1fo-st 0 div, 299,387 pairs, clean, 52 first-execs.
  (The first stock attempt died with a container restart; rerun detached.)
- --p32 renderer: expression-or-register per operand with fixpoint
  demotion; continuation dst = @ac2; fold truncation at register-operand
  producers; P32 leaf checks (store-after-load, call-between); varying
  form only when counts equal; chains from cont tags with WSTB accounting,
  nearest-piece matching through joins, preserved-register substitution
  for the total check, bounded copy-outs, in-place (tail-split) builds.
- Findings: pairwise concatenation (longest 2, no cross-block sequence);
  335 totals verified, 0 mismatch; tail splits = bounded concatenation
  (src_count = room); ?UNSIGNED_TO_CHAR returns nothing (design error,
  corrected on main); no len() in ir 5.
- Tool runtime 3.4 s (was 2.1–2.7).
