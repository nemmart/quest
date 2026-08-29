# Project 25 — byte addressing + the closed call ledger (census of record)

Session: Aug 29 2026. Tree vintage: the P24-integrated tree (ir2
input hashes match quest.ir2.book's recorded provenance: dis
1f9153c0…, blocks.split a5efa05f…, pushmap.M4 b8953659…, argmap
39c42d4c…). Target ruled at the plan gate: **566/566** (the user
reversed the borrow exclusion in-session: bracket as @addr
instructions, args as stores).

## 1. The "96 B-form" bucket was three buckets

Replaying lower.py's own site gate over the 566 decorated sites
(quest.pushmap.M4) against the P24 artifacts: 443 lowered, 123 not,
and the 123 decompose as:

| bucket | sites | what it actually was |
|---|---|---|
| truly byte-blocked | **78** | 77 `XPEFB [ac2/ac3+d]` (all GET_INPUT callers) + 1 `LPEFB @[abs]` (INIT_OBJ_TBL) |
| pef_value parse gaps, word-form | **18** | 15 `XPEF [pc+d] (0xFOLD)` — the regex `\S+;` could not span the space before the fold (9 DIED sites + all 6 XCALLs = the OP_EDIT sites); 3 `LPEF @[abs]` (LOCK_FILE ×2, UNLOCK_FILE) — the dis strips the L-form indirect bit into the `@` prefix, the parser expected it in the number |
| WPSH multi-wide | **25** | TERRAIN 13, TERRITORY 7, RETURN_MESSAGE 5; every site a single `WPSH 0,1` or `WPSH 0,2`, group == map wides, no deviant shape |
| borrow-adjacent | **2** | UPDATE_SCREENS ×2; every push a plain expressible XPEF — the bracket was the only blocker |

P23's REPORT §5 lumped the first two buckets as "96 B-form". 18 of
those sites needed no byte machinery at all. Correction recorded per
METHOD §11. (Also corrected: the P25 prompt's liveness pointer — the
LIST_PLAYERS.3 sites were already lowered in P23; the blocked XCALLs
are OP_EDIT sites.)

## 2. Byte-EA semantics, read from the source (METHOD §5)

Machine::eagle_x_byte_indexed / eagle_l_byte_indexed, identical in
both engines (so lockstep could never adjudicate this — METHOD §2):

| form/mode | EA value |
|---|---|
| X ii=0 | set_byte_segment(seg, raw16) — raw16 NOT sign-extended |
| X ii=1 (pc) | (pc+1)*2 + sext16(raw16) — NO masking (constant at emit) |
| X ii=2/3 | set_byte_segment(seg, acN*2 + sext16(raw16)) → **bp(acN, d)** |
| L ii=0 | the raw 32-bit displacement, unchanged (constant) |
| L ii=1 (pc) | (pc+1)*2 + raw32 — NO masking (constant) |
| L ii=2/3 | acN*2 + raw32 — NO masking → **acN*2 + 0x…** |

Findings against the spec and the toolchain:

- **IR.md §8's parked formula** ((base<<1)+disp)&0x1FFFFFFF|(seg<<29)
  described only X ii=0/2/3. The L-form masks in NO mode; X pc-rel
  masks not at all (numerically coincidental for seg-7 pcs). §8
  corrected.
- **The "indirect LPEFB at 7015C2B4" hard case dissolved.** Raw words
  in QUEST.PR (the only ii=0 LPEFB in the binary, offset 0x28FD68):
  displacement 0xE0001998. Neither engine's L byte helper has any
  indirection or an ii=0 arm — the raw wide is the pushed value, and
  0xE0001998 is precisely the byte pointer for word 0x70000CCC. No
  dereference exists to lower.
- **Disassembler defect (§14 flag; fix owned by the user).** For
  L-form BYTE operands, bit 31 is data (top bit of a byte-pointer
  constant), not an indirect flag; the renderer strips it into `@`
  and prints the wrong value. **184 lines** (183 LLEFB + 1 LPEFB;
  the 10 non-@ LLEFB lines are genuine small relative displacements).
  Note the session first counted 3 by grepping one constant — a
  METHOD §10-class error, corrected by the pattern grep. Until the
  listings regenerate, lower.py reconstructs raw = printed|bit31 on
  '@' byte operands (user instruction) and accepts BOTH word-form
  conventions (X keeps the bit in the number with decorative '@';
  L strips it), refusing bit-set-without-'@'.
- **IR.md listed `<<` as a binop; IRExec never implemented it.**
  Removed in favor of `*` (host multiply), which the L-form byte EA
  needs anyway.
- **Latent unwrapped XLEF/LLEF values.** The old lower.py emitted
  `acN = ac3 + 26` for XLEF where the hardware applies copy_segment —
  identity for seg-7 frame pointers in practice, but unproven, and
  inconsistent with pef_value's wrapped emission of the same
  computation. wp() now carries the wrap in the executor for every
  base-indexed word EA, value and index positions alike (index wrap
  on a wp result is identity).

## 3. What lower.py emits now (the ruled grammar)

- Word pushes: `M32[slot] = wp(acN, d)` / constant / `R[…]`.
- Byte pushes and XLEFB/LLEFB values: per the §2 table — `bp(acN, d)`
  (X), `acN*2 + 0x…` (L register-indexed), constants otherwise; the
  X pc-relative fold `(0xWORD:B)` is verified against the computed
  value, refusing mismatch.
- Byte loads/stores: `acN = M8[…]`, `M8[…] = zx8(acN)`; WLDB/WSTB are
  `M8[acII]` with the register value raw (registerRegister renders
  II,AA — Disassembler.java bits 14:13 then 12:11; EagleGeneral::setup
  AA=12:11, II=14:13).
- WPSH x,a at a decorated push pc: wides ascending stores
  `M32[slot+2k] = ac((x+k)&3)` — AC[XX] at the base slot, the
  emulated hook's verified ordering; group size must equal the map's
  wides (the same check the hook enforces; ((a-x)&3)+1 is ambiguous
  between (x,a) and (a,x) readings for group 3, so the rendering
  order was pinned from the decoder bit layout, not arithmetic).
- Borrow brackets in decorated blocks: `@addr WPSH` / `@addr WPOP`
  instruction pairs among the lowered statements; hooks fire on the
  normal execute path; accounting nets zero.

## 4. Ledger and artifact deltas

Regenerated book: **566 call sites lowered (566/566)**, skips = the 3
standing block exclusions only (7015BD6B, ENQT/DEQUE ×2). Census:
28,661 expr / 27,600 instr / 1,352 argpush / 566 call / 165 ret /
3,243 goto. Embeds 31,116 → 27,600 (−3,516: 2,907 XLEFB/LLEFB, ~277
byte loads/stores, the rest newly-lowered site pushes and their
neighbors' re-classification); statements 26,417 → 30,013.

## 5. Local gates (METHOD §15; ~15 min) and coverage

| leg | cfg | driver | result |
|---|---|---|---|
| k1fo | book, K=1 strict | failopen | **0 div**, 1,607 IR blocks |
| k1play | book, K=1 strict | play | **0 div**, 2,284 IR blocks |
| st-fo | stock, K=50 | failopen | **0 div**, 1,580 IR blocks |

Predicted-live blocks (stated before the runs) and outcomes, by
IRExec first-execution lines:

- 7015C2B2 (INIT_OBJ_TBL — **the LPEFB site**): LIVE, k1fo+k1play
  (startup path, as predicted).
- 70177F03 (LOCK_FILE, LPEF@): LIVE, k1fo. 70177EF4/70177E7A: not
  reached by these drivers (the turn-cadence path the drivers took
  routed via 70177F03).
- 7015C4FB (TERRAIN WPSH): LIVE, k1fo. k1play additionally executed
  WPSH blocks 70160E9A (TERRITORY), 701670D7, 70171164, 70171325,
  70172642 — 6/25 WPSH blocks live total.
- B-form blocks: 7/78 live across the two book legs (GET_INPUT is
  input-loop-dependent; the drivers reach the handlers they script).
- Borrow blocks 7015F795/7016A3C7 (UPDATE_SCREENS): **0/2 — not
  reachable by the scripted legs.** Carried on census classification:
  the sites' arg pushes are plain XPEF wp-stores (the most-exercised
  statement class in the file), and the bracket instructions execute
  the P20 slot-redirect hooks unchanged (3/23 brackets
  driver-reachable since P20). Recorded as-is, per the carry-coverage
  precedent; no tuning around evidence.

Server battery: task 035 (034 parallel template + coverage line)
carries the breadth; landing bar 13/13 green with the above coverage
appended to verdicts.
