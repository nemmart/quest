# Project 8 — Phase 2: the Stack-Free L2 (the M3b conforming implementation)

Hi Claude! Solo implementation session; the user reviews at the plan
gate and at each landing. (Numbering note: "Project 7" is a burned
name — see the superseded banner in Project7/PROMPT.md — so Phase 2 is
Project 8.)

This is the project the last three months built toward: replace the
bit-faithful native L2's stack-hosted handler state with native
structures, conforming to L2Contract.md, verified continuously by the
crossings-only checker against the master's original bytes. Staged
M3b scope (Plan.md): frames/args/locals STAY on the MV stack; handler
STATE is fully native; NOTHING walks the MV stack to find a handler;
I.GOTO still cuts the real stack (until M4).

DISCIPLINE: this rewrites the most intricate validated code in the
tree. Plan before code; explicit user go-ahead per stage; every stage
regression-clean before the next; loud failure over silent (METHOD
§8); record corrections as corrections (§11). NO checker changes (the
crossings-only checker is DONE — CrossingsChecker.md; if it needs a
fix, that is a finding to STOP on, not an edit to slip in). NO
contract edits (report friction in REPORT.md; the contract outranks
this session).

## Read IN ORDER

1. docs/METHOD.md — binding.
2. docs/CheckerHistory.md + docs/CrossingsChecker.md — what "verified"
   means now; "What Phase 2 inherits" is your rendezvous obligation.
3. docs/Project6/L2Contract.md IN FULL — the law. §2 (abstract
   state), §3 (per-entry contracts incl. normative exit-register
   images), §5–§7 (crossings, ordering, validation regime), §8
   (stack surgery), §10 (Exclusions Register E7–E10), §11.
4. docs/Project6/NativeDesign.md IN FULL — the design you are
   building: §1 chain, §2 token pin, §3 C-record hosting, §4 B
   protocol, §5 signal_has_handler, §6 A/B matrix, §8 lifecycle, §9
   build list, §10 open flags. Note §7's status banner: harness
   accounting is DONE, differently than sketched.
5. docs/Project6/REPORT.md §6 — hazards H2–H7 (H1 resolved).
6. docs/Project6/REVIEW.md — Q1–Q3 adjudications (Q1: the
   ECA2/restart anomaly stays bit-faithful-emulate PERMANENTLY,
   subject to user veto — confirm it stands before building around
   it).
7. docs/SharedProtocol.md frozen interface 2 (+ its Aug 13 addendum)
   — the transfer-pairing rules your wrappers still owe.
8. runtime/*.cpp as they stand — you are rewriting their bodies, not
   their boundary behavior. The exit-register staging and wsp
   arithmetic in them is capture-validated; treat it as a quarry.
9. docs/Project4/ and Project5/REPORT.md §3 — the DEF?ON predicates
   and parity rules (H4/H5) you must carry over verbatim.

## Rulings — SETTLED with the user (Aug 13 2026; binding)

a. **I.EPILOG mismatched-frame pop**: assert + abort_world(save=false)
   — "the native version gets an extra check on PL/1 correctness."
   Contract inventory: AMENDED (THIRD ADDENDUM item 1, Aug 13) —
   nothing left to add; implement the abort.
b. **Walker outputs**: DROP ENTIRELY (overrides NativeDesign §10's
   compute-and-discard preference). Rationale: the cells are L2's
   own, neither read nor written by the conforming system; the
   no-L1-reader scan is the evidence and the checker is the
   enforcement. Add a one-line closed-gate assert only if it falls
   out naturally.
c. **Chain growth cap**: NONE (overrides the §10 proposal). The game
   is non-reentrant with finitely many routines, and the master hits
   real MV stack limits long before the native container notices; a
   cap is extra checks obfuscating the workings.
d. **Q1**: VETOED → the ECA2/restart non-descending I.GOTO shape is
   ABORT-INTENDED (abort_world(save=false), token and pc named;
   contract THIRD ADDENDUM item 2 carries the ruling — implement the
   abort). Rationale: post-Stage-C there is no
   coherent state to fall back to emulation with, and an abort IS the
   "reclassify only if it ever fires" mechanism, with full state
   captured. The mv_error_handler (below) is the living attic for
   forensics if it ever fires.
e. **Landing strategy**: the error_handler_api shape below;
   staging = instantiation choice, not code churn.

## The architecture (user-specified, Aug 13 2026)

**error_handler_api** — an object of pure virtual methods covering
HANDLER STATE DECISIONS ONLY: establisher push/pop, node
register/revert, chain cut, select, has_handler, sig-record
read/write, resume-flag read/write. Three implementations:

- **mv_error_handler** — the bit-faithful behavior: owns all
  real-memory chain/state cell writes, exactly today's wrapper
  semantics. This is also the LIVING ATTIC: the original translation
  kept alive as a buildable subclass, the forensic reference if any
  ABORT-INTENDED site ever fires.
- **native_error_handler** — TaskL2State (NativeDesign §1) as sole
  authority; writes NO private cells (Registers E9/E10).
- **check_error_handler** — instantiates BOTH, calls both, compares
  OUTCOMES; loud throw naming both sides on mismatch. Side-effect
  ownership is strict: mv owns real-memory writes, native owns the
  chain; the composite never lets native touch cells or mv touch the
  chain — genuinely both-in-parallel, not an entangled hybrid.

**Boundary discipline** (keeps the API honest):

1. Only handler STATE goes behind the interface. Exit-register
   staging, wsp reservations (H3: I.PROLOG +4, O.ON allocate +8,
   I.GOTO snapshot restore), and transfer pairing stay as COMMON
   code outside it — normative contract outputs identical across
   implementations, staged in exactly ONE place.
2. API decision methods return OUTCOME STRUCTS, not staged
   registers: select → {found, handler_pc, token, establisher_frame,
   ...}; cut → {target_frame, wsp_restore, ...}; etc. The shared
   wrapper body maps the outcome onto ac0-3/wsp once, using staging
   expressions lifted from today's capture-validated wrappers.
   check_error_handler compares the two outcome structs
   field-by-field BEFORE staging and stages from the verified-equal
   result — exit register state cannot differ between modes by
   construction; modes can only differ in outcomes, which is
   exactly what a divergence report should name.
3. RESIDUE registers whose frozen values are byproducts of mv
   internals become NAMED FIELDS in the outcome structs — the
   native implementation computes them deliberately (chain record +
   frame arithmetic; frames are real in M3b, addresses are
   landmarks even when cells go unwritten). Where residue is pure
   walk garbage, lift the producing expression verbatim from
   today's wrapper. The structs make the contract's residue
   obligations visible in types — the E-register story made
   explicit. Note the luck distribution (user-anticipated): the
   value-returning entries are near-WSAVS-clean (tiny structs); the
   frame machinery and the non-returning transfer paths are
   structurally not (constructed landing states, P4 §6.2
   inner-leaf residue), and the contract's per-entry exit images
   already catalog exactly which is which — transcribe, don't
   guess.

**Selection**: `-handler=mv|native|check` alongside -lockstep
(default check during bring-up, native once the matrix passes; mv =
the attic run). Staging is instantiation: check IS the shadow stage;
native alone IS the final stage; there is no throwaway scaffolding
to remove — the comparison machinery lives permanently in
check_error_handler.

**Landing order**: (1) carve the api out of the current wrappers
with mv_error_handler as the only implementation — pure refactor,
zero behavior change, full suite green; (2) build
native_error_handler + check_error_handler, run the whole matrix in
check mode; (3) flip the default to native, run the matrix again
(this is where E9/E10 blindness becomes real — last, alone,
everything else proven); mv stays in the tree as the attic.

## The hazard spine (REPORT §6 — address each explicitly in REPORT)

- **H2 token pin**: ac1 = the real frame address, exactly today's
  value; nothing may DEREFERENCE it for handler state. Do not
  "improve" to an id — that is M4's move.
- **H3 wsp reservations normative**: +4 / +8 / snapshot restores;
  login script is the cheap regression.
- **H4 resume-flag staleness**: resume_flag written ONLY by the
  O?SIGNAL entry path; shorthands never write it;
  def_on_would_run_native's no-flag-parameter rule carries verbatim.
- **H5 fallback-gate parity**: any predicate list living in two
  places changes in lockstep or count skew appears at terminal
  pairs.
- **H6 DISPATCH_RET**: the rendezvous FIRES already (checker piece
  4); build the native handling of the EE40 tail after that pair —
  replacing today's arm-whole-span-emulation continuation. This is
  the one place Phase 2 touches pairing behavior at all; prove it on
  inject shape 3.
- **H7 new inject shape**: deliberate bad-token injection →
  I.GOTO bad-chain ABORT-INTENDED, exercising the third result class
  end to end ONCE, on purpose (abort_world(save=false), `aborting`
  silences the checker, one banner).

## Validation

- The A/B matrix (NativeDesign §6) is the acceptance suite: login
  bracket traffic, M+dir+abc, FAIL_OPEN both signals through the
  ?FATAL terminal pair, inject shapes 1–3 (RESUME exercises H6),
  store-"ABC" node recycling, the H7 bad-token run, the :ABORT test
  terminal. 0 divergences everywhere; detach/abort/retire lines and
  write-back identical to the CrossingsChecker.md evidence.
- Exit-register fidelity is NORMATIVE including residue registers
  (Contract §7); the register file is never contract-private.
- Footprint captures: retired for contract-private storage
  (E7–E10), RETAINED for SHARED-PROTOCOL windows (B record) during
  bring-up — use QUEST_CAPTURE with DEST on ?LIB_ERROR paths at
  least once in Stage B.
- METHOD §10: every expected value in your checks comes from running
  that exact command.

## Deliverables

- The code: error_handler_api + the three implementations +
  rewritten runtime/ wrapper bodies staging from outcome structs;
  boundary behavior (signatures, exit images, transfer pairing,
  fallback gates) unchanged; warning-free build; `-handler=` switch.
- docs/Project8/REPORT.md: per-stage evidence, per-hazard
  disposition, the rulings as given by the user, corrections as
  corrections, interfaces touched, and the M4 revisit list (token
  re-mint; chain records' frame/wsp fields become the shadow
  accounting seeds — CheckerHistory.md Generation 3).
- CheckerHistory.md: NO new generation (the checker is unchanged);
  if Phase 2 forces any checker fix, STOP, get user sign-off, and
  record it there as a Generation-2 correction.
- Updates: NextSession.md rotation at session end; SessionPlan.md
  record; README.md index row for Project8/.

## Gotchas (the standing set + Phase-2 specifics)

- The standing environment set: NextSession.md "Environment
  gotchas" — ~49 s turns, scratch-copy QUEST/, stdbuf, login
  CL/Claude/quest/Y/any/F, M+dir+abc trigger, FAIL_OPEN recipe,
  QUEST_CAPTURE needs DEST, the two baseline-reproduced driver
  artifacts (plain L→P ESC never detaches here; shape 1 dies at
  ?FATAL if you wait a full turn after P — both are environmental,
  do not chase them).
- Drivers live in Project1/ (drive.py, drive_move2.py) and the
  crossings-checker session (drive_lp.py, drive_login.py — see
  CrossingsChecker.md evidence table for which run used which).
- Entry rendezvous precede your wrapper bodies (SharedProtocol
  addendum): argument state is compared BEFORE your code runs, so an
  entry-state bug shows at the door, not inside your span.
- The nested-in-fallback guard and the conditional fallback protocol
  (METHOD §12 addendum) still bind every wrapper.
- Sign-extended listing immediates (DEF?ON's "65535" is −1); the
  [0x70000124]→R.SIGREC restart vector; inner-leaf-calls-as-residue
  (P4 DERIVATION §6.2) — the three recorded DEF?ON traps.
