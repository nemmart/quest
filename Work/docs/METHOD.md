# How To Work On This Project

The other docs record *what* we found. This one records *how*, because
the method transfers and the findings do not. Every hard-won correction
in this project traces back to one of the principles below being
followed — or, more often, to it being skipped.

Read this once before starting. It is short on purpose.

---

## 1. The disassembly wins

Where any other source disagrees with the disassembly, the disassembly
is right. That includes the Opus-era translations in `rt/`, `emu_rt/`,
`types/` (reference only — re-derive, never copy), this project's own
docs, and your recollection of how the routine "obviously" works.

Corollary: **do not implement from intent.** Port instruction by
instruction. `SQR31?3` contains `FRDS 0,0` where every sibling path
implies `FRDS 1,1` — a genuine 1986 bug that must be replicated, and
one that any from-scratch `sqrt` would silently "fix". Two of its four
paths round and two do not, so the result is not a pure function of the
input value. `std::sqrt` is not an acceptable implementation.

## 2. Lockstep cannot adjudicate anything the two engines share

The harness verifies clone == master. Both run the same emulator, so a
missing or wrong *instruction* implementation, a wrong *syscall*, or a
wrong shared assumption is invisible: both sides are identically wrong.

Three things can catch those, and only three:
- an outright abort,
- external documentation (hardware manuals),
- reasoning about paths the game has never taken.

This is why `docs/UNIMPLEMENTED.md` exists, and why the queue
instructions needed the DG manual rather than another test session.

## 3. An unimplemented instruction throwing is evidence

A decoder entry with an empty class and `oper = -1` builds a base
`hw::Instruction`, whose `execute` throws. Therefore:

> **"The game ran clean" proves no unimplemented instruction executed.**

That is the strongest reachability evidence available, and several
conclusions rest on it — e.g. the heap free-list paths have provably
never run, because `QSEARCH` and `XCT` would have aborted.

Use it deliberately: to test whether a path is live, make it throw and
play. That is how `ENQT` was found to run on **every turn** via
`LOCK_FILE`, after being assumed dead.

## 4. Static reading does not tell you which instructions a routine uses

`XCT` executes an instruction held in a register. Quest builds enqueue
opcodes as immediates (`NLDAI 0xC7E9` / `0xC7F9`) and runs them through
`XCT`, so `ENQH` and `ENQT` appear **nowhere** as mnemonics in the
runtime. A grep-based hazard check missed them completely.

So checklist step 0 must also look for:
- `XCT` (known sites: 0x7017E9F6, 0x7017ECF4),
- opcode-shaped constants in `WBR 2`-skipped holes
  (0x7017E86B = `WRTN`, 0x7017E899 = `ISZTS`),
- ranges `StartStop` classified as `mem` that are actually code —
  **all four holes in the heap region are executable**.

To recover hidden code, write an addrs file and disassemble explicitly.
The addrs file is read from the **host** cwd, not the emulated
filesystem:

```
printf 'code 7017e97c 7017e9b3\n' > holes.addrs
java -cp Tools Disassemble QUEST QUEST holes.addrs
```

## 5. Verify flag and residue side effects from the emulator source

Never from the apparent intent of an instruction. `WSUB x,x` zeroes a
register **and clears carry** — a one-bit residue error that only a
capture diff caught. Know which instruction owns each convention side
effect: `WSSVS` pushes the shadow call-stack frame for `LJSR` routines,
not the call.

Every translation must leave a **bit-identical footprint**: the frame
image via `RTBridge::emulate_frame()`, plus every local the body wrote,
plus anything written outside the frame. Some code reads dead-stack
residue (`?WRITE_SCREEN` loads never-initialised locals into ac0 for a
syscall).

## 6. Prove residue empirically before registering a translation

Derivation is not validation. Use the capture tooling:

```
QUEST_CAPTURE=<entry-pc-hex>        A/B snapshots at entry and return
QUEST_CAPTURE_DEST=<word-addr-hex>  fixed second window
```

Diff the master's `RETURN` blocks against the clone's `NATIVE` blocks
and require **zero differing words** before gameplay. `QUEST_CAPTURE_DEST`
exists because some routines write to a static address rather than a
caller-supplied one — `I.LOCK`'s entire non-frame footprint is the lock
object at 0x70000200, which no ac2-derived window would have covered.

The harness earns its keep here: `?FILL_WORDS`'s first version read the
destination one dereference too shallow, caught in-session at the exact
byte.

## 7. Never re-enter emulation mid-routine

A native wrapper must not dispatch back into emulated code partway
through. Whole subtrees may go native together (the master's
run-to-return absorbs nested entries), but a half-native routine breaks
pairing.

When a branch cannot be translated, prefer **entry-detection plus
fallback to emulation** over aborting. Both engines then emulate
identically and the fallback is symmetric. `I.LOCK` detects contention
before any side effect and falls back; aborting instead would have been
wrong, because `SYSCALL 0525` is implemented and the master would
*block* rather than throw. Check what the master actually does before
choosing an abort.

If you do abort, match the master's exception **string** — `compare_pair`
compares exception text with `strcmp`.

## 8. Prefer a loud failure to a silent one

The failure mode this project most needs to avoid is silent compounding
— a wrong value that keeps running and surfaces later as an
unattributable divergence, or never surfaces at all.

Concretely: `REC`'s blocking branch used to poll in 3-second sleeps
forever, waiting for a wake that could never come (`SYSCALL 0523` is
unimplemented). It now throws. A hang teaches nothing; an abort names
the situation.

Same reasoning applies to guessing. `QSEARCH` is still unimplemented
because a wrong search condition does not crash — it selects a
different free block, the heap stays self-consistent, the game runs,
and lockstep agrees. That is precisely the invisible class of error.
**Do not guess at semantics whose errors are silent.** Wait for the
documentation.

## 9. Choose translation targets by validation path

Not by size or apparent simplicity. A routine gameplay never exercises
gives no validation. `?CHAR_TO_UNSIGNED` was the original first target
and was dropped after a play session showed zero calls.

Before starting, answer: *what makes this run, and how often?*

- `?FILL_WORDS` — thousands of calls per session
- `I.LOCK`/`I.UNLOCK` — 3 each, every session, via `I.ALLOC`/`I.FREE`
- `I.ALLOC`/`I.FREE` — `?CREATE_TASK` at startup, **not** `?LIB_ERROR`
- `ENQT`/`DEQUE` — every turn via `LOCK_FILE` ← `SIGNAL_TURN`

Prefer a target whose live path avoids machinery that does not exist
yet. `I.LOCK` was chosen precisely because its uncontended path
contains no syscall, so it could land before `RTBridge::syscall`.

## 10. A verification command's expected value must come from that exact command

When handing someone a check ("run X, expect N"), N has to come from
running X — not from a similar command run earlier, and not from
remembered output. A mis-specified check reads as a failed check, and a
failed check rightly stops work: an `i_lock` wiring audit expected 5
from `grep -c i_lock` when the 5 came from `grep -c "i_lock\|i_unlock"`,
and the session correctly halted on the mismatch instead of "fixing" a
tree that was fine (the fix would have created duplicate registrations).

Prefer behavioural checks over textual ones where possible: a lockstep
run showing `I.LOCK(native)` in the rtcalls trace cannot be faked by a
stale file.

## 11. Record corrections as corrections

When a previous conclusion turns out wrong, say so in the doc rather
than quietly editing it. The wrong turns are load-bearing: they explain
why the code has the shape it does.

Examples now in the docs: implementing `ENQH` did *not* unblock the
free-list insert (`XCT` aborts first); `emulate_frame` did *not* need
an `ovk` parameter; the contended-path abort would *not* have matched
the master.

---

## 12. Translation checklist additions (from the heap chunk)

Learned translating I.ALLOC/I.FREE* (see I_ALLOC.md "Validation" for
the incidents):

- **Every fallback path must arm machine.rt_pending_return =
  machine.ac[3] before returning entry_address.** The master commits to
  run-to-return from translated_bits alone; an unarmed clone fallback
  produces asymmetric batch boundaries and a structural divergence at
  the first inner native call. The i_lock.cpp fallbacks predated this
  rule (never live-exercised, would have diverged); retrofitted in the
  same session.
- **Cast every Memory::read_wide before signed comparison.** read_wide
  returns uint32_t; the machine's compares are signed. Route reads
  through int32_t variables or cast at the comparison.
- **Gate-reason strings in fallback logs are cheap and pay off
  immediately** — one rtcalls line localized a wrong-sign gate that
  bit-perfect footprints could never show.
- **The footprint diff catches what lockstep cannot.** The pair gate
  compares registers and counts at boundaries only; two engines can
  agree on every register while their heaps differ. Diff the NATIVE
  snapshot against the master's RETURN snapshot before declaring a
  translation done.


### §12 addendum (Aug 2026, from Project 2's review): the standard
fallback protocol is CONDITIONAL — `if(machine.rt_pending_return != 0)
return RTStubs::entry_address(name);` FIRST (no re-arm: an outer
fallback span is re-emulating and re-arming retargets its return), and
only then the routine's own gates with `rt_pending_return = ac[3]` on
fallback. A central nested-span guard also exists at all four dispatch
sites (LCALL/XCALL in EagleStack.cpp, LJSR/XJSR in EagleGeneral.cpp),
so in-span dispatch never reaches a wrapper; the in-wrapper guard is
belt-and-braces.

## 13. Terminal paths, and how to make them reachable

Learned reaching ?FATAL for the first time (five walls in one session).

- **Fidelity is owed to what the GAME can observe.** A path that never
  returns to game code only has to terminate. Terminal subtrees stay
  emulated: it is the one place native code may hand control to the
  emulator, because the pairing concern — a return that never comes — is
  exactly what a terminal path guarantees anyway. `?FATAL` (1662w) plus
  the seven routines reachable only from it are excluded permanently,
  not deferred.
- **Milestone 4 deletes the DG stack, so stack-DESCRIBING machinery is
  scaffolding by construction** — `?FATAL`, `?SNAP`/`P?SNAP`, `I?LINE`,
  and the `I?LINEID`/`?FIND_SCOPE`/`?FIND_LINEID_INDEX`/
  `?GET_LINEID_ENTRY` subtree. Do not invest in it. (Caveat: `O.SET`
  calls `I?LINEID` on the LIVE signal path and branches on the result,
  so that subtree cannot simply be dropped.)
- **Making dead code reachable is how gaps are found.** Every wall hit
  on the way into `?FATAL` was a genuine unimplemented AOS/VS call on
  code that had never executed. Fix, re-run, hit the next one. The
  emulator's own throw is the progress meter.
- **Look for the answer before deriving it.** Three times in one session
  something about to be approximated was already available: the queue
  manual settled `ENQT`/`DEQUE`, the syscall pages settled four calls,
  and `AOSVSSymbols.cpp` already held every AOS/VS error mnemonic
  (`ERRNL`, `ERIRB`, `ERFNO`, `ERTXT`) and `?DSCH` — after placeholder
  constants had been invented for them.
- **Documentation corrects plausible readings.** `?RNGPR` was guessed as
  "get message text"; it returns the .PR filename for a ring. `?ERMSG`'s
  AC1 was read as a 16-bit length; it is two 8-bit fields, and only the
  doc explains why `0x0000FFFF` means "255-byte buffer, channel 0377".

## Quick reference

| Symptom | Meaning |
|---|---|
| `Instruction subclass must override execute <NAME>` | unimplemented instruction — and proof this path had never run |
| `Unimplemented system call <octal>` | unimplemented syscall |
| `REC (0525) would block: ...` | inter-task wait; heap-lock contention or `MT?REC` |
| `LOCKSTEP DIVERGENCE` | clone != master; report on stdout |
| `Segment fault - block 0, page 1 not loaded` | benign, `QUEST_SERVER` startup, every run |
| `INTWT interrupted` / `EXIT!` | benign, normal shutdown |

Backtraces go to **stdout**, exception lines to **stderr** — run under
`stdbuf -o0 -e0` or a crash can lose the buffered backtrace entirely.

Scripted play: background the emulator and run the driver in the *same*
shell invocation. Always scratch-copy `QUEST/` first — data files are
written back at shutdown.

## 14. Tooling integrity (ruling, Aug 2026)

The disassembly is a MAP, not ground truth — and a defective map is
worse than a missing one, because it is trusted. RULING: **when a
disassembler defect is found (omitted operand fields, misdecoded
instructions, wrong classification), STOP and flag it; fix the
disassembler and regenerate the listings before proceeding with any
work that depends on them.** After regeneration, diff old-vs-new
listings and skim every changed line — the diff is a free audit,
since each changed line is a place where every prior reading worked
from a wrong rendering; anything load-bearing in the diff is a
finding worth recording. Precedent: the WLDAI omitted-register-field
bug hid an immediate-zero gate and produced a months-long wrong
description of O.SET's I?LINEID branch (fixed by Project 1; verified
at byte level; see Layering ruling / M3Plan). The bytes settled it —
raw-word checks remain the arbiter when the listing is suspect.

## 15. Validation-cost policy (user ruling, Aug 14 2026)

No multi-hour validation matrices. A project's own gate is the
MINIMAL SUFFICIENT set proving ITS change — prefer login-fast
triggers (FAIL_OPEN, REFRESH-site injections) over turn-cadence
triggers (M-trigger ~95s/turn, store navigation); target ≤ ~30 min
total. Breadth belongs to reviewer spot-checks and the NEXT
project's baseline (each project's step-1 baseline re-proves the
world as it found it). Honest partial evidence delivered promptly
beats exhaustive evidence delivered late; a red run is a
STOP-and-report, never a solo iteration loop.
