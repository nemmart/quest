# Finding B — characterization of the fail-open −14-wide wsl motion

*Deliverable for docs/Project14/FINDING_B_INVESTIGATION.md. Analysis
only: no Mapper change, no invariant relaxed, no handler edit. The
ruling belongs to the planning session; §5 is a recommendation.*

**Verdict up front: outcome 1 — the wsl motion is legitimate,
disassembly-attested real-machine behaviour. It is not a signal *frame*,
and it is not a native accounting slip. It is a heap allocation: the
error-message buffer for the fail-open signal, carved from the top of
the stack segment by I?ALLOC, whose real code lowers wsl (`STASL` at
0x7017E903) as the stack/heap fence moves. The 14 wides are the exact
class-3 block size for the 15-byte message ":USER_DATA_FILE". I2 as
stated ("wsl constant while records live") is too strict against the
machine's actual semantics.**

## 1. The identified wsl write

- **Real machine:** `STASL 2` at **0x7017E903**, inside I?ALLOC
  (entry 0x7017E866, quest-rt.dis):
  ```
  7017e8fe LDASL 2        ; wsl
  7017e8ff WSUB 0,2       ; wsl - size
  7017e900 LDASP 1
  7017e901 WSGT 2,1       ; collision check vs wsp
  7017e903 STASL 2        ; wsl := wsl - size   <-- the write
  ```
- **Native mirror:** `c_src/runtime/i_alloc.cpp:191`
  `machine.wsl = machine.wsl - size;  // STASL 0x7017E903` — executed
  on the fo path as ?LIB_ERROR's inner allocation
  (`c_src/runtime/lib_error.cpp:355`, `emu_rt::i_alloc` at staged
  state). If ?LIB_ERROR instead falls back whole, the emulated `STASL`
  (hw/EagleStack.cpp:404 `machine.wsl=machine.ac[AA]`) performs the
  identical write. Both engines run it; hence 0 divergences.
- The restore side is I?FREE's `wsl += size` (0x7017E9D2-D5 /
  i_alloc.cpp:271). On this runtime the heap grows *down* from the top
  of the wide-stack segment and **wsl is the stack/heap fence**
  (docs/I_ALLOC.md): every live-path alloc/free moves wsl and the heap
  break (0x700001F0) in lockstep, by the same size.

**Why the abort pc is 7017EC7C and not the write pc:** I2 is only
checked at Mapper touchpoints (`push_record` / `wrtn_fixup` /
`unwind_to`, Mapper.cpp:345/412/439). The wsl write happens inside
?LIB_ERROR's dispatch batch, where the Mapper is never consulted. The
first touchpoint after the write is the game ON-handler's non-local
goto: `LJSR I.GOTO` (game pc 0x7016EC71) → native `i_goto`
(frames.cpp:237, entry pc **0x7017EC7C**) → `Mapper::unwind_to` →
`i2_assert` → abort. 7017EC7C is the *detection* site, ~one dispatch
after the *write* site.

## 2. The full fail-open chain (each hop verified in the trees)

1. Game (turn loop, near 0x7016EC83) calls **?OPEN_FILE**
   (0x7017DD27, WSAVS 0x10) on ":USER_DATA_FILE".
2. The wrapper issues `SYSCALL 0300` (?OPEN, 0x7017DD9B).
   `OSContextFS::OPEN_call` sees `QUEST_FAIL_OPEN`, prints
   `FAIL_OPEN: … denied :USER_DATA_FILE`, returns FS_FILE_NOT_FOUND
   (os/OSContextFS.cpp:74-78).
3. Wrapper error branch (0x7017DDAA-DDAE): pushes **@[ac3+0xFFF2]
   (the caller's filename string, by-ref — this is the message)** and
   [ac3+0x12] (the error code), `LCALL ?LIB_ERROR,2`.
4. **?LIB_ERROR** (0x7017E33A, native lib_error.cpp): argc>1 → reads
   `len` through the message ref, frees the previous buffer if any
   (first fo signal: `oldbuf==0`, no free), then runs the inner
   **I?ALLOC(request, class 3)** → **wsl −= 14** → stores the buffer
   pointer in the task-area slot B+0x3 and copies the counted string
   into it.
5. ?LIB_ERROR dispatches the handler (DEFAULT_EH 0x7017E3D2 → O?SIGNAL
   0x7017EDED → ON-chain search) which finds the game's file-open ON
   handler (established at 0x7016EC53, body at 0x7016EC57).
6. The handler prints via ?WRITE_SCREEN and performs the recovery
   non-local goto: ac2 = label 0x7016F1C4 (a HIT_ANY_CHAR resume),
   `LJSR I.GOTO` → **i2_assert fires at pc 7017EC7C**:
   latched 7001715A, now 7001714C.

## 3. The 14-wide account (exact, no residue unexplained)

The message is the filename as a counted string: **":USER_DATA_FILE" =
15 bytes**, length word first.

- ?LIB_ERROR: `request = ((len+1) >> 1) + 1 = ((15+1)>>1)+1 = 9` wides
  (1 length word + 8 words of text) — lib_error.cpp:293.
- I?ALLOC class-3 rounding (`rt::heap_class_size`, mirroring
  0x7017E88A-91): `n = ((9+1)>>1)<<1 + 4 = 10 + 4 = 14`, min-8 check
  passes. **size = 14 wides**: 10 payload (request rounded to even) +
  4 overhead (the block sits at `newbreak+4`; sentinel at newbreak,
  `-size` header at block−2, `-size` trailer at block+size−4).
- `wsl: 7001715A − 0xE = 7001714C`. Matches the abort byte-for-byte,
  and matches its record-agnosticism: the motion belongs to the signal
  path, so which game record happens to be live is irrelevant.

The size is a pure function of the *message length*, not of any handler
frame: a fail-open on a different filename length would move wsl by a
different amount (e.g. a 19-byte name → 16 wides). "14" is not a frame
constant anywhere in the L2 code — the brief's WMSP/handler-frame/
re-latch candidates are all absent from this path; the only wsl writer
reachable from the fo chain is I?ALLOC (the def_on/p_defon/lib_error
wsl mentions are read-only headroom gates).

**Persistence:** the buffer is deliberately retained (B+0x3) so ?ERMSG
can read it later; it is freed only at the *next* ?LIB_ERROR (old-buffer
free, +14, before the next alloc). So wsl stays at 7001714C
indefinitely — I2 cannot be saved by any transient-window argument.

## 4. Which outcome

**Outcome 1 — legitimate.** The real MV/8000 binary demonstrably
executes this exact wsl write on this exact path (`STASL` 0x7017E903);
the native handler reproduces it faithfully (and validated: I_ALLOC.md,
footprint diff 0). There is no accounting slip to fix in the handler —
"fixing" it would make the emulation *wrong*: the 14 words between new
and old wsl now genuinely hold heap data (the message buffer), and wsl
is the machine's own record of that fence.

One refinement to the brief's framing: this is not a "handler stack
frame" adjustment. wsl on this runtime is dual-purpose — stack limit
*and* heap fence — and the fo path moves it in its heap-fence role.

## 5. Recommended ruling (for the planning session)

I2's *intent* — the stack leg's domain bound may not silently shift
under live records — is right and worth keeping. Its *implementation*
("wsl constant while records live") over-approximates that intent,
because wsl legitimately moves at I?ALLOC/I?FREE commit points.
Recommended refinement, strictness preserved:

1. **Latch `wsl − heap_break` instead of wsl.** On every live path,
   I?ALLOC and I?FREE move wsl and the heap break (0x700001F0) by the
   same size in the same direction, so this difference is a genuine
   invariant of legitimate heap motion — it stays constant across the
   fo path, and would still catch every slip I2 was written for (a wsl
   write with no matching heap-break motion, e.g. a botched handler
   re-latch). Cheap: `i2_assert` reads one extra wide.
2. **Plus a stack-clearance bound at each i2_assert:** current
   `wsl > max(live wsp, every record's master-side extent hi)` — the
   reclassified band `[new_wsl, old_wsl)` must lie strictly above all
   stack-leg activity, so the leg's identity/compression split stays
   well-defined. On the fo runs this holds by a wide margin (wsp
   ~700010F0, extents ~70001500 vs wsl 7001714C). Note the Finding-A
   family is the case that can crowd this bound (DISPLAY_SCREEN's
   shadow_wsp 70001FD0 > wsl) — one more reason A's exclusion ruling
   and this refinement should land together.

Alternative (brief's option "re-latch across the handler window") is
weaker: the motion is *not* confined to a window (§3 persistence), so a
window re-latch would have to become "re-latch on any alloc/free",
which is (1) without the cross-check. Prefer (1)+(2).

**Explicitly not proposed:** tolerating arbitrary wsl deltas, or
commenting routines (proven useless — the motion is record-agnostic).

After the ruling lands in the Mapper, fo should re-run on the
101-book expecting green with the same 0-div profile; no handler or
book change is needed.

## 6. Evidence pointers

- Abort dumps: docs/Project14/evidence/b2/finding_B_failopen_I2.txt
  (+ runs/b2_fo, b2_fo2 tails).
- Real wsl write: Disassembled/quest-rt.dis @ 0x7017E8FE-E903.
- Native write: c_src/runtime/i_alloc.cpp:191; inner call
  c_src/runtime/lib_error.cpp:344-357; size math i_alloc.cpp:42-69
  (heap_class_size) + lib_error.cpp:292-303.
- fo entry: c_src/os/OSContextFS.cpp:74-78; wrapper ?OPEN_FILE
  0x7017DD27 (error branch 0x7017DDAA-DDAE, quest-rt.dis).
- ON handler + goto: quest.dis 0x7016EC53-EC71, label 0x7016F1C4.
- Detection: c_src/runtime/frames.cpp:237 (i_goto, entry 0x7017EC7C) →
  c_src/hw/Mapper.cpp:439 (unwind_to) → :328 (i2_assert); latch site
  :343.
