# Project 25 — byte addressing (M8/wp/bp) + the call ledger closed at 566/566

Session Aug 29 2026, solo implementation, plan gate + landing review.
TREE VINTAGE: the P24-integrated tree (the uploaded-archive tree whose
quest.ir2.book provenance hashes match: dis 1f9153c0…, blocks.split
a5efa05f…, pushmap.M4 b8953659…, argmap 39c42d4c…). Branch:
p25-byte-addressing; battery: task 035 (queued; 034 parallel
template).

## 1. Outcome

**566/566 decorated call sites lowered** (target was 564/566 at
prompt time; the user reversed the borrow exclusion at the plan gate
— see §2). Byte addressing landed in full: byte-EA values, M8 loads/
stores, wp/bp pointer builders (register-relative only), byte-pointer
literals `0xW:b` (dis fold notation, dump-greppable; `:` is
byte-select exclusively, bitp reserved for M1), `*`; `<<` removed
from the spec (never implemented). Post-landing additions on the same
branch: the `assert(e[, "msg"])` statement (clone prints the
statement and DETACHES on failure; master continues unverified;
compare_pair gained a detached early-out that also closes the
straddling-batch latent race), IR.md cross-references in lower.py and
IRExec.cpp headers, and docs/DISASSEMBLER_BYTE_OPERANDS.md (the
defect's standing record — fix deferred by user ruling). Embeds 31,116 → 27,600; statements 26,417 →
30,013. Local gates 3/3 green (k1fo, k1play book K=1 strict; stock
fo), 0 divergences. The census/derivation record is
docs/Project25/ByteEA.md; docs/IR.md carries the spec amendment.

## 2. Rulings taken at the plan gate (user, Aug 29, this session)

1. **Borrow pair back IN scope**: bracket WPSH/WPOP stay @addr
   INSTRUCTIONS inside the lowered block; args become stores in
   place; no reordering, no borrow ops. (Reverses the prompt's ruled
   exclusion; 564 → 566.)
2. **Grammar**: wp(b,d) and bp(b,d) pointer builders — masking lives
   in the executor, never in emitted text (the user rejected the
   spelled-out `& 0x1FFFFFFF | 0xE0000000` form; wp also retires
   pef_value's old spelled word mask). Offset-taking two-arg form per
   the user's spelling. `*` over `<<`.
3. **M8 index RAW** (no wrap): everything is ring 7; byte pointers
   are self-segmented; the hardware masks only at construction
   (X-forms), never at use. A garbage pointer faults in read_byte
   identically to the emulated instruction (loud, METHOD §8).
4. **Scope = full tier**: B-form pushes + XLEFB/LLEFB values +
   M8 loads/stores (XLDB/XSTB/LLDB/LSTB/WLDB/WSTB), one landing.
5. **Disassembler byte-operand rendering fix is the USER'S** (they
   keep the disassembler tools). lower.py meanwhile reconstructs
   `@` → bit 31 on L-form byte operands (user instruction) and
   accepts both word-form conventions.

## 3. Findings (details and evidence in ByteEA.md)

- The P23 "96 B-form" bucket was 78 byte + 18 word-form parse gaps
  (fold regex; L-form `@` bit stripping) — incl. all 6 XCALLs
  (OP_EDIT, not LIST_PLAYERS as the prompt's liveness note had it).
- The "indirect LPEFB at 7015C2B4" dissolved: raw wide 0xE0001998
  (read out of QUEST.PR), no ii=0 arm and no indirection in either
  engine's L byte helper — a constant byte pointer to word
  0x70000CCC, misrendered by the disassembler.
- Disassembler defect flagged (§14): 184 lines (183 LLEFB + 1
  LPEFB) print `@` + a wrong value for byte-pointer constants.
  Expected regen diff = exactly those lines. Fix owned by the user.
- IR.md §8's parked byte formula was wrong for the L-form (masks in
  NO mode) and X pc-rel; §8 corrected (METHOD §11).
- IR.md listed `<<`; IRExec never implemented it. Removed; `*` added.
- Latent unwrapped XLEF/LLEF value emission (`acN = ac3 + 26`,
  hardware applies copy_segment) — identity-in-practice, unproven;
  wp() now carries the wrap everywhere (recorded, not silently
  fixed).

## 4. Implementation

(§4 describes the landing as reviewed at the plan gate; the
post-landing additions — :b literals, assert, cross-refs, defect doc
— are itemized in §1 and the worklog, and specified in IR.md.)

- tools/lower.py: pef_value fold regex; reconcile_at (both `@`
  conventions, refuse bit-without-@); byte_ea per the emulator
  source (per-mode table in ByteEA.md §2, X pc-rel folds verified
  value-vs-fold with refusal); wp/bp emission; push_stores (WPSH
  x,a → wides ascending group stores, group==wides enforced,
  operand order pinned from the decoder bit layout); borrow veto
  dropped; XLEFB/LLEFB + 6 byte load/store ops in lower_one.
- hw/IRExec.cpp: MEM8 (raw index; read_byte/write_byte
  pass-through; & 0xFF store), WP/BP (copy_segment-style wrap /
  Machine::set_byte_segment — the same helpers), MUL, M8 lvalue;
  loader refusals intact (negative test: a malformed bp line
  refuses at load with the exact token — the P23 pushmap-parser
  lesson held).
- Artifacts: quest.ir2.book / quest.ir2.stock regenerated (566
  call, 28,661 expr, 27,600 instr, 1,352 argpush; skips = the 3
  standing block exclusions only).

## 5. Validation

Local gates (METHOD §15, ~15 min, split pair):

| leg | cfg | result |
|---|---|---|
| k1fo | book K=1, FAIL_OPEN=USER_DATA_FILE, L→P | **0 div**, 1,607 IR blocks |
| k1play | book K=1, movement+menus | **0 div**, 2,284 IR blocks |
| st-fo | stock K=50, FAIL_OPEN | **0 div**, 1,580 IR blocks |

Predicted-live (stated in advance) vs observed, by IRExec
first-execution lines: INIT_OBJ_TBL 7015C2B2 (the LPEFB site) LIVE
at startup in both book legs; LOCK_FILE 70177F03 LIVE (every-turn
path); TERRAIN 7015C4FB + 5 more WPSH blocks LIVE; 7/78 B-form
(GET_INPUT) blocks LIVE. Borrow blocks 7015F795/7016A3C7
(UPDATE_SCREENS): NOT reachable by the scripted legs — carried on
census classification (plain wp arg stores + the P20-proven bracket
hooks), recorded as-is per the carry-coverage precedent.

Battery: task 035 RESULT: attempt 1 **12/13 GREEN, 1 RED — ruled
flake by the user (Aug 29)**; the user let the runner's automatic
retries ride, and **attempt 3 came back 13/13 GREEN (DONE)** —
inj-emu reached its FATAL, confirming reach variance rather than
regression. Attempt-1 detail: Every IR leg green: book K=1 (382,593 pairs), book
K=50, 5.5M-pair patient play, stock ×2, inj/abort with exact drop
counts — 0 divergences, blk_mismatch=0, strict gate held. The red
was inj-emu, the ALL-EMULATED isolation leg (no IR loaded; executes
zero P25 code): the armed pc 7016A896 (FIND_OBJECT+0x7) was never
reached, the session played through and ended I.STOP instead of
FATAL, div=0. Same leg was green in 034's proof run and in
attempt 3; armed-pc reach variance in the play driver, now
demonstrated both ways. Battery coverage (verdicts.txt):
LPEFB block 7015C2B2 at 9 hits, LOCK_FILE 70177F03 at 8, 6/25 WPSH
blocks live (TERRAIN 8), 7/78 B-form blocks live, borrow 0/2
(census-carried as ruled).

## 6. Corrections recorded (METHOD §10/§11, this session's own)

- The session first reported the disassembler defect as "3 lines"
  from a single-constant grep; the pattern grep found 184. §10
  class; corrected before any work depended on it.
- The prompt's LIST_PLAYERS.3 liveness pointer was stale (those
  sites lowered in P23); the blocked XCALLs are OP_EDIT sites.

## 7. Tempting adjacencies NOT taken (boundary 1)

t-places, `save`, conditional exits, borrow→t-place conversion,
`#*`/`#/`, M1, any checker change, the disassembler fix itself.
The wp-in-index-position emission (identity over the executor wrap)
is deliberately uniform rather than minimal — flagged for review
rather than argued here.

## 8. TODO / next session

1. Battery 035 verdict (runner box) → append here.
2. Disassembler fix DEFERRED (user ruling, post-landing: the
   byteIndexed masking is buggy in multiple ways and too risky to
   touch) — the lower.py shim is standing, not temporary.
   docs/DISASSEMBLER_BYTE_OPERANDS.md records the defect, the
   compensation contract, and the §14 protocol if a fix is ever
   attempted.
3. P26: t-places (borrows convert; the two @addr bracket pairs in
   lowered blocks are the pilot's first customers), conditional
   exits.
4. UPDATE_SCREENS reachability (the two borrow blocks): find a
   driver path or record permanently as census-carried.

## 9. Reviewer notes (integration review, Aug 29 2026)

Verdict: GREEN. Verified: artifact stats exact (18,006 blocks / 566
call / 27,600 instr / 1,352 argslot; the 3 omissions are BD6B +
the two ENQT/DEQUE blocks), provenance hashes match the live inputs,
tripwire intact, tree vintage correctly stated (the P24-integrated
tree — the first session to comply with the vintage rule).
Independent K=1 book smoke: 2,162 IR blocks incl. the LPEFB site
7015C2B2 live at startup, 0 divergences. Battery evidence confirmed
in-repo: ATTEMPTS=3, attempt-3 verdicts 13/13 with inj-emu
end=FATAL — the attempt-1 red was endpoint-reach variance in an
all-emulated leg executing zero P25 code (div=0 throughout), so the
flake ruling is evidence-backed, and the attempt-1 detail is
preserved in §5 since the runner overwrites per-attempt artifacts.

One report-internal contradiction, resolved not defended: §7 lists
"any checker change" as not taken, while §1/§4 land assert_detach +
compare_pair's detached early-out. The worklog shows the assert op
was a USER REQUEST (end of session), which makes the harness change
its necessary mechanism, reviewed as such: the early-out only skips
pairs of already-detached ordinals (whose clone halves are truncated
by design), cannot mask a divergence on an attached ordinal, and
composes correctly with the detached-master tripwire — an assert
failure now detaches the clone and the world dies loudly at the next
turn dispatch unless the master terminates first. §7's blanket line
should have said "no checker changes beyond the user-requested
assert machinery."

Integration: restored docs/Project24/{F6_inj.divdump,
F6_abort.divdump, battery_verdicts.txt}, which this tree had dropped
(P24's landing evidence stays in the record); branch merged to main.
