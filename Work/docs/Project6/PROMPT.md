# Project 6 — M3b PHASE 1: The L2 Contract + Native State Design (planning; NO implementation)

**Two-phase plan (user-ratified)**: Phase 1 (THIS project) is planning —
the contract AND the data-structure design for the native
implementation. Phase 2 (Project 7, separate session) is coding it.
Phase 1 produces TWO documents with a strict relationship: the
contract is the BOUNDARY view (what any implementation owes L1,
implementation-agnostic — the governing principle below); the design
is the INTERNAL view (how the private state is represented natively).
The contract constrains the design; the design must NEVER leak into
the contract — if you catch yourself citing the design to justify a
contract clause, stop.

Hi Claude! Solo session; a reviewer session audits your deliverables.
This project produces the CONTRACT that a future stack-free L2
implementation will be built against. You write documents and scan
tooling; you do NOT modify the emulator, the runtime/, or any shared
doc (SharedProtocol merge rules apply; scan scripts go in
docs/Project6/tools/).

Read IN ORDER: docs/METHOD.md; docs/Layering.md (the "Why this
exists" section is this project's charter — the contract exists so L2
state can leave the MV stack, which unblocks M4); docs/Project1-5
DERIVATIONs (the per-routine ground truth); docs/O_ON.md;
docs/TerminalDetach.md; runtime/*.cpp — THE NATIVE L2 SOURCE IS YOUR
PRIMARY ANALYSIS MEDIUM (Layering "Analysis method"): every outbound
L2 crossing is a typed C++ construct (native_transfer /
native_return(_ss) / entry_address fallbacks / terminal
convergences); enumerate crossings by reading wrapper return
expressions, and use the disassembly only where the scans require it.

## Deliverable 1 — DONE (SESSION_REPORT_AUG13.md §§5–6). Cite it;
   extend only where drafting exposes a gap. Original spec kept below
   for reference:

## (reference) Deliverable 1 — docs/Project6/SCANS.md

Run these; report method, exact commands, and findings per METHOD
§10 (claims come from commands you actually ran):

1. **No-L1-reader scan.** Enumerate L2-private state candidates from
   the DERIVATIONs: the chain head [wsb-0x40], chain-node/condition-
   frame fields (P3's frame layout; P1's node layout), walker outputs
   [wsb-0x36]/[wsb-0x38], O.SET/DEF?ON scratch. Then scan ALL of L1
   (game code + L1 runtime, emulated OR native) for any instruction
   that reads or writes those locations/offsets. sb-relative
   addressing makes this tractable (grep XWLDA/XWSTA/XLEF with the
   sb-relative displacements; then the harder part — indirect access
   through captured pointers — argue it or bound it honestly).
   Every location with ZERO L1 readers/writers becomes CONTRACT-
   PRIVATE. Any with L1 access becomes CONTRACT-OBSERVABLE and must
   be preserved exactly by any implementation.
2. **The task-area field census.** The area (rt::t_area, wsb-0x29,
   record base wsb-0x21) is SHARED L1/L2 state — ?LIB_ERROR_CODE
   (L1) reads it, ?LIB_ERROR (L1) reads/writes [+0x1]/[+0x3]/
   [+0x1E]/[+0x20]/[+0x16]. Classify EVERY area field touched by any
   code as PRIVATE / OBSERVABLE / SHARED-PROTOCOL, with the accessor
   list per field. Where evidence is ambiguous, PROPOSE a
   classification and FLAG it for review — do not decide silently.
3. **Opaque-token scan.** The 26 ON-unit bodies (ON_ERROR_CATALOG)
   receive the establisher-frame token and hand it to I.GOTO. Verify
   each body holds/passes the token WITHOUT dereferencing it. Any
   dereference is a finding that shapes the contract (the token may
   need to stay a real address).
4. **Jump-edge pass.** Scan branch/jump targets (not calls) in
   emulated L1 and L2 code for layer-crossing transfers the census
   missed. (L2's native side is already covered by reading source.)
5. **Boundary inventory from source.** The complete list of L2
   crossings: every L1→L2 entry (registration table + LJSR/vector
   entries), every L2 outbound (each wrapper's return expressions),
   every L2→L3 descent. Cross-check against Project1-5 REPORTs.

## GOVERNING PRINCIPLE (user-ratified — read before writing anything)

The contract specifies **what the L2 code DOES and RETURNS — full
behavior, every branch — derived from the native source and the
Project 1–5 DERIVATIONs.** It is NOT "what Quest has been observed to
use": observed use inherits every hole in our coverage, and cold
branches are still contract. Deviations from full behavior are
permitted ONLY as explicit entries in a **Deliberate Exclusions
Register** inside the contract: each entry states what is excluded,
the evidence of non-use (cite the scan or coverage data), and the
risk if the evidence is wrong. Empty register is a fine outcome.
"Horrendous complexity that Quest provably never touches" is the
intended use of the register — corner-cutting is the exception you
argue for, never the default you drift into. Consequently the scans
below do NOT define the contract's scope; they LICENSE individual
exclusions (e.g. the no-L1-reader scan justifies marking specific
residue contract-private, entry by entry).

## Expectation calibration (from the user)

The contract will likely be CONCEPTUALLY SIMPLE — this is a glorified
try/catch with a finite, known call surface. Do not manufacture
abstraction. The genuinely hard part, where the effort belongs, is
MECHANICAL PRECISION: the exact input-register / output-register /
flags state for every call, and the **in-stream literal words that
follow some call sites** — I.PROLOG's LJSR is followed by data words
its pc+7 continuation skips (P3's derivation), O.ON's by its pc+3
convention; enumerate every such convention, exactly which words,
who reads them, and what the continuation pc arithmetic is. If your
draft contract is short and its tables are dense, you are probably
doing it right.

## APPLIED RULINGS (from the pre-project session — SESSION_REPORT_AUG13.md
is in this directory and is binding; scans 1–5 are DONE there, do not
re-run them — BUILD ON their results)

- **Defensive raises → ABORT-INTENDED** (second addendum + A1–A3 in
  the report): classify by principle (defensive = runtime's own
  invariants broken; semantic raises keep full signal contracts);
  contract annotates L2-INTERNAL defensive branches ABORT-INTENDED
  (abort_world, save=false), no recovery path specced; L1 caller
  sites listed informationally only; census updates deferred.
- **Rendezvous granularity** (report §3): the contract's sync surface
  is L1↔L2 CROSSINGS only — interior L2→L2 calls are invisible
  implementation. Define what a rendezvous IS in the VALIDATION
  REGIME; name the harness consequence (batch accounting must count
  crossings only once the stack-free L2 lands).
- **Exit-register fidelity** (report §4): FULL register-file compare;
  Outputs tables are NORMATIVE for every register and flag at every
  exit including scratch residue. The only narrowed compare is
  L2-private STORAGE footprints, licensed entry-by-entry.
- **PRIVATE-with-L3-observer** (report scan-2 flag 1): RATIFIED as
  proposed — contract-private for the clone; L3 observers run
  master-only where authentic cells exist by construction. Revisit
  only if an ABORT-INTENDED path ever routes live into clone-side
  ?FATAL (it must not — that is what ABORT prevents).
- **ADJUDICATION — I?LINEID descent**: report scan 5 lists
  "O.SET walker → I?LINEID — LIVE" — this is WRONG as written and is
  OVERRIDDEN by Project 1 correction 4b (verified against the .PR
  bytes; M3Plan.md): the walker's WLDAI-0 gate makes that branch
  statically dead in this binary; the native walker re-checks the
  gate wide at runtime and falls back if it were ever patched live.
  The contract specifies the branch as DEAD-GUARDED (full behavior:
  document the gate, the re-check, and the fallback), not as a live
  descent. If the scans session has NEW evidence, surface it before
  drafting.
- The six DEAD untranslated entries (report scan 5): Deliberate-
  Exclusions-Register entries with zero-reference evidence; the
  requested tripwires ALREADY EXIST by construction (every ST symbol
  carries a logging stub). The four frozen entries cite ruling 6.

## Deliverable 2 — docs/Project6/L2Contract.md

For EVERY L2 entry (the M3Plan table + the Project4/5 cluster +
O.ON/O.REVERT), specify:
- **Inputs**: registers/arg cells/area fields consumed, with the
  staging convention (cite the DERIVATION).
- **Outputs — FULL behavior**: registers AND flags at every exit
  (return/transfer/descent, all branches incl. cold ones), memory
  written, continuation/transfer semantics (WRTN vs pc+k vs
  transfer-to-arbitrary-pc vs descent). Residue moves OUT of the
  contract only via a Deliberate Exclusions Register entry citing
  scan 1 — never by omission.
- **Ordering/state obligations**: what persists across calls (the
  chain as an abstract stack of establisher records; the area
  protocol; the message buffer), specified ABSTRACTLY — the whole
  point is that the representation is free.
- **Error/edge behavior**: fallback conditions, terminal descents,
  the resume-flag convention (P5), recursion behavior.
Plus three general sections: the VALIDATION REGIME (the pair gate
keeps verifying pc+registers at every rendezvous — a conforming
implementation is A/B'd against the bit-faithful native L2 under
lockstep; footprint captures of contract-private memory are retired);
the STACK-SURGERY clause (non-local wsp/wfp manipulation is L2's
exclusive right — state what a stack-free implementation owes the
MASTER's stack, which continues to exist and be walked by L3: answer
= nothing post-detach, but the master emulates L2 too, so its stack
stays authentic BY the A/B design — spell this out carefully, it is
the subtlest paragraph in the document); and OPEN QUESTIONS you
could not settle, flagged loudly.

## Deliverable 3 — docs/Project6/NativeDesign.md (the Phase-1 data structures)

Design the native handler-state representation for the STAGED M3b
implementation (Plan.md: MV stack keeps frames/args/locals; handler
state fully native; NOTHING walks the MV stack to find a handler;
I.GOTO still cuts the real stack until M4). Specify:

- **The native chain**: per-task structure replacing the walked
  chain — each record carries at minimum the establisher's wsp LEVEL
  (I.GOTO must still cut the stack to it), the handler body address,
  and whatever the O.SET select needs (type/key discrimination per
  the derivations). Push/pop discipline mapped to I.PROLOG/I.EPILOG/
  O.ON/O.REVERT per their contract entries.
- **Token representation**: scan 3 licenses an abstract token; pick
  one (record id vs pointer), define mint/validate/round-trip through
  dispatch→body→I.GOTO, and the bad-token behavior (ABORT-INTENDED
  per the addendum — this is I.GOTO's bad-chain defensive branch).
- **C-record hosting**: where each contract-private cell
  (SESSION_REPORT census) lives natively; the address-as-landmark
  cells stay physically present on the master by construction —
  state what, if anything, the clone still writes to the real static
  area (goal: nothing).
- **The B-record / area protocol**: stays in real memory (it is
  SHARED-PROTOCOL with L1 — ?LIB_ERROR/?LIB_ERROR_CODE are L1);
  design only L2's accessors.
- **rt::signal_has_handler (strong)**: specified over the native
  chain (scan-1 finding a: an exported L2 predicate, never an L1 raw
  read).
- **Harness accounting**: the crossings-only rendezvous change
  (report §3) — name the exact mechanisms that must change (batch
  break sites, span accounting) WITHOUT implementing them; Phase 2
  inherits this as its harness worklist.
- **Per-task lifecycle**: creation (T.INIT epoch), the boot fault
  window, teardown (?UKIL/retire) — when native chains are created
  and destroyed relative to task lifetime.
- **A/B plan sketch**: how Phase 2 validates — bit-faithful L2 vs
  design-conforming L2 under lockstep, which QUEST_INJECT shapes
  exercise which structures, the ABORT-INTENDED third result class.

## Report

docs/Project6/REPORT.md per SharedProtocol format: status per scan
and per contract section; shared-doc corrections; the ambiguity
flags; integration hazards for the future implementation session.

## Gotchas

Turn cadence ~49s on slow containers; M+dir+abc = cheap signal
trigger; QUEST_INJECT exists (Project5) if you need live evidence of
a staging convention; scratch-copy QUEST/; stdbuf; login
CL/Claude/quest/Y/any/F; grep -c "warning" false-positives on full
builds. You may RUN the emulator freely for evidence; you may not
change it.
