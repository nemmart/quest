# q001 — Project 49 plan gate

Sep 12 2026. Part 1 of PROMPT.md. Nothing built, nothing outside
`docs/Project49/` touched.

**Headline: the carried-in diagnosis is wrong in its mechanism, and two of
the three coverage costings it rests on do not survive contact with the
game.** The keys *do* land. The driver kills its own session with a stray
ESC, and in the battery it is killed by `timeout 480` before it sends any of
them. Separately, `a002`'s character-creation route to `PICK_X_Y` is refuted;
the operator login is *cheaper* than costed and needs no fixture at all.

Recommendation at the end of each section. §7 is the decision I need.

---

## 0. Provenance — where this evidence comes from

**`play/session.log` is not in the repo and never has been.** `tasks/052`'s
`leg()` copies `session.log` only when a leg FAILS (`tail -20` →
`$tag.session_tail`). The play legs are scored OK (`want=clean,I.STOP`
accepts `clean`), so no play session log has ever been committed. The only
`*.session_tail` files in `results/` are from `derr`, `inj-emu` and `abort`.

So I built the emulator in the container (`make -j2`, clean) and ran the
game directly. Three runs, all against a fresh `cp -r QUEST` from the
pristine repo copy:

| run | driver | purpose | log |
|---|---|---|---|
| **R1** | `docs/Project13/drive.py play` verbatim | what the driver of record actually does | 3,398 B |
| **R2** | a throwaway prompt-aware probe (`/tmp`, not committed) | where each key lands when it is given time | 7,402 B |
| **R3** | initials `GOD` | the operator gate | — |

These are **plain-emulation** runs (no `-lockstep`, no `QUEST_IR`). That is
deliberate — it isolates *game behaviour* from *lockstep pacing*, which
turns out to be exactly the distinction the old diagnosis missed. Block-level
coverage cannot be measured this way (`first execution of block` is emitted
only by `IRExec`, i.e. only on the clone with `QUEST_IR` set), so every
coverage number below comes from `results/052-p46-final`, not from my runs.

**Baseline reproduced.** `compiler/callgraph.py --coverage
results/052-p46-final` → **8,300 / 53,588 = 15.5%**, 2,086 / 13,507 blocks,
**41 / 80 nodes**. P47's 047b figure is 8,305 / 41 nodes. The method
reproduces; I will use it unchanged before and after (PROMPT ruling 7).
One discrepancy: the tool prints 22 nodes marked `LEAF` and 17 reached,
where P47 reports 20 and 16. I have not chased it; I will report leaves on
the tool's own count and note the delta.

---

## 1. What the session log actually shows (gate item 1)

### 1.1 The keys land. All of them.

R2 sent `O`, `ESC`, `D`, `L`, `ESC`, `H`, `1`, `0` after the auto-move
settled, and every one reached the command prompt and did its work:

- `O` → `Observe item` → `Hit space bar at object to observe, or escape to
  exit` → `ESC` → `You are carrying 11 lbs. with a maximum of 68 lbs.`
- `D` → `All spells ready!` → straight back to the prompt
- `L` → `Hit (P) for players (C) for castles (A) for alliances (I) for
  castle inventory:`
- `H` → the Help topic menu → `1` → the full Terrain symbol table → `0` →
  back to the map

So FINDINGS_SUMMARY §2's *"the keys are sent but never land on the command
prompt"* describes a symptom, not a cause. There is nothing wrong with the
keys or the prompt.

### 1.2 Cause 1 — `ESC` after `D` quits the game

The driver's shape is `send(key); send(ESC)` for every menu key, on the
assumption that each opens a screen you escape from. **`D` (DISPLAY_MAGIC)
does not open a screen.** It prints `All spells ready!` and returns to the
command prompt. The ESC that follows therefore lands *on the command
prompt*, and ESC at the command prompt detaches.

This is R1, the driver of record, verbatim: the session died at the next
key. `drv.out` shows `BrokenPipeError` on `send("L")`, and the emulator's
`out.txt` shows

```
System Call 310, called from 7017FCF0
TERMINATING PROCESS
frame  2 -- START_TURN+0xB01   [70178D20]
```

— i.e. a genuine `I.STOP` quit out of START_TURN, triggered three keys early.
`L`, `H`, `1`, `0` and the intended final ESC were never sent. **The driver
of record ends its own session with the ESC after `D`.**

Note the irony: the leg *did* quit properly, so the "never quits" half of
the diagnosis is also mis-stated. It quits — just at the wrong moment, and
only when it runs fast enough to get that far.

### 1.3 Cause 2 — `L` needs a sub-key, and never gets one

`L` opens a chooser: `(P) players (C) castles (A) alliances (I) castle
inventory`. The driver answers it with `ESC`. So LIST_PLAYERS renders its
menu and exits without ever listing anything — which is precisely why
Coverage.md has LIST_PLAYERS at **5% (52/1,058)** rather than 0: the menu
block runs, the body never does. This bug is not in the carried-in diagnosis
at all.

### 1.4 Cause 3 — in the battery, the keys are never sent

This is the one that matters for the numbers, and it has nothing to do with
keys or screens.

`leg()` runs the driver under **`timeout 480`**. The `play` leg uses
`docs/Project14/drive_patient.py`, whose budget is:

```
login ~28 s + F 12 s + M/n/3 38 s  ≈ 78 s, then wait_turn_done(420)
```

`78 + 420 = 498 > 480`. **The timeout fires inside `wait_turn_done`, before
the first menu key is sent.** The leg is then SIGTERMed (`end=clean`).

`play-st` uses the stock `drive.py`, whose `send("3\r", 160)` is a fixed
160 s drain. Under lockstep that is not enough for three turns either, so
its keys go out mid-auto-move and are swallowed as invalid commands.

The evidence is in the per-leg block counts from `052`:

| leg | distinct blocks |
|---|---|
| play-st | 1,978 |
| play | 1,976 |
| inj | 1,513 |
| m | 1,355 |

`play` and `play-st` are within 2 blocks of each other. If either had
reached the menu keys they would separate by hundreds. Neither gets past the
auto-move.

**Summary of the diagnosis:** three independent faults — a stray ESC that
quits, a missing sub-key, and a driver budget that exceeds the leg timeout.
The first two are invisible in the battery because the third fires first.

---

## 2. Driver design (gate item 2)

### 2.1 The timing principle

The single most useful thing I learned in R2: **the game consumes keys on
its turn cycle and emits the response later.** Sending `O` returned 0 bytes;
the `Observe item` text arrived after the next tick. A driver that waits a
fixed number of seconds, or that keys off "did I get bytes back", is
guessing. Both current drivers do one or the other.

The robust signal is the command prompt itself. The game writes `-> ` and
then goes quiet. So:

```
wait_for_prompt(ceiling):
    drain in short slices until BOTH:
      * the tail of everything seen ends in the "-> " prompt, and
      * no "Waiting for your turn" appeared in the last slice, and
      * the last slice was quiet (< N bytes)
    return early the moment that holds; give up at ceiling and say so
```

This is tuned to *the game's own handshake*, not to one run's wall clock,
which is what PROMPT item 2 asks for. It returns in seconds on a fast
emulation-only run and in minutes under lockstep, with no change.

**It must be paired with raising the leg's `timeout 480`.** A prompt-aware
wait with a 420 s ceiling cannot live inside a 480 s budget that already
spent 78 s on login. I propose `timeout 1200` for play legs, and — more
importantly — a driver that *reports* when a ceiling is hit rather than
dying silently, so this class of failure can never again look like `clean`.

### 2.2 The key sequence

```
login → create → class letter
M, n, 3        auto-move
wait_for_prompt
O              OBSERVE          → sub-screen
ESC              ...exit it
D              DISPLAY_MAGIC    → returns to prompt; NO ESC
L              LIST_PLAYERS     → chooser
P                ...players list          [new: the body, not just the menu]
ESC              ...exit it
H              HELP             → topic menu
1                Terrain topic (the P32 164||150→314 scratch chain)
<space>          "Hit any character to continue"
0                exit help
ESC            → I.STOP
```

Changes from the driver of record: no ESC after `D`; `P` after `L`; an
explicit space for HELP's "hit any character"; prompt-aware waits
throughout. Everything else is preserved — Project 13's ruling is *grow
`play`, do not fork*, and I intend to keep `drive.py` the single driver and
retire `drive_patient.py` rather than maintain two.

### 2.3 Ending in a real quit

The final ESC at the command prompt is the real quit, evidenced by R1's
unintended one: `I.STOP` via `LJSR [0x7017FCE8]`, which is exactly what the
leg's `grep -q 'DETACHED at 7017FCE8'` scores as `end=I.STOP`. Per PROMPT
ruling 2 I will change the `play`/`play-st` verdicts from
`clean,I.STOP` to `I.STOP` only.

**This is a teeth change and it should be landed with the driver fix in the
same stage, not before it** — tightening the verdict first would turn the
battery red for a reason unrelated to what the stage is testing.

---

## 3. The login legs (gate item 3)

### 3.1 The user data file does not persist — but say so out loud

PROMPT item 3 asks how a leg gets unused initials given that the user data
file persists between runs. **In the battery it does not persist.** `leg()`
does `cp -r $ROOT/QUEST $R/QUEST` into a per-leg `/tmp` scratch, and the
emulator writes `:USER_DATA_FILE` back to *that copy* at shutdown
(confirmed in R1's shutdown log). The repo's `QUEST/` is pristine, and each
leg gets its own copy, so parallel legs cannot collide either.

I verified the pristine copy contains no `CL`/`Claude` record: R1 got
`That USERNAME/PASSWORD does not exist. Do you wish to create this
character?`. So `CL`/`Claude` is already an unused-initials creation, every
leg, today.

The risk is not persistence, it is **silent drift** — if anyone ever commits
a dirtied `QUEST/`, the creation path stops running and nothing notices.
Proposed: a leg-level check that the session log contains `does not exist`,
so "we are still exercising creation" is asserted rather than assumed.

### 3.2 Character creation is already exercised — and a002's route is wrong

This is the gate item 6 finding, and it is load-bearing, so it is here too.

Block **`70178246`** — the `What would you like your character to be` prompt,
`a002`'s own anchor — **executes in 8 of the 16 legs in `052` today**: `fo`,
`fo-st`, `inj`, `inj3`, `k1fo`, `m`, `play`, `play-st`. Character creation is
not a coverage addition. It is already in every leg that logs in.

And it does not reach what `a002` says it reaches. In the same green battery:

```
DIED           0/73 blocks   0/495 stmts
PICK_X_Y       0/18          0/64
REPOSITION     0/4           0/11
PLACE_PLAYER   0/88          0/399
```

`a002` reasons: the class prompt is in START_TURN, START_TURN calls DIED at
`7017868D` and `70178801`, therefore creation → DIED → REPOSITION →
PICK_X_Y. **Both of those call sites are starvation deaths.** From the book:

```
block 7017867E:  "You have died of thirst!"   → call DIED
block 701787F2:  "You have died of hungar!"   → call DIED
```

Neither is a creation path. `PICK_X_Y` has exactly one caller (`70176FC7`,
inside REPOSITION) and REPOSITION has eight, of which `70166323` is DIED's;
the two in QUEST (`7015C950`, `7015CBDD`) sit behind a `RANDOM` test and
none of the surrounding blocks execute. So the route to PICK_X_Y is real but
is **not** the one `a002` names, and answering the class prompt — which the
driver already does — does not take it.

**`a002`'s "very likely the cheapest statements-per-unit-of-work item in the
whole table" is not supported.** The general point it makes — that play
evidence beats static narrative — is exactly right, and is what produced
this correction too.

### 3.3 The operator login is cheaper than costed — no fixture at all

Coverage.md §3.2 group F says OP_EDIT is *"gated by the operator flag at
`0x7000021A`"* and needs *"a login fixture"*. The flag is real; the fixture
is not needed.

`QUEST` at `7015C0A3` compares the initials string at `0x7000021C` against
the literal `"GOD"`. On a match it takes the branch that sets
`M16[0x7000021A] = 0x8000` at `7015C113`; `7015C2BB` then tests that sign
bit and calls `OP_EDIT`.

R3 confirms it end-to-end. Initials `GOD` → **no Player name prompt, no
Password prompt** → straight into the operator map editor (`Is this a
citadel?`). That is a four-keystroke leg with no data file surgery, no
maintained test artifact, and no new mechanism.

**So of P47's `+4,989` "cheap fixtures": the operator half is cheaper than
described, and the killed-off/creation half does not deliver what is
claimed.** `KILL_PLAYER` (395 stmts), incidentally, already has a driver
mode — `drive.py` mode `kp`, written for P19 — that the battery does not
launch. That is the cheapest unclaimed item I found.

---

## 4. Task 051 (gate item 4)

Every number below is justified independently of what the tree prints
(PROMPT ruling 6). I verified each against the current tree.

**The P33 lowering emitted 229 sites, and they decompose exactly:**

```
96 assigns  +  57 bases (WADI)  +  57 claims  +  19 releases  =  229
```

### Line 179 — `embeds_book` 729 → **557**, `embeds_stock` 2608 → **2436**

Justification is the one PROMPT names: **the same file's P33-B section
already wants 557 / 2436 and gets them** (`verdicts.txt`: `embeds book=557
(want 557) stock=2436 (want 2436)`). Task 052 currently asserts 729 and 557
for the same quantity in one script. The delta is 172 embeds each side,
consumed by the P33 lowering. Not a retune — a contradiction inside one
file, resolved toward the section that was written *after* P33.

### Line 212 — `string_statements` 1593 → **1689**, literals 857 → **871**

The check counts IR lines matching `^  (\[@|ac1 = cmp\(|words\(@)`. Of the
229 P33 sites, **exactly 96 emit `[@...]` assign forms**; bases, claims and
releases do not match that regex. So `1593 + 96 = 1689`. Verified: the tree
prints 1689.

Of those 96 assigns, **exactly 14 match the literal-assignment form**
`\[@[^;]*\] = \[@0x[0-9A-F]+:[01], "`. So `857 + 14 = 871`. Verified: 871.

`cmp` stays 40 and `words` stays 12 — P33 emitted none of either. Both
already pass and I will not touch them.

These are derived from the artifact, not read off the tree; if the lowering
changed, they would move and the check would go red as intended.

### Line 221 — widen, do not retune

Structurally stale exactly as PROMPT ruling 5 says: it diffs `lower.py`'s
ledger against `p31.tsv + p32.tsv` and never consults `p33.tsv`. I propose:

- `SE` 1593 → **1822** = `1593 + 229` (p33 EMIT rows; `p33.tsv` is 229 rows,
  all EMIT, 0 REFUSE)
- add a `p33` census pair to the printed line: `229/0`
- extend both `diff`s to a **three-way** concatenation — the pc set from
  `$1` and the IR text from `$11` with the `; ...` suffix stripped, same
  shape as the p31/p32 arms

I confirmed `p33.tsv`'s `$1` is the ledger pc (229/229 match; `$10` matches
0/107), so the existing awk shape carries over unchanged.

**I ran the widened comparison against the current tree: `same pc set=yes`,
`same text=yes`.** This is not a tautology — the ledger is `lower.py`'s
output and `p33.tsv` is `tools/string_sites.py`'s; the check compares two
independently generated artifacts and would fail if either drifted.

### Nothing left red

I did not find a number I could not justify. If Stage A turns up one, it
stays red with its reason, per ruling 6.

### The retry point

`052` carries a hand-written `DONE` explaining that it exists *"to stop the
retry loop"*. Once 051 lands, a green run exits 0 and earns `DONE` on its
own. I will confirm that on the Stage A result rather than assume it.

---

## 5. Predicted coverage (gate item 5)

### 5.1 Why I think P47's numbers are upper bounds, not predictions

Coverage.md §3.2 costs group A at `+7,474` and D+F+G at `+4,989`. Those are
**the full statement counts of the nodes involved** — the sum of the
`0/N` rows. But the metric is *statements in executed blocks*, and nothing
in the table is executed to completion except five tiny leaves. The battery
runs DISPLAY_INVENTORY at 64%, START_TURN at 45%, MOVE_PLAYER at 18%.

A scripted leg that presses `O` once will not execute all 1,044 statements
of OBSERVE. Costing it as if it will inflates the prediction by roughly
2–3×. P47's own §4 makes this argument about other people's numbers; it
does not apply it to its own.

Second, group A's 14 nodes are not one bug. Four of them (**HELP 609,
OBSERVE 1,044, DISPLAY_MAGIC 204, LIST_PLAYERS 1,006 remaining**) are keys
the driver already sends — that is the driver fix, ~2,863 statements of
headroom. The other ten (LOOK, SPYGLASS, BACKPACK, CASTLE_INVENTORY, DROP,
ALLY_PLAYER + private callees, ~3,367 + callees) are keys **the driver has
never sent at all**, two of which need game state (an item to DROP, another
player to ALLY). Calling all fourteen "the driver bug" is not right.

### 5.2 My numbers, to be scored

Baseline **8,300 / 53,588 = 15.5%**, 41/80 nodes (my measurement on 052).

| stage | nodes | executed stmts | reasoning |
|---|---|---|---|
| B driver fix (keys already sent, + `P` sub-key) | +4 | **+1,000 to +1,400** | ~2,863 stmts of headroom at the 35–50% in-node rate the battery achieves elsewhere |
| C operator login (`GOD`) | +2 to +3 | **+800 to +1,400** | OP_EDIT 3,812 at 20–35%; OP_HELP 31 whole; WRITE_OBJECT 162 only if a scripted edit writes |
| C `kp` leg (free, mode exists) | +1 | **+150 to +250** | KILL_PLAYER 395 |
| C creation leg | **+0** | **+0** | §3.2 — already exercised |

**Central estimate: +2,600 executed statements → 10,900 / 53,588 ≈ 20.3%,
47–49 of 80 nodes.** Range +2,000 to +4,000 (19.2% – 23.0%).

Against P47: they imply `+12,463` → 38.7%. **I am predicting roughly a fifth
of that.** If I am wrong I would rather be wrong in writing and scored for
it, which is the point of putting it here.

Falsifiable sub-predictions, so the estimate can be graded in parts:

1. OBSERVE, DISPLAY_MAGIC and HELP go from 0 to non-zero — **high
   confidence**, observed in R2.
2. None of the four reaches 80% of its statements — **high confidence**.
3. LIST_PLAYERS rises well above 5% once `P` is sent — high confidence.
4. DIED, PICK_X_Y, REPOSITION, PLACE_PLAYER stay at **0** through Stage C —
   this is the one that most directly contradicts the carried-in plan.
5. OP_EDIT lands between 15% and 45%.

### 5.3 Divergence risk

Boundary 4 says a divergence in a new leg is a STOP-and-report. OP_EDIT
(3,812 stmts) and the OBSERVE/HELP screens have **never** run under
lockstep. P33-C's experience says the first thing found in newly-exercised
code is often the harness, not the game. I am flagging in advance that Stage
C has a real chance of stopping, and that I will treat that as the
project's most valuable output rather than something to route around.

---

## 6. What does not survive contact with the log (gate item 6)

Collected, since each is a correction to a document of record:

1. **"The keys never land on the command prompt"** (FINDINGS_SUMMARY §2) —
   false. They land. §1.1.
2. **"The game is never quit"** (same) — false. The driver of record quits
   itself early via the ESC after `D`. §1.2.
3. **The real battery-level cause is `timeout 480`**, not key handling —
   not mentioned anywhere. §1.4.
4. **`L` needs a sub-key** — not mentioned anywhere. §1.3.
5. **`a002`'s creation → DIED → PICK_X_Y route** — refuted; both START_TURN
   →DIED sites are starvation deaths, and the class prompt block already
   runs in 8 legs while all four targets sit at 0. §3.2.
6. **Group F needs no fixture** — initials `GOD`, four keystrokes. §3.3.
7. **P47's `+7,474` / `+4,989` are node totals, not executed-statement
   predictions**, and group A is two different kinds of work. §5.1.

`052`'s own informational line — *"the play driver's post-auto-move keys do
not reach the prompt — driver fix owed"* — is the symptom stated as the
cause, and it propagated from there into three documents.

---

## 7. The decision I need

The plan in PROMPT Part 2 is Stage A (051) → B (driver) → C (login legs) →
D (standing verdict). **A, B and D are unaffected by anything above and I am
ready to run them as written.** Stage C is affected, because one of its two
legs delivers nothing.

**Options for Stage C:**

- **C1 — build both legs as prompted.** Cost: a creation leg that duplicates
  what `CL`/`Claude` already does in 8 legs. Honest, and cheap, but it adds
  a leg that cannot move the number.
- **C2 — operator leg + `kp` leg; drop the creation leg; spend the
  remainder chasing PICK_X_Y's real caller.** `kp` costs nothing (the mode
  is written). The PICK_X_Y route is then an open finding for the report
  rather than a claimed win.
- **C3 — C2, plus keep a creation *assertion* rather than a creation leg**:
  add the `does not exist` check from §3.1 to the existing legs, so the
  coverage we already get is defended against a dirtied `QUEST/`.

**RECOMMENDATION: C3.** It gets the whole of the real coverage win (operator
+ kp), spends nothing on a leg that duplicates existing behaviour, and
converts the creation path from an assumed win into a guarded fact. It also
keeps the report honest about PICK_X_Y being unsolved, which I think is
worth more than a leg that appears to address it.

Two smaller rulings I would like confirmed rather than assumed:

- **7a — retire `drive_patient.py`?** Project 13's rule is grow `play`, do
  not fork; `drive_patient.py` is an acknowledged one-off fork that is now
  the thing running in the `play` leg. I would fold its prompt-awareness
  into `drive.py` and point both legs at it. It is inside my write boundary,
  but it is P14's artifact, so I am asking.
- **7b — raise `timeout 480`.** Required for any prompt-aware wait to
  survive. I propose 1200 for play legs, with the driver reporting a
  ceiling hit loudly. This lengthens the battery; `052` ran 868 s wall
  clock at JOBS=3 and the play legs are the long poles.

Stopping here for `a001`. Nothing is built and no branch exists yet.
