# Project 17 — stack_offset (mid-window checkpoints) + QUEST base record (REPORT)

**Session:** Aug 22 2026. **Verdict: LANDED. The M4bNotes ruling is
implemented (with one Stage-0 timing amendment, ratified by the user),
QUEST is the base record, and the full battery is GREEN — div=0 on all
five legs, the P16 mid-window condition passing with direct evidence.**
Branch p17-stackoffset, merged to main; runner task 020.

## 0. Stage-0 ratification — subtraction at the write-mode WSAVS, not the LCALL

The M4bNotes ruling wrote "the LCALL subtracts its own 2·argc". Stage-0
analysis found a one-instruction gap: the quantum boundary can land
BETWEEN the decorated LCALL and the callee's WSAVS. At that boundary the
master's wsp still leads the shadow by 2·argc (args elided on the clone;
the marker push is symmetric), so zeroing the offset at the LCALL would
leave that boundary un-coverable — violating the ruling's own claim 4
("every instruction boundary stays a valid compare point").

**Ratified (user, this session):** the −2·argc consumption moves to the
**write-mode WSAVS** (in `push_record`, on the caller's record, just
before the callee's record is pushed) — the same event where the record
arithmetic (`elided_args` in `shift_after`) takes over the accounting,
so the offset hands off with no gap. The user's condition — the
subtraction fires only for a DECORATED LCALL — is satisfied
structurally: write mode exists only when the args-written flag was set,
and only a decorated LCALL sets it. Copy-mode/undecorated callers can
never reach the subtraction.

**The battery then vindicated the amendment empirically:** m-leg pair
`seq=006039` landed at pc=70166E1C (post-LCALL) with off=8 and PASSED.
Under LCALL-time subtraction that exact pair would have been div=1.

## 1. What was built (delta on the P16 mechanism; master untouched)

- **Mapper.hpp** — `LiveRecord.stack_offset` (init 0 at record push);
  `checkpoint_offset()` (`records_.empty() ? 0 : back().stack_offset` —
  empty → 0 → the closed form exactly); `note_arg_write(m, wides)`.
- **Mapper.cpp** — `note_arg_write`: `back().stack_offset += 2·wides`,
  empty → mapper_abort (fail-loud; unreachable once QUEST is the base
  record). In `push_record`, write mode consumes the caller-record
  offset (`−2·argc`, empty → mapper_abort) before pushing the callee's
  record.
- **EagleStack.cpp** — the XPEF/LPEF caller-map hits call
  `note_arg_write(m, 1)`; the decorated LCALL deliberately does NOT
  touch the offset (comment in place); `off=` added to the ARGWR/LCALL
  trace lines.
- **Lockstep.cpp** — the checkpoint compare (the P16 divergence site,
  `compare_pair`) is now
  `master.wsp != clone.shadow_wsp() + clone.mapper.checkpoint_offset()`;
  `off=` added to pair trace lines and divergence dumps.
- **quest.addrbook** — QUEST uncommented (line 7). Nothing else: the
  layout already reserved its block (74000000, 80 words, next entry at
  74000060), and the flag-clear + book rule routes it to ordinary M4a
  copy mode. 102 live / 40 pages.

Only decorated ops move the offset: MSP, the tombstone push, and all
non-decorated pushes move both wsps equally and touch it not at all.
`unwind_to`'s suffix-pop and `wrtn_fixup`'s pop carry/discard it with
zero new code, as the ruling predicted.

## 2. QUEST as the base record — boot result

Exactly as ruled: ordinary copy-mode migration, no special handling.
Every leg's FIRST redirect line is

```
WSAVS QUEST  mode=C pc=7015C005 area_wfp=7400000A argc=0 frame=34
             real_wsp=7000108E shadow_wsp=700010DC master_wfp=70001098 depth=1
```

- Loader-entry state is clean through the redirect: the boot [wsp] frame
  word yields argc=0 (== book max), the first-record branch latches I2,
  and `fwd(W)` on the empty chain gives the degenerate base formula.
  Boot is lockstep-clean on all legs (`quest_base=1` in every verdict
  row) — per the ruling's own test, any error would have diverged at
  ~instruction 1.
- `records_` is never empty from boot on; the empty→abort guards in
  `note_arg_write`/write-mode consume never fired (mapper_aborts=0
  everywhere).

## 3. Stage 2 battery (runner task 020; inj at normal driver speed, m/fo/abort/play at 10x)

```
fo    div=0 i2=0 probes=0 m4b_aborts=0 mapper_aborts=0 writeWSAVS=891 argwr=3564 quest_base=1 midwin_pass=10 end=clean+FATAL
m     div=0 i2=0 probes=0 m4b_aborts=0 mapper_aborts=0 writeWSAVS=594 argwr=2376 quest_base=1 midwin_pass=7  end=I.STOP+FATAL
inj   div=0 i2=0 probes=0 m4b_aborts=0 mapper_aborts=0 writeWSAVS=594 argwr=2376 quest_base=1 midwin_pass=6  end=clean+FATAL
abort div=0 i2=0 probes=0 m4b_aborts=0 mapper_aborts=0 writeWSAVS=0   argwr=0    quest_base=1 midwin_pass=0  end=WORLD-ABORT+FATAL
play  div=0 i2=0 probes=0 m4b_aborts=0 mapper_aborts=0 writeWSAVS=891 argwr=3564 quest_base=1 midwin_pass=10 end=clean+FATAL
```

**div=0 on every leg** (task 018 was div=1 on every site-reaching leg).
Volumes are an order of magnitude past P16's failure point: 594 complete
write-mode DIST calls in m/inj, 891 in fo/play (P16's m leg diverged at
call 85). Arithmetic exact throughout: argwr = 4·writeWSAVS on every
site-reaching leg, write-mode WRTN count == WSAVS count, copy mode
coexisting (51 mode=C frames in the m leg), gcalls/redirect cross-checks
clean. The abort leg's zero site coverage is the pre-existing leg shape
(terminal pc 7016871D sits inside DIST's body, fires on the first DIST
call from ANY site — noted in the P16 report), not a regression; its job
is the both-engines abort banner, which it delivered.

**The mid-window pairs — the P16 target case, now passing.** m-leg
evidence (results/020…/m.midwindow_pairs.txt; every leg's file shows the
same shape, 33 such pairs across the battery, zero DIVERGED):

```
pair pc=70166E19 insns=500 clone_pc=70166E19 off=6   ← THE P16 case: k=3 pushes, 2k=6
pair pc=70166E19 insns=500 clone_pc=70166E19 off=6
pair pc=70166E0E insns=500 clone_pc=70166E0E off=0   ← boundary at the push pc, pre-execution
pair pc=70166E1C insns=500 clone_pc=70166E1C off=8   ← post-LCALL (the Stage-0 amendment's case)
pair pc=70166E14 insns=500 clone_pc=70166E14 off=4
pair pc=70166E19 insns=500 clone_pc=70166E19 off=6
pair pc=70166E10 insns=500 clone_pc=70166E10 off=2
```

`shadow + stack_offset == master.wsp` at each (the pair passed — a
mismatch is div by construction). The 70166E19/off=6 rows are literally
the P16 §4 dump's quantum-boundary case (master 70001FD6 vs shadow
70001FD0, off by 2k=6), now green. The off values sweep 0/2/4/6/8 —
every window phase observed, plus the post-LCALL boundary.

**The window trace (ruled shape, amended timing):**

```
ARGWR pc=70166E0E slot=74003950 off=2
ARGWR pc=70166E10 slot=74003952 off=4
ARGWR pc=70166E14 slot=74003954 off=6
ARGWR pc=70166E19 slot=74003956 off=8
LCALL pc=70166E1C slot=74003958 value=80000004 off=8   ← holds (marker symmetric)
WSAVS DIST mode=W  shadow_wsp=70001FE4 master_wfp=70001FE4  ← consumed to 0, anchors exact
```

## 4. Frictions / notes

1. The LCALL-subtraction timing (Section 0) — the one design-vs-reality
   item; amended at Stage 0 with a user ruling, then confirmed by pair
   seq=006039.
2. Noted at Stage 0, left per the ruling (fail-loud, no code): a
   mid-window SIGNAL whose O.ON handler lives in the SAME frame as the
   window would strand a nonzero offset on the surviving record
   (suffix-pop only clears cut frames). No QUEST window contains a
   call/syscall, so only an async signal inside a ≤6-instruction span
   can hit it; if one ever does, the next pair diverges loudly. A
   one-line zero-on-unwind is the fix if it ever fires.
3. The container reaps background processes between tool invocations —
   in-container smoke needs server + driver in a single shell command.
   (Runner flow unaffected.)

## 5. State / how to resume

- main = branch p17-stackoffset merged (commit 07070de + landing docs).
  Book at 102 live (QUEST in). Task 020 results in
  results/020-p17-stackoffset-battery/ (DONE).
- Rollback levers unchanged: unset QUEST_PUSH_MAP → pure M4a on 102;
  re-comment QUEST → the 101 book.
- NEXT (later projects, per the binding scope): widen M4b to N sites
  from quest.argmap (mechanism + checkpoint accounting now both proven);
  the WPSH multi-slot arg case; then M4c residue.
