# NativeDesign.md — Phase-1 Data Structures for the Stack-Free L2

Project 6, Phase 1 deliverable 3. Status: DESIGN FOR REVIEW — NO
implementation in this phase.

This is the INTERNAL view of the staged M3b implementation (Plan.md):
the MV stack keeps frames/args/locals; **handler state is fully
native**; NOTHING walks the MV stack to find a handler; I.GOTO still
cuts the real stack (until M4). Constrained by L2Contract.md; nothing
here may be cited to justify a contract clause.

## 1. The native chain

Two-level structure mirroring the abstract state (Contract §2.1):
establisher records each carrying a node list.

```cpp
struct HandlerNode {          // one O.ON registration
  int32_t  type;              // node[+2] semantics: 0 == inactive/reusable
  int32_t  key2;              // node[+4]
  uint32_t handler;           // node[+6], game pc
};

struct EstablisherRecord {    // one I.PROLOG bracket
  uint32_t frame;             // the establishing routine's MV frame
                              //   pointer — record identity, the
                              //   dispatch token, AND I.GOTO's cut
                              //   level (the stack is still real in
                              //   M3b)
  uint32_t wsp_snapshot;      // [frame+2]'s value: entry wsp + 4 —
                              //   I.GOTO's landing restore point
  int32_t  slot4, slot6;      // I.PROLOG inline words (always 0 in
                              //   Quest; kept because the O.SET
                              //   walker's live semantics read them)
  std::vector<HandlerNode> nodes;   // registration order; search =
                              //   first key match, backstop = LAST
                              //   inactive node (chain order == the
                              //   on-stack node order: O.ON pushes at
                              //   the head, so nodes here are stored
                              //   head-first)
};

struct TaskL2State {          // per task, keyed by wsb (see §8)
  std::vector<EstablisherRecord> chain;   // stack; back() = innermost
  int32_t sig_type, sig_key2, sig_code;   // re-hosted [C+2/4/6]
  int32_t resume_flag;                    // re-hosted [wsb-0x2A] wide
  // walker outputs deliberately NOT hosted (Register E7)
};
```

Push/pop discipline mapped to the contract entries:

| Entry | Chain operation |
|---|---|
| I.PROLOG | push {frame=wfp, wsp_snapshot=entry_wsp+4, slot4, slot6, nodes={}} ; perform the normative +4 wsp reservation on the real stack (contents unwritten — Register E9) |
| I.EPILOG | pop back() (no identity check — Contract §3.2's cold edge: if wfp != back().frame, replicate the bit-faithful head-write semantics by popping to the record matching [wfp+8]'s abstract equivalent; in practice assert-and-abort in debug, since PL/1 bracketing guarantees pairing — flagged §10) |
| O.ON | find record with .frame == caller wfp (it is back() in every observed shape, but the contract only requires "the caller's record"); search nodes; reuse (first key match, else last inactive) or append; perform +8 wsp reservation on allocate (E9) |
| O.REVERT | if back().frame == caller wfp: search; found -> node.type = 0 (deactivate in place, node retained for reuse) |
| I.GOTO | cut: pop every record with .frame strictly above the target (walk order = the record stack order); a record AT the target survives. The MV-stack cut itself (walk, patches, WRTN, snapshot restore) is unchanged M3b behavior — only the HEAD/chain bookkeeping moves here |
| select (raise) | iterate records back()..front(), per record the node search with the catch-all preamble (key2=0 when type<=0) |

The chain is the ONLY authority: rt::signal_has_handler, the select
loop, and DEF?ON's would-run predicate all read it; no code reads
[wsb−0x40] or walks frames for handler state. The real cells
[wsb−0x40] band stay physically present (address-as-landmark) and are
simply never written by the clone (Register E10); the master's are
authentic by construction.

## 2. Token representation

**Choice: the establisher's real MV frame address** (record.frame),
i.e. the same 32-bit value the bit-faithful code passes — NOT an
abstract id. Rationale, in order:

1. Scan 3 licenses abstraction, but I.GOTO's ac0 is not exclusively
   the round-tripped token: two game non-handler sites pass frame
   values captured from their OWN frames (label variables). In M3b
   the stack is real, so frame addresses are the one currency both
   populations share; an id scheme would need an address side-table
   anyway to serve the non-handler sites.
2. The full register-file compare makes the dispatch ac1 NORMATIVE
   (Contract §3.9 exit A): the clone must present the same ac1 the
   master does — which IS the frame address. An abstract id would
   diverge at the dispatch rendezvous. (This is the contract
   constraining the design, as intended: in M3b the token's VALUE is
   pinned; the freedom scan 3 bought is that nothing may DEREFERENCE
   it, which is what lets M4 later re-mint it as a static-area id
   when frames stop existing. Record that as the M4 revisit.)

Mint: dispatch loads sel.record->frame into ac1. Validate: I.GOTO
resolves ac0 against the machine's descending wfp chain exactly as
today (the pre-walk); the native chain is then cut to match (§1).
Round-trip: dispatch → handler body (held, never dereferenced —
scan 3 clean verdict) → I.GOTO ac0. **Bad-token behavior**: the
pre-walk's non-positive/bad-chain outcome is I.GOTO's bad-chain
defensive branch = **ABORT-INTENDED** (Contract §3.3 shape 3a):
abort_world(save=false) with the token and pc named. The
non-descending (ECA2/restart) shape stays bit-faithful-emulate
pending contract Q1.

## 3. C-record hosting

Per SESSION_REPORT census, where each contract-private cell lives:

| Cell | Native home | Clone writes real cell? |
|---|---|---|
| [wsb−0x40] chain head | TaskL2State.chain (structure itself) | NO (landmark address only; I.GINIT's arithmetic use needs no value) |
| [wsb−0x3E/−0x3C/−0x3A] type/key2/code | sig_type/sig_key2/sig_code | NO |
| [wsb−0x36/−0x38] walker outputs | NOT HOSTED (Register E7) — the live walker's RESULT computation still runs (it feeds nothing today, but the gate re-check and the read semantics remain; computing and discarding keeps the DEAD-GUARDED clause honest at zero cost) | NO |
| [wsb−0x2A] resume flag | resume_flag | NO |
| [wsb−0x34/−0x32/−0x30/−0x2C] | frozen family state — untouched (ruling 6; no native code reads or writes) | NO |
| wsb−0x29 area ptr cell | not L2 state (T?AREA computes wsb−0x29 arithmetically; the cell itself is P?SNAP's, L3) | NO |

Goal met: **the clone writes nothing to the static band.** The
master's band stays authentic because the master emulates the
original bytes — L3 observers (?FATAL, P?SNAP, I?LINE) run
master-only against real cells by construction (ratified
PRIVATE-with-L3-observer).

T?AREA/O?AREA keep exporting the real ADDRESSES (wsb−0x29/−0x40):
their L1/L3 consumers do pointer arithmetic against real memory (the
B record is real; §4), and the addresses are landmarks.

## 4. The B-record / area protocol

Stays in real memory — it is SHARED-PROTOCOL with L1 (?LIB_ERROR /
?LIB_ERROR_CODE are L1 and keep their real reads/writes). L2's only
accessor is designed as:

```cpp
namespace rt {
  inline uint32_t b_record(hw::Machine& m)      { return t_area(m) + 8; }
  inline int32_t  b_last_code(hw::Machine& m)   { return rd_wide(m, b_record(m) + 1); }  // ?DEFAULT_ERROR_HANDLER's sole B read
}
```

No L2 writer exists and none is designed. The [B+0x1E] install
invariant remains L1's; the stack-free ?DEFAULT_ERROR_HANDLER keeps
the defensive check (value ∈ {0, 0x7017E3D2}) as a debug assert, not
a gate (it is unreachable through the sole dispatch site when the
invariant holds).

## 5. rt::signal_has_handler (strong)

Specified over the native chain — an exported L2 predicate, never an
L1 raw read (scan-1 finding a; contract §2.4):

```cpp
bool rt::signal_has_handler(hw::Machine& m, int32_t code) {
  // catch-all raise prediction: type=-1, key2=0 (the ?LIB_ERROR shape)
  (void)code;
  if(rt::walker_gate_open(m)) return false;        // DEAD-GUARDED honesty
  TaskL2State& s = l2_state(m);                     // keyed by wsb, §8
  for(auto it = s.chain.rbegin(); it != s.chain.rend(); ++it)
    for(const auto& n : it->nodes)
      if(n.type == -1 && n.key2 == 0) return true;
  return false;
}
```

(The node scan replicates chain_search's first-match ordering per
record; the backstop is irrelevant to has-handler — inactive nodes
never match a −1 key.)

## 6. The A/B validation plan sketch (Phase 2 inherits)

Configuration: master = original bytes (emulated, run-to-return
absorption); clone = the design-conforming L2 of this document;
lockstep at every crossing (Contract §7).

| Trigger / QUEST_INJECT shape | Structures exercised |
|---|---|
| Login (scripted, no injection) | I.PROLOG/I.EPILOG push-pop cadence, O.ON reuse+allocate, O.REVERT deactivate, wsp reservations (the ×10/session bracket traffic) |
| M + dir + abc (CONVERSION) | shorthand raise (O.SCONVE via X.CB), stale resume flag, select over the native chain, token mint→dispatch→I.GOTO cut, snapshot restore |
| QUEST_FAIL_OPEN + L→P | ?LIB_ERROR → B protocol → signal_has_handler gate → indirect entry §3.16 → dispatch; then the unhandled second signal: exhaustion → DEF?ON → ?FATAL terminal pair (crossings-only accounting's hardest case) |
| INJECT pc:-1:code (shape 1, in-scope) | full handled chain at an arbitrary pc; token round-trip |
| INJECT pc:-1:code (shape 2, out-of-scope) | exhaustion cascade, terminal pairing |
| INJECT pc:-1:code:RESUME (shape 3) | native resume_flag hosting, DEF?ON resume path, DISPATCH_RET return crossing |
| store "ABC" | REVERT-then-re-ON reuse at the same record (node recycling) |
| (test hook, new) INJECT bad-token | I.GOTO bad-chain **ABORT-INTENDED third result class**: clone aborts via abort_world(save=false), `aborting` silences the checker, one banner — the A3 composition, exercised deliberately once |

Cold-by-construction paths (P?DEFON positive-type composite, type==2
C?INIT resume, [-5..-2] resignal) remain predicate-guarded and
self-announcing, per the Project 5 parking decision — unchanged.

## 7. Harness accounting — the crossings-only change (IMPLEMENTED Aug 13 2026)

> **STATUS NOTE (Aug 13 2026).** Landed ahead of Phase 2, against the
> bit-faithful L2, in a direct session — see docs/CrossingsChecker.md.
> The as-built mechanism DIFFERS from the sketch below: there is no
> l2_depth counter and no flag. Instead: (1) a layer map
> (RTStubs::l2_bits) keys the break rules; (2) translated-L2 dispatch
> is DEFERRED (Machine::pending_native) so both engines pair AT the
> entry pc before the native code runs; (3) untranslated L2 entries
> arm rt_pending_return on BOTH roles, making the emulated subtree one
> absorbed span (the existing pending machinery provides the depth
> semantics, as this section's own note hoped); (4) DISPATCH_RET/E3EF
> arrivals at depth 0 are explicit rendezvous. Items 1–3 of the
> worklist below are therefore DONE; item 2's DISPATCH_RET re-entry
> rendezvous exists and its NATIVE tail handling remains Phase 2 (H6);
> item 4 (capture retirement) is a Phase-2 conformance statement;
> item 5 (per-crossing rtcalls) was NOT needed — the existing lines
> remained readable. The sketch is kept below for the record.

Mechanisms that must change when the stack-free L2 lands (Phase 2
worklist; definitions from Contract §7):

1. **Batch break sites** — Machine::run_steps' rt_sync entry blocks
   currently break at every entry in entry_bits. Needed: break only
   on L1→L2 transitions. Sketch: a per-machine `l2_depth` (0 = in
   L1), incremented at a crossing entry, decremented at the matching
   exit (native_return/transfer to L1 / WRTN to an L1 pc); an entry
   reached with l2_depth > 0 is interior — no break, no rendezvous.
   Note T?AREA's dual role (crossing from ?LIB_ERROR, interior from
   ?DEFAULT_ERROR_HANDLER): the depth flag, not the address, decides.
2. **Master run-to-return** — must absorb the WHOLE L2 subtree from
   the L1 entry to the L1 continuation (it already absorbs nested
   native calls in spans; the change is that interior entries stop
   re-arming/breaking). The DISPATCH_RET return crossing needs an
   explicit re-entry point: the handler's WRTN to 0x7017EE40 re-opens
   an L2 span (a NEW rendezvous), which the current
   pc3==DISPATCH_RET continuation logic (o_signal.cpp) already
   prefigures.
3. **Span/ordinal accounting in compare_pair** — count-exemption and
   native_span flags must be produced per-crossing, not per-entry;
   terminal pairs (?FATAL descent) keep the equal-count requirement,
   which is exactly why interior entries must stop skewing counts.
4. **QUEST_CAPTURE retirement** — footprint captures of
   contract-private memory retire (Register E7–E10); the capture tool
   remains for SHARED-PROTOCOL windows (B record) during bring-up.
5. **rtcalls logging** — per-crossing lines (entry+exit) instead of
   per-wrapper, or the traces become unreadable across the two
   accounting regimes.

## 8. Per-task lifecycle

- **Keying**: TaskL2State lives in a per-process map keyed by wsb
  (the task's static base uniquely identifies the task; every L2
  entry can derive it from machine.wsb at dispatch). Created lazily
  on first touch (first I.PROLOG / first raise), which naturally
  handles:
- **Creation / T.INIT epoch**: T.INIT (L1) builds the new task's
  static block (including the init-epoch write at [new_wsb−0x2E],
  scan-1 hit 5 — outside L2's set) and zero static area ⇒ a fresh
  task's abstract chain is EMPTY, matching the zero-filled
  [wsb−0x40]. Lazy creation yields exactly that. sig_* / resume_flag
  initialize to 0 (the zero-fill semantics — normative: a raise
  before any O?SIGNAL reads flag 0, i.e. non-resumable, which is
  what the real zeroed cell gives).
- **The boot fault window**: the deliberate wsp==wsl probe fires
  BEFORE I.GINIT installs anything and vectors into I.INIT (Layering
  ruling 6's mechanism) — no signal machinery, no L2 state involved;
  lazy creation means no ordering hazard. I.GINIT's
  address-as-landmark use of &[wsb−0x40] is arithmetic only and runs
  identically on both engines against real memory.
- **Teardown**: ?UKIL / task retire are L3, master-only. On the
  clone, records die with the process object (detach halts tasks; no
  destructor-order dependency because nothing else references the
  map). Multi-task note: QUEST runs the client main task + the
  C_A_LISTENER; only the main task raises today, but the map design
  is per-task from day one because T.INIT provably creates tasks
  with their own wsb.

## 9. What Phase 2 builds, in one list

1. TaskL2State + the six entry mappings (§1) behind the existing
   wrapper surface (the emu_rt functions keep their signatures; their
   bodies swap stack-hosted state for chain operations while
   continuing to lay the NORMATIVE exit-register images and wsp
   reservations).
2. The token pin (§2) — no code change from today's values, plus the
   bad-token abort wiring.
3. signal_has_handler over the chain (§5), replacing the
   select_frames memory walk.
4. The harness accounting change (§7) — the largest single item and
   the one to land FIRST behind a flag, because pairing asymmetry
   blocks everything else's validation.
5. The A/B matrix (§6), including the deliberate bad-token
   ABORT-INTENDED run.

## 10. Design flags (for the review session)

> **ALL THREE RESOLVED (Aug 13 2026** — rulings recorded in
> docs/Project8/PROMPT.md "Rulings — SETTLED"**):** (1) I.EPILOG
> mismatch → assert + abort_world(save=false), converging with
> REVIEW F3's independent adjudication; contract THIRD ADDENDUM
> item 1. (2) Walker outputs → DROP ENTIRELY, OVERRIDING the
> compute-and-discard preference below (the cells are L2's own,
> unread by the conforming system; the no-L1-reader scan is the
> evidence, the checker the enforcement). (3) Chain cap → NONE,
> OVERRIDING the proposal below (non-reentrant game, finite
> routines; the master hits real MV stack limits long before the
> native container notices). Text below kept for the record.

- I.EPILOG mismatched-frame handling (§1's table): the bit-faithful
  behavior is "write [wfp+8] to the head" even when wfp is not the
  establisher; the native chain has no natural analog for a
  mismatched pop. Proposal: debug assert + abort (the condition is
  PL/1-impossible and smells defensive), but that quietly adds an
  ABORT-INTENDED site the contract does not list. Needs a ruling
  before Phase 2 (A1 sweep candidate).
- Walker outputs: computed-and-discarded (§3) vs not-computed-at-all.
  Discarding is free and keeps the gate semantics exercised; a purist
  reading of Register E7 allows dropping the computation too. Mild
  preference recorded for compute-and-discard.
- The chain vector's growth: unbounded in principle; the bit-faithful
  world was bounded by stack headroom. A cap (existing
  FRAME_WALK_LIMIT=1024 spirit) with a loud throw preserves the
  METHOD §8 stance.
