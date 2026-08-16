# Project 14 — Phase B, Stage B1 Report (state save)

**Status: B1 partially landed — stopped on user instruction to save state.**
Evidence items (a) and (c) are proven live under lockstep at 0 divergences; (b) and
(d1) are decoded and scripted but not yet exercised; the four short battery legs
(m, fo, standing inj, abort) have not been run on the B1 book. Everything needed
to finish is in this tree; §9 is the punch list, §10 the B2 continuation note.

## 1. Nomination (gate ruling: approved)

Family: **ATTACK + ATTACK.1/.2/.3/.5** (ATTACK.4 excluded: nocall + WPOP/WPSH —
it is ON-unit #3's body). Decisive facts, independent of driver reach:

- BARGAIN's family cannot produce evidence (b): BARGAIN.1's five sites and
  BARGAIN.2's two sites are all in BARGAIN's own body, link form `WMOV 3,1` only —
  no sibling `XWLDA 1,[ac3+0x7FFA]` anywhere in that family. BARGAIN's only static
  caller is 7015DB36 **inside ATTACK** — it is the (B)argain answer to ATTACK's
  fight prompt, not a STORE path. The prompt's "BARGAIN at the store" parenthetical
  does not match the code; the disassembly wins.
- ATTACK's family has every required shape: parent→child `WMOV 3,1` (7015DC1E→.3,
  7015DC8E→.5); sibling reloads `XWLDA 1,[ac3+0x7FFA]` inside ATTACK.3's body
  (7015E8F2→.5, 7015E9D4→.1, plus the EA13/EE88/EEC5/EF55/F025/F062/F0F2/F158/F15D/F3D7
  set); child uplevel `[ac2+d]` access throughout .3.

Book: current 45-live + the 5 family entries = **50 live**
(`c_src/quest.addrbook.b1fam`, also in evidence/). Diff vs the landed batch-2 book
is exactly the five family lines plus the header count.

## 2. Family semantics (decoded from the disassembly)

ATTACK is the `A` command handler (dispatched from START_TURN at 701791D4/70179728).
On engaging a being: "You are attacking a <being> / Do you want to (F) fight,
(C) cast, (B) bargain, or (I) ignore?"

- **ATTACK.3** = cast-attack-spell handler. Entry (7015E853) does
  `XWLDA 2,[ac3+0x7FFA]` — reload of the static link from its **own saved-ac1 area
  slot** — then uplevel reads of ATTACK's locals: `[ac2+0x6]` spell index,
  `[ac2+0x15]` target-is-player flag, `[ac2+0x7]`. Reached by: A → C → (cycle
  ready-spell list; any key advances, 0x19 goes back, ESC exits) → space casts.
- **ATTACK.1** = target-death processing. Weapon-kill path calls it from ATTACK's
  body at 7015E00D (`WMOV 3,1` — parent link); **spell-kill path calls it from
  inside ATTACK.3 at 7015E9D6 via `XWLDA 1,[ac3+0x7FFA]`** — the evidence-(b)
  occurrence. Gate: target strength field goes ≤ 0 (7015DFF0 subtract-and-test).
- **ATTACK.2** = second kill/derivative branch (sites 7015DF07/E032 in ATTACK,
  EA13/EEC5/F062/F15D in .3); the DF07 instance clears a player-record status
  field (== 118) for the target before the call — player-target related.
- **ATTACK.5** = attacking-a-PLAYER path gated on a target-player record bit
  (magic-shield class); sites 7015DC8F (parent) and E8F4/EDCD/EF55/F0F2/F3D7 (.3).
- The plain damage exchange (F fight round, and a non-killing bolt) crosses none
  of the sibling XCALLs — a **kill** is the reliable sibling trigger; a spell-kill
  specifically is the XWLDA one.
- Spells are one-shot until REGEN_SPELLS; the ready-list is shown one spell per
  keypress. "Obtain knowledge" reports target strength ("You detect N strength
  points in your opponent") — the deterministic kill-planning tool.
- Fighter class has no spells; the play driver's combat requires class W.

## 3. Evidence status

**(a) parent→child static link = parent's AREA wfp via WMOV 3,1, consumed — PROVEN.**
Lockstep steered session (0 divergences at capture time, 323,673 redirect lines):

```
redirect seq=5119291 caller=QUEST2 WSAVS ATTACK    pc=7015D7A7 area_wfp=7400051A ... depth=1
redirect seq=5119344 caller=QUEST2 WSAVS ATTACK.3@7015E84A pc=7015E84A area_wfp=7400080A argc=0
        frame=115 ... ret=7015DC22 depth=2 link=7400051A
```
The child's `link=` field (the saved-ac1 wide of its area image, new B1 instrument,
§5) equals the parent's `area_wfp` exactly. ret=7015DC22 = return past the
`WMOV 3,1; XCALL` site at 7015DC1E/7015DC1F. Also fired three times non-lockstep
(recon2, recon4 — evidence/redirect_recon*_family.log) with the same link value.
Continuous verification: at the XCALL the link is in ac1, so the register rule
checks `equivalent(master_ac1, clone_area_wfp)` at every pair while held.

**(c) child uplevel `[ac2+d]` dereference into the parent's area — PROVEN.**
ATTACK.3's entry sequence (disassembly at 7015E853+: `XWLDA 2,[ac3+0x7FFA]` then
`[ac2+0x6]/[ac2+0x15]/[ac2+0x7]` reads) executes on every .3 activation; four
activations captured (one lockstep at 0 div — the clone's uplevel reads hit its
area image and every downstream compare paired).

**(b) sibling→sibling XWLDA link reload — DECODED, NOT YET EXERCISED.**
Requires a spell-kill (ATTACK.3 → ATTACK.1 at 7015E9D6, preceded by
`XWLDA 1,[ac3+0x7FFA]` at 7015E9D4). Recipe proven up to the kill: engage, C,
"Obtain knowledge" (read N), soften with F rounds tracking damage dealt, cast
Lightning bolt (17–28 damage observed) when N-estimate ≤ 15. Three characters were
lost to attrition/reaping before a kill landed (§6). The gcalls call-site pc
(7015E9D6) plus the child's `link=` line is the planned log excerpt.

**(d) unwind crossing a nested pair — RULED: unreachable; (d1) substitute approved, NOT YET RUN.**
Survey (the ruled evidence of unreachability): every unwind requires a live
ON-unit; the game's 26 handlers are all tight ON→(protected op)→REVERT windows
that never enclose a nested child's execution:
- ATTACK #3: O.ON at 7015F727 → READ_IN at 7015F733 → O.REVERT at 7015F737. The
  window is the charm-naming prompt loop (string 7015D32F, resume 7015F70F);
  it contains no XCALLs.
- START_TURN #25: O.ON 70178DA5 → command READ_IN → O.REVERT at 70178E5D —
  reverted before command dispatch (ATTACK is called later, at 701791D4/70179728).
- BARGAIN/CAVE_ATTACK equivalents: no O.ON inside BARGAIN's range at all; the
  only handler inside any nominable family's range is #3 above. Handlers in
  callers are reverted pre-dispatch as with #25.
So no game state runs a nested child under a live handler; an injected signal in
a child always takes the ?FATAL terminal path.
**(d1)** (approved): `QUEST_INJECT=7015E853:-1:0x2006` — pc inside ATTACK.3's
entry, fires on the first cast with ATTACK + ATTACK.3 records live → ?FATAL
terminal abandonment crossing the pair. Scripted, not yet run.
**(d2)** (optional, opportunistic only): inj inside the 7015F733 READ_IN if a
charm lands naturally — a true I.GOTO unwind landing in the family parent's area
frame. Not farmed.

## 4. Battery status (B1 book, 50 live)

| leg | status | result |
|---|---|---|
| baseline m (pristine tree, 45-live) | DONE | 0 div, 1260 lines (band 1258–1268), anchors exact (READ_IN=4, LOGON=1, GET_INPUT=8, INIT_SCREEN=1, REFRESH_SCREEN=1, HIT_ANY_CHAR=1), probes 0, I.STOP detach 7017FCE8, cross-check OK |
| play (autonomous, ×3) | RAN, tour-green, family not engaged | each 0 div, cross-check strict, probes 0; 73K/111K/111K redirect lines; endpoints I.STOP; **valid regressions** (ruling §3 of session of record). New live-validated coverage: TAKE, MOVE, UNLOCK_FILE; MOVE_PLAYER.1 fired naturally as stacked gcalls |
| play (steered lockstep) | RAN, family fired, reaped mid-session | 0 div at capture, 323,673 redirect lines, ATTACK + ATTACK.3 live with link= evidence; no clean endpoint (environment reaping, §7) — evidence-bearing, not a battery leg |
| m / fo / inj(standing) / abort | NOT RUN | drivers unchanged and proven; ~3–4 min each, serial |
| inj (d1) | NOT RUN | recipe in §3(d1); needs one steered cast (~10–15 min) |

The AUTO_MOVE.1 "master gcalls 1 != clone 0" coverage note in every m run is the
expected native-L2 asymmetry: the M-trigger's CONVERSION handler body (ON-unit
#4) is dispatched at O?SIGNAL site 7017EE3D — the master's emulated dispatch
XCALL emits gcalls, the clone's native L2 does not. coverage.py notes it and
cross-checks OK.

Natural death (autonomous play #2): the death path runs at 0 divergences and
detaches at **I.STOP** 7017FCE8 — ruled a validated battery-grade endpoint.
RETURN_MESSAGE stays honestly UNEXERCISED (no ?FATAL occurred naturally).

## 5. Tool changes (both ruled, boundary-3 class, warning-free build)

1. **`link=` on nested redirect WSAVS lines** (hw/Mapper.cpp): nested entries
   (name form `PARENT.n@pc`) append ` link=%08X` = the saved-ac1 wide of the
   frame's area image (wfp-6). Additive at line end; named routines' lines are
   byte-identical to before. This is the B1 evidence instrument — items (a)/(b)
   read straight off the trace.
2. **`QUEST_PORT`** (Launch.cpp): env knob for the terminal port, default 8781,
   validated eagerly at launch; unparseable or out-of-range values refuse to
   launch with a message (the QUEST_INJECT fail-loud lesson). Verified:
   `notaport` and `99999` both refuse.

Driver growth (in place, per the P13 ruling): drive.py and explore.py take an
optional trailing port argument; drive.py `play` logs in as class W, has a
reply-aware tour (§6), an embedded D215 screen parser, viewport steering, and
the adaptive combat step (`combat()`). New recon/steering tools:
`docs/Project14/screen.py` (D215 screen reconstructor; note: D215 NEW LINE is
CR+LF), `docs/Project14/combat_recon.py` (standalone combat prototype),
`docs/Project14/run.sh` (battery runner with PORT support).

## 6. Findings (driver/game interaction — all root-caused, fixed, inherited)

1. **Wizard `D` has no submenu when all spells are ready** ("All spells ready!"
   inline) — a blind ESC after it quits the game (I.STOP). Burned play run #1.
   Fix: conditional ESC.
2. **`L` (LIST_PLAYERS) submenu rendering varies run-to-run** (invisible submenu
   vs plain prompt); a mis-timed ESC (run #3) or follow-up EP (run #4) quits the
   game via a GET_INPUT at START_TURN. Burned two runs. Fix: L dropped from the
   wizard tour; LIST_PLAYERS is UNEXERCISED in play until B2's live session.
3. **Blind patrol dies without engaging**: beings that attack the player are
   co-located, and ATTACK's target scan only sees adjacent cells — a pursued
   character can be killed without ever being able to fight back (play #2 died to
   a Sphinx this way; two recon characters died to attrition). Fix: viewport
   steering — the driver parses the live D215 screen, finds being glyphs, and
   walks toward a visible being until adjacent. The steered-session engagement
   (Black Knight, §3) validates the approach; the autonomous driver with steering
   has not yet had a surviving engagement.
4. Being spawn/positions and player spawn city are wall-clock/RNG-dependent
   (CL spawned Kildare, Lizards Lair, Zol across runs) — the combat step must be
   adaptive; fixed scripts cannot work.

## 7. Environment section (ruled items)

- **Turn-survival constraint**: background processes in this container are reaped
  between assistant turns and sometimes during long in-turn sleeps. Battery legs
  must each complete within one turn; long steered sessions are at risk (one was
  lost mid-fight). All state-bearing artifacts are written incrementally to disk.
- **Parallelization resolution**: QUEST_PORT landed and verified, but parallel
  battery runs are deferred until cores exist — this container is 1-core, lockstep
  is CPU-bound (~50 s/turn), and the fixed-wait drivers are load-sensitive: under
  2×+ slowdown their waits desynchronize (the ESC-endpoint hazard). Serial is the
  correct discipline here; the knob is ready for the runner box.

## 8. Evidence files (docs/Project14/evidence/)

- `redirect_p14b_base_m.log` — baseline m redirect log (45-live, pristine tree)
- `redirect_p14b_steered_family.log`, `redirect_p14b_steered_parent.log`,
  `redirect_p14b_steered_context.log`, `steered_div_count.txt` — the lockstep
  family fire: parent area_wfp 7400051A, child link=7400051A, 0 div, 323,673 lines
- `redirect_recon2_family.log`, `redirect_recon4_family.log`,
  `redirect_recon4_parent.log` — non-lockstep family fires (same link value)
- `play_autonomous_summary.txt` — the three autonomous play regressions
- `quest.addrbook.b1fam` — the B1 book (50 live)

## 9. Punch list to finish B1 (fresh session)

1. `m`, `fo`, standing `inj` (7016A896:-1:0x2006 → ?FATAL 7017F036), `abort`
   (7016871D:ABORT → WORLD ABORT both engines) on the B1 book — serial, via
   `docs/Project14/run.sh` (~15 min total; on the runner box, seconds).
2. (d1): steered session (or runner task) with `QUEST_INJECT=7015E853:-1:0x2006`,
   engage, cast once → ?FATAL with ATTACK+ATTACK.3 records live; capture the
   terminal-path record-abandonment lines.
3. (b): steered kill — engage, Obtain knowledge, soften to ≤15, Lightning bolt;
   capture ATTACK.1's `link=` line + the gcalls line at site 7015E9D6.
4. Banded matrix table + roll-call; CheckerHistory Gen-4 append including the
   stride-masking near-miss sentence (per the phase prompt).

## 10. B2 continuation note (per rulings of the session of record)

**Scope change (supersedes the phase prompt):** M4a now covers ALL CALLABLE game
routines — the wave-one filter drops to **nocall-only** (29 excluded: boot,
C_A_LISTENER, ON-unit bodies — assigned to M5). Dyn and push routines are IN:
the dynamic ops are symmetric (both engines' wsp move identically, closed-form
shadow accounting holds), the compression leg already maps their addresses, and
the WRTN fixup discards dynamic residue on both sides. Expect **~101 live**.

**Workflow change:** B2 works IN THE REPO — github.com/nemmart/quest. Clone,
branch `p14-phase-b2`, push. A 16-core runner box executes tasks from `tasks/`
and pushes results to `results/` (see PIPELINE.md in the repo root). The m leg at
the new 10× driver speed is ~20 s; a full battery is minutes. Book regen and the
sanity legs run as **runner tasks**, not in-container. Parallel batteries are now
appropriate there (QUEST_PORT is the knob; give each task its own scratch QUEST
copy and port).

B2 shape (unchanged): regenerate the book with the reduced filter
(`python3 tools/build_address_book.py <disdir> --out-book quest.addrbook
--live <all-non-nocall>` — extend the tool's default from "all pure" to
"all non-nocall" or pass the explicit ~101-name list), one sanity `m`
(login survives, 0 div, anchors exact), `inj` + `abort`, then STOP and hand back:
**the live-play handoff is the user playing against the runner-built emulator**
with redirect+gcalls tracing and QUEST_CAPTURE armed.

Live-play handoff checklist:
1. Runner-built emulator, B2 book (~101 live), lockstep, traces on, capture armed.
2. User plays: combat (spell kill for (b) if still outstanding), a store visit,
   bargain (B answer), cave entry if reachable, castle interactions, and any
   organic play — breadth is the point; UNEXERCISED is legitimate.
3. On any divergence: the checker names the routine — comment it out of the book,
   record it, regenerate, continue playing.
4. Roll-call from the traces afterwards; final book; CheckerHistory Gen-4 append.
5. Driver notes for whoever scripts against the runner: the findings in §6 all
   still apply at 10× speed (reply-aware menus, adaptive combat, co-located-being
   hazard), and screen.py parses the D215 stream if steering is needed.
