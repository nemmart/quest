# Project 54 — REPORT-2: the reopening (a003, three items)

Worker session, Sep 12 2026, on `main`. `bridge_selftest` GREEN **97 cases**
(86 → 97); the F-1 reproducer exits 0 through P53's own rig; the four other
self-tests unchanged GREEN. No battery, per a003 item 3.

---

## Item 1 — the silent segfault: CAUSE, FIX, TEETH

**What it was.** Neither the bridge's logic nor P48's rig, but a null
pointer under both: `debug::CallStack` dereferences its `symbols` pointer
in `call()` (`debug.count(symbols->name_for_address(entry))`), in
`augment()`'s warning, in `call_return()` and in `location_description()`.
A scratch Machine has no symbol table — `tests/lowerc_rig.cpp:111` builds
one with `nullptr`, as `vform_selftest` did until P54 — and nothing had
ever pushed a shadow frame on such a machine, because no call had ever
executed from IR. The bridge's `call_stack->call(...)` on a naive
game→game `call` is the first, and the process dies in the lookup before
anything can print. `rt_call` on the same rig is fine for an unrelated
reason: it checks `machine.symbols` for callee resolution and throws its
diagnostic before reaching the shadow push.

I had met this exact deref in the vform rig during P54 (REPORT §7, "both
rigs now own a `SymbolTable`") and fixed the *rigs*. That was the wrong
layer: the next rig without a table would crash the same way, and did.

**Where the fix is: the emulator, not the bridge and not the rig.**
`debug/CallStack.{hpp,cpp}`: a `sym(address)` helper returns `"?"` when
there is no table; every name lookup goes through it;
`location_description` prints the address alone. `tests/lowerc_rig.cpp` is
untouched (P53's, and it is not wrong — a machine without symbols is a
legitimate state). The reproducer now runs to its `ret` and exits 0.

**Teeth, two pins.** `bridge_selftest` leg 1c runs the valued game→game
program on a rig constructed WITHOUT a symbol table and requires the call
to round-trip. `run_bridge_selftest.sh` builds `lowerc_rig` and runs P53's
`f1-repro/call_symbolic_crash.{ir,vmap}` (from a `/tmp` copy — the rig
writes and removes `<program>.rigerr` beside its input, and that directory
is P53's with a committed `.rigerr`; my first run deleted it and I restored
it), requiring exit 0. A crash on this path can no longer be silent,
because it can no longer be a crash.

## Item 2 — the argument-KIND check: built, tested, spec and comments corrected

**The check** (`hw/IRExec.cpp`, the post-pass that binds a naive call's
target): for each pointer `a` cell of the callee, the LAST write to that
cell in the CALLING BLOCK before the `call` must match the declared kind
wherever the value's kind is manifest — `wp()` word, `bp()` byte, a pointer
cell its declared kind. Refuses by name: *"call GAMMA: argument cell
GAMMA.a1 is a BYTE pointer but the value written to it in ALPHA.b0 is a
WORD pointer (wp()/a word-pointer cell)"*. P53's measured pair
(`bp(HIT_ANY_CHAR.v0, 0)` vs `wp(HIT_ANY_CHAR.v0, 0)` into a `*char`) now
loads and refuses respectively.

**What it does not claim, stated in the spec rather than implied:** a value
of unknowable kind (a constant, a memory read, a register) passes; a write
in an earlier block is not examined. The alternative — refusing any pointer
`a` cell not written in the calling block — would refuse legitimate shapes
and would be the same overstatement in the other direction.

**Self-test leg 1b**, nine cases: `wp()` into `*char`, `bp()` into `*i16`,
a `*char` cell into `*i16`, a `*u32` cell into `*char`, the LAST write
counts (a correct write followed by a wrong one refuses); matching kinds
load, matching pointer cells load (pointee width advisory), a constant and
a memory read load, a write in an earlier block loads.

**Spec, `docs/IR.md`** (P52's file, edited under a003's licence):
- §5.1 REFUSED AT LOAD: the arity/kind bullet now says what runs — arity,
  and a manifestly wrong kind in the calling block — instead of "whose
  argument pointer KINDS disagree".
- §6, the naive `call` bullet: "must have been written by a preceding
  statement whose value is of the matching KIND" replaced by the check as
  built, with its two non-claims, and the relation to §5.10.5 (kind where a
  pointer is USED vs where it is PASSED).
- §6, same bullet: the a001 R1 execution semantics (`b0` entry, argc-0
  marker, WSAVS image, `ret` = WRTN) — the wording a001 asked for and that
  had not landed.
- §5.10.6 "What ir 8 does NOT do" and §6 "Not built here" said calls do
  not execute; both now point at P54.

**Comments, `hw/IRExec.cpp`:** `:887` ("arity/kind checked at end of load")
now says arity, `arg_count` and `b0` are checked there and the kind check
runs in the post-pass; `:1349`'s paragraph no longer claims the kind check
lives in the loop below it, and says why it once did.

## Item 3 — the battery

Not queued. Nothing here changes the strict surface: `CallStack` behaves
identically with a symbol table present (the emulator always has one), and
the kind check applies only to naive calls, of which both artifacts have
zero.

## Files

`emulation/debug/CallStack.{cpp,hpp}`, `emulation/hw/IRExec.cpp`,
`emulation/tests/{bridge_selftest.cpp,run_bridge_selftest.sh}`,
`docs/IR.md`, `docs/Project54/REPORT-2.md`. Read-only and unchanged:
`docs/Project53/f1-repro/**`, `tests/lowerc_rig.cpp`.
