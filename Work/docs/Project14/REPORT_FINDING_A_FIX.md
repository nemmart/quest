# Project 14 — Finding A fix report (the stack-leg >= attribution)

*Implemented Aug 22 2026 under the FINDING_A_MAPPER_FIX.md ruling.
Status: LANDED and verified in-container on the full 101 book. One
material correction to the ruling doc's mechanism narrative is recorded
in §1 — the RULING (attribute to the owning activation; close the round
trip; verify both directions) stands; its geometry was inverted.*

## 0. The change

`hw/Mapper.cpp`, ToMaster stack leg: `s > it->W` → `s >= it->W`, with the
attributed record now reported through `*rec`. Plus the precisely-scoped
overlay predicates in `map_checked` (I4) and `equivalent` (I6) for the
new merge point (§2). ToClone is UNCHANGED. No new record fields. No
other file touched.

## 1. Corrected mechanism (differs from FINDING_A_MAPPER_FIX.md's narrative)

The ruling doc reasoned from the crash dump and said so ("do not eyeball
it from this one dump"). Against the emulator's real layout:

- **This stack grows UP** (`Machine::wide_push`: wsp += 2 then write;
  `WMSP` positive delta is the claim). There is no "descent below the
  frame"; that reading came from comparing a CLONE address (700010E6)
  against MASTER extent bounds — different coordinate systems.
- **Both crashes had s == W exactly** — the live record's own anchor,
  not a tail address. The redirect trace proves it: after each nested
  DIST WRTN, real_wsp returns to 700010E6 = W(DISPLAY_SCREEN), and the
  abort's ac0 == wsp == that value. The game had captured wsp via LDASP
  (the dyn-routine idiom — MSP buffer-base cursors hold the same value),
  and the sync-point register compare fed it to `equivalent()`.
- The old strict `>` skipped the record's own entry and applied the
  OUTER record's shift: 700010E6 + 0x6A = 70001150 = master_wfp − 10,
  INSIDE the frame's master band [lo, hi) — so ToClone resolved it to
  the area and I4 fired. The forward value itself was "correct" only
  under the data-interpretation of s (the frame word's master address);
  but a register holding W is a POSITION value, whose master counterpart
  is the master's wsp = master_wfp + 2*frame = s + the record's OWN
  shift_after.
- That threshold already existed in the codebase: `shadow_wsp` has
  always used `>=` with exactly this rationale ("the record whose frame
  word the clone's wsp still points at counts as above"). The fix makes
  the stack leg agree with it. Verified against the dump: 700010E6 +
  0x4BA = 700015A0 = the traced shadow_wsp = master ac0 at the crash
  pair — the mapped value now MATCHES (0 div), it does not merely avoid
  the abort.
- Why DISPLAY_SCREEN and not every dyn routine: exposure requires a
  register to hold exactly W at a compare pair while the record is
  live — timing-dependent, which also explains the two GTOD-shifted
  crash variants. The fix handles the class, not the specimen.

The ruling's "least-upper-bound attribution over markers" survives as:
the boundary s == W attributes to that record (its own activation); for
the up-growing stack no search change and no new record fields are
needed — the LUB collapses to the `>=` threshold.

## 2. The new merge point (Q2-style, ruled by the same principle)

With `>=`, master_wfp + 2*frame acquires TWO preimages per live record:
the stack anchor W (a position value) and the area's last-local word
area_wfp + 2*frame (a data location). Same two-to-one shape as the Q2
closed-end overlay, at the opposite edge of the band, same resolution by
the same principle: **the inverse resolves to the DATA (the area)** —
which is what ToClone's [lo, hi) walk already does, hence no ToClone
edit — and the stack-leg mapping of u == W asserts the I4/I6 FIXPOINT
(forward(inverse(v)) == v), not the strict trip.

Scoping is precise so every ordinary mapping keeps the STRICT round
trip: area overlay = area-leg source (in_range(u)) with image ≥ extent
end; stack merge = stack-leg source with u == W. Ordinary tail
addresses (u > W), which now also carry a record in the verdict, are
strict as before.

## 3. Verification (in-container, full 101 book — DISPLAY_SCREEN IN)

| leg | result | endpoint |
|---|---|---|
| m (×2, both GTOD crash variants) | GREEN — 0 div, probes 0, anchors exact (READ_IN=4, LOGON=1, GET_INPUT=8, INIT_SCREEN=1, REFRESH_SCREEN=1, HIT_ANY_CHAR=1), cross-check OK; DISPLAY_SCREEN redirect=2 through WRTN both runs | I.STOP detach 7017FCE8 |
| inj (play mode, normal speed, QUEST_INJECT=7016A896:-1:0x2006) | GREEN — 0 div, probes 0, cross-check OK | ?FATAL detach 7017F036 |
| abort (QUEST_TERMINAL=7016871D:ABORT) | GREEN — 0 div | TERMINAL-ABORT both engines, banner wides 1B8A7016 AC037000 |
| fo (QUEST_FAIL_OPEN=USER_DATA_FILE) | RED — **Finding B, byte-identical** (I2 latched 7001715A → 7001714C, same pc): the fix does not touch or mask it | WORLD ABORT (expected; separate ruling) |
| play (no inject) | GREEN — 0 div, probes 0, cross-check OK, 39 routines incl. MOVE_PLAYER(+nested), FIND_OBJECT, TAKE, UPDATE_SCREENS, BEING_ATTACK | tour truncated (below) |

Coverage note (owed): the play tour stalls at "Waiting for your turn"
after the 3-turn AUTO_MOVE in this container (turn pacing outlasts the
stock 160s drain), so the menu dyn routines — DISPLAY_MAGIC,
DISPLAY_CAVE, DIED, DROP, LIST_PLAYERS — were NOT re-exercised this
session. DISPLAY_SCREEN (547, the widest dyn/push) and
DISPLAY_INVENTORY (dyn, 158) are validated above. A patient driver
variant (longer + prompt-aware wait after auto-move; same steps) is at
docs/Project14/drive_patient.py, run as
`DRV=docs/Project14/drive_patient.py bash docs/Project14/run.sh <tag> play` —
handed to the user to run server-side; fold its result into the
roll-call before the Stage-3 play handoff.

## 4. Doc trail

- Mapper.md: Q2 precision extended with the merge point; the `>=` leg
  noted as subsuming §3b's record-order-for-ties (the innermost-first
  walk with `>=` hits the innermost tied record, whose cumulative
  shift_after is the sum over the tie — ties become legal under the
  future zero-arg protocol with no further stack-leg change).
- Mapper.hpp/cpp: header map description, Q2 comment, stack-leg comment.
- FINDING_A_MAPPER_FIX.md: left as ruled (it is a ruling record); this
  report is the corrective companion — read them together.
