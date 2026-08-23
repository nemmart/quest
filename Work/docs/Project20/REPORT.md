# Project 20 REPORT — WPSH/WPOP frame-borrows off-stack. M4 DE-STACKIFICATION COMPLETE.

**Landed Aug 23 2026, branch p20-borrows, battery task 028 GREEN.**

All 23 WPSH/WPOP frame-borrow brackets are redirected off the stack into
a 23-slot reserved block at the area base. With frames (M4a), args (M4b
tranches A–D), and now the borrows all off the stack, the game code's
last stack residue is gone: **M4 is done.** (WMSP/STASP string buffers
stay put by design — purely local, lockstep-transparent, they dissolve
at translation; M4cNotes.md. ON-handler control flow is M5.)

## What was reused vs new (per the prompt: mostly REUSE)

REUSED, no code: the WPSH store is the stock P18 hook — a borrow WPSH
r,r is the wides=1 single-register path (`group=((AA-XX)&3)+1 == 1 ==`
the loader's default wides), verified before relying on it. The offset
machinery is P17's stack_offset untouched.

NEW:
1. **The one new hook** — `EagleStack.cpp case WPOP`: caller-map hit ⇒
   load AC[r] from the slot (r from the instruction's OWN operand,
   single-register enforced fail-loud), do not pop, wsp untouched,
   `note_arg_pop(m,1)`. First decorated POP in the system.
2. `Mapper::note_arg_pop` — the −2·wides mirror of note_arg_write, same
   fail-loud empty-records rule. Net zero across a bracket.
3. Tool: ArgWindows borrow pass (below). Loader: `borrow_slots N` book
   line + the third pushmap validation arm (`borrow <pc> <slot>`, slot ∈
   [BASE, BASE+4N), 4-aligned). Book builder: reserves [BASE, BASE+4N)
   before frame layout, shift derived from N. Generators:
   gen_pushmap_borrows.py (freezes ABSOLUTE base+4n at generation — no
   runtime arithmetic, same discipline as argN) and rebase_pushmaps.py.

## The proof (step 1)

ArgWindows detects brackets by WPOP back-scan (WPOP occurs at exactly 23
pcs in the whole program, all bracket closes) and proves each
single-block with the SAME targets set as the arg-window proof:
`targets.subSet(wpsh+1, wpop+1)` empty (nothing lands in the interior or
on the WPOP), plus the interior discipline (no flow / stack ops / skips /
calls / undecoded; the interior is exactly LDAFP + one frame-relative
store). Result: **23/23 proven, 0 flagged**; arg lines byte-identical to
P19's 1352. Emission: `_PAIRS count 23` first line + two
`_PAIRS slotN at PC` lines per pair — 0-indexed, flat base+4n, a
deliberately DIFFERENT word and equation than the 1-indexed
argN-at-wfp−10−2N, so no reader can conflate the two. Cross-checked
against the coordinator scan (borrowmap.crosscheck.txt): pcs, numbering,
slots, AC3×22 + AC2×1 all agree (the P18 two-generator precedent). Per
the trust-anchor note in PROMPT.md, the proof stakes nothing new: it
rides the same StartStop/Follow targets set that 566 arg sites already
hard-stress-tested green.

## The book shift (step 2) — and one judgment call

build_address_book.py reads `_PAIRS count N`, reserves N slots, starts
frames at BASE+4N. N=23 ⇒ frames at **0x7400005C**, matching the design
cross-check. The regenerated book was verified to be EXACTLY the old
book with +0x5C on every alloc/wfp base, live set identical (102, incl.
the P17 hand-uncommented QUEST — now `--also-live QUEST`).

**Judgment call, flagged for review:** the loader's absolute 16-word
alignment check on alloc_base conflicts with a 0x5C frame floor (92 is
not a multiple of 16). Nothing functional depends on absolute alignment
(audited every consumer: the loader assertion + a comment; T-containment
rests on the size+16 stride, not the grid), and the design pins
0x7400005C twice, so the check was made RELATIVE to frames_base:
`((alloc_base − (BASE+4N)) & 15) == 0`. borrow_slots=0 recovers the old
check bit-for-bit. If this should instead be ruled "pad the block to
16", it is a 3-line change in the builder + regen.

## Map regeneration (step 3) + straggler scan

Every existing pushmap stores absolutes, so all six were rebased +0x5C —
and PROVEN equal to regeneration by recomputing every line against the
new book: all 1352 argmap transcriptions (slot = wfp−10−2N), every push
wide inside its arg region, every call slot equal to its named callee's
marker. quest.pushmap.M4 = ABCD + borrows (1313 push / 566 call / 46
borrow lines). Straggler scan: no pre-shift 0x74xxxx literal anywhere in
code, scripts, or drivers outside the maps/book themselves.

## Battery (task 028) — GREEN

All five legs div=0, i2=0, probes=0, m4b/mapper aborts=0. Load check:
23 slots at [74000000, 7400005C), 1313/566/46. Baseline vs task 026 is
exact: argwr deltas are precisely the borrow stores (m/fo 2553−2452=101,
inj 2581−2480=101, abort 13−12=1); wWSAVS/wWRTN unchanged on comparable
legs (630/630, 635/633, 8/6).

**AC round-trip BY VALUE** (the criterion that would have caught a
silent P18-ordering-style bug): every fired bracket verified — the value
stored at the WPSH equals the value loaded at its WPOP, the ARGRD's
offset equals the ARGWR's + 2 (the exactly-1-wide mid-bracket window),
and every bracket closes. m/fo 101 round-trips, inj 101, abort 1, play
146; 0 mismatches, 0 unclosed, 0 misordered.

## Coverage

3 of 23 pairs are driver-reachable: slot 0x40/pair 16 (WPOP 70166ED7),
0x4C/pair 19 (7016BCEA), 0x50/pair 20 (7016DF93); the abort leg reaches
pair 20 alone. The other 20 pairs are decorated, BB-proven, and
unexercised by the current drivers — the same low-risk coverage-backlog
pattern as P19's C/D sites and M4a's 57 routines; swept
opportunistically. **No unprovable brackets exist — nothing was left
on-stack.**

## Files

Tools/ArgWindows.java; Work/c_src/tools/{build_address_book.py,
gen_pushmap_borrows.py, rebase_pushmaps.py}; Work/c_src/hw/
{AddressBook.cpp,.hpp, Mapper.cpp,.hpp, EagleStack.cpp};
Work/c_src/{quest.addrbook, quest.pushmap.*, quest.pushmap.M4};
Disassembled/{quest.argmap, quest.callsites}; tasks/028.
