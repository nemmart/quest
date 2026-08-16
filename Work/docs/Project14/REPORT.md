# Project 14 — Phase A: the Mapper module (spec-first refactor)

Status: **Phase A complete; stopped at the phase gate.** Phase B (batch-3
widening) not started, per the prompt.

Result: docs/Mapper.md is implemented as one module, `hw/Mapper.{hpp,cpp}`;
the accreted mapping code is deleted; the full regression battery passes
with 0 divergences, strict cross-checks, all named signatures paired, and
0 probe fires. Build is warning-free. Two tool defects were found and
fixed on the way (both boundary 3, both with user rulings where they
touched landed artifacts): the address-book layout off-by-2 (R-C) and the
silently-degrading QUEST_INJECT knob. One regression-methodology
correction landed: wall-clock enters the world via ?GTOD, so
world-state-downstream counts are banded, not fixed (matrix restated,
ruled; see §6).

## 1. As-built

One module, owned per-`Machine`, configured at `OSTask` construction:

| Surface (Mapper.md §1.3 + Q1-a ruling) | Purpose | Consumers |
|---|---|---|
| `equivalent(master_v, clone_v) → Verdict{RAW/MAPPED/MISMATCH, decodings, mapped, record}` | equality verdict; clone→master inside; asymmetric by definition | Lockstep register rule + divergence dumps; mediator `verify_arrival` regs; OSContext `verify_read` value check |
| `frame_precedes(a, b)` | order verdict in master coordinates (signed, like the walks it serves) | the four Ruling-A chain walks: `runtime/frames.cpp` (I.GOTO pre-walk), `runtime/def_on.cpp`, `runtime/r_signal.cpp`, `runtime/native_error_handler.cpp` |
| `clone_location(master_addr)` | dereference, master→clone; form-aware (codec decomposes word/byte/@-word) | mediator write replay (inline decomposition deleted); OSContext verify reads, dual writes, dump |

Configuration: `configure(owner, book, is_main_task)` from both `OSTask`
ctors; `book == nullptr` on the master and on non-QUEST processes
(`OSProcess::mapper_book`, set beside the `map_pages` gate at launch).
Master-vs-clone is a property of the configuration; no code path queries
roles (`EagleStack`'s redirect gate is now `mapper.entry_for_pc(pc)`).

Mutations (the exactly-three kinds, Mapper.md §3): `push_record` (called
from the EagleStack WSAVS redirect after the memory/register work, before
`copy_segment`; computes master_wfp/shift, asserts, sets live, traces),
`wrtn_fixup` (via the `Machine::area_wrtn_fixup` forwarder; every WRTN
replica), `unwind_to` (I.GOTO cut). Trace lines are byte-identical to the
pre-refactor format.

`Machine` keeps forwarders (`equivalent`, `clone_location`,
`frame_precedes`, `shadow_wsp()`, `mapping_active()`, `mapping_depth()`,
`area_wrtn_fixup`, `area_unwind_to`); os/-layer code calls those, never
the Mapper.

Codec (I3): word 0x70/0x74, byte 0xE0/0xE8, @-word 0xF0/0xF4 — derived
from BASE by `static_assert` (prefix separability, plus a pin that the
`decode()` table matches the base). Unlisted forms decode to None →
identity + probe.

## 2. Deletions

`Machine::T`, `T_any`, `T_inv`, `shadow_wsp` (free function form),
`area_redirect_enabled`, the `areas` vector and `LiveArea` (now
`LiveRecord`, owned by the Mapper), `OSContext::clone_word_address`, the
mediator's inline byte/@ decomposition, and the inline record-build/trace
block in EagleStack. Grep-proof: `T_any|T_inv|clone_word_address` and bare
`.T(` appear nowhere outside Mapper.hpp's history comment.

## 3. Assert inventory (what runs, where)

- **I1** — blocks strictly disjoint including closed ends: loader
  (strictened: rejects `alloc_base <= prev_end`, with a first-entry
  carve-out) and re-asserted at `configure`.
- **I2** — wsl latched at the empty→nonempty push (post-steal value,
  ruling); asserted constant at every mutation while records live; the
  latched value bounds the stack leg in BOTH directions.
- **I3** — static prefix separability + codec-table pin (compile time).
- **I4** — round trip on every non-identity mapping (`map_checked`):
  strict on interval interiors; master-value fixpoint on the overlay band
  (Q2 ruling). Fires on `equivalent`, `clone_location`, `frame_precedes`,
  and `push_record`'s master_wfp computation.
- **I5** — LIFO nesting: push must nest above the innermost (`W >
  back().W`); pop must take the innermost (out-of-order return aborts,
  message preserved verbatim); re-entry backstop at push (primary
  tripwire stays at the WSAVS site, before side effects); unwind pops a
  suffix by construction.
- **I6** — on every MAPPED verdict, the inverse agrees with the clone's
  value (fixpoint form on the overlay band) — bijectivity as a tested
  invariant, running on every mediated pointer-field pass.
- **Main-task assert** — at every push. **Interpretation note:** the
  spec's gloss "equivalently: wsp within the main stack bounds" is not
  statically available — the .PR header ships `wsb == wsl` (the
  deliberate boot-probe overflow; I.INIT builds the real stack at
  runtime). Implemented as the primary form: task identity
  (`is_main_task` at configure; the launch()-made task is main,
  ?TASK-created tasks are not). First A3 run caught the wrong
  implementation (bounds form aborted on READ_IN at login); reworked.
- **Extent-fits-block** (new, R-C ruling): occupied span
  `[alloc_base, wfp_base + 2 + 2*frame)` inside the block — at
  `configure` over the whole book (fires at load) and re-checked at every
  push with the instruction's own frame size (catches book-vs-code
  drift). This assert exists because of the finding in §4.
- **Probe** (Mapper.md §1.2): unlisted pointer form whose shifted/masked
  readings land in a mapped range → stderr `MAPPER PROBE` + counter.
  0 fires across the battery.

## 4. Finding: address-book layout off-by-2 (tool bug; R-C ruling)

While deriving the Q2 fixpoint against the real layout: the tool placed
`wfp_base = alloc_base + 2*argc + 12`, but the frame's occupied span is
`[wfp-10-2argc, wfp+2+2f)` — the ac3|carry image wide sits AT
`[wfp, wfp+2)` — so blocks sized by the design formula
`round16(2argc+12+2f)` were 2 words short of the true extent whenever
`(2argc+12+2f) % 16 == 0`. Fifteen book entries were in that class, seven
of them live, including GET_INPUT which redirects in every run.

**The design was right and the tool deviated** (M4aDesign §3's layout is
self-consistent with args at alloc_base and `wfp = base + 2argc + 10`).
Ruled R-C: fix the tool to the design. Applied to
`tools/build_address_book.py`, the loader check, and the header comment;
both regression books regenerated; diff audited — 130 entries, wfp column
−2 only, nothing else moved (`evidence/book_rc_diff.txt`). The R-C book
was sanity-proven on the pre-refactor emulator first: 0 div, 1260 lines,
P13 §6.6 counts exactly.

**Near-miss, recorded honestly:** every green battery to date wrote two
words past those blocks into the stride gap. The stride ruling (adopted
for I1 disjointness) is what masked it harmlessly — one invariant
accidentally covering for the violation of another until the arithmetic
caught it. A sentence for the CheckerHistory Gen-4 entry is queued for
the Phase B landing (with the roll-call), per the prompt's landing list.

**Consequence for evidence reading:** redirect traces now show wfp values
−2 vs P13 logs. Coverage counts are the comparison basis, not
byte-identical log diffs.

## 5. Finding: QUEST_INJECT silently degrades to "never fires" (tool bug)

The first b2 inj run used `QUEST_INJECT=7016A896` (the abbreviated form
this project's own notes carried); the knob wants `site:type:code
[:RESUME]`, and the malformed spec produced a stderr note and **a normal
clean play run** — an injection battery entry that goes green while
testing nothing. It was caught only because the matrix pins endpoints per
run: the expected ?FATAL detach at 7017F036 did not appear, and a "clean
play run" was treated as an anomaly, not a pass. **That is the argument
for endpoint pinning.** Fixed fail-loud: an unparseable QUEST_INJECT now
refuses to launch (`exit(2)`, message names the wanted shape); verified
in the harness (run dies immediately, nothing plausible-green). The rerun
with the established shape `7016A896:-1:0x2006` reproduced P13 exactly
(1269 lines, ?FATAL 7017F036).

## 6. Finding: wall clock enters via ?GTOD; world-downstream counts are banded

First A3 m run: 1268 redirect lines vs the expected 1260 — four extra
DIST calls from site 7017D2B6 inside TOWER_ATTACK, made by BOTH engines
identically (gcalls symmetric, 0 div). Isolation, in order: repeats gave
1268/1266/1260/1258 (run 3 reproducing P13's 1260 exactly on the
refactored binary — the same binary produces old and new counts, so the
refactor is exonerated by construction); the source tree is
tarball-identical (checksummed, every file), so the variance arises
during the run; the world file is written during each run (169/179 bytes
vs pristine; 165 between runs); ?GTOD returns different wall-clock values
per run and is **mediated** — "World-facing calls: master executes, clone
gets checked copies" (`os/LockstepMediator.cpp:68`) — so both engines
agree while runs differ.

**Permanent methodology fact (matrix restated, ruled):** pass criteria
per run are 0 divergences; cross-check strict (gcalls == redirect per
live routine); named signatures pair; probe count 0; endpoint pinned per
run (m/inj detach pcs, abort's WORLD ABORT ×both engines, fo/play clean
or clean-quit — play's endpoint is itself a band: I.STOP detach or socket
close, by driver timing). Per-routine coverage for world-state-touching
counts is banded, with login-phase anchors exact: across all five m runs
READ_IN=4, LOGON=1, INIT_SCREEN=1, REFRESH_SCREEN=1, GET_INPUT=8
(HIT_ANY_CHAR 1, one run 2 — keypress timing). Note the band is wider
than first thought: even DIST's main scan site 70166E1C moved (594→590 in
m4), so site-stability must be demonstrated, not assumed. P13's five-run
1260 was a same-window artifact. Recorded in docs/Run.md as well (§8).

## 7. Regression battery (evidence: `evidence/summary_p14.txt` + logs)

| run | div | lines | endpoint |
|---|---|---|---|
| b2 m ×4 + final_m | 0 | 1258–1268 (band; anchors exact) | detach 7017FCE8 (I.STOP) |
| b2 fo | 0 | 1863 | clean |
| b2 play | 0 | 52958, cross-check strict | clean (socket close) |
| b2 inj (`7016A896:-1:0x2006`) | 0 | 1269 | detach 7017F036 (?FATAL) — P13 exact |
| b2 abort (`7016871D:ABORT`) | 0 | 19 | WORLD ABORT both engines, same banner wides |
| b1 m | 0 | 1210 — P13 exact | detach I.STOP |
| b1 fo | 0 | 1808 — P13 exact | clean |
| b1 play | 0 | 16428, all 6 live reached | detach I.STOP (clean quit) |
| b1 inj | 0 | 1217 — P13 exact | detach 7017F036 (?FATAL) |
| b1 abort | 0 | 7 | WORLD ABORT both engines |

Probe fires: 0 in all fourteen runs. Signatures: LOGON's record live
across seq 160→1602 in m (every mediated pair in that window ran the
`equivalent` rule with I6 asserting on each MAPPED verdict); the @-flag
path via REFRESH_SCREEN (fired every m run); one-past-end via
HIT_ANY_CHAR — **with a caveat, next paragraph**.

**Residue-interior note (honesty):** under R-C, HIT_ANY_CHAR's known
one-past-end residue lands interior (`base+30` against closed end
`base+32`), so the closed-end point itself currently has **no live
specimen** — end-inclusive attribution at the exact closed end is
validated by the I4-fixpoint/I6 asserts in principle but by no observed
producer. Only the x%16==0 class (GET_INPUT et al.) has its residue AT
the closed end, and none of those routines has been seen to leak a
residue into a compare. A one-shot log line now marks the first real
exercise if it ever happens (`MAPPER: first closed-end residue mapping
exercised (...)`) so the session that lands on it will know the path had
never run before.

Setup near-miss (mine, recorded): I repeated P13 §6.2's scratch-copy
mishap — symlinked `work/QUEST`, `cp -r` carried the symlink, the first
baseline's write-back polluted the source ("Initials already in use" on
the next run). Restored from the tarball as a real directory; the one
run made on polluted data was discarded and rerun. Baselines above are
all from pristine copies.

Binary provenance: the battery ran on the warning-free refactored build;
`p14_b1_abort` and `p14_final_m` ran after the two late touches (inj
fail-loud, closed-end log line), both unreachable in green runs
(launch-parse-only / trace-only). `p14_final_m`: 0 div, 1260, anchors
exact, probes 0.

## 8. Rulings record (for folding into Mapper.md — doc edits stay with the user)

1. **wsl-latch (I2):** latched at the empty→nonempty push (the boot
   wsl-steal precedes any redirect); asserted constant while records
   live; the latched value bounds the stack leg — both directions. A
   declared, spec-mandated behavior delta vs the old unbounded shift.
2. **Q1-a:** the surface is THREE calls — `equivalent` (equality),
   `frame_precedes(a, b)` (order, master coordinates, the Ruling-A
   walks' only legal instrument), `clone_location` (dereference). Q1-b
   (direction-flagged public map) rejected.
3. **Q2:** A is a bijection on interval interiors; the band from a
   frame's extent end to its block's closed right end is a deliberate
   forward-only two-to-one overlay (an end pointer is a reference to
   extent, never a data location). Inverse resolves overlay masters to
   the stack-leg preimage (hi exclusive in the ToClone walk); I4 strict
   on interiors, fixpoint `fwd(inv(m)) == m` on the overlay; I6
   likewise on master value.
4. **R-C:** fix the tool to the design layout (`wfp_base = alloc_base +
   2*argc + 10`); R-A (enshrine the slack) rejected. Plus the
   extent-fits-block assert at configure AND push.
5. **Matrix restatement** (§6): banded world-downstream counts; strict
   invariants unchanged.

## 9. Declared behavior deltas (all ruled or spec-mandated; none observable in the battery)

- I2 bound on both legs of A (old T/T_inv shifted unboundedly above W;
  addresses above wsl now take the identity — the old T_inv would have
  mis-shifted a master heap/static address ≥ the innermost hi, latent).
- Gate alignment: non-lockstep non-QUEST processes are no longer
  redirect-eligible (old role-query gate would have redirected a
  same-pc match in e.g. QUEST_SERVER onto unmapped pages; configuration
  gate closes it).
- Cosmetics: divergence dumps print the raw value for RAW-equal
  registers (old printed `T_any`, which could differ); `describe()`
  takes a master-reference entry; redirect wfp values −2 vs P13 logs
  (R-C); `areas_depth(now)` dump text kept, value = `mapping_depth()`.

## 10. For the next session (Phase B gate)

Batch-3 widening per the prompt: the 33 remaining wave-one routines,
parents + callable children in the same batch; grow the play driver
toward the 19 armed-but-unexercised routines; RETURN_MESSAGE's fatal
path reported UNEXERCISED if unreached; landing = roll-call +
CheckerHistory.md Gen-4 append including the stride-masking near-miss
sentence (§4). Nothing else is carried: the inj knob is fixed, the
closed-end log line is in, Run.md carries the banding note.
