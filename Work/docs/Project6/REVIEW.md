# Project 6 Phase-1 Review (reviewer session, Aug 13 2026)

VERDICT: **APPROVED — both documents** (L2Contract.md, NativeDesign.md),
with the adjudications below. Corrections c1 and c2 were independently
verified against ground truth (c1: o_signal.cpp's own overwrite
comments confirm the three [E+12] writes — the P1 DERIVATION prose is
+2 words off from XPSHJ onward; c2: Project4/DERIVATION §5.2 confirms
the [0x70000124]→0x7017EB63→R.SIGREC vector verbatim, incl. their live
capture). Addendum notes placed in both source docs.

## Adjudications (F1–F3 / Q1–Q3)

- **Q1/F1 (I.GOTO shape 3 / R?SIGNAL anomaly)** — RULED: the interim
  becomes PERMANENT conforming behavior: bit-faithful emulation of the
  branch. Rationale: the condition smells defensive, but the
  deliberately-installed boot vector shows design intent (a DG restart
  mechanism Quest never uses); forcing ABORT-INTENDED onto apparent
  design intent buys nothing, and bit-faithful-emulate is strictly
  safer than either classification — it executes whatever 1988 built,
  verified, and cannot be silently wrong (Register E5's argument).
  Reclassify only if it ever fires. [User may veto.]
  *(Veto EXERCISED, Aug 13 2026 → ABORT-INTENDED; L2Contract THIRD
  ADDENDUM item 2 carries the ruling and rationale. The E5
  "cannot be silently wrong" property is preserved by the abort
  itself: nothing executes unverified — the world stops loudly
  instead.)*
- **Q2/F2 (per-frame slot privacy)** — ACCEPTED on the ratified
  provenance + lockstep-backstop basis; no frame-slot scan funded.
- **F3 (I.EPILOG vs non-establisher frame)** — A1 applied: broken
  codegen bracketing = defensive → ABORT-INTENDED; the design's
  debug-assert-and-abort is ratified; added to the contract's
  ABORT-INTENDED set at next revision.
- **Q3 (tail ac0 semantics)** — the contract's treatment (specify the
  observed mechanism, note no deliberate handler-result has ever been
  produced) is correct as written; no change.
- **Q4, Q5** — recorded as written; no action.

## Review highlights

The token-value pin (NativeDesign §2) is the review's centerpiece: the
full-register-file ruling PINS ac1 to the real frame address in M3b,
deferring scan-3's abstraction license to M4 — the contract
constraining the design exactly as the phase split intended (REPORT
H2). E5's "cannot be silently wrong" risk argument, the three-sentence
conformance summary (§11), and the T?AREA dual-role acceptance test
for the l2_depth mechanism (H1) are the other keepers. *(Note, Aug 13
2026: H1 has since been RESOLVED without l2_depth — see REPORT H1 and
docs/CrossingsChecker.md; the T?AREA dual-role case resolves by
construction there. Do not implement l2_depth.)*

## Phase 2

Project 7 = implement NativeDesign against L2Contract, harness
accounting FIRST behind a flag (H1), then structures, then wrappers,
A/B per the §6 matrix incl. the new bad-token inject shape (H7).
Prompt to be written by the reviewer when launched.
