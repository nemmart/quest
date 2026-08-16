# Pre-Project-6 discussion session — rulings and prompt deltas (Aug 13 2026)

Discussion session with the user, held before launching Project 6.
Everything below is user-ratified. Take these into the Project 6
session as prompt addenda / contract requirements.

## 1. Second addendum status

The GOVERNING PRINCIPLE addendum is already in Project6/PROMPT.md.
The SECOND ADDENDUM (defensive raises → ABORT-INTENDED) is NOT yet in
the tree copy — it must be added, together with rulings A1–A3 below.
Text of the second addendum (verbatim, user-supplied):

> Layering open question (a) is now CLOSED, subsumed by ruling 7 —
> defensive raises that detect corrupted internal state (the O.SERROR
> sites in the heap, I.GOTO's bad-chain raise, ?CREATE_TASK failure
> paths) are ABORT-class. In the contract, mark those specific
> branches ABORT-INTENDED: specify the detecting condition and current
> raise behavior per the governing principle (full behavior), but
> annotate that a conforming implementation aborts (abort_world,
> save=false) rather than signaling — do not spec a recovery path for
> them.

## 2. Rulings A1–A3 (clarifications to the second addendum)

**A1 — the ABORT-INTENDED list is NOT closed; classify by principle.**
The three named sites were illustrative. Sweep — but the ruling covers
DEFENSIVE raises only: conditions indicating the runtime's own
invariants are broken (corrupt chain, bad heap header, impossible
internal state). It does NOT cover semantic raises — conversion
errors, failed opens, user-facing conditions — which are live, handled
gameplay and get full signal-behavior contracts. Classifier test:
"does this condition mean our own state is corrupt, or is it reporting
a legitimate external condition?" Flag ambiguous sites for review
rather than deciding. SCOPE: most defensive-raise sites (heap,
?CREATE_TASK) are L1 callers into L2 — outside the contract's surface;
list them informationally in the REPORT (governed by the deferred
wire-when-touched instruction, not the contract). What the contract
itself annotates ABORT-INTENDED is the L2-INTERNAL defensive raises —
I.GOTO's bad-chain, and any O.SET/DEF?ON-cluster anomaly branches the
full-behavior enumeration surfaces. The branches are being read anyway;
classification is nearly free.

**A2 — defer the census.** The Layering census records facts about the
code as it is; heap/tasking remain raisers in fact until those sites
are converted (when next touched, not now). Do NOT update Layering.md.
The REPORT may footnote which symbols become L0-eligible
post-conversion; the practical payoff already evaporated under lazy
closure.

**A3 — ABORT-INTENDED firing is a distinguished third result class.**
The VALIDATION REGIME section must state: an ABORT-INTENDED branch
firing on EITHER side terminates the A/B run as a third result class —
neither pass nor compare-failure — and mandates investigation before
further runs, because per ruling 7 the branch firing at all means
corrupted state was detected: the A/B just found something real (a
game bug faithfully reproduced, or an implementation bug that
corrupted the chain). Mechanically this composes with built machinery:
the conforming implementation's branch calls abort_world(save=false),
the `aborting` flag silences the checker, the world stops with the
named reason — no divergence spam, one banner. Write exactly that
composition into the section.

## 3. Rendezvous granularity (settled this session)

**Problem raised by the user**: interior L2→L2 calls (e.g. I.PROLOG
calling FUNCTION_A) are today pairing events, because both engines run
the bit-faithful native code. If the contract inherits that, every
conforming implementation is forced to "call FUNCTION_A" — interior
structure leaks into the contract, contradicting Layering's "interior
structure is explicitly unclassified implementation."

**Ruling: rendezvous points are LAYER CROSSINGS, not function calls.**
- The contract's sync surface is the set of L1↔L2 crossings only.
  When the stack-free L2 runs, an L2 entry from L1 opens a span; the
  master run-to-returns the whole subtree (METHOD §7 absorption,
  exactly as inner native calls behave in spans today); comparison
  happens at the L1 continuation. Interior L2 entries are not
  rendezvous.
- L2 spans are not simple call-return: one signal produces several
  crossings (raise L1→L2, dispatch L2→L1 into the ON-unit body,
  I.GOTO L1→L2, unwind transfer L2→L1). The rule is "sync at every
  L1↔L2 crossing"; L2-internal calls between crossings are invisible.
- **Harness consequence (name it in the VALIDATION REGIME)**: the
  batch/ordinal accounting must count only L1↔L2 crossings once the
  stack-free L2 lands, or pairing goes structurally asymmetric before
  a single register is compared. The contract must define what a
  rendezvous IS post-contract; the implementation session inherits
  that definition rather than discovering it.

## 4. Exit-register fidelity (settled this session)

**Question**: at L2 exits, narrow the compare to contract-specified
outputs only (scratch registers become contract-private), or keep the
full register-file compare and require the implementation to reproduce
scratch residue?

**Ruling: full-file compare; exit registers are normative down to
scratch.** Rationale (user): we do not know what the 1988 compiler
assumed about register state at these return events; if it assumed
more than we think, a narrowed compare produces
difficult-to-debug situations. So:
- The contract's Outputs tables are NORMATIVE for every register and
  flag at every L2 exit, including apparent garbage/residue. A
  conforming implementation that computes the result differently must
  still stage the same exit-register image (source: DERIVATIONs +
  native source; enforcement: lockstep, continuously).
- No new "contract-private register" concept; no compare-gate changes.
  The ONLY narrowed compare is memory footprints of L2-private
  STORAGE, licensed entry-by-entry by the scans (footprint-capture
  retirement as already specced).

## 5. Scan 1 (no-L1-reader) — STATIC PASS DONE this session

Tooling: docs/Project6/tools/scan1_no_l1_reader.py (v1, seven candidate
cells) and scan1_full_band.py (v2, authoritative — full wsb band
−0x40..−0x21 in both encodings: wrapped 15-bit displacements
`+0x7FC0..7FDF]` off any base, and absolutes `[0x70001000..0x7000108F]`
for main-task wsb=0x7000108C). v1 found every known L2 accessor from
the Project 1–5 DERIVATIONs, validating the method against ground
truth. Encoding discovery recorded for reuse: the disassembly renders
sb-relative access as `[acN+0x7FXX]` — sign-wrapped, register-
dependent; "[wsb-0x40]" in the derivations = `+0x7FC0]` with the base
holding wsb. Adjudication of base-register provenance is MANDATORY.

**RESULT: zero live L1 readers or writers of any condition-system
cell.** Complete adjudicated hit list (7 hits, reproducible):

1–4. GAME 0x70168164/78, 0x70168440/67 (`XN* [ac2+0x7FC2/0x7FC0]`) —
   **FALSE POSITIVES.** In each, ac2 is loaded from game-world
   pointers ([0x70000210]/[0x70000212] world bases → player records)
   in the instructions immediately preceding; these are narrow game-
   data field accesses at negative record offsets that coincidentally
   share the displacement encoding. Not wsb-relative.
5. T.INIT 0x7017E7AA writes [new_wsb−0x2E] — L1 task creation
   initializing a per-task cell in the NEW task's static block, after
   `STASB 3` installs the new wsb. Outside the condition-cell set;
   init-epoch write. Classify wsb−0x2E during the area census.
6–7. I.GINIT 0x7017EAD0/E5: `LLEF n,[0x7000104C]` — **address-as-
   landmark**: chain_head's ADDRESS is used as an arithmetic base for
   heap-bounds computation at boot; the content is never read or
   written. Contract note: the cell's VALUE is contract-private; its
   LOCATION in the static area is load-bearing for L1 init (no
   constraint on the clone's re-hosted representation — the master's
   static area is untouched, and I.GINIT runs identically on both
   engines against the real static area regardless).

Coverage of the METHOD §4 traps: the five heap-region disassembly
holes (0x7017E86B, 0x7017E899, 0x7017E97C–B3, 0x7017EA34–58,
0x7017EA6D–7A) were checked by raw-word scan — X-format displacements
are literal words in the instruction stream, so a candidate reference
would surface as a 7FC0–7FD7 or 104C word; **all five holes clean**
(Tools/ absent from this environment, so re-disassembly was not
possible; the raw-word check is conservative and sufficient for this
question). The two XCT sites (0x7017E9F6, 0x7017ECF4) build
ENQH/ENQT queue opcodes from immediates operating on queue
descriptors — no cell references.

**Indirect-access bounding argument (user-ratified basis)**:
provenance, not exhaustion. Compiled PL/1 gives game code no way to
name runtime-private cells; every pointer game code captures
originates from game data structures (the false positives above are
exactly this pattern); no game instruction references the runtime
static window in either encoding; and the stack-surgery law reserves
structural stack access to L2. Backstop: if the privacy claim is
wrong anyway, it surfaces as a lockstep divergence the moment the
stack-free L2 runs the offending path — a wrong scan conclusion is
loud, not latent.

**Contract-shaping findings (fold into L2Contract.md):**
a. `rt::signal_has_handler` (native lib_error.cpp, weak stub) is a
   REAL L1→L2 crossing: its proposed strong contract is "true iff the
   walk from [wsb-0x40] finds a handler for this code." It must be
   specified as an **L2 contract operation** (an exported predicate
   over abstract chain state), never as an L1 raw read — otherwise
   re-hosting the chain breaks ?LIB_ERROR's gate. This is the
   governing principle catching a would-be corner.
b. Address-as-landmark class exists (finding 6–7); the contract
   should name it so "contract-private" is understood as privacy of
   VALUE, with static-area geometry preserved on the master by
   construction.
c. The candidate set for privacy claims is the FULL wsb band, not the
   seven condition cells: L2 also uses wsb−0x34/−0x32/−0x30/−0x2C
   (I.SFALT/I.SFCON/DERR state — frozen per ruling 6 but present),
   wsb−0x2A (O?SIGNAL arg4), wsb−0x29 (area pointer, T?AREA/O?AREA),
   and wsb−0x22 appears once; per-cell classification belongs in the
   scan-2 area/static census. Zero L1 access to ANY of the band
   (findings 5–7 aside, adjudicated above).

## 6. Evidence scans 2–5

Scan 1 (no-L1-reader) is recorded in section 5 above.
Scans below use the same encoding rules established there: sb-relative
access renders as `[acN+0x7FXX]` (15-bit sign-wrapped displacement);
narrow (XN*) displacements are halfword-indexed; base-register
provenance adjudication is mandatory on every hit. Tools live in
docs/Project6/tools/; every table below is reproducible from them plus
the greps quoted in the session transcript.

---

### Scan 2 — Task-area / static-band field census

Two records, two getters:

- **C = wsb−0x40** — condition record, exported by O?AREA
  (`rt::o_area` = wsb−0x40, confirmed native + emulated).
- **B = wsb−0x21** — task error record: T?AREA returns P = wsb−0x29;
  every caller immediately forms B = P+8 (`WMOV 0,2; XLEF 2,[ac2+0x8]`;
  native `rt::t_area(machine) + 8`).

T?AREA/O?AREA caller sets (complete; NO game callers of either):
T?AREA ← ?LIB_ERROR_CODE (0x7017DE27), ?LIB_ERROR (×9 sites
7017E33C..7017E3C2), ?DEFAULT_ERROR_HANDLER (7017E3D4).
O?AREA ← DEF?ON (7017EF07), ?FATAL (7017F03C).

#### Census table (direct-encoding sites from full-band scan, all
layers, merged with pointer-based accessors resolved above)

| Cell | Accessors (r/w, layer) | Proposed class |
|---|---|---|
| wsb−0x40 chain head | L2: I.PROLOG, I.EPILOG, I.GOTO, O.SET, O.REVERT, R?SIGNAL, I.SFALT, O?AREA (addr export). L3 r: I?LINE. L1: I.GINIT address-as-landmark (§scan-1), T.INIT init-epoch analog. | CONTRACT-PRIVATE value; address is landmark (master static area untouched by construction) |
| wsb−0x3E type (C+2) | w O.SET; r R?SIGNAL, DEF?ON(L2), ?FATAL(L3) | PRIVATE-with-L3-observer (FLAGGED, see below) |
| wsb−0x3C key2 (C+4) | w O.SET; r DEF?ON(L2), ?FATAL(L3) | PRIVATE-with-L3-observer — **corrects Project 1** ("no reader anywhere" was direct-encoding-only) |
| wsb−0x3A code (C+6) | w O.SET, O.SERROR; r R?SIGNAL, DEF?ON, ?FATAL | PRIVATE-with-L3-observer |
| wsb−0x38 walker res2 (C+8) | w O.SET; r ?FATAL(L3) only | PRIVATE-with-L3-observer — corrects Project 1 note likewise |
| wsb−0x36 walker res1 (C+A) | w O.SET; r ?FATAL(L3) only | PRIVATE-with-L3-observer |
| wsb−0x3D, −0x39 (narrow) | L3 P?SNAP traceback rendering only | L3-only; private w.r.t. L1/L2 contract |
| wsb−0x34/−0x32/−0x30 | I.SFALT only | FROZEN-PRIVATE (ruling 6) |
| wsb−0x2C | DERR.TRP (L3) w | FROZEN-PRIVATE (DERR family, master-only) |
| wsb−0x2A resume/arg4 | w O?SIGNAL; r O.SERROR, DEF?ON (narrow [C+0x16], bit15 = resume flag; def_on.cpp:105–164) | CONTRACT-PRIVATE (all-L2) |
| wsb−0x29 area ptr cell | T?AREA XLEF (addr export); P?SNAP(L3) | address-landmark; value private |
| wsb−0x2E | w T.INIT (L1, init-epoch, new task's block); r ?UKIL (L3) | L1-INIT-OBSERVABLE; outside condition system |
| wsb−0x22 | I?PCS (L3) only | L3-only |

#### B record (task error record, wsb−0x21; all pointer-based —
positive offsets never appear in the 7FCx band)

| Field | Accessors | Class |
|---|---|---|
| B+0 flags | ?LIB_ERROR sets bit 0 (`WSUB 0,0; WBTO 2,0` @7017E35D) | SHARED-PROTOCOL |
| B+1 error code | w ?LIB_ERROR (native lib_error.cpp:312); r ?LIB_ERROR_CODE (:454), ?DEFAULT_ERROR_HANDLER (:143 via handler path) | SHARED-PROTOCOL. Game observes ONLY via ?LIB_ERROR_CODE — exactly one game call site, 0x70175EE4 |
| B+3 msg buffer ptr | rw ?LIB_ERROR (:267 r, :330 zero, :351 w); freed/alloc'd via I.FREEW/I.ALLOC (L1 heap) | SHARED-PROTOCOL |
| *(B+3) buffer | len narrow @ buf[0] (`XNSTA 0,@[ac2+0x8003]`), then text bytes (WCMV copy) | SHARED-PROTOCOL (contents) |
| B+0x1E handler fn ptr | r/w ?LIB_ERROR; lazily defaulted to 0x7017E3D2 (?DEFAULT_ERROR_HANDLER) when 0 (:306); **called through** via `XCALL [ac2+0x0]` @7017E3CD | SHARED-PROTOCOL — this is the ONLY indirect L1→L2 entry (see scan 5) |
| B+0x20 companion arg | w ?LIB_ERROR (:307 zeroed with default; :386 r), passed in ac1 to handler | SHARED-PROTOCOL |

Initial state: static area zero-fill at load; B+0x1E/B+0x20 lazily
installed by ?LIB_ERROR on first use (no boot-time initializer —
SWAT.NIN's zeroing block is 0x70000126+0x74, not our band).

#### FLAGGED for review (per prompt: propose, don't decide silently)

1. **PRIVATE-with-L3-observer** as a class: every non-frozen condition
   cell has zero L1 readers but ?FATAL/P?SNAP/I?LINE (all L3,
   master-only, terminal or diagnostic) read them via O?AREA or
   directly. Proposal: contract-private for the stack-free clone L2
   (L3 never runs on the clone; on the master the authentic cells
   exist by construction). If the ABORT-INTENDED addendum ever routes
   a live path into ?FATAL on the clone, this class must be revisited.
2. **Project 1 correction**: the "no reader" notes for wsb−0x3C/−0x38/
   −0x36 hold only for direct encodings; pointer-based readers exist
   (DEF?ON — already contract-covered by Project 5; ?FATAL — L3).
   DERIVATION text should gain a pointer to this census.
3. Prompt's scan-2 seed list said ?LIB_ERROR touches "[+0x16]" — not
   found in ?LIB_ERROR (native or emulated); the +0x16 access is
   DEF?ON's read of wsb−0x2A. Prompt to be corrected when updated.

---

### Scan 3 — Opaque-token scan: VERDICT CLEAN

Tool: automated token-tracker over all 26 catalog bodies (body = XLEF
target preceding each `LJSR O.ON`), token = entry ac1 plus its WSAVS
save slot fp−6, flagging any use of a token-holding register as a
memory base before clobber. Result: **zero dereferences in all 26
bodies.** 24 exit via I.GOTO; #22 (0x70175EB0) and #23 (0x70175EE2)
exit WRTN — exactly the catalog's Category D fatal pair. Category C
(#24, READ_IN, body 0x701766FE) stores its default to the global
INPUT_RESULT (0x7000021C), not through the token; spot-checked by eye
along with Category B (#10) and Category A (#5).

Full I.GOTO caller tally (27 sites) closes the token story beyond the
26 bodies:
- 21 × `WMOV 1,0` (handler bodies, token = entry ac1)
- 3 × `XWLDA 0,[ac3+0x7FFA]` (handler bodies with locals; sanctioned
  fp−6 reload)
- 2 × `WMOV 2,0` — game NON-handler non-local exits; ac2 sourced from
  the caller's own frame slot; held, moved, never dereferenced
- 1 × `XWLDA 0,[ac2+0x7FCE]` @7017EC0A = I.SFALT internal (wsb−0x32,
  frozen)

24 handler I.GOTO exits = 26 − 2 fatal ✓ (catalog agreement).

**Contract consequence**: L1 constrains the token to a round-tripped
opaque value only — the stack-free L2 may mint tokens in any
representation, provided dispatch→body→I.GOTO round-trips. The
machinery that TREATS the token as a frame address (R?SIGNAL match,
I.GOTO unwind/landing) is entirely L2-internal.

---

### Scan 4 — Jump-edge pass: VERDICT CLEAN

All non-call transfers (WBR/XJMP/LJMP + jump tables) in both images,
source layer vs target layer:

- Static layer-crossing jumps: only SWAT.REX (L3) `LJMP @[0x700001B4]`
  ×2 — master-only debugger break vector through low statics. No
  game↔runtime static jump edges in either direction.
- Indirect jumps, all known machinery: I.GOTO `XJMP [ac2+0x0]`
  @7017ECA0 = the landing dispatch (THE designed L2→L1 crossing);
  I.SFALT `XJMP @[ac2+0xFFCC]` (frozen); I.START ×2 (boot); ?UKIL/
  ?UTSK (L3 kill trampolines).
- Jump tables: game@7016018D (the apparent L3 targets are 0xFFFFFFFF
  sentinels — classifier artifact, all real targets game-side),
  game@7016BF1B, SWAT.REX@7017E552 (L3→L3), I.ALLOC@7017E875 (L1→L1),
  DERR.TRP@7017ED75 (L3→L3). No crossings.

---

### Scan 5 — Boundary inventory (native source + REPORT cross-check)

#### L1→L2 entries
1. **Registered translations** (RTStubs translation_table, 31 total),
   of which the L2 members (Layering census) are the 20:
   O.ON, O.REVERT, T?AREA, I.PROLOG, I.EPILOG, I.GOTO, O?SIGNAL,
   O.SET, O.SERROR, O.SCONVE, O.SSUBSC, O.SFIXED, O.SZEROD, O.SOVERF,
   O.SUNDER, ?DEFAULT_ERROR_HANDLER, O?AREA, P?DEFON, R?SIGNAL,
   DEF?ON. (Others in the table are L0/L1: fill/udiv/u2c, lock pair,
   heap four, ?LIB_ERROR, ?LIB_ERROR_CODE.)
2. **Indirect entry**: ?LIB_ERROR (L1) `XCALL [ac2+0x0]` through
   B+0x1E — the handler-pointer protocol field, default
   ?DEFAULT_ERROR_HANDLER (L2). The ONLY data-held L1→L2 entry.
3. **Untranslated L2 surface** (census symbols NOT in the table) —
   the contract must specify or explicitly exclude each:
   I.WPROLO, I.DISPLA, I.SFALT, I.SFCON, R.GOTO (alias branch into
   I.GOTO), I.FFALT, O.SEARCH, O.SIGNAL, R.SIGREC, R.SIGNAL.
   (I.SFALT/I.SFCON frozen per ruling 6; per-symbol liveness
   evidence in the caller tally below.)


#### Untranslated-L2 surface: caller tally (closes the quiet gap)

Every one of the ten symbols was searched for (a) static code
references, (b) appearance as a data/vector word in every memory_data
dump in both images, (c) fall-through reachability from the preceding
instruction. Results:

| Symbol | Addr | References | Verdict |
|---|---|---|---|
| I.WPROLO | 7017E750 | none (code, data, fall-through) | DEAD — wide-prolog variant, never used |
| I.DISPLA | 7017E766 | none | DEAD — display-chain walker, never used |
| I.SFALT | 7017EBC0 | only I.SFCON's XJMP @7017EC77 | frozen (ruling 6, never installed) |
| I.SFCON | 7017EC39 | vector slot 0x700001BB only (LJMP, per Layering refinement); preceding SYSCALL 0310 is process-exit, not fall-through | frozen family, vector-gated |
| R.GOTO | 7017EC7B | none; body = `WBR → I.GOTO+2` alias head | DEAD — the feared silent clone/master asymmetry is moot |
| I.FFALT | 7017ECCC | none; census: emulator FP unit THROWS, can never dispatch | frozen tripwire |
| O.SEARCH | 7017EDDD | sole caller I.FFALT @7017ECDB | transitively frozen |
| O.SIGNAL | 7017EDE7 | none; alt entry (WSSVS + code 0x11601) into O.SET tail @EE38 | DEAD |
| R.SIGREC | 7017EE02 | none; alt entry (arg shuffle + WSSVS) | DEAD |
| R.SIGNAL | 7017EF51 | none; alt entry (WSAVS + WBR → R?SIGNAL body @EF69) | DEAD |

Contract consequence: the six DEAD entries are Deliberate-Exclusions-
Register candidates with the strongest evidence class (zero
references of any kind in either image); the four frozen ones are
already governed by ruling 6 / the tripwire convention. Recommended
belt-and-braces: register log-if-hit tripwire stubs on the six DEAD
entries in the clone, so even a wrong deadness conclusion is loud.

#### L2 outbound returns (uniform protocol, per-wrapper verified)
Every wrapper: fallback path sets `rt_pending_return = ac3` (nested-
in-fallback guards where reachable inside another fallback span:
o_area, p_defon, r_signal, def_on, lib_error's `if pending==0`) and
returns `entry_address(name)`; native path returns
`bridge.native_return()` or `native_return_ss(wsp)` (o_on, o_revert).
Named return-PC constants (lib_error A_RET_XCALL/A_RET_TAREA_H/
A_RET_OSIG; o_signal OSET_XJSR_RET/OSET_CALL_RET/HELPER_XJSR_RET/
DISPATCH_RET; p_defon A_C_INIT_RET/A_OSIG_RET; i_alloc A_UNLOCK_RET)
are L2-internal composite boundaries, not layer crossings.

#### L2→L3 descents
- O.SET walker → I?LINEID (0x7017F730) — **LIVE path** (match
  branch of the chain walk)
- DEF?ON → ?FATAL; R?SIGNAL → ?FATAL (0x7017EF89 vicinity)
- O.SERROR → I.STOP (0x7017EE52)
- I.SFALT family → ?FATAL / syscalls (frozen)
These run on the master (L3 = master-only); the clone's stack-free L2
must reproduce the L1-visible consequences (or ABORT-INTENDED class
per the addendum) without depending on L3 internals.

#### L2→L1 crossings
- I.GOTO landing dispatch (scan 4) — the rendezvous crossing.
- ?LIB_ERROR-mediated: ?DEFAULT_ERROR_HANDLER is ENTERED FROM L1 via
  the B+0x1E pointer (inventoried above as L1→L2).
- Heap: ?LIB_ERROR calls I.FREEW/I.ALLOC — but ?LIB_ERROR is itself
  L1, so this is L1-internal, listed only to close the census.

#### REPORT cross-check
No contradictions. P2 REPORT documents the ?LIB_ERROR→O?SIGNAL
integration boundary and the `rt::signal_has_handler` gate (= scan-1
finding a). P4 documents DEF?ON→P?DEFON resignal as a single-
native_return composite (matches outbound inventory). P1/P3/P5
REPORTs carry no separate crossing claims beyond their DERIVATIONs.

---

### Status after this session

| Scan | Status |
|---|---|
| 1 no-L1-reader | DONE (report §5) |
| 2 area census | DONE — 3 items FLAGGED for review above |
| 3 opaque token | DONE — clean, contract consequence recorded |
| 4 jump edges | DONE — clean |
| 5 boundary inventory | DONE — untranslated-L2 list is the open surface |

Contract inputs now assembled; L2Contract.md drafting can start next
session. Prompt updates from report §1 remain to be applied.


## 7. Symbol-prefix taxonomy (side finding — TO DOCUMENT PERMANENTLY)

**Action for a future session: fold this table into a shared doc
(Layering.md or a new NamingConventions.md) — it is independent
corroboration of the layer census and will save any fresh session the
re-derivation. Shared-doc change, so it stays out of this session's
scope per the no-code/no-shared-doc agreement.**

Derived from quest.symbols + the census + call-graph evidence:

| Prefix | Family | Evidence |
|---|---|---|
| I. | Internal codegen support: frame linkage (I.PROLOG/I.EPILOG/I.WPROLO), heap (I.ALLOC/I.FREE*), boot/task (I.START/I.GINIT), faults (I.SFALT/I.FFALT), non-local GOTO (I.GOTO) | census |
| O. | ON-condition subsystem; maps to PL/1 verbatim: O.ON=`ON`, O.REVERT=`REVERT`, O.S* = signal-specific-condition (SCONVE=CONVERSION, SSUBSC=SUBSCRIPTRANGE, SFIXED=FIXEDOVERFLOW, SZEROD=ZERODIVIDE, SOVERF=OVERFLOW, SUNDER=UNDERFLOW, SERROR=ERROR) | PL/1 condition list, airtight |
| R. | Alternate-linkage heads into signal/unwind (R.SIGNAL, R.SIGREC, R.GOTO): shim registers/frame then branch into shared body (scan-5 tally: all dead in Quest) | bodies read this session |
| X. | Conversions: X.CB = char→binary (game-called; raises O.SCONVE on failure — the CONVERSION-condition signature); X.IC/X.AIC = integer↔char siblings (X.IC called by ?FATAL for message formatting) | call graph |
| C. | Character/string builtins, names verbatim: C.INDEX=`INDEX`, C.TRANS/C.TRANSL=`TRANSLATE`, C.COLLAT = collating table (DATA block at 7017E688, not code). C.ERRNO = C-runtime residue, odd one out | builtin names |
| B. | Block move: B.MOVE | |
| D. / F. | Decimal / floating arithmetic: D.MOD/F.MOD (shared entry, in ?RANDOM_NUMBER's float path). CAVEAT: F.STOP/F.STOPN are L3 near ?FATAL — there "F" may mean "fatal", not "float"; left ambiguous, do not over-document | |
| T. | Tasking: T.INIT, T?AREA | |
| SWAT. / DERR. | DG debugger (SWAT) and its error trapping — L3, master-only | |
| ? | AOS/VS convention for globally visible library/system names (?LIB_ERROR_CODE, ?READ_SCREEN, T?AREA…) — the intended public surface | |
| ?G.* | NOT code: gate/identifier constants (values 0x3, 0x30000000–5) for AOS/VS task primitives (?G.SYSCA, ?G.UKILL, ?G.BKPT ↔ the ?UTSK/?UKIL/?IXMT L3 machinery) | symbol values |

Census corroboration: the ?-vs-dot seam matches the L1/L2 boundary
almost exactly — dot-named entries are runtime-internal, ?-named are
the callable surface. Instruction-mnemonic leading letters (X/L/W/N =
extended/absolute/wide/narrow) are unrelated to symbol prefixes.

## 8. Standing agreements re-confirmed

Plan before code; explicit go-ahead; short replies over long
agreement. This session's full record is THIS SINGLE FILE (rulings, scan 1 in
section 5, scans 2–5 in section 6 including the untranslated-surface
caller tally, tool sources in the appendix). Scan tooling also lives
in docs/Project6/tools/; no emulator, runtime, or shared-doc changes. PROMPT.md update (addendum + A1–A3 + §§3–4
above) is approved in principle but NOT yet applied — apply it in the
tree before or at the start of the Project 6 session.


---

## Appendix — scan tool sources (also in docs/Project6/tools/)

### scan1_no_l1_reader.py

```python
#!/usr/bin/env python3
"""Scan 1 — no-L1-reader scan for L2-private state candidates.

Partitions disassembly lines by layer (census parsed from Layering.md),
then finds every instruction in the L1 partition (game code + L1 runtime)
that could touch a candidate cell, in either encoding:
  - absolute:      [0x7000104C] etc.  (main-task wsb = 0x7000108C)
  - displacement:  +0x7FC0] family    (15-bit-signed negative disp off any base)
Emits every hit with address, symbol, layer, and matched pattern.
Adjudication of base-register contents is manual, downstream.
"""
import re, sys, bisect

WSB = 0x7000108C
# candidate cells: name -> wsb-relative word offset (negative)
CANDIDATES = {
    "chain_head":    -0x40,
    "oset_type":     -0x3E,
    "oset_key2":     -0x3C,
    "oset_code":     -0x3A,
    "walker_res2":   -0x38,
    "walker_res1":   -0x36,
    "osignal_arg4":  -0x2A,
}

def disp_pattern(off):
    # 15-bit signed encoding as it prints: 0x8000 + off (off negative)
    return 0x8000 + off

def parse_census(layering_path):
    rows = []
    for line in open(layering_path):
        m = re.match(r"\|\s*([^|]+?)\s*\|\s*([0-9A-Fa-f]{8})\s*\|\s*(L[0-3]\??)\s*\|", line)
        if m:
            rows.append((m.group(1), int(m.group(2), 16), m.group(3)))
    rows.sort(key=lambda r: r[1])
    return rows

def layer_of(addr, rows, starts):
    i = bisect.bisect_right(starts, addr) - 1
    if i < 0:
        return ("GAME", "L1")  # before first runtime symbol => game code
    name, start, layer = rows[i]
    return (name, layer)

def scan(dis_path, rows, starts, out):
    abs_pats = {n: f"[0X{(WSB + off):08X}]" for n, off in CANDIDATES.items()}
    disp_pats = {n: f"+0X{disp_pattern(off):04X}]" for n, off in CANDIDATES.items()}
    insn_re = re.compile(r"^([0-9a-f]{8})\s+(\S.*)$")
    hits = []
    for line in open(dis_path):
        m = insn_re.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        text = m.group(2)
        up = text.upper()
        for n, p in abs_pats.items():
            if p in up:
                hits.append((addr, text.strip(), n, "ABS"))
        for n, p in disp_pats.items():
            if p in up:
                hits.append((addr, text.strip(), n, "DISP"))
    for addr, text, cell, kind in hits:
        sym, layer = layer_of(addr, rows, starts)
        print(f"{addr:08X} {layer:4} {sym:24} {cell:14} {kind:4} {text}", file=out)
    return hits

def main():
    layering = sys.argv[1]
    dis_files = sys.argv[2:]
    rows = parse_census(layering)
    # census covers the runtime; game code below first runtime addr is L1 GAME
    starts = [r[1] for r in rows]
    print(f"# census symbols: {len(rows)}; wsb=0x{WSB:08X}")
    print("# addr     layer sym                      cell           enc  insn")
    for f in dis_files:
        print(f"# --- {f}")
        scan(f, rows, starts, sys.stdout)

if __name__ == "__main__":
    main()
```

### scan1_full_band.py

```python
#!/usr/bin/env python3
"""Scan 1 v2 — full wsb-band no-L1-reader scan.

Scans ALL L1/L0 code (game + runtime, per the Layering.md census) for any
reference to the runtime's wsb-relative static band, in both encodings:
  - displacement: +0x7FC0..+0x7FDF off any base register (15-bit-signed
    negatives -0x40..-0x21; covers condition cells, SFALT/SFCON/DERR state,
    the area pointer cell, and neighbors)
  - absolute: [0x70001000..0x7000108F] (main-task wsb = 0x7000108C)
Every hit requires manual base-register-provenance adjudication (see
SESSION_REPORT_AUG13.md section 6 for the four adjudicated false positives).
Usage: scan1_full_band.py ../../Layering.md <dis files...>
"""
import re, sys, bisect

def main():
    rows = []
    for line in open(sys.argv[1]):
        m = re.match(r"\|\s*([^|]+?)\s*\|\s*([0-9A-Fa-f]{8})\s*\|\s*(L[0-3]\??)\s*\|", line)
        if m:
            rows.append((m.group(1), int(m.group(2), 16), m.group(3)))
    rows.sort(key=lambda r: r[1])
    starts = [r[1] for r in rows]
    insn = re.compile(r"^([0-9a-f]{8})\s+(\S.*)$")
    disp = re.compile(r"\+0X7F[CD][0-9A-F]\]")
    absw = re.compile(r"\[0X700010[0-8][0-9A-F]\]")
    for f in sys.argv[2:]:
        for line in open(f):
            m = insn.match(line)
            if not m:
                continue
            a = int(m.group(1), 16)
            t = m.group(2).upper()
            if disp.search(t) or absw.search(t):
                i = bisect.bisect_right(starts, a) - 1
                sym, layer = ('GAME', 'L1') if i < 0 else (rows[i][0], rows[i][2])
                if layer.startswith(('L1', 'L0')):
                    print(f"{a:08X} {layer:4} {sym:20} {m.group(2).strip()}")

if __name__ == "__main__":
    main()
```

## Reviewer note (Aug 13, Project 6 correction c2)

Scan-5's R.SIGREC row ("References: none") is incomplete: the boot-
installed restart vector [0x70000124]→0x7017EB63 (word0 = R.SIGREC)
is a dynamic path — see Project4/DERIVATION §5.2 and Exclusions
Register E5, which carries the corrected evidence class and risk
argument. DEAD verdict stands.
