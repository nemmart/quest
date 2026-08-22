# WPSH / WPOP in QUEST — complete characterization

*Aug 22 2026. Full classification of every WPSH/WPOP instruction, for
M4b (arg redirect) and M4c (in-body stack residue). Per-instruction
listing: Disassembled/quest.wpsh_wpop.*

## Instruction semantics

- **WPSH XX,AA** pushes AC[XX]..AC[AA] walking XX→AA incrementing mod 4.
- **WPOP XX,AA** pops into AC[AA]..AC[XX] walking AA→XX decrementing mod 4.
- Same operands → proper inverses (each AC restored to its slot).
- Width = (AA − XX) mod 4 + 1.

**No wraparound in QUEST**: every WPSH/WPOP has XX ≤ AA (verified over
quest.dis). So width = AA − XX + 1, a contiguous ascending register run;
the emulator's mod-4 wrap path is dead code for this program.

## Counts

82 WPSH/WPOP total = **59 WPSH + 23 WPOP**, in three mutually-exclusive,
exhaustive cases:

| case | count | annotation in quest.wpsh_wpop |
|------|-------|-------------------------------|
| save/restore bracket | 46 (23 WPSH + 23 WPOP) | `// paired with <addr>` |
| argument marshalling | 33 WPSH (25 game, 8 runtime) | `// arg push for LCALL <R>` |
| ref-arg temp construction | 3 WPSH | `// creates a temp on the stack` |

(59 WPSH = 23 paired + 33 arg + 3 temp. 23 WPOP all paired.)

## Case 1 — save/restore brackets (46 = 23 pairs)

The frame-pointer-borrow idiom, uniform across all 23 (verified):
```
WPSH 3,3               ; save AC3
LDAFP 3                ; load frame pointer into AC3
X{W,N}STA / XSTB … [ac3+off]   ; store one register into a frame slot
WPOP 3,3               ; restore AC3
```
- 23/23 contain LDAFP, 0/23 contain a call, span EXACTLY 3 instructions.
- Store opcode varies: XWSTA ×19, XNSTA ×2, XSTB ×2.
- **Always single-register**: 22× `3,3`, 1× `2,2` (all width 1). Multi-
  register WPSH is never a save/restore bracket.
- Emitted when the compiler must write a frame slot but AC3 is live.
  NOT block-move/WCMV related; NOT around calls.

## Case 2 — argument marshalling (33 WPSH)

WPSH's dominant role. One instruction pushes a contiguous register run
of args. Drained by the callee's WRTN (frame teardown), never by a WPOP.
- **25 → game routines** (in quest.argmap; M4b's concern): TERRAIN
  `WPSH 0,2` (3 slots), TERRITORY `WPSH 0,1` (2), RETURN_MESSAGE 2–3.
  The only multi-slot-per-pc arg case — push_map must map one pc to
  several area slots (argmap encodes it as repeated `argN at <pc>`).
- **8 → runtime** (?OPEN_SHARED_IO_FILE ×3, B.MOVE ×2, C.TRANS ×3):
  game→RT, out of M4b scope (stays stock; RT → M5 intrinsics).

## Case 3 — ref-arg temp construction (3 WPSH) — RETURN_MESSAGE only

RETURN_MESSAGE takes args BY REFERENCE. The caller materializes stack
temporaries and passes their addresses:
```
WPSH 0,0 ; LDASP 0     ; reserve a word, addr → AC0  (value in AC is garbage)
WPSH 1,1 ; LDASP 1     ; …
WPSH 2,2 ; LDASP 2     ; …
WPSH 0,2              ; push AC0,AC1,AC2 = the three ADDRESSES (the real args)
LCALL RETURN_MESSAGE,3
```
- Stack depth at the call = 6 wides (3 arg-pointers + 3 referenced temps).
- The temp-create WPSHes are NOT arg pushes (census correctly excludes
  them; only the final WPSH 0,2 is in argmap). Distinguished by the
  following LDASP (address-take), and the pushed AC content being dead.
- **Only RETURN_MESSAGE** does this (verified — all 3 temp-create WPSH
  are its caller). It is the sole by-reference-marshalled call so found.

## Lifecycle: how pushes are reclaimed

- **Returning calls** (TERRAIN/TERRITORY/most): args are NOT popped per
  call. They sit above the caller's wfp and are discarded wholesale when
  the CALLER returns — WRTN/WPOPB resets wsp to wfp (frame-scoped
  cleanup, not call-scoped). This is why arg pushes cannot leak stack,
  and why redirecting them (M4b) is leak-safe.
- **RETURN_MESSAGE** ([[noreturn]], SYSCALL 0310): args/temps never
  popped — process death reclaims the whole stack. See NORETURN.md.

## Arg-window correctness precondition

The arg finder walks backward from the call accumulating pushes; this is
correct ONLY if the push sequence is straight-line within ONE BASIC
BLOCK (no branch target inside the window). The census's CLEAN
classification enforces exactly this (disqualifies any window containing
a branch/skip or an interior branch-target), and the census is 100%
CLEAN — so every arg window is single-basic-block. Register setup
interleaved between pushes (e.g. a WMOV in the 9-arg TERRAIN window) is
fine — stack-neutral, straight-line.

## OPEN ITEM (not yet verified)

We have NOT checked that the **paired (save/restore) brackets are always
within a single basic block** — i.e. that no branch target lands between
a WPSH r,r and its matching WPOP r,r. All 23 are 3-instruction spans
with the fixed WPSH/LDAFP/store/WPOP shape, which makes an interior
branch target unlikely, but it has not been confirmed against the target
set the way the ARG windows were. If M4c (or any pass) relies on these
brackets being atomic straight-line regions, verify basic-block
containment first (same check ArgWindows applies to arg windows: no
interior member of quest.targets).
