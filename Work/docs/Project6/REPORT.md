# Project 6 — REPORT (Phase 1: contract + native design)

Per SharedProtocol.md REPORT format. Solo session; planning only — no
emulator, runtime, or shared-doc changes; new files are
docs/Project6/{L2Contract.md, NativeDesign.md, REPORT.md}.

## 1. Status

| Deliverable | Status |
|---|---|
| Scans 1–5 | DONE pre-session (SESSION_REPORT_AUG13.md §§5–6); cited, not re-run; extended by two gap findings below (§4 c1, c2) |
| L2Contract.md | DRAFTED — 20 registered entries + the indirect entry specified with full-behavior Outputs tables; 10 untranslated symbols specified-or-excluded; Exclusions Register E1–E10; VALIDATION REGIME, STACK-SURGERY clause, OPEN QUESTIONS Q1–Q5 |
| NativeDesign.md | DRAFTED — chain/record/node structures, token pin, C-record hosting map, B accessors, strong signal_has_handler, harness-accounting worklist, lifecycle, A/B matrix |
| Contract sections per prompt | Inputs/Outputs/Ordering/Edge per entry: done. VALIDATION REGIME incl. rendezvous definition, A3 third-class composition, footprint retirement: done. STACK-SURGERY: done (the master-oracle argument spelled out). Deliberate Exclusions Register: 10 entries, each with evidence + risk |
| Applied rulings | All honored: defensive→ABORT-INTENDED (I.GOTO bad-chain annotated; L1 sites listed informationally at Contract §3.11; census untouched per A2); rendezvous=crossings (Contract §5/§7 + NativeDesign §7); full-file exit fidelity (every Outputs table normative incl. residue registers); PRIVATE-with-L3-observer restated with revisit condition; I?LINEID adjudication applied as DEAD-GUARDED (Contract §3.8) — no new evidence surfaced contradicting the override; six DEAD entries → Register E1–E6 citing existing tripwires |

## 2. translation_table entries

None — Phase 1 writes no code. Phase 2's registration surface is
unchanged (the existing 31-row table); NativeDesign §9 is its
worklist.

## 3. Interfaces exposed / consumed

Exposed (specification only): the contract itself; the
rt::signal_has_handler STRONG definition is restated over the native
chain (NativeDesign §5) — signature unchanged from o_signal.cpp's.
Consumed: SESSION_REPORT_AUG13.md (binding), Project 1–5 DERIVATIONs,
runtime/*.cpp as the analysis medium, Layering/METHOD/TerminalDetach/
M3Plan/O_ON.md.

## 4. Shared-doc / spec corrections (evidence cited; docs NOT edited)

- **c1 — Project1/DERIVATION.md §5, O?SIGNAL residue map: the
  select-loop segment's offsets are +2 words high.** From the XPSHJ
  line onward the prose forgets that O.SET's WRTN popped the EE38
  frame word (wsp returns to E+10, not E+12). Correct layout — which
  is what the capture-validated o_signal.cpp lays and what the 0-diff
  captures certify: XPSHJ wide at [E+12] (not E+14); key2 backup
  [E+14] (not E+16); helper images [E+16..E+26] + scratch [E+28]
  (not E+18../[E+30]); dispatch frame word [E+12] and wsp = E+12 at
  transfer (not E+14). The Contract §3.9 memory row uses the code's
  offsets and cites this correction. DERIVATION text should gain a
  pointer when next touched.
- **c2 — SESSION_REPORT scan-5 untranslated tally: R.SIGREC
  "References: none" is incomplete.** Project4/DERIVATION §5.2:
  I.GINIT installs [0x70000124] → 0x7017EB63, whose word0 =
  0x7017EE02 = R.SIGREC — a DYNAMIC dispatch path through the
  R?SIGNAL/I.GOTO anomaly branches (themselves never executed). The
  DEAD verdict stands with the weaker evidence class; Exclusions
  Register E5 carries the caveat and the risk argument. The scan-5
  table row should note the vector path when the report is next
  revised.
- (Restated, already flagged pre-session:) the Project 6 PROMPT's
  scan-2 seed "?LIB_ERROR reads/writes [+0x16]" — the +0x16 access
  is DEF?ON's read of [wsb−0x2A]'s high narrow; ?LIB_ERROR never
  touches it. Contract §2.2/§3.13 carry the corrected accessor.

## 5. Ambiguity flags (propose, don't decide)

- **F1 (= Contract Q1): I.GOTO shape 3 / R?SIGNAL anomaly
  classification.** The upward-wfp-link condition reads defensive
  (⇒ ABORT-INTENDED), but the restart vector is deliberately
  installed at boot by I.GINIT and targets R.SIGREC — a designed
  cross-task restart mechanism would be a SEMANTIC path deserving a
  full contract. Never executed; A1 says flag. Interim conforming
  behavior specified as bit-faithful emulation of the branch (today's
  implementation). Recommend: rule at review; if ABORT-INTENDED,
  Register E5's dynamic path collapses to "unreachable by
  construction".
- **F2 (= Contract Q2): per-frame condition-slot privacy** rests on
  the ratified provenance argument + lockstep backstop; scan 1
  enumerated the wsb BAND only, not frame slots or on-stack nodes.
  Proposal: accept on the same basis the user ratified for
  indirect-pointer access (compiled PL/1 cannot name
  compiler-reserved slots; violations are divergence-loud). A
  frame-slot scan is possible (grep positive small displacements off
  frame-provenance bases) but expensive to adjudicate; not
  recommended unless the backstop argument is judged insufficient.
- **F3 (NativeDesign §10): I.EPILOG against a non-establisher frame**
  has no native-chain analog; proposal = debug-assert-and-abort,
  which would add an unlisted ABORT-INTENDED site. Needs the A1
  classifier applied at review (my read: defensive — the condition
  means broken codegen bracketing — but it is also PL/1-impossible,
  so the annotation is nearly free either way).

## 6. Integration hazards for Phase 2 (the implementation session inherits)

- **H1 — land the crossings-only harness accounting FIRST, behind a
  flag** (NativeDesign §7): every other validation depends on pairing
  symmetry; the T?AREA dual-role case (crossing from ?LIB_ERROR,
  interior from ?DEFAULT_ERROR_HANDLER) is the acceptance test for
  the l2_depth mechanism.
  **RESOLVED (Aug 13 2026)** — implemented without the flag (user
  ruling: one sync model, no modes) and without l2_depth (deferred
  dispatch + both-role pending spans instead); recalibration gate
  passed at 0 divergences. The T?AREA dual-role case resolves by
  construction: called from native ?LIB_ERROR it is C++-interior to
  the composite (subsumed by the composite's boundary pairs); a
  future L1-emulated caller would pair at its entry via l2_bits.
  See docs/CrossingsChecker.md.
- **H2 — the dispatch token is VALUE-pinned in M3b** (full-file
  compare ⇒ ac1 must equal the master's frame address). Do not
  "improve" it to an id in Phase 2; that is an M4 change, tied to the
  frames ceasing to exist (NativeDesign §2).
- **H3 — wsp reservations are normative** (Contract §2.1/§6):
  I.PROLOG +4, O.ON allocate +8, I.GOTO snapshot restore. A
  stack-free chain that forgets the reservations diverges on wsp at
  the first rendezvous after an allocate. Cheap regression: the login
  script (O.ON allocate ×N).
- **H4 — the resume-flag staleness** (shorthands never write it) must
  survive re-hosting: resume_flag is written ONLY by the O?SIGNAL
  entry path, exactly as [wsb−0x2A] was. def_on_would_run_native's
  no-flag-parameter rule (P5 REPORT §3) carries over verbatim.
- **H5 — fallback-gate parity rule** (P5 REPORT §3's hazard rule)
  extends to the new implementation: any predicate/gate list that
  exists in two places (o_signal / def_on / p_defon) must change in
  lockstep or count skew appears at terminal pairs.
- **H6 — the DISPATCH_RET return crossing** (handler WRTN to
  0x7017EE40) is a rendezvous under the new definition; the existing
  pc3==DISPATCH_RET continuation logic is the seed of its handling
  but currently arms a whole-span emulation — Phase 2 must make it a
  genuine re-entry (NativeDesign §7.2). **Status Aug 13 2026: the
  rendezvous itself is BUILT** (run_steps breaks at EE40/E3EF arrival
  when no span is pending — CrossingsChecker.md piece 4, dormant on
  live paths since live handlers unwind via I.GOTO); what remains for
  Phase 2 is only the NATIVE handling of the tail after that pair.
- **H7 — QUEST_INJECT gains one shape**: a deliberate bad-token
  injection to exercise the ABORT-INTENDED third result class end to
  end (A3 composition), once, on purpose (NativeDesign §6).

## 7. Validation evidence

None generated — planning session; no emulator runs were needed (the
drafting questions all resolved from source + DERIVATIONs + the
pre-session scan evidence). The two corrections in §4 were found by
cross-checking the capture-validated code against its derivation
prose (c1) and the scan-5 tally against Project 4's vector finding
(c2) — both are documentation findings, not new machine evidence.
