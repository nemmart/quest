# Queue Instructions (ENQH / ENQT / DEQUE / QSEARCH / WMESS)

Status: **ENQH, ENQT, DEQUE implemented and live** (Aug 2026, rewritten
from the Data General queue-management chapter supplied by the user).
**QSEARCH still traps** — not enough documentation to implement.

Source of truth: the DG queue-management chapter (OCR'd by the user).
That chapter fully specifies the *data structure*; it does **not**
specify the skip conventions or the search-instruction encodings.

## Documented semantics

**Data element** — two links, each a 32-bit double word:

| Offset | Contents |
|---|---|
| `[elem+0]` | **forward** link — address of the following element |
| `[elem+2]` | **backward** link — address of the preceding element |

- forward link `-1` → the element is at the **tail**
- backward link `-1` → the element is at the **head**
- both `-1` → it is the only element in the queue

User information may precede or follow the links (Tables 6-1 and 6-2);
its length is not fixed, because links always point at other links.
Quest uses both layouts: `LOCK_FILE`'s records put user data *after*
the links, while the heap's free blocks put the size word *before* them
(`I.ALLOC` reads `[elem-2]`).

**Queue descriptor** — two double words: `[desc+0]` = head element
address, `[desc+2]` = tail element address. An empty queue is `-1` in
both. Confirmed independently by Quest's own heap init at 0x7017EB4C
(`WADC 0,0` then stores to `[ac3+0]` and `[ac3+2]`).

**Dequeue clears the removed element's links.** From the NOTES:
dequeuing sets both forward and backward links to `-1`, and dequeuing
the same element twice therefore leaves the *descriptor* holding `-1`
in both words (an empty queue). That consequence is a useful
self-check: it falls out automatically from a correct implementation,
and does not hold without the clearing.

**Registers**, from the manual's `QMOVE` and `PDEQ` examples:

| Register | Role |
|---|---|
| ac0 | queue descriptor address |
| ac1 | reference element; `-1` selects the default position |
| ac2 | element to enqueue (ENQH/ENQT only) |

For `ENQT` the manual's example comments `WADC 1,1` as "At the end", so
`ac1 = -1` means tail for ENQT and (by symmetry) head for ENQH. For
`DEQUE`, `ac1 = -1` selecting the head element is an **inference**, not
documented — it matches the original implementation and the general
`-1` = "default position" pattern.

## Defects found in the original implementation

The emulator's Java-derived ENQT/DEQUE were written from this same
manual and never validated, because the code paths were believed
unexercised. Two real defects, one non-defect:

1. **Link order transposed.** The original used `[elem+0]` = backward
   and `[elem+2]` = forward — the mirror of Table 6-1. It was
   *internally* consistent (ENQT and DEQUE agreed with each other), so
   nothing ever broke: only these instructions touch the links, and
   mirroring both cancels out. It would break as soon as a search
   instruction (which traverses forward vs backward) or any direct link
   access entered the picture.
2. **DEQUE never cleared the removed element's links.** Neighbours and
   descriptor were updated correctly; the element itself was left with
   stale pointers, so the documented double-dequeue behaviour did not
   hold.
3. **Skip conventions were correct** — see below. No change needed.

## Skip conventions (derived, not documented)

The chapter never states them, and both worked examples deliberately
pad with `NOP` after the instruction so the skip cannot be observed.
Quest uses the same idiom in `I.ALLOC` (`WMOV 0,0` after `DEQUE`).

`LOCK_FILE` (0x70169B0F), however, *consumes* both skips, and pins them:

```
70169b21 ENQT;
70169b22 WBR 14 (0x70169B30);   ; no skip -> we hold the lock
70169b23 WMOV 0,0;              ; skip (pad) -> falls into
70169b24 SYSCALL 0246           ; ...wait for the lock
```

```
70169b54 CRYTZ;                 ; carry = 0
70169b55 DEQUE;
70169b56 CRYTO;                 ; carry = 1, only if DEQUE did NOT skip
70169b57 WMOV 1,2;
...
70169b5d MOV.# 0,0,SNC;         ; carry gates the wake-up path
70169b65 ...  SYSCALL 0245      ; wake the next waiter
```

Reading both against lock semantics:

- **ENQH / ENQT: skip iff the queue was NON-EMPTY before the enqueue.**
  Empty (no skip) = "you are first, you have the lock".
- **DEQUE: skip iff the queue is NON-EMPTY after the dequeue.**
  No skip → carry 1 → "queue now empty, nobody left to wake".

Both agree with the original implementation, which is why `LOCK_FILE`
has always worked despite the transposed links.

## Reachability (established empirically, Aug 2026)

An unimplemented opcode is a `-1` decoder entry with an empty class,
which builds a base `Instruction`; `Instruction::execute` throws. So
`ENQH` and `QSEARCH` executing has always been immediately fatal —
their absence from every clean session is *proof* they never ran.

- **Heap free-list paths: never executed.** `I.ALLOC`'s coverage stops
  at 0x7017E8B4, the `WBR` taken when the free-list head is `-1`.
  `QSEARCH` (0x7017E8B7) and `DEQUE` (0x7017E8BC) are unmarked, and the
  free-list insert at 0x7017E9DD–0x7017E9F6 has zero coverage.
  Allocation never enqueues — only freeing does, and only for a block
  that does not abut the heap break (otherwise 0x7017E9C8 just retracts
  the break). Confirmed under deliberate `?LIB_ERROR` provocation.
- **`LOCK_FILE`: live on every turn.** `ENQT` at 0x70169B21 and `DEQUE`
  at 0x70169B55, reached via `SIGNAL_TURN` ← `START_TURN`. Found by
  temporarily making both instructions throw; it fired immediately.
  This is the validation path for the rewrite.
- **`WMESS`: live** — `I.UNLOCK` (0x7017E7F8) and `I.GINIT`
  (0x7017EA88), both covered. It is a compare-and-swap that never
  touches queue links, so the link-order question does not reach it.
  Deliberately left alone.

## CHECKLIST CORRECTION: grepping for mnemonics is not sufficient

`ENQH` and `ENQT` **do not appear as mnemonics anywhere in the
runtime.** The free-list insert builds the opcode as an immediate and
executes it indirectly:

```
7017e9e7 QSEARCH 0x001A          ; find the insertion point
7017e9eb NLDAI 51193 (0xC7F9),3  ; = ENQT opcode
7017e9ed WBR   -> 7017E9F2
7017e9ee XWLDA 1,[ac2+0x0]
7017e9f0 NLDAI 51177 (0xC7E9),3  ; = ENQH opcode
7017e9f2 WMOV  2,0
7017e9f6 XCT                     ; execute whichever was selected
```

`0xC7E9` and `0xC7F9` are exactly the decoder's ENQH/ENQT bit patterns.
SessionPlan.md checklist step 0 says to grep a routine and its callees
for `ENQH`/`ENQT`/`DEQUE`; that grep would miss this entirely.

**`XCT` IS ITSELF UNIMPLEMENTED** (decoder entry `-1`), so this path
aborts at 0x7017E9F6 *before* any enqueue runs. Implementing `ENQH` did
not unblock the free-list insert — an earlier note claiming the abort
merely moved to the next allocation's `QSEARCH` was wrong. The insert
needs `XCT` **and** `QSEARCH`. See UNIMPLEMENTED.md §1.

**Step 0 must also check for `XCT`** and for opcode-shaped constants.
Known `XCT` sites in the runtime: 0x7017E9F6 (this one) and 0x7017ECF4
(inside `I.GOTO`). Opcode-shaped constants also sit in `WBR 2`-skipped
holes — 0x7017E86B holds `87A9` (`WRTN`), 0x7017E899 holds `C7C9`
(`ISZTS`) — so XCT-dispatched code is an idiom here, not a one-off.

Note also that this code lives at 0x7017E97C–0x7017E9B3, which
`StartStop` classifies as `mem` (it is in RTWorklist's "hidden live
code" list). It has to be disassembled explicitly:

```
printf 'code 7017e97c 7017e9b3\n' > QUEST/hidden.addrs
java -cp Tools Disassemble QUEST QUEST hidden.addrs
```

## QSEARCH — still unimplemented, still a hard stop

Table 6-3 lists the search family as `NBStc` / `NFStc` / `WBStc` /
`WFStc` (narrow/wide × backward/forward × test condition). The
emulator collapses them into one `QSEARCH*` decoder pattern
(`1100011100011001`) with a second word. The chapter gives no format
and no `tc` encoding; the only operational hint is that searches
reference the *user* information, so callers must account for its
length.

What the two Quest call sites constrain:

| | I.ALLOC 0x7017E8B7 | free-insert 0x7017E9E7 |
|---|---|---|
| second word | `0x001C` | `0x001A` |
| ac0 | requested size in words | address of the block being freed |
| ac1 | current element (head on entry) | current element (head on entry) |
| ac2 | queue descriptor | queue descriptor |
| ac3 | `-2` (field offset? `[elem-2]` is the size word) | `-2` |

Both sites have the same three-way shape:

```
QSEARCH <imm>
WBR  -> not found / exhausted
WBR  -> back to the QSEARCH   (continue)
(found)
```

Unknown: which of NBS/NFS/WBS/WFS each immediate denotes, the `tc`
encoding, what is compared against what, and what the three outcomes
mean precisely. **Do not guess.** A wrong condition does not crash — it
selects a different free block, the heap stays internally consistent,
and lockstep cannot catch it because both engines share the guess.
That is the silent-compounding failure mode the harness exists to
prevent.

To close this we need the per-instruction reference pages for the
search instructions. Those pages would also pin the skip conventions
above, which are currently derived rather than documented.

## Validation of the rewrite

- **Manual figures as unit tests.** Figures 6-2 through 6-6
  transcribed with exact expected link and descriptor values after each
  operation, plus the double-dequeue NOTE and both skip edge cases.
  30 assertions, all passing. `tools/queue_test.cpp`; build with
  `g++ -std=c++17 -o /tmp/qtest tools/queue_test.cpp && /tmp/qtest`.
- **Live session.** Full login, character creation, 12 `L`→`P`
  iterations, clean shutdown, no new exceptions. `ENQT` and `DEQUE` now
  run every turn through `LOCK_FILE` with corrected link order.
- **Not yet exercised live:** `ENQH` in any form, and `ENQT` with an
  explicit (non-`-1`) reference element. Both validated only against
  the figures. They become reachable if the heap free-list path ever
  runs, which additionally requires `QSEARCH`.

## Original implementation (preserved for reference)

The pre-rewrite ENQT/DEQUE, for the record — note `[ac2+0]` used as the
backward link and the absence of any link clearing in DEQUE:

```cpp
case ENQT:
  if(machine.ac[1]==-1) {
    int32_t tail=machine.memory->read_wide(machine.ac[0]+2);
    if(tail==-1) {
      machine.memory->write_wide(machine.ac[0], machine.ac[2]);
      machine.memory->write_wide(machine.ac[0]+2, machine.ac[2]);
      machine.memory->write_wide(machine.ac[2], -1);
      machine.memory->write_wide(machine.ac[2]+2, -1);
      return copy_segment(address, address+1);
    }
    else {
      machine.memory->write_wide(machine.ac[0]+2, machine.ac[2]);
      machine.memory->write_wide(tail+2, machine.ac[2]);
      machine.memory->write_wide(machine.ac[2], tail);
      machine.memory->write_wide(machine.ac[2]+2, -1);
    }
  }
  else {
    int32_t tail=machine.memory->read_wide(machine.ac[0]+2);
    if(tail==machine.ac[1]) {
      machine.memory->write_wide(machine.ac[1]+2, machine.ac[2]);
      machine.memory->write_wide(machine.ac[0]+2, machine.ac[2]);
      machine.memory->write_wide(machine.ac[2], tail);
      machine.memory->write_wide(machine.ac[2]+2, -1);
    }
    else {
      next=machine.memory->read_wide(machine.ac[1]+2);
      machine.memory->write_wide(machine.ac[2]+2, next);
      machine.memory->write_wide(machine.ac[1]+2, machine.ac[2]);
      machine.memory->write_wide(machine.ac[2], machine.ac[1]);
      machine.memory->write_wide(next, machine.ac[2]);
    }
  }
  return copy_segment(address, address+2);

case DEQUE:
  if(machine.ac[1]==-1)
    machine.ac[1]=machine.memory->read_wide(machine.ac[0]);
  if(machine.ac[1]==-1)
    return copy_segment(address, address+1);
  prev=machine.memory->read_wide(machine.ac[1]);
  next=machine.memory->read_wide(machine.ac[1]+2);
  if(prev==-1)
    machine.memory->write_wide(machine.ac[0], next);
  else
    machine.memory->write_wide(prev+2, next);
  if(next==-1)
    machine.memory->write_wide(machine.ac[0]+2, prev);
  else
    machine.memory->write_wide(next, prev);
  if(next==-1 && prev==-1)
    return copy_segment(address, address+1);
  return copy_segment(address, address+2);
```

## Open items

1. `QSEARCH` — needs the reference pages (blocks I.ALLOC, ?LIB_ERROR,
   and the heap cluster for translation).
1b. `XCT` — unimplemented; blocks the same free-list insert path and
   also appears in `I.GOTO` (0x7017ECF4). Simpler than QSEARCH (execute
   the instruction in a register) but not yet done.
2. Skip conventions are derived from one call site each; the reference
   pages would confirm them.
3. `ENQH` / explicit-reference `ENQT` have no live validation.
4. Whether any queue descriptor lives in a file-backed shared page and
   therefore persists across runs — if so, the link-order change alters
   on-disk layout. Not investigated; `LOCK_FILE`'s queue appears to be
   rebuilt per run.
