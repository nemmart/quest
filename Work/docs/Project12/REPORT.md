# Project 12 — REPORT: M4a first migration (READ_IN off the stack)

*Solo implementation session, Aug 15 2026. Design of record:
docs/M4aDesign.md (NOT edited; findings and rulings below). Prompt:
docs/Project12/PROMPT.md. Result: READ_IN runs its WSAVS frame in a
fixed area at 0x78006C50 on the clone, at 0 divergences through the
agreed regression, with Checker Generation 4 armed. ONE routine; stopped.*

## 0. Rulings taken this session (design amendments — for M4aDesign.md)

**Boundary-2 stop at the plan gate — RULING A (user).** Four chain
walks test frame-chain well-formedness by ADDRESS ORDER of the
saved-wfp chain: R?SIGNAL (original code `ef6e WSGT 3,2` → restart/
?FATAL; native mirror runtime/r_signal.cpp:62), the DEF?ON pre-walk
(runtime/def_on.cpp:73), I.GOTO's unwind walk (runtime/frames.cpp:278,
ABORT-INTENDED in native/check), and `native_error_handler::cut`
(runtime/native_error_handler.cpp:114, pops records while
`frame > target`). An area frame (0x78xxxxxx) linked below a real-stack
frame (0x70xxxxxx) reads as ASCENDING, so any signal raised while a
migrated frame is anywhere on the chain would send the clone down the
anomaly/abort path while the master does not. **Ruling A: every
comparison the master makes in stack coordinates, the clone makes in
T()-translated coordinates.** The four walks changed ONLY their
comparison operands to `T(·)`; no other logic touched. Under a book the
clone is native/check-only (the mv attic cannot run with area frames).
Exercised and green: §5 run `readin_inj`.

**Clarification 1 (approved):** T covers the real stack ABOVE a hijacked
frame — every clone real-stack address above a live area frame's frame
word is shifted by the cumulative `10 + 2*frame` words per live frame.
The live table carries the cumulative shift, not just area records.

**Clarification 2 (approved):** mediated syscalls translate the master's
addresses back to the clone's for read-verification and write-replay
(T⁻¹). §5's "the clone executes its own syscall locally" holds only for
LOCAL calls.

## 1. As-built decision table — the hijack (hw/EagleStack.cpp WSAVS/WSAVR, WRTN)

| Moment | Decision | As built |
|---|---|---|
| WSAVS/WSAVR pc in book, clone role (or non-lockstep QUEST) | hijack | `Machine::area_hijack_enabled()` && `AddressBook::lookup_pc(pc)` |
| Routine already live | abort_world (re-entrancy tripwire) | `BookEntry::live` set at hijack, cleared at WRTN/unwind |
| Master-side overflow symmetry | fault iff the master would | overflow test on `shadow_wsp()+10+2f > wsl` |
| Actual argc | from the LCALL frame word at [wsp] | `frame_word & 0x7FFF`; `> book max_argc` → abort |
| Args + frame word | verbatim copy into the area at the same offsets from wfp | words `[W-2argc .. W+1]` → `[area_wfp-10-2argc ..]`; real stack untouched |
| Restore image | five wides INTO the area | ac0 wfp-8, ac1 wfp-6, ac2 wfp-4, prev wfp wfp-2, ac3\|c wfp+0 |
| wfp / ac3 | area wfp | `wfp = ac3 = book wfp_base`; wsp stays = W |
| WSAVS space | zeroed per ruling 8 | words `[wfp+2, wfp+2+2f)` when zero_claims |
| Live record | pushed | `LiveArea{entry, area_wfp, W, argc, frame, master_wfp = T(W)+10, shift_after}` |
| WRTN, wfp in area range | stock sequence, then `wsp = W - 2 - 2*argc`, drop record | `Machine::area_wrtn_fixup(pre_wfp)` — called from EagleStack WRTN AND `runtime/frames.cpp::wrtn` (I.EPILOG, I.GOTO's cut) |
| WRTN out of order (area frame not innermost) | abort_world | in `area_wrtn_fixup` |
| I.GOTO cut | drop live records above the target | `Machine::area_unwind_to(target)` after the snapshot restore |
| Log | `hijack` trace type | one line per hijacked WSAVS/WRTN/unwind-drop: routine, area wfp, argc, frame, real wsp, shadow wsp, master wfp, depth |

**Correction (METHOD §11) to §3 prose:** the WSAVS operand counts WIDES
(`EagleStack: wsp += frame_size*2`), so the WSAVS space is `2*frame`
words; sizing = `2*max_argc + 12 + 2*frame`, rounded to 16. The tool and
the book use this.

## 2. As-built decision table — Checker Generation 4

| Surface | Rule | As built |
|---|---|---|
| Live table | `(clone_addr, master_addr, size)` per allocation, LIFO | `Machine::areas` — area record + cumulative real-stack shift |
| T(v) | area address of a live frame → `master_wfp + (v - area_wfp)`; real address `> W` of a live record → `v + shift_after` (innermost applicable); else identity | `Machine::T` |
| T_any(v) | registers/fields hold word addresses, @-flagged word addresses (bit 31, 0xF0…), and BYTE addresses (0xE0…): try word, then bit-31-stripped word, then byte (`v>>1`) | `Machine::T_any` — **correction**: the design's "pointer-shaped fields" needed the three readings spelled out; both non-word forms were hit by ?READ's packet on the first live run |
| Register rule at every pair | pass iff `clone == master \|\| T_any(clone) == master`, all four ACs; carry strict | `Lockstep::compare_pair` |
| wsp | pass iff `shadow_wsp == master wsp` | NEW compare (wsp was not compared before); armed on the empty book too — proved wsp parity in the baseline at every pair |
| shadow_wsp | the master's wsp | **as built: closed form** `T` of the clone's wsp with a `>=` threshold (a clone wsp still sitting on a hijacked frame's frame word counts as above it). This is the design's per-instruction simulation in closed form: master wsp − clone wsp = Σ(10+2f) over live hijacked frames whose frame word is at or below wsp, always. Recorded as a correction/simplification; the check has identical strength (any missed accounting fails every subsequent pair). User may veto → per-instruction hooks at every wsp mover |
| Mediation, ACs | T_any on ac0–2 at `verify_arrival` | os/LockstepMediator.cpp |
| Mediation, packet reads | master reads its address, verifies against the clone at `T⁻¹(address)`; a differing field value passes iff `T_any(clone value) == master value` | os/OSContext.cpp `clone_word_address`, `verify_read` |
| Mediation, write replay | replay at `T⁻¹(master address)` (byte writes: word part translated) | os/LockstepMediator.cpp replay loop; dual writes likewise |
| T⁻¹(m) | master frame region of a live record `[master_wfp-10-2argc, master_wfp+2+2f)` → area; above → `m - shift_after` | `Machine::T_inv` |
| L2 door | crossings pairs use compare_pair (rule above); L2 walks compare in master coordinates (ruling A) | frames/r_signal/def_on/native_error_handler |
| pc streams, counts, exceptions, trap sites, spans, terminal machinery | unchanged, strict | — |
| Book empty | byte-identical behavior | `areas` empty → T identity, shadow_wsp = wsp; verified §5 |

## 3. Stage-2 audit (a): readers of `machine.wfp` / wfp-as-address

| Site | Class | Verdict |
|---|---|---|
| hw/EagleStack.cpp WRTN (`wsp = wfp` + pops) | STRUCTURAL | design §4; fixup added |
| hw/EagleStack.cpp WSAVS/WSAVR/WSSVS (push wfp; wfp = wsp) | VALUE / hijack point | hijacked for book pcs; WSSVS none in game code |
| hw/EagleStack.cpp LDAFP/STAFP | VALUE | round-trips |
| hw/EagleStack.cpp WPOPB (pops from wsp) | VALUE (wsp-only) | not wfp-derived; not in game code |
| hw/EagleIntegration.cpp ctor/wrtn/wrtn_void | STRUCTURAL but self-contained | its own frame is pushed on the real stack by the ctor; never a game frame → left alone |
| hw/RTBridge.cpp emulate_frame/_ss (writes wfp into image), native_return(_ss) (`ac3 = wfp`) | VALUE | frames of native leaves are real-stack residue |
| hw/RTBridge.cpp RTBridge ctor (`frame_word` at [wsp]) | VALUE (wsp) | args stay on the real stack under hijack ✓ |
| hw/Machine.cpp:58 restore from context; hw/Lockstep.cpp/debug/Capture/ProbeSuppressions prints | VALUE | — |
| debug/CallStack augment/call_return | VALUE (diagnostic) | — |
| os/OSTask/OSProcess boot wfp | VALUE | boot only |
| runtime/frames.cpp `wrtn` (I.EPILOG's caller return; I.GOTO's cut) | STRUCTURAL | fixup added — **missed hook point in the design list; found by the audit** |
| runtime/frames.cpp i_prolog (`wfp0` slots, `LDASP` snapshot, `LDAFP`) | VALUE | slots live in the area; snapshot is a real value restored to a real wsp |
| runtime/frames.cpp i_goto walk (`below < cursor`) | ORDERING | ruling A |
| runtime/frames.cpp i_goto `wfp = cursor; wrtn(); wsp = snapshot` | STRUCTURAL via wrtn | fixup + `area_unwind_to` |
| runtime/o_on.cpp `caller = wfp` | VALUE | registration key |
| runtime/o_signal.cpp `entry_wfp`, dispatch `wfp = F`, `wsp = E+12` | VALUE | F/E are O?SIGNAL's own real RT frame |
| runtime/lib_error.cpp replay_wrtn (`wsp = wfp`) and `wfp = F/Fh` | STRUCTURAL but self-contained | replays frames this translation wrote (RT frames on the real stack) → left alone; would take the same fixup if a game frame ever passed through |
| runtime/def_on.cpp pre-walk, runtime/r_signal.cpp walk | ORDERING | ruling A |
| runtime/native_error_handler.cpp cut (`frame > target`), I.EPILOG pop check (`==`) | ORDERING / VALUE | ruling A / fine |
| runtime/mv_error_handler.cpp slot writes `[wfp+2..0xC]`, `[wfp+0xA] = &head slot` | VALUE (memory) | slots land in the area; head-slot pointer is a real (shifted) address in L2-private memory |
| runtime/p_defon.cpp `wfp = F` | VALUE | own frame |

## 4. Stage-2 audit (b): the native L2

I.PROLOG builds the head slot on the REAL stack (`wsp += 4`, LDASP
snapshot) and the condition-frame slots at `[wfp+2..0xC]` (area under
hijack); it never derives wsp from wfp or vice versa. O.ON keys on
`wfp` as a value. I.EPILOG pops the record by equality and returns
through `wrtn()` (STRUCTURAL → fixup). I.GOTO walks by order (ruling A),
returns through the cursor (fixup if the cursor is an area frame), and
restores wsp from the establishment snapshot (a real value) —
`area_unwind_to(target)` after it drops every live record above the
target (area target: its own record stays). Chain records
(`TaskL2State::chain[].frame`) hold area frame pointers as values; the
cut compares them in master coordinates.

Unwind hook design: shadow wsp needs no explicit reset — it is T of the
restored real wsp, and the records above the cut are dropped by
`area_unwind_to`, so `T(snapshot)` after the drop equals the master's
restored snapshot (the shift below the target is unchanged).

## 5. Census + regression evidence (docs/Project12/evidence/)

Census ride-along: `Memory::map_page` ever-mapped bitmap + `pagemap`
trace + shutdown dump (`census_dump.txt`): **70000000..7019F7FF (1662
pages), nothing at or above 0x78000000**, in every run. Static sweep
of quest.dis/quest.mem for ≥0x78000000: only pc-relative displacement
renderings (`[pc+0x7FEA444E]`) and `0x7FFFFFFF` immediates — no absolute
address. Static layout mapped at launch: 28 pages RW/no-exec on the clone
(book with READ_IN live: total 27,760 words because commented routines
still reserve their blocks — the book is stable when lines are flipped).

| Run | Book | Result |
|---|---|---|
| base_m (pristine binary) | — | M-trigger (M,n,abc → CONVERSION chain), ESC → I.STOP detach; 0 div |
| empty_m | none | same, 0 div, **shadow_wsp check armed** |
| empty_fo | none | FAIL_OPEN=USER_DATA_FILE + L→P: ?LIB_ERROR→O?SIGNAL→I.GOTO handled; 0 div |
| readin_m | READ_IN | login + M-trigger + ESC/I.STOP detach; 0 div; **8 hijacked WSAVS/WRTN pairs** (`hijack_readin_m.log`: area_wfp 78006C5C, real_wsp e.g. 700010DE, shadow_wsp 700010F4, master_wfp 700010E8; WRTN restores real_wsp 700010DC) |
| readin_fo | READ_IN | FAIL_OPEN + L→P handled; 0 div; 6 hijack lines |
| readin_inj | READ_IN | `QUEST_INJECT=70176716:-1:0x2006` — a signal raised INSIDE READ_IN while its area frame is live: O?SIGNAL(native) → the ON-unit → I.GOTO(native) unwinds THROUGH the area frame (ruling-A walk, cursor real, target area) → I.EPILOG returns the area frame → game reaches I.STOP (login abandoned by the on-unit's 0 store) → detach; 0 div. This is the ON-unit/unwind exercise the design chose READ_IN for |

Notes: (1) The L→P FAIL_OPEN driver ends at socket close, not I.STOP
(known driver artifact, NextSession.md); the M driver is the ESC/detach
regression. (2) READ_IN's frame is short-lived (login prompts), so the
natural M-trigger/FAIL_OPEN signals never fire while it is live; the
injection run is what proves ruling A. (3) `-handler=` default CHECK
(mv+native compared) — mv's slots and native records both took the area
frame without complaint.

## 6. Two mediation corrections found live (boundary 3)

First READ_IN run: `mediated-call input mismatch` at ?READ's packet —
master `E00022B6` vs clone `E000228A` (a BYTE address into ?READ_SCREEN's
real-stack buffer, differing by 0x2C bytes = 22 words = 10+2·6). Second:
`F0001152` vs `F000113C` (an @-flagged word address). Both are
"pointer-shaped fields" in the design's words; T_any now tries the three
readings. Also `read_verify` reads the clone at `T⁻¹`, and replay writes
at `T⁻¹` — without these the clone would have read the master's packet
cell at the wrong address (the mismatch that fired) and got its ?READ
results written to a stale stack cell.

## 7. Files changed (file:function)

- tools/build_address_book.py (new); c_src/quest.addrbook (new; READ_IN live); docs/Project12/addrbook_report.md
- hw/AddressBook.{hpp,cpp} (new): book loader (`QUEST_ADDRESS_BOOK`), `map_pages`, `LiveArea`
- hw/Machine.{hpp,cpp}: `areas`, `area_hijack_enabled`, `T`, `T_any`, `T_inv`, `shadow_wsp`, `area_wrtn_fixup`, `area_unwind_to`
- hw/EagleStack.cpp: WSAVS/WSAVR hijack; WRTN `pre_wfp` + fixup
- hw/Lockstep.cpp: compare_pair register rule + wsp compare; describe() prints shadow_wsp/T
- hw/Memory.{hpp,cpp}: ever-mapped bitmap, `pagemap` trace, `dump_ever_mapped`; Launch.cpp calls it at shutdown
- os/Trace.cpp: types `pagemap`, `hijack`
- os/LockstepMediator.cpp: T_any on ACs; T⁻¹ replay
- os/OSContext.{hpp,cpp}: `clone_word_address` (T⁻¹) on read-verify and dual writes; T_any in verify_read
- os/OSProcess.cpp: clone launch mapping; Launch.cpp: book load
- runtime/frames.cpp: `wrtn` fixup; i_goto walk operands T(); `area_unwind_to`
- runtime/r_signal.cpp, runtime/def_on.cpp, runtime/native_error_handler.cpp: walk/cut operands T() (ruling A)
- Makefile: hw/AddressBook.cpp
- docs/Project12/{drive.py,run.sh,evidence/}, docs/CheckerHistory.md Generation 4

## 8. Corrections to design facts (METHOD §11)

- §2 counts: measured 81 named entries (74 in the design; the design's
  own 74+49 ≠ 126), 49 nested, 130 entries, 128 WSAVS + 2 WSAVR, 63
  XCALL sites all conforming, 0 unnamed LCALL targets, 16 slot-patch
  entries in 14 symbols (design 18), 16 dyn segments (design 13: DROP
  and LOCK_FILE have real LDASPs; "OP_HELP(3)" are OP_EDIT.4/6/8).
  Pure = 95 of 130 (design "~74"). Segmentation caveat in the report.
- §3: WSAVS operand is in wides (see §1 above).
- §5: shadow wsp built in closed form (see §2, user may veto);
  "clone executes locally" is LOCAL-only (clarification 2).
- §6 open item "any other reader of wfp": frames.cpp::wrtn was one.

## 9. Not done / for Project 13

Widening (pure list review; OWNS/FIND_OBJECT/RANDOM next per §7);
`hijack` trace count check as a standing regression; the master-side
overflow symmetry test is on the shadow value (never exercised);
`lib_error::replay_wrtn`/`EagleIntegration::wrtn` should take the fixup
call if a game frame is ever routed through them (today provably not).
