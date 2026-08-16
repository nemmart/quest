# Entry-Point Layering (agreed Aug 2026 — pre-contract definition)

## Why this exists — the big picture

The end goal of the whole project is reconstructed source: readable
C++ that IS the game (Plan.md Steps 3–4). The road there runs through
**M4: de-stackify the game** — Quest's routines are not re-entrant, so
every stack slot can become fixed static storage, and the MV/8000 call
stack can be **blown up entirely**.

What stops us from blowing it up today is that the PL/1 condition
system reads the stack STRUCTURALLY: handler chains threaded through
frames, wsp snapshots, unwind walks. You cannot destroy a stack that
machinery still walks. By the stack-surgery law below, ALL of that
machinery is L2 — so the prerequisite for M4 is precisely:

> **Every piece of L2 state must live in native structures with zero
> dependence on the MV stack.**

The current native L2 does not satisfy this. It is *bit-faithful*: it
still HOSTS its state (condition frames, chain nodes, walker scratch)
on the MV stack, because fidelity demanded it during translation. To
re-host that state in C++ structures, L2 needs permission to differ
from the bytes — **contract-fidelity in place of bit-fidelity, for
this one layer only**. That requires a contract to build against: a
precise statement of which state is L2-private (free to re-host) and
which is L1-observable (must be preserved exactly), plus the full set
of boundary crossings.

This document is what makes that contract writable. The layering
defines the boundary; the census enumerates the crossings; the scans
prove the privacy claims; and the fact that L2 is already 100% native
means its crossings are read from source and every conceptual mistake
becomes a lockstep divergence rather than an argument.

Two facts make this plan sound rather than merely hopeful:

**L3 may freely walk the stack — because it only ever runs on the
master.** "Blow up the MV call stack" means blow it up ON THE CLONE.
The clone detaches at the L2/L3 crossing (TerminalDetach.md) and never
executes one instruction of L3; the master keeps running the
unmodified original code, MV stack intact, forever. So ?FATAL's
traceback, the I?LINEID line-number machinery, the whole frozen
terminal world — all of it walks a stack that genuinely continues to
exist, on the engine where it lives. L3 needs no translation, no
contract, and imposes no constraint on the clone's stack-free future.

**Contract verification is continuous and free — the master IS the
oracle.** The master always runs the unmodified original bytes; the
clone runs the contract-conforming implementation; and lockstep
checkpoints the clone against the master at every rendezvous
(pc + registers) on every play session. If we code carefully — keep
the rendezvous points and every L1-observable output exactly per the
contract — then contract compliance is not something we verify once
and trust; it is re-verified continuously, by construction, every
time anyone plays. A contract violation is not a latent bug; it is a
divergence report with a backtrace, usually within seconds of the
violating path running. This is the same property that carried the
entire M3 lift, extended from bit-fidelity to contract-fidelity.

**On "overkill."** It would be easy to dismiss this design as too
much machinery. What that misses is the guarantee it buys. A
conventional reimplementation of a 40-year-old binary — no source, no
test suite, no original hardware — discovers its mistakes BY SYMPTOM:
a corrupted save, a drifted behavior, a mis-fired handler, months
later, with nothing to diff against. This design keeps the only
ground truth in existence (the emulated original) as a permanent live
participant, so every mistake — a wrong instruction, a wrong pairing
rule, a wrong CONCEPT — surfaces at the moment its path first runs,
as a machine-state diff with a backtrace. The rigor is paid upfront;
the return is a faithful translation with no horrible-to-find latent
bugs, where "done" actually means done.

So the dependency chain, backwards from the goal:

    reconstructed source (Step 4)
      ← live-range split (M5/Step 3)
        ← de-stackify L1, destroy the MV stack (M4/Step 2)
          ← stack-free L2 (contract-conforming implementation)
            ← L2Contract.md
              ← THIS DOCUMENT (+ the ~535w translate list + scans)

Next step: the pre-contract translate list, the verification scans,
then L2Contract.md.

## The four strata (refined in discussion, Aug 11 2026)

- **L0 — kernel/services (unlayered)**: syscalls + pure-service leaves.
  Membership test is BEHAVIORAL, not static: always returns to caller,
  and never actually raises on any input the program can produce. A
  statically-present raise edge does not disqualify if it is proven
  dynamically dead (X.CB: its O.SCONVE edge exists in the bytes but
  both game call sites convert digit constants — Project1 §4d — so
  behaviorally it is a pure service). Callable from ANY layer; never
  transfers, never terminates. L2's uses of L0-style leaves are
  SUBSUMED into the native L2 implementation (already true: the native
  L2 calls no leaf entries).
- **L1 — the resumable world**: game code (incl. ON-unit bodies) and
  every routine whose raising IS its live semantics: ?CHAR_TO_UNSIGNED
  (the M-trigger is its raise), the I/O wrappers (failed open/write
  signaling is handled gameplay), ?LIB_ERROR family,
  ?AWAIT_CONSOLE_INTERRUPT. The startup init cluster is L1 booting the
  resumable world.
- **L2 — handler machinery**: enters from L1 (raise / registration /
  frame ops); exits ONLY by return, transfer-to-L1 (handler dispatch,
  I.GOTO unwind — the footnote-return), or descent to L3.
- **L3 — terminal**: no upward edges; may call L0. Includes terminal
  SYSCALLS (0310 process-exit does not return; classify every syscall
  behaviorally: always-return = L0, fail-by-returning-error = still L0
  since escalation is the caller's L1 act, never-return = L3).

**Edge law**: any→L0 (call/return only); L1→L2 (call/return incl.
footnote-return); descents L1→L3, L2→L3 one-way, layer-skipping legal
(game calls I.STOP directly); within-layer free. FORBIDDEN: any upward
call, and L0 calling anything above itself.

**Stack-surgery law**: non-local stack manipulation (LDSP/STASP/wfp
rewrites beyond a routine's own frame — I.PROLOG's wsp snapshot, the
landing stub's STASP, I.GOTO's cut) is **L2's exclusive privilege**.
L1 pushes/pops only locally; L0 never touches wsp beyond its own frame;
L3 inherits what it is handed and dies — and L3 may walk the stack
freely, because L3 only ever runs on the MASTER, where the MV stack
remains alive and bit-faithful (see "Why this exists"). This single law is most of what
M4 (de-stackification) needs from the model.

## Definitions tightened in discussion

- **Entry** = an address where a layer crossing can occur by ANY
  transfer mechanism: LCALL/XCALL/LJSR/XJSR, indirect variants (the
  area-handler XCALL [ac2+0]), plain jumps (XJMP/LJMP/WBR/WPOPJ
  landing cross-layer), fall-through across a symbol boundary, patched
  return addresses (I.GOTO's landing stub — a jump wearing a WRTN
  costume), trap vectors, native_transfer, syscall entry. Every
  crossing has a CONTROL component (where pc goes) and a STACK
  component (what happens to wsp/wfp).
- **Entry vs interior label**: fall-through and shared tails mean some
  symbols are doors into one multi-entry body (the O?SIGNAL..O.SET
  shorthand cluster is ONE L2 body with nine doors). Classification
  covers CROSSING POINTS; interior structure is explicitly
  unclassified implementation — exactly the freedom the L2 contract
  claims.
- **Lazy caller-closure classification**: anything called only from L1
  is L1, no analysis needed. Default everything to L1 top-down;
  litigate only symbols with multi-layer callers or symbols someone
  wants promoted to L0. Parking a could-be-L0 symbol in L1 costs
  nothing (L1 is most permissive); the only harmful misclassification
  is a raiser in L0. Promotion to L0 requires the behavioral test.
- **L2/L3 asymmetric error cost**: misclassify L3-ward and you freeze
  something the contract needs (structural failure); misclassify
  L2-ward and you waste a translation (bounded). TIEBREAK: when in
  doubt, it is L2 — translate it.

## Analysis method (three tiers)

1. **L2 crossings: from NATIVE SOURCE** (authoritative). Every
   outbound L2 crossing is a typed C++ construct — native_transfer
   (transfer), native_return/_ss (return), entry_address returns
   (fallback). Enumerating L2's edges = reviewing ~15 wrapper return
   expressions, not branch-target archaeology. Lockstep continuously
   certifies the native code against the bytes, so this inherits the
   empirical guarantee. Enforcement idea: a debug assert at
   native_transfer/native_return consulting the layer map — the edge
   law as tripwire, not documentation.
2. **L1→L2 entries: registration table + caller census** (below).
3. **L3: frozen, unanalyzed by policy.** Remaining disassembly work is
   only: does any L1/L2 EMULATED code BRANCH (not call) across a layer
   boundary — a small jump-edge pass.

## Corner rulings

1. Multi-layer-called utilities → L0 (the syscall generalization):
   I.LOCK/I.UNLOCK (L1 heap + L3 ?FATAL), C?INIT (init + P?DEFON +
   LANG?STOP). Body scans pending.
2. Descents may skip layers.
3. Init cluster = L1; C?INIT carved to L0.
4. Name families straddle layers (C.*, X.*, SWAT.*): per-symbol
   assignment by call evidence only. The famous underflow bug was in
   L3's formatter (C?TRIM).
5. **Terminal syscalls**: game code issues SYSCALL 0310 directly at
   0x7017700F — an L1→L3 descent bypassing I.STOP, prime suspect for
   the residual "Forced exit" shutdown hang (no terminal entry crossed
   → no detach). TASK: make terminal syscalls detach points.
6. **Fault vectors — ruling revised TWICE by live evidence (keep this
   history; it is the method working)**. FP faults: the emulator
   THROWS (EagleFloat.cpp) — I.FFALT can never run → frozen, tripwire
   at the throw site. Stack faults: first ruled frozen-by-symmetry
   (wrong — the emulator vectors faithfully); then ruled "ANY STACK
   LIMIT KILLS" (tried Aug 11, VOID within one run: **the program
   deliberately overflows its stack at startup** — wsp==wsl, every
   process, every launch, a stack-sizing protocol whose fault→handler→
   relaunch choreography is the "Unregistering process / reopen"
   sequence in every session log). Final ruling: `handle_overflow`
   vectors FAITHFULLY and must keep doing so; the per-task handlers
   observed are loader/game-side (L1); **I.SFALT has never been
   observed installed or executed** — it stays frozen-unimplemented,
   and the question reopens only if it ever appears as an installed
   handler (tripwire comment at handle_overflow says exactly this).
   **FINAL REFINEMENT (Aug 13, proven from the binaries)**: both .PR
   preambles ship wsp==wsl (QUEST 70000E87, SERVER 7000082E — the
   exact observed boot-fault values; sfh word12 = 0x01B8, NOT
   I.SFALT's 0x01C0 slot). So `wsp==wsl at fault ⟺ the image's
   deliberate zero-headroom boot probe`, a provable signature.
   RULING: wsp==wsl → vector faithfully (load-bearing, verified);
   ANY other stack fault → C++ throw (post-detach: death during
   death dies now, never re-enters the signal machinery — this also
   closes the L3→L2 trap-vector hole in the edge law; symmetric:
   both engines throw identically, agreed death; master-in-native-
   span: upgraded to forced shutdown when that machinery lands).
   No counters or thresholds — the binary signs its one legal fault.
   **The full boot mechanism (Aug 13, from the live memory capture)**:
   the image's sfh (preamble word 12) = 0x01B8; startup code installs
   `LJMP → I.INIT` at 0x700001B8 and `LJMP → I.SFCON` at 0x700001BB —
   so the boot fault VECTORS INTO THE RUNTIME INITIALIZER ("fault
   your way into init": I.INIT builds heap + real stack, resumes),
   while the adjacent slot holds the signal-side handler. Per-task
   sfh is an immutable creation-time snapshot; secondary tasks name
   theirs via the ?CREATE_TASK packet's ?DSFLT field. HONEST
   REFINEMENT: I.SFCON is therefore wired live-capable in slot 0x1BB
   (and I.INIT may chain to it when already initialized — unobserved);
   the wsp==wsl-else-throw ruling DELIBERATELY replaces that
   signal-flavored path — fix-if-it-fires, self-announcing, same
   family as DERR/QSEARCH.
   IMPLEMENTATION: the wsp==wsl gate LANDED Aug 13 (C++ throw in
   handle_overflow, regression-clean); only the abort_world upgrade
   remains bundled with the forced-shutdown task.
7. **DERR — FINAL RULING (Aug 13, third revision; history kept)**:
   DERR.TRP raises ERROR via O.SERROR ×2, so 1988's catch-all
   handlers CAN "recover" a DERR — but a DERR means a subscript
   overran an array: memory beyond it is already stomped, and
   recovery proceeds into a silently corrupted world that would
   poison save files and the ground truth this project depends on.
   Faithfully reproducing silent corruption is faithfully reproducing
   a bug. RULING — the project's THIRD DELIBERATE INFIDELITY (after
   QSEARCH and non-boot stack faults), chosen explicitly:
   **DERR.TRP → terminal-with-ABORT, symmetric-verified.** Both
   engines vector to DERR.TRP; the terminal machinery takes one
   final verified pair there — proving BOTH engines agree the game's
   own latent bug fired, at which pc, with what state — then the
   world hard-stops with pc + DERR code named (no detach-and-play-on:
   the master's world is corrupt too). A MASTER-ONLY DERR inside a
   native span stays a divergence report: that is OUR translation
   dropping an impossible-state check, our bug, correctly fingered.
   Implementation: terminal_table gains a per-entry kind
   (DETACH | ABORT); DERR.TRP registers as ABORT; the abort tail is
   shared with abort_world (data write-back deliberately EXCLUDED
   for DERR — the state is presumed corrupt; print-and-stop only).
   Cold (never fired); revision history above is the method working.

## The pre-contract translate list (~535 words)

(~348 words after ruling 6's final revision removed I.SFALT/I.SFCON.)
Live-capable L2 still emulated — per the asymmetric-cost tiebreak,
these must go native BEFORE the L2 contract is written:

| Cluster | Words | Note |
|---|---|---|
| DEF?ON | 76 | resignal machinery is resumption plumbing; every observed run died, but "observed" = two trigger shapes |
| P?DEFON, O?AREA | 46 | satellites |
| R?SIGNAL / ?ERROR | 226 | the ?ERROR alias smells like a public raise entry |

Validation hook: translating the DEF?ON cluster moves the detach point
DEEPER — from DEF?ON's entry to the true L3 crossings (?FATAL entry,
terminal syscalls). If native DEF?ON ever RESUMES instead of
descending, lockstep verifies it instead of a detached master silently
revealing it. Honest cost: these are the coldest paths in the system —
validating them needs a fault-injection extension, which is the SAME
work the parked M3 provocation criterion was waiting on. The two
parked questions are one question.

8. **Stack-claim zeroing — the FOURTH deliberate infidelity
   (user-ratified, Aug 14)**: the emulated machine ZEROES stack space
   at claim — WSAVS/WSAVR/WSSVS/WSSVR frame areas and positive-delta
   WMSP claims — on both engines, in the shared instruction path
   (symmetric by construction). Real hardware only reserved (DG
   Instruction Dictionary, archived screenshots Aug 14 — "reserving
   space", a period cost decision, not a semantic commitment).
   Rationale: (a) correct code cannot depend on unclaimed-space
   contents; (b) per-site comparison accommodation is unbounded
   (stale values survive syscalls — value-lifetime chasing); (c) the
   DECISIVE argument — M4: the master forever reads its stale stack,
   while a de-stackified clone's read-before-write locals see
   different deterministic garbage (previous-invocation values at
   fixed addresses); NO write-through can reconcile that. Zeroing +
   M4 zero-initializing prologues make both sides read 0 for the
   whole class, permanently. Residual risk (load-bearing garbage =
   behavior change invisible to symmetric lockstep) is probed
   empirically by the user's asymmetric design: master unzeroed vs
   clone zeroed under a `-probe` flag — register-value compare
   relaxed, pc + instruction counts + syscall mediation ARMED — so
   the harness itself is the laboratory and free play is the
   controlled experiment (mediation guarantees identical stimulus).
   Consequential garbage-dependence surfaces as a located pc fork /
   count skew / mediation mismatch; dead cargo passes. Ship
   both-zeroed with full checking afterward.
   Resolves Project 8 blocker B1 (B2 = Project 9's Generation-3
   amendment). IMPLEMENTATION: after Project 9 lands; tripwire
   comment at the WSAVS site when it does.

## Open questions (deliberately undecided)

a. **Defensive-raise → abort: CLOSED (Aug 2026) — subsumed by
   ruling 7.** A defensive raise (heap corruption → O.SERROR, I.GOTO
   bad-chain → O.SERROR, ?CREATE_TASK failure) firing VERIFIED ON
   BOTH ENGINES means the game corrupted the runtime's own state —
   the same species as DERR — so it is ABORT-class by the ratified
   DERR rationale; no independent judgment remained to make. (A
   translation bug is caught earlier and differently: it fires
   one-sided and surfaces as an ordinary divergence — lockstep's
   jurisdiction, not this ruling's.) NO implementation now: every
   site is cold, and the original L0-promotion payoff evaporated
   under lazy closure. Standing instruction for Project 6 and the
   M3b/M4 implementers: mark these branches ABORT-INTENDED in the
   contract rather than specifying a recovery path already
   condemned; wire them to abort_world(save=false) whenever the
   surrounding code is next touched.
b. **M3 provocation exit criterion** — now merged with the DEF?ON
   validation question (above).
c. The L0? census rows resolve LAZILY on demand, not wholesale.

## Expected lifecycle

This document will be WRONG somewhere — the method of this project is
that clarity comes from working through things (HeapSignalPlan was the
document we had to write to discover the right document). Write the
minimal version, let the scans and the first stack-free L2 attempt
correct it, and let lockstep turn conceptual mistakes into divergences
instead of arguments.

## Census (per-symbol, with caller evidence)

| Symbol | Addr | Layer | Callers (game/RT) |
|---|---|---|---|
| ?CHAR_TO_UNSIGNED | 7017D99B | L1 | GAME |
| ?UNSIGNED_TO_CHAR | 7017DA75 | L1 | GAME |
| ?UMUL32 | 7017DB1B | L0? | ?CHAR_TO_UNSIGNED |
| ?UDIV32 | 7017DB3E | L0? | ?UNSIGNED_TO_CHAR |
| ?AWAIT_CONSOLE_INTERRUPT | 7017DB4B | L1 | GAME |
| ?CLOSE_FILE | 7017DB63 | L1 | GAME |
| ?CONNECT | 7017DB9B | L1 | GAME |
| ?CREATE_TASK | 7017DBB3 | L1 | GAME |
| MT?LOCK | 7017DC3F | L0? |  |
| MT?UNLOCK | 7017DC4F | L0? |  |
| ?DELAY | 7017DC63 | L1 | GAME |
| ?GET_SHARED_PAGE | 7017DC7D | L1 | GAME |
| ?LOOKUP_PORT | 7017DCCB | L1 | GAME |
| ?OPEN_FILE | 7017DD27 | L1 | GAME |
| ?OPEN_SHARED_IO_FILE | 7017DDBB | L1 | GAME |
| ?LIB_ERROR_CODE | 7017DE25 | L1 | GAME |
| ?RANDOM_NUMBER | 7017DE33 | L1 | GAME |
| ?READ | 7017DE5F | L1 | GAME |
| ?READ_SCREEN | 7017DEEB | L1 | GAME |
| ?RECEIVE_TASK_MESSAGE | 7017E095 | L1 | MT?LOCK |
| ?SEND_TASK_MESSAGE | 7017E0BB | L1 | MT?UNLOCK |
| ?CURRENT_PID | 7017E12B | L1 | GAME |
| MT?SUS | 7017E14D | L1 |  |
| MT?IDSUS | 7017E154 | L1 |  |
| MT?IDKIL | 7017E15E | L1 |  |
| MT?PRI | 7017E168 | L1 |  |
| MT?IDPRI | 7017E171 | L1 |  |
| MT?IDRDY | 7017E17F | L1 |  |
| MT?ERSCH | 7017E189 | L1 |  |
| MT?DRSCH | 7017E190 | L1 |  |
| MT?XMT | 7017E197 | L1 | ?SEND_TASK_MESSAGE |
| MT?XMTW | 7017E1A5 | L1 | ?SEND_TASK_MESSAGE |
| MT?REC | 7017E1B5 | L1 | ?RECEIVE_TASK_MESSAGE |
| MT?RECNW | 7017E1C0 | L1 | ?RECEIVE_TASK_MESSAGE |
| MT?IDGO2 | 7017E1CB | L1 |  |
| MT?TASK | 7017E1DC | L1 | ?CREATE_TASK |
| ?WRITE | 7017E1FC | L1 | GAME |
| ?WRITE_SCREEN | 7017E27A | L1 | GAME |
| ?FILL_WORDS | 7017E31C | L0? | ?CLOSE_FILE,?GET_SHARED_PAGE,?READ,?READ_SCREEN... |
| ?LIB_ERROR | 7017E33A | L1 | ?AWAIT_CONSOLE_INTERRUPT,?CHAR_TO_UNSIGNED,?CLOSE_FILE,?CONNECT... |
| ?DEFAULT_ERROR_HANDLER | 7017E3D2 | L2 |  |
| SWAT.NIN | 7017E3F0 | L1 | LANG?INIT |
| SWAT.REX | 7017E4B8 | L3 | ?FATAL |
| B.MOVE | 7017E5CB | L0? | GAME |
| C.INDEX | 7017E5F4 | L0? | ?CHAR_TO_UNSIGNED |
| C.TRANS | 7017E64A | L0? | GAME |
| C.COLLAT | 7017E688 | L0? |  |
| X.CB | 7017E708 | L1 | GAME |
| D.MOD | 7017E722 | L0? | ?RANDOM_NUMBER |
| I.PROLOG | 7017E733 | L2 | GAME |
| I.WPROLO | 7017E750 | L2 |  |
| I.DISPLA | 7017E766 | L2 |  |
| I.EPILOG | 7017E77D | L2 | GAME |
| T?INITN | 7017E784 | L1 |  |
| T.INIT | 7017E786 | L1 |  |
| T?KILL | 7017E7C4 | L1 |  |
| I.LOCK | 7017E7D0 | L0 | ?FATAL,I.FREE |
| I.UNLOCK | 7017E7ED | L0 | ?FATAL,I.FREE |
| I?HPOWNER | 7017E805 | L1 |  |
| I?ASIZE | 7017E811 | L1 |  |
| I?INHPB | 7017E81C | L1 |  |
| I?INHPW | 7017E820 | L1 | ?UKIL |
| I?INHPBS | 7017E834 | L1 |  |
| I?INHPWS | 7017E838 | L1 |  |
| I?SALLOC | 7017E853 | L1 |  |
| I.ALLOC | 7017E866 | L1 | ?CREATE_TASK,?LIB_ERROR |
| I.FREEB | 7017E945 | L1 |  |
| I.FREEW | 7017E949 | L1 | ?CREATE_TASK,?LIB_ERROR |
| I.FREE | 7017E94C | L1 |  |
| I.TOFREE | 7017EA1E | L1 |  |
| I.INIT | 7017EA2E | L1 |  |
| ?UKIL | 7017EA5E | L3 |  |
| ?STACK_OVERHEAD | 7017EA78 | L1 |  |
| I.GINIT | 7017EA7A | L1 | I.INIT |
| I.NOTICE | 7017EB67 | L1 |  |
| I.FPU | 7017EBA8 | L1 | T.INIT |
| I.SFALT | 7017EBC0 | L2 | (frozen — never installed; ruling 6) |
| I.SFCON | 7017EC39 | L2 | (frozen with I.SFALT; ruling 6) |
| R.GOTO | 7017EC7B | L2 |  |
| I.GOTO | 7017EC7C | L2 | GAME |
| I.FFALT | 7017ECCC | L2 |  |
| DERR.TRP | 7017ED1C | L3 |  |
| T?AREA | 7017ED93 | L2 | ?DEFAULT_ERROR_HANDLER,?LIB_ERROR,?LIB_ERROR_CODE |
| O.ON | 7017ED9B | L2 | GAME |
| O.REVERT | 7017EDCB | L2 | GAME |
| O.SEARCH | 7017EDDD | L2 | I.FFALT |
| O.SIGNAL | 7017EDE7 | L2 |  |
| O?SIGNAL | 7017EDED | L2 | ?DEFAULT_ERROR_HANDLER,DEF?ON,P?DEFON |
| R.SIGREC | 7017EE02 | L2 |  |
| O.SUNDER | 7017EE07 | L2 | I.FFALT |
| O.SOVERF | 7017EE0F | L2 | I.FFALT |
| O.SZEROD | 7017EE17 | L2 | I.FFALT |
| O.SFIXED | 7017EE1F | L2 | I.FFALT |
| O.SSUBSC | 7017EE27 | L2 |  |
| O.SCONVE | 7017EE2D | L2 | X.CB |
| O.SERROR | 7017EE33 | L2 | DERR.TRP,DERR.USR,I.ALLOC,I.FREE... |
| O.SET | 7017EE56 | L2 | I.SFALT |
| DEF?ON | 7017EF05 | L2 |  |
| R.SIGNAL | 7017EF51 | L2 |  |
| R?SIGNAL | 7017EF54 | L2 | DEF?ON |
| ?SNAP | 7017F030 | L3 |  |
| ?FATAL | 7017F036 | L3 | DEF?ON |
| I?PCS | 7017F6B4 | L3 |  |
| I?LINEID | 7017F730 | L3 | ?FATAL,O.SET |
| ?FIND_SCOPE | 7017F774 | L3 | I?LINEID,I?PCS |
| ?FIND_LINEID_INDEX | 7017F87B | L3 | I?LINEID,I?PCS |
| ?GET_LINEID_ENTRY | 7017FA31 | L3 | I?LINEID,I?PCS |
| P?SNAP | 7017FABC | L3 | ?FATAL |
| C?TRIM | 7017FB81 | L0? | ?FATAL |
| I?LINE | 7017FBAE | L3 | ?FATAL |
| ?WRITE_ERROR_CHANNEL | 7017FBE3 | L3 | ?FATAL |
| O?AREA | 7017FC39 | L2 | ?FATAL,DEF?ON |
| P?IPKT | 7017FC41 | L3 | ?WRITE_ERROR_CHANNEL |
| X.AIC | 7017FC4D | L0? |  |
| X.IC | 7017FC50 | L0? | ?FATAL |
| F.STOP | 7017FCAB | L3 |  |
| F.STOPN | 7017FCCD | L3 |  |
| I.STOPM | 7017FCD8 | L3 |  |
| I.STOP | 7017FCE8 | L3 | GAME,O.SERROR |
| LANG?STOP | 7017FCF3 | L3 | ?FATAL,I.STOP |
| LANG?INIT | 7017FD16 | L1 |  |
| LANG?FLSH | 7017FD3D | L3 | ?FATAL |
| ?SCOPE_INIT | 7017FD61 | L3 | ?FIND_SCOPE |
| P?DEFON | 7017FD7A | L2 | DEF?ON |
| I.START | 7017FDA0 | L1 |  |
| C?INIT | 7017FDB9 | L0 | LANG?FLSH,LANG?INIT,LANG?STOP,P?DEFON |
| R.INERR | 7017FDBF | DATA |  |
| C.ERRNO | 7017FDC0 | DATA |  |
| DERR.USR | 7017FDC2 | L3 |  |
| ?URTB | 7017FDCE | DATA |  |
| .BOMB | 7017FDD0 | L3 |  |
| .KILL | 7017FDD3 | L3 |  |
| .UTSK | 7017FDD9 | L3 |  |
| .UKIL | 7017FDE5 | L3 |  |
| ?BOMB | 7017FE05 | L3 |  |
| ?UTSK | 7017FE26 | L3 |  |

`L0?` = L0 pending the leaf-purity scan; falls to L1 (or L3 if its only
callers are L3) on any failure. DATA = data symbols, unlayered.
