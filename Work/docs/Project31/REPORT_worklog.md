# Project 31 — worklog

## Sep 6 2026 — Phase A (plan gate)

- Tree verified (main @ 00f641c; Provenance table matches). Read METHOD,
  StringsDesign, P29 Census, IR.md ir 4, P28 PROMPT/REPORT, P27 Census §2
  (option A/B), the string_sites.py value model.
- Pre-gate findings taken to the user and ruled: F7 verified (stub
  7017FDEC ends `LDAFP 3`); min diamond = no-delist form (the join keeps
  the statement); WCMP = `ac1 = cmp(a, b)` root statement; literal check
  lazy at first execution.
- string_sites.py: SYSCALL arm → ac3 = fp (F7). Diff vs P29 outputs: 0
  lines.
- string_sites.py `--p31 / --p31-tsv`: operand renderer (ir_word / ir_bp
  / piece_of), fold analysis (contiguous pure run + absorbed length
  store; LDAFP kept), leaf checks (register last-writer rule, memory
  stores in the window chain, fold consistency), register form fallback,
  min-diamond finder + predecessor census, categorised refusals, ledger
  + tsv writers.
- Two P29 tool defects found by the renderer and fixed: T1 byte-EA deps
  dropped the base register's producers (744 windows grow); T2
  WUSGT/WUSGE/WULEI modelled as register writers (51 classifications
  change; WMSP claim sizes become clean). Census regenerated into
  docs/Project31/{sites.txt,census_raw.txt}; quest.strings byte-identical.
- Correction to Census §5: all 12 WBLMs are self-overlapping fills
  (dst − src = 1 or 2 words), none a record copy.
- Result: 776 candidates, 652 EMIT, 124 REFUSE (86 P32-COPYOUT, 15
  P32-SUBSTR, 9 P32-CAPACITY, 6 P33-TEMP, 4 COMPUTED-OPERAND, 2
  OPERAND-DEAD, 1 residue, 1 call result). 40 diamonds, 40/40 preds
  inside. Embeds prediction 2,322 → 1,670 (book), 4,201 → 3,549 (stock).
  Liveness: 63/652 EMIT blocks live in 042's legs, every statement kind.
- Tool runtime 2.1–3.1 s.
- STOP: plan gate (Census.md §7 open questions O1–O5, defaults stated).
- Gate rulings received (O1–O5, buckets, bar). Re-ran StringsDesign
  §1.3–1.5 on the regenerated census: all hold (19/19, 19/19, 57/57);
  count correction 11 ?WRITE_SCREEN groups + 8 copy-outs (not 12 + 7).
  Census.md §1.5, §9 added. Waiting for P30 on main (043 re-run).
