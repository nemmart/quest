# Project 1 — The Signal Core: O.SEARCH → O.SET cluster (296 words)

Hi Claude! You are one of three parallel sessions translating the PL/1
condition system to native C++ (Quest reconstruction, Milestone 3).

Read IN ORDER before any work: docs/METHOD.md, docs/SharedProtocol.md
(binding merge rules + frozen interfaces), docs/M3Plan.md, then this.
Background as needed: docs/ERROR_PROCESSING.md, docs/ON_ERROR_CATALOG.md,
docs/O_ON.md (the adjacent, already-native routines), docs/Run.md.

## Your routines — one contiguous block, 0x7017EDDD–0x7017EF05

| Addr | Symbol | Words | Notes |
|---|---|---|---|
| 0x7017EDDD | O.SEARCH | 10 | called from I.FFALT only (dead in emulation) — derive anyway, it shares the select loop |
| 0x7017EDE7 | O.SIGNAL | 6 | |
| 0x7017EDED | O?SIGNAL | 21 | the raise entry; LIVE per signal |
| 0x7017EE07–2D | O.SUNDER/SOVERF/SZEROD/SFIXED/SSUBSC/SCONVE | 44 | fixed-condition shorthands; O.SCONVE has a live caller (X.CB) |
| 0x7017EE33 | O.SERROR | 35 | 9 RT callers (heap corruption paths); tail reaches I.STOP (terminal) |
| 0x7017EE56 | O.SET | 175 | LIVE per signal; interiors: EE62 select loop, EE7A + EE9D walkers |

Disassembly: Disassembled/quest-rt.dis. Verify every boundary yourself.

## Facts already established this project (verify, don't re-litigate)

- The EE9D walker is PURE READS until its result stores at 0x7017EEFF —
  the basis for the terminal treatment below.
- O.SET's LCALL to I?LINEID at 0x7017EECF is guarded by a mid-walk
  comparison against a value cached from a stack-base-relative global
  ([sb-0x40] area, loaded at EEA4); the branch is empirically untaken on
  handled signals (two shapes tested) and SUSPECTED to be the no-handler
  path. **Decision (locked): treat it like ?FATAL** — your native O.SET
  reaching that branch does `RTBridge::native_transfer` onto... derive
  the right terminal entry: if the branch truly heads to default
  handling, transferring to the ORIGINAL pc (let the master emulate from
  entry) is NOT available — instead detect the branch BEFORE any store
  (the walker is pure) and fall back to emulation from entry
  (`return entry_address(...)`, arming rt_pending_return), which the
  master's range-exit/terminal rules absorb: if the emulated path then
  reaches DEF?ON/?FATAL both engines detach there. Log the gate reason
  loudly. Do NOT translate the I?LINEID subtree.
- Live chain observed (fault-injected LIST_PLAYERS): ?LIB_ERROR →
  T?AREA×9 → O?SIGNAL(ret 7017E3EF) → O.SET(ret 7017EE3B — i.e. called
  from within O.SERROR's extent!) → I.GOTO. Map the real control flow
  between O?SIGNAL / the shorthands / O.SERROR / O.SET carefully — the
  entries fall through / branch into shared bodies; the extent table
  above is symbol accounting, not control-flow truth.
- O.SET is also LCALLed from I.SFALT (dead in emulation).
- I.GOTO is Project 3's; the handler-dispatch/unwind transfer OUT of
  your cluster into game code is yours wherever your code performs it.
  Use `RTBridge::native_transfer` (SharedProtocol.md).
- `rt::t_area` is frozen and implemented — call it, don't re-derive.

## Deliverables

- `runtime/o_signal.{cpp,hpp}` (name at your discretion; one or two
  files for the whole cluster is fine — it shares state)
- `docs/Project1/DERIVATION.md` — I_ALLOC.md is the quality bar:
  instruction semantics, frame images, every flag side effect from the
  emulator source, residue maps, the derived handler-chain data layout
  (this cluster READS the chain O.ON writes — O_ON.md documents the
  writer side; your derivation must agree with it or say why not).
- `docs/Project1/REPORT.md` per SharedProtocol.md.

## Validation you can do alone

Both triggers exercise your cluster end-to-end THROUGH EMULATION today;
with your translations registered LOCALLY (temporary table edit in your
session only — revert before REPORT, list entries instead), you can run
the full capture + lockstep validation on O?SIGNAL and O.SET, because
your callees are: T?AREA (call rt::t_area natively), I?LINEID (fallback
branch, above), and the transfer out (native_transfer). O.SERROR's
I.STOP tail: native_transfer to I.STOP's entry — terminal machinery
detaches. The shorthands validate via the store-"ABC" CONVERSION path
(X.CB → O.SCONVE) if reachable; otherwise capture-validate and mark
lockstep-pending.

Work plan-first: post your derivation plan in DERIVATION.md as you go;
translate one entry at a time; validate before the next. Report honestly
— blocked is a fine status with a reason.
