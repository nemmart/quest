# I.ALLOC / I.FREE — Translation Derivation

Status: **COMPLETE — translated and validated (Aug 2026).** Derivation,
residue maps, and captures below; translation in `runtime/i_alloc.cpp`
(native fast paths for the two live paths, fallback-span protocol for the
rest). Validated: footprint diffs 0 differing words, four consecutive
clean lockstep sessions via the ?CREATE_TASK startup path. The heap is
CLOSED as a work area — see M3Plan.md's heap note (Quest barely
allocates; free-list paths structurally dead; QSEARCH now a hard
terminal).

Step 1 of `docs/HeapSignalPlan.md`. Translated as one chunk: allocate
and free share the heap-break words, the free-list descriptor, the
locking wrappers, and the block-header layout.

**DONE so far: I.LOCK / I.UNLOCK** (`runtime/i_lock.{hpp,cpp}`,
validated, footprint diff 0 words — see SessionPlan.md). The remaining
work is I.ALLOC, the I.FREE family, the coalescer, and the free-list
insert.

**Validation path (from Play Session 5): `?CREATE_TASK`**, not
`?LIB_ERROR`. The live allocator call sites are 0x7017DBD1 /
0x7017DBFF (I.ALLOC) and 0x7017DC3D (I.FREEW), all inside
`?CREATE_TASK`, which runs once at startup in **every** session. No
fault injection needed. `?LIB_ERROR`'s allocator use is error-path-only
and is the second validation source.

## Heap globals (all in the `I.HEAP` area at 0x700001F0)

Statically initialised to `FFFF...` in the program image, so the free
list starts as a well-formed empty queue with no code required.

| Address | Meaning |
|---|---|
| 0x700001F0 | heap break / current top |
| 0x700001F2 | direction or limit sign (`WSGE`/`WSGT` tested throughout) |
| 0x700001F4 | **free-list queue descriptor** (head, tail) |
| 0x700001FA | deferred-free chain head (walked by 0x7017EA09) |
| 0x700001FC / 0x700001FE | heap low / high water marks (0x7017E92D) |
| 0x70000200 | heap lock object (passed to I.LOCK / I.UNLOCK) |
| 0x70000204 | stack-base comparand for the ownership check (0x7017E970) |

## Block layout

User data starts at the returned address; the **size word lives below
it** — Table 6-2 of the queue chapter ("user information preceding
links"), except here the size precedes the *block*:

- `[block-2]` — own size, **negated** (header writer 0x7017E920)
- `[block-4]` — the **PRECEDING block's trailing size word** (see below)
- `[block+0]`, `[block+2]` — the queue links, valid only while the block
  is **free** and enqueued.

**Trailing size word (derived Aug 2026 from the header writer):**
0x7017E91D-22 stores the negated size at `[block-2]` AND at
`[block+size-4]` — i.e. each block ends with a copy of its own size,
which is the NEXT block's `[base-4]`. That is what the coalescer reads
at 0x7017E97F (`XWLDA 1,[ac2+0x7FFC]`) to find its *predecessor*
without walking the heap. So the 4-word header = own leading size wide
+ the neighbour's trailing size wide sharing the gap.

`I?ASIZE` (0x7017E811) exposes the size: reads `[ac2-2]`, negates,
subtracts 4 — the stored size is negative and includes the 4-word
header.

**How the result reaches the caller (derived Aug 2026):** I.ALLOC never
touches its own saved-ac0 slot directly. The success paths load
`LDAFP 0` (ac0 := I.ALLOC's frame pointer) and XJSR the header writer
(0x7017E91B) with it; the header writer moves it to ac3 (0x7017E924)
and stores the block pointer **through it into the OUTER frame's
saved-ac0 slot** (`XWSTA 2,[ac3+0x7FF8]`, 0x7017E92A) — for class 2
first shifting the pointer left 1 = byte pointer (0x7017E929; skipped
for every other
class via `WSNE 1,1` on class−2). I.ALLOC's own WRTN then restores the
patched value. Saved-ac1/ac2 are never patched, so callers see their
entry ac1/ac2 back — which is why ?CREATE_TASK's second call site can
rely on ac1 still being 3.

## Entry points and control flow

### I.ALLOC (0x7017E866)

```
7017e866 WSAVS 0x0000
7017e868 XPSHJ 0x7017E871        ; size computation (skip-return)
7017e86a WBR   2  -> 0x7017E86C  ; failure path
7017e86b WRTN                    ; success (skip target - CODE, not data)
7017e86c WMOV  0,2
7017e86d LCALL O.SERROR          ; signal
```

`0x7017E871` computes the rounded size, dispatching on the allocation
class in ac1 through a jump table:

```
7017e871 LDSP 1,[0x7017E879]     ; VALID RANGE [1,4]
         targets: 0x7017E881, 0x7017E886, 0x7017E88A, 0x7017E88C
7017e881 WNADI 0,31 ; WLSHI 0,-5   ; class 1: bytes -> words, round up 32
7017e886 WADI  3,0 ; WLSHI 0,-2    ; class 2: round up 4, /4
7017e88a WINC  0,0 ; WHLV 0        ; class 3: (n+1)/2
7017e88c WLSI  1,0                 ; class 4: shift left
7017e88d WADI  4,0                 ; + 4-word header
7017e88e NLDAI 8,1 ; WSLE 1,0      ; minimum block = 8 words
7017e891 WMOV  1,0
7017e892 XPSHJ 0x7017E9F9         ; I.LOCK wrapper
7017e894 XPSHJ 0x7017EA09         ; drain deferred frees
7017e896 XPSHJ 0x7017E8A4         ; the allocation itself
7017e898 WBR   2 -> 0x7017E89A
7017e899 ISZTS                    ; skip-return bump (CODE, not data)
7017e89a WMOV  0,0
7017e89b XPSHJ 0x7017EA01         ; I.UNLOCK wrapper
7017e89d WPOPJ
```

The allocation core at 0x7017E8A4 tries the free list first:

```
7017e8ad LLEF  2,[0x700001F4]     ; free-list descriptor
7017e8b0 XWLDA 1,[ac2+0x0]        ; head
7017e8b2 WADC  3,3                ; -1
7017e8b3 WSNE  1,3
7017e8b4 WBR   -> 0x7017E8CF      ; EMPTY: extend the heap instead
7017e8b5 NLDAI 0xFFFE,3           ; ac3 = -2 (field offset)
7017e8b7 QSEARCH 0x001C           ; *** UNIMPLEMENTED - abort here ***
7017e8b9 WBR   -> 0x7017E8CF      ;   not found
7017e8ba WBR   -> 0x7017E8B7      ;   continue
7017e8bb WMOV  2,0
7017e8bc DEQUE                    ; unlink the chosen block
```

and otherwise extends the break via the OS (0x7017E8CF):

```
7017e8ec SYSCALL 014              ; extend
7017e8f0 SYSCALL 03               ; MEM
7017e906 LWSTA 3,[0x700001F0]     ; publish the new break
7017e909 XPSHJ 0x7017E92D         ; update the water marks
7017e910 XJSR  0x7017E91B         ; write the block header
7017e915 ... WLDAI 0x00011613     ; out-of-memory signal code
```

`0x7017E91B` writes the header: negated size to `[block-2]` and to the
frame, size class handling at `[ac3-6]`, result address in ac0.

### I.FREE family (0x7017E945 / 49 / 4C)

Three entries, one shared body.

**CORRECTION (Aug 2026):** an earlier version of this section said the
entries "normalise the size argument". Wrong — they normalise the
**block pointer**. The argument in ac0 is the address I.ALLOC returned
(call site 0x7017DC38 loads the *saved allocation result*, not a size):

```
7017e945 I.FREEB: WSSVS 0 ; WMOVR 0        ; byte ptr -> word ptr (logical >>1)
7017e949 I.FREEW: WSSVS 0                  ; word ptr -> as-is
7017e94c I.FREE : WSSVS 0 ; WSGE 0,0 ; WMOVR 0
                       ; auto-detect: halve ONLY if ac0 < 0 — a byte
                       ; pointer is word<<1, which for segment-7
                       ; addresses (0x70000000+) sets bit 31
7017e950 XPSHJ 0x7017E9F9   ; I.LOCK
7017e952 XPSHJ 0x7017EA09   ; drain deferred frees
7017e954 XPSHJ 0x7017E970   ; ownership check + the free itself
7017e956 XPSHJ 0x7017EA01   ; I.UNLOCK
7017e958 WRTN
```

Error codes signalled via `O.SERROR`: `0x1163B`, `0x11632`, `0x11623`
(0x7017E959–0x7017E96C) — heap corruption / bad free / not-owner.

`0x7017E970` checks the block belongs to this heap (`LDASB`,
0x70000204, `I?INHPW`) then falls into the coalescer.

### Coalescer (0x7017E97C–0x7017E9B3) — HIDDEN CODE

Classified `mem` by StartStop; recovered with an explicit addrs file
(see below). Merges the block with its neighbours, **dequeuing each
neighbour from the free list first**:

```
7017e98e LLEF  0,[0x700001F4]
7017e991 DEQUE            ; unlink predecessor
7017e992 WMOV  0,0        ; pad (skip absorber)
7017e994 DEQUE            ; unlink successor
7017e995 WMOV  0,0        ; pad
7017e998 XJSR  0x7017E9B3 ; re-insert the merged block
```

### Free-list insert (0x7017E9B3)

Either retracts the break (block abuts the top) or splices into the
free list:

```
7017e9bf WSEQ  0,2          ; block abuts the break?
7017e9c0 WBR   -> 0x7017E9D6  ; no: enqueue
7017e9c8 LWSTA 2,[0x700001F0] ; yes: just move the break, no queue op
...
7017e9dd LLEF  2,[0x700001F4]
7017e9e0 XWLDA 1,[ac2+0x0]
7017e9e3 WSNE  3,1
7017e9e4 WBR   -> 0x7017E9EE   ; empty list
7017e9e7 QSEARCH 0x001A        ; *** UNIMPLEMENTED - abort here ***
7017e9eb NLDAI 0xC7F9,3        ; = ENQT opcode
7017e9ee XWLDA 1,[ac2+0x0]
7017e9f0 NLDAI 0xC7E9,3        ; = ENQH opcode
7017e9f6 XCT                   ; execute the selected enqueue
```

**This is why the free list has never had an entry**: the common case
(freeing the most recent allocation) takes the break-retraction path at
0x7017E9C8 and never touches a queue.

## Hazards

1. **`QSEARCH` is unimplemented** (0x7017E8B7, 0x7017E9E7). Native code
   must abort at both, throwing the **exact** string the master's base
   `Instruction::execute` produces, because `compare_pair` compares
   exception text with `strcmp`:
   `Instruction subclass must override execute QSEARCH*`
1b. **`XCT` is ALSO unimplemented** (0x7017E9F6). The free-list insert
   aborts there before any enqueue runs, so implementing `ENQH` did not
   unblock that path — an earlier claim to the contrary is corrected in
   UNIMPLEMENTED.md §1. Native code must abort with
   `Instruction subclass must override execute XCT*`.
2. **`XCT` at 0x7017E9F6** dispatches `ENQH`/`ENQT` from constructed
   opcodes (`0xC7E9` / `0xC7F9`). Grep for mnemonics does not find
   them — see QUEUE_INSTRUCTIONS.md.
3. **All four `mem` holes in this range are CODE**, recovered and
   listed below. None is data.
4. **Skip-returns everywhere.** `ISZTS` at 0x7017E861 and 0x7017E899
   bumps the saved return address (the O_ON.md convention). The
   `XPSHJ`/`WPOPJ` helpers are push-jump subroutines, not `LCALL`.
5. **I.LOCK / I.UNLOCK syscalls** — see HeapSignalPlan.md. `SYSCALL
   0523` (I.UNLOCK wake) is unimplemented in the emulator, so the
   contended path already aborts on both engines today.
6. **`ENQH` is now implemented**, but that does NOT make the free-list
   insert reachable — `XCT` aborts first (hazard 1b). The insert path
   needs `XCT` and `QSEARCH` both.

## Recovering the hidden code

```
cd <project root>            # the addrs file is read from the HOST cwd,
                             # not the emulated filesystem
printf 'code 7017e85c 7017e863\ncode 7017e86b 7017e86c\ncode 7017e899 7017e89a\ncode 7017e97c 7017e9b3\n' > holes.addrs
java -cp Tools Disassemble QUEST QUEST holes.addrs
```

| Hole | Contents |
|---|---|
| 0x7017E85C–63 | `XWADD`/`WSBI`/`XWSTA`/`ISZTS`/`WRTN` — I?SALLOC tail |
| 0x7017E86B | `WRTN` — I.ALLOC success skip-target |
| 0x7017E899 | `ISZTS` — skip-return bump |
| 0x7017E97C–9B3 | the coalescer (39 words) |

## Still to derive before any code

- ~~Instruction-level semantics of every body instruction~~ **DONE** —
  see "Instruction semantics" below.
- ~~The `LDSP` jump-table dispatch~~ **DONE** — see "Call sites and
  allocation classes".
- ~~Exact register/skip contract of each `XPSHJ` helper~~ **DONE** (see
  "Skip-return mechanics" and "Residue maps").
- ~~Full residue map~~ **DONE** for the two live paths (see "Residue
  maps"); dead paths are handled by native-fallback, not translation.
- The `SYSCALL 014` / `SYSCALL 03` extend path against
  `RTBridge::syscall` — bridge implemented (hw/RTBridge.cpp, Aug 2026);
  MEMI (014) and MEM (03) confirmed non-blocking with plain register
  arguments.
- ~~Empirical captures~~ **DONE** (at 0x7017E866 and 0x7017E949 — see
  below); footprint-diff to zero words remains as the implementation
  gate.

## Empirical captures (Aug 2026) — docs/captures/

Scripted lockstep sessions with `QUEST_CAPTURE=<entry>` and
`QUEST_CAPTURE_DEST=700001F0`, zero divergences. Files:
`captures/i_alloc-master.txt` (entry 0x7017E866, both ?CREATE_TASK
sites) and `captures/i_freew-master.txt` (entry 0x7017E949, the
descriptor free at 0x7017DC38). NOTE: the plan docs said to capture at
0x7017E94C (I.FREE) — nothing ever calls that entry; all live sites
use I.FREEW (0x7017E949).

### Heap-state variables (dest window), decoded

| Addr | Meaning | Observed |
|---|---|---|
| 0x700001F0 | heap break (grows DOWNWARD; block = break+4) | 0x70017BFC → 0x7001764C → 0x70017632 → 0x7001764C |
| 0x700001F2 | extend mode: 0 = steal from stack-limit ceiling; ≠0 = MEMI | **always 0** |
| 0x700001F4 | free-list queue head (2 wides, −1/−1 = empty) | **always −1** |
| 0x700001FA | deferred-free chain head (drain tests > 0) | always −1 |
| 0x700001FC | low water mark = break+2 (helper 0x7017E92D, [1F2] ≥ 0 branch) | tracks break+2 |
| 0x700001FE | high water mark ([1F2] < 0 branch; static here) | 0x70017BFC |
| 0x70000200 | heap lock word (WSZBO bit) | 0 outside calls |
| 0x70000204 | owner comparand (stack base; 0 = unset) | (outside window) |

### Verified against live data

- Call 1: ac0=0x5AC, ac1=3 → returned 0x70017650, break moved exactly
  0x5B0 (= round-even + 4). Call 2: ac0=0x16, **ac1=3 with no reload**
  (the inheritance claim, confirmed), returned 0x70017636 = new
  break+4.
- Return residue shows the ISZTS-bumped XPSHJ return (0x7017E86B) on
  the stack — the two-level skip-return chain, live.
- WSSVS saved-ac0 slot patched to the result; saved ac1/ac2 restored
  untouched. c=0, ovr=0 at both returns (entry values).
- I.FREEW of the top-of-heap block **retracts the break** (0x70017632
  → 0x7001764C), raises the stack limit back by the size
  (0x7017E9D2-D5, [1F2]==0 branch), free-list head stays −1, and
  restores all entry ACs. The free-list insert + XCT'd ENQH/ENQT
  (constants 0xC7E9/0xC7F9 at 0x7017E9EB/E9F0) never runs.

### Consequences for the translation

1. **The live path never touches**: the free-list (QSEARCH sites), the
   XCT'd enqueue, SYSCALL 014/03 (MEMI extend — dead while [1F2]==0,
   and only MEMI's own success path sets [1F2]). The translation still
   implements the [1F2]≠0 branch via `RTBridge::syscall` for fidelity,
   and keeps the matched-abort at both QSEARCH sites
   (QUEUE_INSTRUCTIONS.md).
2. **Extend = stack-limit steal**: alloc lowers wsl by the block size
   after a heap/stack collision check (fail → O.SERROR 0x11613);
   top-block free raises wsl back. The translation must replicate the
   wsl updates exactly — wsl is lockstep-visible state.
3. Live error codes: 0x11613 (out of memory), 0x11623 (not owner),
   0x11632 (bad block), 0x1163B (bad pointer/context) — all via
   O.SERROR (stubbed; reaching one under lockstep is
   divergence-visible regardless).

## Instruction semantics (verified from emulator source, Aug 2026)

Checklist step 0b for this chunk. Source files: EagleCompute.cpp,
EagleGeneral.cpp, EagleStack.cpp, EagleSpecial.cpp, EagleInstruction.cpp
(add/sub/shift helpers).

**READING KEY — same-register skips compare against ZERO.** For
WSEQ/WSNE/WSLT/WSLE/WSGT/WSGE, `dst=(XX!=YY)?ac[YY]:0`. So `WSNE 1,1`
= "skip if ac1 ≠ 0", `WSEQ 0,0` = "skip if ac0 == 0", `WSGT 2,2` =
"skip if ac2 > 0", `WSGE 0,0` = "skip if ac0 ≥ 0". Without this the
ownership checks and the drain loop read as dead code. (Same pattern in
WCLM's II==AA case: limits inline, not from a register.)

### Flag side effects

- **add/sub helpers SET carry unconditionally and OR into ovr**:
  WADD, WSUB, WADC, WNEG, WINC, WADI, WSBI, WNADI, XWADD, XWSUB.
  - `WSUB x,x` → x=0, **c=0** (the O_ON lesson, reconfirmed).
  - `WADC s,d` is d := d + ~s. **`WADC x,x` → x=−1, c=1** (x + ~x =
    0xFFFFFFFF; carry bit set; no ovr). This is the "−1" idiom used at
    0x7017E8B2/E90B/E9E2/EA0F/EA2A/E9CD.
  - `WNEG s,d` is d := 0 − s; c=1 iff s ≠ 0.
  - `WINC s,d` is d := s+1. `WADI n,d` / `WSBI n,d`: n = XX+1 ∈ [1,4].
  - `WNADI imm,d`: two-word, 16-bit sign-extended immediate.
- **No flag changes**: WMOV, WXCH, WMOVR (logical >>1), WHLV
  (arithmetic >>1), WLSI (left shift, count=XX+1), WLSHI (two-word,
  8-bit sign-extended count, negative = logical right), WLSH, all
  loads/stores (XWLDA/XWSTA/LWLDA/LWSTA/NLDAI/WLDAI), LLEF/XLEF,
  LDSP, WBR, LJSR, WCLM, skips.
- **LCALL zeroes ovr** and pushes the frame wide `(psr<<16)|argc`
  (or `argc&0x7FFF` alone if bit 15 of the argument word is set);
  ac3 := call site+4. **LJSR does neither**: ac3 := site+3, jump, no
  push, flags untouched. Both consult the native registry.
- **WSSVS/WSSVR zero ovr; WSAVR/WSAVS do NOT.** Both set ovk (S=1, R=0).

### Frame images and slot offsets

`wide_push` pre-increments wsp by 2 then writes, so the LAST push sits
AT the final wsp.

**WSSVS/WSSVR** (LJSR/XJSR callees — I.ALLOC, I.FREE*, I?INHP*,
I?SALLOC, header writer, insert): pushes psr<<16 FIRST, then ac0, ac1,
ac2, wfp, ac3|c<<31. ac3 = wfp = wsp after pushes. Slots relative to
the frame pointer (disassembly offsets are 15-bit two's complement,
0x8000 bit = indirect):

| Slot | Offset | Disasm form |
|---|---|---|
| saved ac3 \| c<<31 (return) | +0 | `[ac3+0x0]` |
| saved wfp | −2 | `[ac3+0x7FFE]` |
| saved ac2 | −4 | `[ac3+0x7FFC]` |
| saved ac1 | −6 | `[ac3+0x7FFA]` |
| saved ac0 | −8 | `[ac3+0x7FF8]` |
| psr<<16 | −10 | `[ac3+0x7FF6]` |

**WSAVR/WSAVS** (LCALL callees — I.LOCK, I.UNLOCK, I?HPOWNER, I?ASIZE,
I.TOFREE): five wides, NO psr (WRTN takes psr from the LCALL frame
wide). Same top five slots; below them the LCALL frame wide at −10 and
arg-N pointers at −12, −14, ... (`[ac3+0x7FF4]` = first arg slot —
matches i_lock.cpp and RTBridge).

**WRTN** (both conventions): wsp := wfp; pop return|c, wfp, ac2, ac1,
ac0, frame wide; psr := frame>>16; wsp −= 2·(frame&0x7FFF); ac3 := the
POPPED wfp. **Return-value convention**: a routine returns a value by
patching its own saved-ac0 slot** (`[ac3+0x7FF8]`) so WRTN restores it
— I?SALLOC at 0x7017E863, I?ASIZE at 0x7017E819, the header writer at
0x7017E92A all do exactly this.

### Skip-return mechanics — THREE distinct mechanisms in this chunk

1. **`XWISZ [ac3+0x0]`** (0x7017E831, 0x7017E850): wide-increment the
   saved return address in the WSSVS frame → after WRTN, execution
   resumes one word later. I?INHPW/I?INHPWS success path. (Skip-if-zero
   on the incremented value never fires here.)
2. **`ISZTS`** (0x7017E861, 0x7017E899-hole, 0x7017E913): wide-increment
   the wide AT [wsp] — which at those points is an XPSHJ return address
   → the pushed-jump returns one word later. This is how the allocation
   core signals success through TWO levels: e913 ISZTS bumps e896's
   pushed e898→e899; the WPOPJ lands on the e899 hole, which is ITSELF
   an ISZTS bumping e868's pushed e86A→e86B (= WRTN, the success exit),
   before falling into e89A/WPOPJ. Failure paths skip the bumps and
   WPOPJ lands on the un-bumped addresses (e86A → O.SERROR path).
3. **QSEARCH/DEQUE/WSZBO/WMESS/XNISZ/XNDSZ hardware skips** — pads
   (`WMOV 0,0`) absorb them where the outcome is ignored.

### XPSHJ helper contracts (register I/O)

- **0x7017E9F9 (lock)** / **0x7017EA01 (unlock)**: `LPEF [0x70000200]`
  pushes the lock address as the single LCALL argument; I.LOCK/I.UNLOCK
  are WSAVR routines that restore all ACs — **caller-transparent**.
- **0x7017EA09 (drain deferred frees)**: reads chain head
  [0x700001FA]; `WSGT 2,2` → empty (≤0) returns immediately. Else:
  saves ac0 (WPSH 0,0), head := −1, then walks: ac0 := node, push
  next-link, XPSHJ 0x7017E970 (free the node), pop link, `WSLE 2,2`
  loop while link > 0. Restores ac0. **Clobbers ac1, ac2, ac3.**
- **0x7017E970 (ownership check + free)**: ac0 = block word-pointer.
  `LDASB 2; LWLDA 1,[0x70000204]; WSNE 1,1` — if the stored comparand
  is 0 (first use) substitute the current stack base; `WSEQ 1,2` — if
  comparand ≠ current SB → O.SERROR 0x11623 (not owner). Then ac2 :=
  block, XJSR I?INHPW: WCLM range check against [0x700001FC/1FE]
  (skip = in range), `[block-2]` must be **negative** (size word;
  `WSGT 0,0`... path at e83F/e845 reads `[ac2-2]`), and `[block+0]`
  compared to −1 catches a double-free (links cleared by DEQUE). Fail
  → WRTN un-bumped → e97B → e960: `WSNE 2,2` selects O.SERROR 0x11632
  (bad block) vs 0x1163B; success → XWISZ-bumped WRTN → falls into the
  coalescer at e97C.

### Call sites and allocation classes (complete inventory)

Five call sites in the whole binary, **all runtime-internal, none in
game code** (grep of quest.dis and quest-rt.dis for the three entry
addresses):

| Site | Caller | Entry | ac0 | ac1 (class) |
|---|---|---|---|---|
| 0x7017DBCE | ?CREATE_TASK | I.ALLOC | stack size + [0x7017EA78] | 3 (`NLDAI 3,1`) |
| 0x7017DBFC | ?CREATE_TASK | I.ALLOC | 22 | 3 (preserved: WRTN restored ac1 across site 1; no write between) |
| 0x7017DC38 | ?CREATE_TASK | I.FREEW | saved site-2 result (block ptr) | — |
| 0x7017E37E | ?LIB_ERROR | I.FREEW | `[T?AREA+8 area +3]` (msg buffer ptr) | — |
| 0x7017E399 | ?LIB_ERROR | I.ALLOC | ((len+1)>>1)+1 | 3 (`NLDAI 3,1`) |

**Only class 3 is ever used.** The LDSP table (valid range [1,4],
targets E881/E886/E88A/E88C) dispatches; classes 1–3 then fall/branch
into E88C, which is ALSO the class-4 target — a shared `WLSI 1,0` that
doubles every class's unit count into words.

**CORRECTION (Aug 2026, empirical):** an earlier draft read `WLSI 1,0`
as "<<2". The disassembler prints the EFFECTIVE shift amount (XX+1),
so `WLSI 1,0` is **<<1**. Verified by capture: request 0x5AC, class 3
→ break moved exactly 0x5B0 = round-up-to-even(0x5AC) + 4. So:

| Class | Words allocated (before +4 header, min 8) |
|---|---|
| 1 | `((n+31)>>5)<<1` (n in 32-word pages? → doublewords ×2) |
| 2 | `((n+3)>>2)<<1` |
| 3 | `((n+1)>>1)<<1` = **n rounded up to even** (n in words) |
| 4 | `n<<1` (n in doublewords) |

Then all: `WADI 4,0` (+4-word header), minimum 8 (`NLDAI 8,1;
WSLE 1,0; WMOV 1,0`). Out-of-range class falls through (+3) to E88A =
the class-3 path — class 3 is also the default.

The same correction applies to the header writer's class-2 result
shift: `WLSI 1,2` at 0x7017E929 is ac2 **<<1** — class 2 returns a
BYTE pointer (word<<1), every other class a word pointer.

Classes 1, 2, 4 are **statically unreachable** at current call sites
but must still be translated faithfully (the dispatch is data-driven).

### LDSP mechanics (for the translation)

`LDSP II,[table]`: bounds [L,H] are the two wides at table−4/table−2;
in-range → entry wide at table+(ac−L)·2; entry == −1 → fall through;
else jump to **entry + its own address** (self-relative). Out-of-range
→ fall through (+3).

### WMESS / WSZBO (lock primitives, for completeness)

- `WSZBO s,d`: bit index in ac_d (word = ac_d>>4, mask =
  0x8000>>(ac_d&15)) at word address ac_s + word (s==d → address 0);
  **always sets the bit**; skip iff it was previously 0.
- `WMESS`: CAS at [ac2]: if (mem ^ ac0) & ac3 == 0 → mem := ac1,
  ac1 := old, **skip**; else ac1 := old, no skip.


## Residue maps (live paths; word-for-word against the captures)

All offsets relative to F = entry wsp + 12 (the WSSVS frame pointer).
"Final value" = last writer wins; intermediate writers shown when they
explain a value. Verified bit-for-bit against docs/captures/.

### I.ALLOC, extend path ([1F2]==0, free-list empty, chain empty)

| Addr | Final value | Last writer |
|---|---|---|
| F−10 | entry psr<<16 | WSSVS |
| F−8 | **block** (the result) | header writer 0x7017E92A patching saved-ac0 |
| F−6 | entry ac1 | WSSVS |
| F−4 | entry ac2 | WSSVS |
| F−2 | entry wfp | WSSVS |
| F | entry ac3 \| entry c<<31 | WSSVS |
| F+2 | 0x7017E86B | e868 push, ISZTS-bumped via the e899 hole |
| F+4 | 0x7017E89D | e89B push (unlock call; earlier: e894/e896/e898→e899) |
| F+6 | 0x70000200 | unlock LPEF (earlier: core WPSH size) |
| F+8 | (psr<<16)\|1 | unlock LCALL (earlier: e909 push, header psr) |
| F+10 | size | unlock WSAVR ac0 (earlier: lock ac0, water WPSH, header ac0=F) |
| F+12 | size | unlock WSAVR ac1 (earlier: lock ac1=8, header ac1=size) |
| F+14 | block | unlock WSAVR ac2 |
| F+16 | F | unlock WSAVR wfp |
| F+18 | 0x7017EA08 \| c<<31, **c=1** (WADC 1,1 at e90B) | unlock WSAVR ac3\|c (earlier: header ret\|c) |

Heap effects: wsl −= size; [1F0] := newbreak = oldbreak − size;
[1FC] := newbreak+2; [newbreak] (= [block−4]) := −1 (sentinel: no
predecessor); [block−2] := −size; [block+size−4] := −size. Lock word
round-trips to 0. Registers out: ac0=block, ac1/ac2/c = entry, ac3=F.
Residue extends to F+19 and NO further (capture: [F+20]+ untouched).

### I.FREEW, retract path (block bottom-adjacent, successor allocated)

| Addr | Final value | Last writer |
|---|---|---|
| F−10..F | WSSVS image, saved-ac0 NOT patched (entry ac0 returns) | WSSVS |
| F+2 | 0x7017E958 | e956 push (earlier e950/e952/e954→e956 via XWISZ path) |
| F+4 | 0x70000200 | unlock LPEF |
| F+6 | (psr<<16)\|1 | unlock LCALL |
| F+8 | block | unlock WSAVR ac0 |
| F+10 | size (+, from WNEG) | unlock WSAVR ac1 |
| F+12 | block+size (= old break) | unlock WSAVR ac2 |
| F+14 | F | unlock WSAVR wfp |
| F+16 | 0x7017EA08, **c=0** | unlock WSAVR ac3\|c — c=0 because the emulator's sub() carry formula `((dst&M)−(src&M))>>31 & 1` yields 0 for WNEG of a negative size (int64 −2 & 1); borrow-intuition says 1 and is WRONG. Capture-proven. |
| F+18 | 0x7017E9CD | e9CB push (coalescer → water helper) |
| F+20 | block | water helper WPSH scratch (ac0) |

Heap effects: [1F0] := block−4 + 4 + size... i.e. restored to
block+size−4 = old break; wsl += size; [1FC] := restored break + 2;
[restored break] := −1 (e9CE, mirrors alloc's sentinel). Registers out:
all three ACs = entry (nothing patched), c = entry.

### Carry chains (source-formula; pre-fix captures — see P24 note)

**P24 CORRECTION (Aug 29 2026, wide-carry fix + WADC user ruling):**
`WADC x,x` → c=**0** (no ALU carry-out of x + ~x; the pre-fix c=1 was
the >>31 bug). The alloc-path chain below is unchanged in SHAPE but the
value flips: the unlock-LCALL residue word is now 0x7017EA08 (bit31
clear), and the 0xF017EA08 in captures/i_alloc-master.txt is a PRE-FIX
artifact. The WNEG row's c=0 keeps its value with a simpler reason:
genuine borrow. i_alloc.cpp re-staged accordingly (P24 report).

- `WADC x,x` → c=1 always [P24: now c=0 — see correction above]. Alloc
  path: set at e90B, survives the header
  writer's WRTN (bit31 of its saved ret wide), reaches the unlock LCALL
  → 0xF017EA08 in residue [P24: now 0x7017EA08].
- `WNEG` of a negative → c=0 (see table note). Free path: c=0 from e9B1
  through the coalescer helper's image and WRTN to the unlock LCALL →
  0x7017EA08 (bit31 clear).
- WRTN restores c from bit31 of the saved ac3|c wide; ISZTS, WPOPJ,
  XPSHJ, LPEF, loads/stores never touch c; LCALL never touches c (it
  zeroes ovr only).

## Translation design (implemented Aug 2026)

Native fast paths = exactly the two capture-verified paths above.
Gates, ALL checked before any write (i_lock.cpp philosophy):

shared: lock uncontended AND no waiters AND deferred chain ≤ 0 AND
[1F2] == 0 AND ownership ([0x70000204] == 0 or == wsb).
I.ALLOC additionally: free-list head == −1, class computation, stack
collision check passes (wsl − size > wsp).
I.FREE* additionally: block in [1FC..1FE] water range, [block−2] < 0,
trailing size == leading size at [block+size−4] (the I?INHPW validity
check — an earlier draft called this a "[block+0] vs −1 double-free
check", which was wrong), predecessor word [block−4] ≤ 0 and successor
word [block+size−2] ≤ 0 (no merges), block == break+4 (bottom-adjacent).

Any gate fails → return RTStubs::entry_address(name): the whole call
runs emulated on the clone, identically to the master — contention,
MEMI, the free-list insert, O.SERROR paths, and the authentic QSEARCH
abort all stay symmetric by construction. QSEARCH is therefore NOT
natively aborted; the fallback reaches the real trap.

Arithmetic replicates hw/EagleInstruction.cpp's add/sub carry formulas
verbatim (local copies, commented with provenance) — see the WNEG row
above for why intuition is not a substitute.


## Validation (Aug 2026) — COMPLETE

- Footprint diff to zero: master RETURN vs clone NATIVE snapshots
  identical word-for-word (92-word frame region + heap-state window)
  for I.ALLOC (both ?CREATE_TASK sites) and I.FREEW. Only the AC lines
  differ, legitimately (NATIVE fires pre-native_return).
- Three consecutive scripted lockstep sessions: zero divergences, all
  three heap calls native, zero fallbacks. Pre-existing startup
  exceptions unchanged.

### Two lessons recorded

1. **Signed comparisons.** Memory::read_wide returns uint32_t; WSGT /
   WSLT / WSGE are SIGNED. A bare `read_wide(...) > 0` gate read the −1
   sentinel as 4294967295 > 0 and mis-fired pred-merge. The
   gate-reason log strings pinpointed it in one run — keep using them.
   Rule: never compare a read_wide result without casting to int32_t or
   assigning through an int32_t variable.
2. **Fallback must arm the pair span.** The master decides
   run-to-return from translated_bits alone and cannot see a clone-side
   fallback. Any translation's fallback path must set
   machine.rt_pending_return = machine.ac[3] before returning
   entry_address, so the clone also produces ONE native_span batch at
   the outer return with inner native breaks swallowed
   (hw/Machine.cpp). Without this, the first fallback in a session is a
   structural lockstep divergence.
