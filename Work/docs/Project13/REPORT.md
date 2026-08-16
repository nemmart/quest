# Project 13 — REPORT: M4a widening (the pure list off the stack)

*Solo implementation session, Aug 15 2026. Design of record:
docs/M4aDesign.md incl. §8 (NOT edited). Prompt: docs/Project13/PROMPT.md.
Predecessor: Project 12 (READ_IN migrated, 0 div).*

**Result in one line:** Batch 1 (the hot leaves + slot-patchers) landed
at 0 divergences under the full standing battery, with the `nocall`
tool change and the `gcalls` coverage instrument in place. Batch 2
**STOPPED at boundary 2**: a design-incompleteness in the §8 `T_any`
pointer-form disambiguation — **an area byte pointer and an @-flagged
word address are indistinguishable by their top nibble (`0xF0…`)**, and
§8 orders the @-word reading before the byte reading, so any migrated
routine that escapes a byte pointer into its own frame to a mediated
syscall diverges. Reproduced on LOGON (register ac2 at ?WRITE_SCREEN)
and, independently, on GET_INPUT (a ?READ packet field). Evidence,
root cause, and candidate rulings below. **M4aDesign.md not edited; the
user rules.**

---

## 1. Stage 0 — tool + instrument (delivered, green)

### 1a. `nocall` flag (M4aDesign §8)

`tools/build_address_book.py` now sets `nocall` on any entry with **no
LCALL/XCALL static caller** (`e['sites']` empty) and the wave-one filter
excludes it (`wave_one = pure and not nocall`). The book prints `nocall`
where it previously printed `nosites`; the address-book *loader* never
read that column, so the flip is inert to behavior. Regenerated book
(READ_IN live) is byte-identical to P12's except the flag word. Counts:

```
pure 95 of 130; nocall 29; wave-one 78; total 38256 words, 38 pages
```

The 29 `nocall` are QUEST (boot), C_A_LISTENER (?CREATE_TASK entry),
TERRAIN_HELP, and 26 ON-unit / task-entry nested bodies — the set
§8 predicted, confirmed by the tool. Baseline re-proof (READ_IN-live
book, `m` driver): 0 div, 8 hijack lines, I.STOP detach — matches P12's
`readin_m`.

### 1b. `gcalls` — the coverage instrument (user ruling, this session)

New permanent trace type in `os/Trace.cpp`; one row in `docs/Run.md`.
Emitted at the LCALL/XCALL **dispatch site in BOTH roles**
(`hw/EagleStack.cpp trace_gcall`), targets in the game range
[QUEST, ?CHAR_TO_UNSIGNED) only. It arms/breaks no batch, touches no
count or pairing state — a pure observer at the site.

The invariant it enforces (`docs/Project13/coverage.py`): **for every
LIVE book routine, the clone's `gcalls` count == its `hijack` WSAVS
count**; a hijack on a stacked routine, or a master/clone `gcalls`
skew, is also flagged. It held on every run in this session (including
the red ones — the checker caught the divergence, not a coverage
mismatch). This is now the routine-coverage tool of record; a routine
with 0 `gcalls` in a run is reported UNEXERCISED for that run.

### 1c. `play` driver (grown, not forked — user ruling)

`docs/Project13/drive.py` gained a `play` mode: login → one 3-turn
AUTO_MOVE north (drives the movement/render leaves) → O / D / L menu
screens → ESC. It is the coverage run and the baseline the next
session **extends in place** (add steps; do not fork). Reached set is
reported explicitly per run (below), so the next session and the user's
live play know exactly what is already covered.

---

## 2. Batch 1 — LANDED at 0 divergences

**Routines (6 live):** READ_IN (carried from P12), **OWNS, FIND_OBJECT,
RANDOM, DIST, DISTANCE_TO_PLAYER**. Five of the six are slot-patchers
(`XWSTA/XNSTA [ac3+0x7FF8/9]`) — the §4 first-class validation target.

Standing battery, all at **0 divergences, cross-check OK**:

| run | divs | detach/abort | coverage note |
|---|---|---|---|
| `m` (login+M+ESC) | 0 | I.STOP detach + write-back | DIST 600, OWNS 1, READ_IN 4 |
| `fo` (FAIL_OPEN L→P) | 0 | handled signal | DIST 900, OWNS 1, READ_IN 3 |
| `play` (auto-move tour) | 0 | socket close | **all 6 LIVE**: FIND_OBJECT 3705, DIST_TO_PLAYER 3718, RANDOM 1116, DIST 1500, OWNS 9, READ_IN 4 |
| `inj` (raise in a live FIND_OBJECT frame) | 0 | ?FATAL detach | FIND_OBJECT hijack=1 (frame live at the raise) |
| `abort` (`:ABORT` at a live DIST body pc) | 0 | WORLD ABORT, save suppressed | DIST hijack=1; banner verified both engines |

Per-routine cross-check `gcalls == hijack` held in every run. Every
batch-1 routine is **LIVE-VALIDATED** (fired > 0 with the invariant
holding).

Two observations (no design impact, recorded per METHOD §11):

- **`inj`-at-FIND_OBJECT terminates via ?FATAL detach, not a WRTN
  unwind.** A terminal path abandons the live area frame at the final
  verified pair rather than returning through it — correct by METHOD
  §13 (terminal subtrees only have to terminate); 0 div to the pair.
  The clean *non-terminal* unwind across an area frame (I.GOTO through
  it) was already proven by P12's `readin_inj`; batch-1 routines have
  no ON-units of their own, so I did not manufacture a second one.
- **`m`/`fo` never reach FIND_OBJECT / DIST_TO_PLAYER / RANDOM** — those
  need movement; only `play` covers them. This is exactly why the
  battery needs `play` and why per-routine coverage (not a global
  "0 div") is the validation unit.

Evidence: `docs/Project13/evidence/{hijack,gcalls}_b1_*.log`,
`summary.txt`.

---

## 3. Batch 2 — STOP AND REPORT (boundary 2): the `0xF0` pointer-form collision

### 3.1 What happened

Book = batch 1 + the 39 leaf named routines (ALLY_PLAYER, AUTO_MOVE,
BACKPACK, … LOGON, GET_INPUT, … UPDATE_SCREENS, WRITE_OBJECT; 45 live).
**All three of `m`, `fo`, `play` diverged at 1**, identically and early
(during login, before movement): only READ_IN + LOGON had hijacked
before the stop.

Divergence (b2_m, at ?WRITE_SCREEN entry, LOGON's area frame live):

```
master: result_pc=7017E27A ... ac2=E0002224 ac3=70175F6B wfp=700010E8
clone : result_pc=7017E27A ... ac2=F000D6EC ac3=70175F6B wfp=78006B4C
 backtrace: ?WRITE_SCREEN ← LOGON+0xC8 [70175F67] ← QUEST
```

### 3.2 Bisection

Commenting LOGON out did **not** clear it — the batch went red again,
now with GET_INPUT live at the stop, at a **?READ packet field**:

```
mediated-call input mismatch (width 4, byte range E0002384..E0002387):
  master value = E00022AC
  clone  value = F0007664
 backtrace: GET_INPUT+0x25 [7016AA5A] ← LOGON ← QUEST
```

So it is **not a single guilty routine** — it is a class. The bisect
did its job (named LOGON, then GET_INPUT), but both are instances of
one design defect, so the batch does not "land minus one routine";
the batch stops.

### 3.3 Root cause — an ambiguity §8's `T_any` cannot resolve as ordered

Byte addresses on this machine are `word << 1`. The area base is
`0x78000000`, so **an area byte pointer is `0x7800…<<1 = 0xF000…`**.
The @-indirection flag (bit 31) over a real-stack word address is ALSO
`0xF000…` (`0x80000000 | 0x70…`). **The two forms share the top
nibble.** M4aDesign §8's `T_any` tries the readings in this order:

1. word address,
2. bit-31-stripped word (`0xF0…` → `0x70…`),
3. byte address (`v >> 1`).

For an area byte pointer `v` (top nibble `0xF0`), reading (2) fires
first: `v & 0x7FFFFFFF` lands on a **real-stack-looking word** above the
live frame's `W`, so `T` shifts it and re-flags bit 31 — a wrong,
non-identity translation — and the correct byte reading (3) never runs.

Worked example (LOGON, `XLEFB 2,[ac3+0x46]` escaping a frame byte
pointer that `WCMV` fills and `?WRITE_SCREEN` consumes):

```
clone ac2      = 0xF000D6EC
  reading (2): 0xF000D6EC & 0x7FFFFFFF = 0x7000D6EC  (> W: shifted → WRONG, fires first)
  reading (3): 0xF000D6EC >> 1        = 0x78006B76  (area word, would be RIGHT)
T(0x78006B76) = master_wfp 0x700010E8 + (0x78006B76 − area_wfp 0x78006B4C)
              = 0x70001112
reflag as byte: (0x70001112 << 1) | 0xE0000000 = 0xE0002224 = master ac2  ✓ (exact)
```

Reading (3) reconstructs the master's value **exactly**; reading (2)
misfires before it. This is a design-ordering / ambiguity gap, not an
off-by-one — hence boundary 2, not boundary 3.

### 3.4 Why P12 (READ_IN) did not hit it

READ_IN's ?READ packet carried a **real-stack** byte pointer (`0xE0…`,
distinct from the `0xF0…` @-flag — no collision) and a real-stack
@-word (`0xF0…`, which reading (2) is *meant* for). No READ_IN field
was an *area* byte pointer, so the ambiguous form never appeared. It is
not READ_IN-specific luck about the routine — it is about which pointer
FORMS a routine escapes. Byte pointers into a routine's own frame
(?READ/?WRITE buffers, `XLEFB [ac3+d]`, `WCMV` destinations) are common,
so the form appears as soon as the migrated set includes an
input/output routine — i.e. immediately in wave one beyond READ_IN.

### 3.5 Candidate rulings (for the user; I did not implement any)

The disambiguation must become **context-aware or unambiguous**, because
prefix alone cannot separate the two `0xF0…` forms. Options, roughly in
order of how surgical they are:

- **R1 — reorder to byte-before-@word, guarded by the area range.**
  Try (3) the byte reading *when `v>>1` lands in the live area range*
  before (2) the @-word reading; keep (2) first otherwise. Rationale:
  an area byte pointer's `>>1` is unambiguously in `0x78…`, whereas a
  real-stack @-word's `>>1` is in `0x38…` (never an area address). This
  is the minimal change and the worked example above shows it pairs
  both P12's real-stack @-word AND this session's area byte pointer.
  Risk: a real-stack byte pointer whose `>>1` *coincidentally* lands in
  the area range — impossible while nothing but areas live at `0x78…`
  (the census invariant), but worth stating as the precondition.
- **R2 — disambiguate by the low bit / known packet layout.** At
  mediation the packet layout is known field-by-field (the mediator
  already reads by name); byte vs word vs @-word is a per-field
  property, not a guess. Carry the expected form per field and apply
  the matching reading. Narrower than R1 (mediation only) but leaves
  the register case (LOGON's ac2 at the crossing) unaddressed, so it is
  **not sufficient alone** — the LOGON divergence is a register compare,
  not a packet field.
- **R3 — make the two forms non-overlapping by construction.** Move the
  area base so area byte pointers do not collide with the `0xF0…`
  @-flag band — i.e. pick a base whose `<<1` avoids `0xF0000000`. This
  reopens the §1 base ruling (0x78000000 was chosen against the
  copy_segment / ring-7 constraints) and touches the book format, so it
  is the heaviest option and probably out of scope for a §8 amendment.

My reading: **R1 is the design-level fix** (it restores the invariant
"every clone value pairs under some reading of T" and is testable), with
R2 as a complementary strictness at mediation if the user wants
form-checking rather than form-guessing. But this is the user's ruling
to make; §8 is the design of record and I have not touched it.

### 3.6 State left behind

- `c_src/quest.addrbook` restored to the **batch-1-live** book (6
  routines, all green) so the tree is in a known-good, landable state.
- The batch-2 book that triggered the finding is reproducible:
  `build_address_book.py … --live <b1>,<39 names>`; the exact list is
  §7 of the prompt / the addrbook wave-one list.
- Evidence: `docs/Project13/evidence/div_b2_*.txt` (both divergence
  signatures), `hijack_b2_*.log`.

---

## 4. Files changed (file:function)

- `tools/build_address_book.py`: `nocall` flag; `wave_one` filter;
  report sections (wave-one list, pure-but-nocall list).
- `os/Trace.cpp`: `gcalls` added to `known_types`.
- `hw/EagleStack.cpp`: `trace_gcall()` helper; call at LCALL and XCALL
  dispatch sites (both roles); `#include ../debug/SymbolTable.hpp`.
- `docs/Run.md`: `hijack` and `gcalls` rows in the trace-type table.
- `docs/Project13/`: `run.sh` (gcalls in `-types`; coverage.py hook),
  `coverage.py` (new), `drive.py` (`play` mode), `explore.{py,sh}`
  (reconnaissance harness), `addrbook_report.md`, `evidence/`.
- `c_src/quest.addrbook`: batch-1-live (final state of this session).
- **M4aDesign.md: NOT edited** (boundary 2).

## 5. Not done / for the next session

- **The §3 boundary-2 finding is the gate.** Batch 2 cannot land until
  the user rules on `T_any` (R1/R2/R3 or other). Once ruled, re-run the
  batch-2 book under the full battery; the two saved divergence
  signatures are the regression targets (they must pair).
- Batches 3 (parents + callable children) and 4 (stragglers/menus)
  remain; both will exercise more mediated pointers and should be
  re-checked against the ruling.
- The `play` driver reaches the movement leaves and O/D/L menus; it
  does NOT yet reach STORE, HELP, OP_* screens, combat, or the map `?`
  screen — grow it (in place) for batch-4 coverage.
- Roll-call (LIVE-VALIDATED / LIVE-UNEXERCISED / EXCLUDED) is deferred
  to the landing, after batches 2–4 complete.

---

## 6. Stage 0b (continuation, Aug 15) — base 0x74000000 + prefix-dispatch `T_any` + rename

*Implements M4aDesign §9 rulings. Solo; user present at the gates.*

### 6.1 Changes (file:function)

- `hw/AddressBook.hpp` BASE, `tools/build_address_book.py` BASE,
  `hw/Memory.cpp` census marker → **0x74000000**. Book FORMAT unchanged;
  both books regenerated (`c_src/quest.addrbook` = batch-1-live, 6;
  `evidence/quest.addrbook.batch2` = batch-1 + 39 leaves, 45).
- `hw/Machine.cpp T_any`: **prefix DISPATCH on the top byte** — 0x70/0x74
  → `T(v)`; 0xE0/0xE8 → `T(v>>1)<<1 | bit0`; 0xF0/0xF4 → `T(v & 0x7FFFFFFF)
  | 0x80000000`; anything else identity. No ordered guessing. `T`, `T_inv`,
  `shadow_wsp` untouched.
- Rename hijack→redirect: `Machine::area_redirect_enabled`, trace type
  `redirect` (Trace.cpp, EagleStack.cpp, Machine.cpp), comments
  (AddressBook.hpp, frames.cpp, tool), `Run.md` rows, `run.sh`,
  `coverage.py`, `explore.sh`. Old evidence logs keep their `hijack_*`
  names. Warning-free build.
- `coverage.py`: the "master" gcalls column was counting QUEST_SERVER's
  own game-range LCALLs (same binary/symbols) as master calls — those
  rows showed as spurious "master != clone" notes. Now only QUEST1
  counts as master. Cosmetic; no invariant affected.

### 6.2 Batch-1 battery re-proof at the new base — GREEN, identical

| run | divs | redirect lines | coverage vs landed b1 | end |
|---|---|---|---|---|
| `m` | 0 | 1210 | DIST 600, OWNS 1, READ_IN 4 — identical | I.STOP detach 7017FCE8 |
| `fo` | 0 | 1808 | DIST 900, OWNS 1, READ_IN 3 — identical | handled signal |
| `play` | 0 | 20501 | all 6 LIVE (D_T_P 3841, F_O 3775, DIST 1500, RANDOM 1122, OWNS 9, READ_IN 4); b1 had 3718/3705/1500/1116 — run-to-run RNG/timing, invariant OK | socket close |
| `inj` (QUEST_INJECT=7016A896, FIND_OBJECT body) | 0 | 1217 | F_O 1, DIST 600, OWNS 4, READ_IN 4 — identical | ?FATAL detach 7017F036 |
| `abort` (QUEST_TERMINAL=7016871D:ABORT, DIST body) | 0 | 7 | READ_IN 3, DIST 1 — identical | WORLD ABORT both engines |

Evidence: `evidence/{redirect,gcalls}_s0b_*.log`, `evidence/summary_s0b.txt`.

Setup note (mine, recorded): I first symlinked the scratch QUEST/ to the
source tree; the `m` write-back polluted it and the first `fo` attempt
logged in as an existing character ("Initials already in use"). Restored
QUEST/ from the tarball as a real directory; every result above is from
the pristine tree. Scratch-copy means COPY.

### 6.3 Batch-2 book, `m` driver — the two §3 signatures PAIR; a NEW stop

Both §9 regression targets pass: LOGON's ac2 at ?WRITE_SCREEN
(previously `0xF000D6EC`) and GET_INPUT's ?READ packet field now pair —
LOGON, GET_INPUT, INIT_SCREEN, HIT_ANY_CHAR all redirect and return
(redirect_b2r_m.log; GET_INPUT WSAVS/WRTN at seq 267/275, LOGON WRTN at
938). The run then diverges at 1, later, in **HIT_ANY_CHAR**:

```
master: pc=7017E27A (?WRITE_SCREEN) ac2=E0003738  wfp=70001B88 wsp=70001BA0
clone : pc=7017E27A                 ac2=E800A580  wfp=740052AC  shadow_wsp=70001BA0 ✓
 backtrace: ?WRITE_SCREEN ← HIT_ANY_CHAR+0x12 ← GET_QUEST+0x757 ← QUEST
```

Decode: `E800A580 >> 1 = 740052C0 = area_wfp + 20`; master `E0003738 >>
1 = 70001B9C = master_wfp + 20`. Same offset — T's *intent* pairs it. But
HIT_ANY_CHAR's block is `[740052A0, 740052C0)` (argc 0, frame 9: 12+18 =
30 → rounded to 32), so `740052C0` is **exactly one past the end of the
block**, T's `in live record` test misses, T returns identity, mismatch.

Where the value comes from (quest.dis 7016DE91):

```
WSAVS 9; NLDAI 30,0; WMOV 0,1; XLEFB 2,[ac3+0xA]; XLEFB 3,[pc+..]; WCMV
LDAFP 3; XPEF [ac3+4]; LPEF [..]; LCALL ?WRITE_SCREEN,2
```

`WCMV` copies 30 bytes into the frame at byte offset 0xA (words wfp+5..
wfp+19 — inside the frame) and leaves ac2 advanced to **one past the last
byte written = word wfp+20**. ac2 is dead residue at the ?WRITE_SCREEN
crossing (the call takes its args from the stack), but the checker's
register rule compares all four ACs, so the residue must pair.

**Design-vs-reality (boundary 2):** M4aDesign §5's `T` maps a value that
"falls in a live record"; a legitimate **one-past-the-end pointer** into
a frame whose block is an exact multiple of 16 falls in the NEXT block
(here INIT_OBJ_TBL's, commented) — or, if that neighbour were live, would
translate to the WRONG master frame. This is not an implementation
off-by-one: the block boundary IS where the design puts it. Candidate
rulings:

Candidates considered: guard slack in the layout only (R1); T lookup
end-inclusive gated on the neighbour (R2); excluding residue registers
from the compare (R3). **Ruling (managing session): both halves —
end-inclusive attribution in T (a one-past-end pointer belongs to the
frame it walked off) plus a layout guarantee that makes it unambiguous.**
Slack alone would translate the end pointer via its own block's pad —
right answer, wrong reason; the T rule states why it's right.

Implementation note: the layout half as first drafted ("exact multiples
of 16 round up a further 16") is insufficient — sizes are rounded to 16
and bases were sequential, so `base+size == next_base` ALWAYS, and the
HIT_ANY_CHAR residue sits at base+ROUNDED size (base+32), not
base+used(30). The invariant `block_end < next_base` is honored in the
STRIDE instead: bases advance by `size+16`. Book cost 38→40 pages.

Landed as:
- `hw/Machine.cpp T`: `u <= alloc_base+size` (end-INCLUSIVE), comment
  citing the invariant.
- `tools/build_address_book.py`: stride = size+16; header comments in
  the book and `AddressBook.hpp` updated. Both books regenerated.

Scope note (per ruling): pointers MORE than one-past-end (arbitrary
strides) remain untranslatable by design; if one ever shows up, that is
a new boundary-2, not a bigger pad. M4aDesign.md not edited.

Evidence: `evidence/div_b2r_m.txt`, `evidence/redirect_b2r_m.log`,
`evidence/gcalls_b2r_m.log`.

### 6.4 Observation (no action): `T`'s real-stack shift is unbounded upward

`T` shifts every non-area value `> W` — including code addresses (e.g.
ac3 = 0x70175F6B in every divergence dump shows `T(ac3)` = +0x5A). It is
harmless today only because every consumer accepts raw-equal OR T-equal
and only shifted-form values ever reach the T-equal leg. Prefix dispatch
now makes the FORM deterministic; bounding the shift to the stack segment
(≤ wsl) would make the RANGE deterministic too. Recorded for a future
ruling; out of scope here.

### 6.5 Second boundary-2: @-flagged word addresses on the INVERSE path

With the end-inclusive ruling in, the batch-2 `m` run advanced past
HIT_ANY_CHAR and stopped at a mediated-call INPUT mismatch in
REFRESH_SCREEN's 5-arg ?WRITE_SCREEN → ?WRITE (master value 0x0200,
clone 0x0000, width 2, byte range E0003808..09). Post-mortem word dumps
added to the divergence report showed BOTH engines holding 0x0200 at
the correct cells — and the new "clone word addr USED at read" line
showed why the verify still failed:

```
clone word addr USED at read = F0001C04
```

The mediated handler dereferences a packet pointer with **bit 31 (the
@-flag, 0xF0 form) still set**. The memory layer masks bit 31, so the
master's read lands correctly and pre-M4a the identity mapping was
harmless. But `T_inv` received the flagged value, matched no region
(negative as int32), returned identity — and the clone-side verify read
the clone's UNSHIFTED real stack (stale zeros) instead of the shifted
packet cell.

Fix — the inverse of T_any's already-ruled 0xF0/0xF4 case (§9 form
taxonomy applied to the inverse path):
- `os/OSContext.cpp clone_word_address`: if bit 31 set → mask,
  `T_inv`, re-encode the form.
- `os/LockstepMediator.cpp` write replay: same guard on word-width
  replayed addresses (byte-width path already decomposes correctly).

Implemented as a direct consequence of the §9 ruling rather than a new
design question; flagged for the managing session if it should be
recorded as a formal ruling. Evidence: `evidence/div_b2r_m4_atflag.txt`
(the caught signature with dumps), `evidence/div_b2r_m2_onepast.txt`
(the §6.3 one-past-end signature).

Diagnostics kept (post-mortem only, print at divergence abort): the
failing clone read address (`last_clone_read_addr`) and ±8-word dumps
of both engines around the failing cell — they are what isolated this
bug in one run.

### 6.6 Batch-2 battery — GREEN (45-live book), and batch-1 re-proof

Batch-2 book (`evidence/quest.addrbook.batch2`, 45 live), full battery:

| run | divs | redirect lines | notes |
|---|---|---|---|
| `m` | 0 | 1260 | 20 live routines redirected, all gcalls==redirect; I.STOP detach 7017FCE8 |
| `fo` | 0 | 1863 | cross-check OK |
| `play` | 0 | **31018** | **26 of 45 live exercised** — UPDATE_SCREENS 4868, TERRITORY 4529, DISTANCE_TO_PLAYER 2561, FIND_OBJECT 2534, DIST 900, RANDOM 79, + 20 more; every routine paired |
| `inj` (FIND_OBJECT body) | 0 | 1269 | ?FATAL detach 7017F036, same shape as batch 1 |
| `abort` (DIST body) | 0 | 19 | WORLD ABORT verified both engines, same banner wides |

19 live routines unreached by the current scripts (ALLY_PLAYER,
BACKPACK, CASTLE_INVENTORY, CLONE_SUNDAR, DISPLAY_FLASK,
GET_OBJECT_INDEX, LOOK, MOVE_IN_CAVE, PICK_X_Y, PLACE_PLAYER,
REPOSITION, RETURN_MESSAGE, SEIGE, SPYGLASS, TAKE_OVER_CASTLE,
TRANSPORT_SUNDAR, TRANSPORT_TERRAK, UNLOCK_FILE, WRITE_OBJECT) — deeper
game actions; they are LIVE and armed, so any future session that
reaches them gets verified redirects for free.

Batch-1 book re-proof after the T/T_inv/stride changes (same emulator,
default book): `m` 0/1210 (DIST 600, READ_IN 4, OWNS 1 — identical to
landed b1), `fo` 0/1808 (identical), `play` 0/12303 all 6 live reached,
`inj` 0/1233 ?FATAL detach, `abort` 0/7 WORLD ABORT verified.

Evidence: `evidence/{redirect,gcalls}_{b2r_*,b1r_*}.log`,
`evidence/summary_b2.txt`.

### 6.7 Batch-2 status: LANDED as the default book

Both books at base 0x74000000, stride size+16. Batch 2 (45 live)
passed the full regression battery; batch-1 book re-proven unchanged.
Remaining for batch 3: the rest of the pure wave-one list (78 total;
45 now live), plus scripted reach for the 19 unexercised live routines.
