# Wide-carry emulator bug — found Aug 28 2026; LANDED by Project 24 (Aug 29 2026)

**P24 landing note.** The patch below is applied; docs/Project24/
CarryCensus.md is the census this doc's plan called for. Corrections to
this doc's own claims (per METHOD §11, annotated not rewritten):
(1) the affected-instruction list below omits **XWDO/LWDO**, which also
call `add()` (census finding F1); (2) the WADC sub-item is RESOLVED by
user ruling, Aug 29 2026: WADC carries the literal ALU carry-out of
dst + ~src — `WADC x,x` → ac=-1, **c=0** — and is fixed with the rest,
no routing (the census proved no wide-produced carry is consumed on any
reachable path, so the routing question was gameplay-unobservable);
(3) the feared "first carry-comparing rendezvous" divergence class was
addressed by re-deriving native residue in the same tranche — see the
P24 report for the file list.



## Finding

`EagleInstruction::add()/sub()` compute CARRY as `>>31` — bit 31 of the
sum/difference (the result's sign bit) — not the ALU carry-out (bit 32).
The DG manual's WADD and WSUB pages (user-supplied scans, this session)
both say "CARRY set according to value of ALU carry": bit 32 of the
unsigned sum for add, and of the complement-add `dst + ~src + 1` for
sub (carry = 1 iff no borrow), matching `narrow_sub`'s own convention.
Affects every caller of the wide helpers: WADD WSUB WNEG WADC WINC WADI
WSBI WNADI WADDI XWADD LWADD XWSUB LWSUB XWADI XWSBI. The narrow
(Nova ALC, `>>16`) path is correct. OVR (`|=`, sticky) is correct per
the manual ("set to 1 if overflow occurs", no clear).

Shared by master and clone → invisible to lockstep forever (METHOD §2);
only the manual could catch it. Consequence if the game ever consumes a
wide-produced carry (ALC datapath, 163 carry-live-in blocks, P22 §8):
gameplay diverges from the real 1986 machine, silently.

## Why deferred, not landed

The fix flips two pervasive idioms:

- `WSUB x,x` (zero a register): c was 0 → becomes **1** (no borrow)
- `WADC x,x` (load −1): c was 1 → becomes **0** (0xFFFFFFFF, no carry-out)

Native translations hand-derived carry residue from the OLD behavior:
Project1 (O.SEARCH cluster) and Project2 (?LIB_ERROR) DERIVATION.md
lean on "WSUB x,x → c=0" repeatedly; `runtime/lib_error.cpp` stages
`machine.c = 0` at three internal handoffs (lines ~172/331/350). With
the helpers fixed, the emulated master produces c=1 at those points and
the clone's native residue diverges at the first carry-comparing
rendezvous. So the correction is a task of its own: apply the patch,
re-derive staged/exit carry in the affected translations (audit both
DERIVATION docs, update the three lib_error stages and any implied exit
residue), K=1 leg to catch what the audit misses, then battery.
METHOD §5's "WSUB x,x clears carry" sentence documents the OLD emulator
behavior and will need a correction note when this lands.

## Why P23 is not blocked (user ruling, Aug 28)

The IR's `#+`/`#-`/`#*`/`#/` are DEFINED as "call the same
`EagleInstruction` helper the emulated instruction calls" — IRExec
invokes `add()/sub()` etc. directly, single source of truth, no
reimplemented formulas, no extracted tables baked into quest.ir. When
the helpers are fixed, `#`-ops are fixed with them, and the correction
battery re-verifies both paths at once. (Supersedes IRPhase1.md §3's
"exact per-op c/ovr formulas are a lower.py deliverable" — lower.py now
only CLASSIFIES ops as #+/#-/plain; semantics live in shared code.)

Related ruling, same session: every `#`-op ends with the emulator's
`ovk && ovr` check, throwing the identical "Overflow occurred at %08X"
string (OVK is enabled inside WSAVS bodies — i.e. all game code — but
has provably never fired: METHOD §3, millions of clean pairs).

## WADC carry — UNRESOLVED sub-item (Aug 28, later same session)

The WADC manual page says only "adds the ones complement of acs to
acd" plus the same "ALU carry" line. A literal carry-out of dst + ~src
gives c=0 for WADC x,x (no column propagates); the current emulator
gives c=1 (coincidentally, via the >>31 bug); an unsourced AI-generated
answer claims real hardware gives 1 via subtraction-pathway
micro-orders (self-contradictory reasoning, not evidence). Game code
demonstrably runs WADC and derivations record its carry (Project2
DERIVATION ~line 310: "WADC sets c=1"). METHOD §8: do not guess —
a wrong choice is silent. When the parked patch lands: WADD/WSUB
carry-out is manual-backed; WADC needs real evidence (DG Principles of
Operation ALU appendix, microcode listing, or a live MV) before its
carry changes. Conservative default: route WADC around the fix so its
carry keeps today's behavior (c=1 for x,x), explicitly flagged.

## The parked patch

`docs/Project23/wide_carry_fix.patch` — apply with
`patch -p0 hw/EagleInstruction.cpp` from c_src/ (or `git apply` with
path fixup). Compiles clean against the Aug 28 tree; formula
sanity-checked (0xFFFFFFFF+1 → c=1; 5−3 → c=1; 3−5 → c=0; x−x → c=1).
NOT applied to the tree.
