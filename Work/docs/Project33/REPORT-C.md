# Project 33-C — the 047 play divergence: localisation and finding

Session Sep 6 2026, solo. Started from branch **p33b-arena-temps @ 9d3e18d**
(P33-B's last push: the docs commit after c3567e0); main 06337ca (the
P33-C prompt). 047's play.divdump / play-st.divdump recovered from results
commit e578e74 (main's b4d1c91 had emptied the directory: a re-run attempt
killed with exit 143). Tool built on the branch; one commit landed
(094ad7a, the Capture extension, §4). **No fix implemented** — the cause is
not in P33-B's lowering, and the two candidate fixes are design items (§3).

## 1. Result in one paragraph

The "divergence" is the lockstep pair comparison of **two batches cut by
the graceful shutdown**: the battery kills the play legs with SIGTERM
(the play driver's post-auto-move keys never reach the prompt, so the leg
never ends at I.STOP — REPORT-B §6, a pre-existing driver defect); P33-B
made SIGTERM take the graceful path (Launch.cpp:374, so the verdict lines
print), which calls `OS::shutdown_all()` → `halt_task()` on every task →
`Machine::run` breaks mid-batch (Machine.cpp:257) → `MachineThread`
submits the truncated master half and the halted clone half (its batch
runs zero instructions) to `Lockstep::compare_pair` unconditionally
(MachineThread.cpp:209; `Lockstep::aborting` is only set by a genuine
abort). Whether the truncated pair happens to compare equal is a race
with the kill: 047 lost it twice, my runs lost 2 of 5. Before P33-B the
same kill was an instant death, so every earlier play leg reported
`end=clean` (044b, 046: `end=clean`, never I.STOP) — a false clean, not a
pass. The twins are formally excluded: the same divergence signature
appears with P32's slice-6 artifact (0 twins) under the P33-B binary (§2.5).

## 2. The localisation trail (the captures that came up clean are the result)

### 2.1 INIT_OBJ_TBL's copy-out (prime suspect) — clean
Login-mode leg, book K=50, checker ON, `QUEST_CAPTURE=7016DF39
QUEST_CAPTURE_DEST=@70000210+1EFB8 QUEST_CAPTURE_LEN=96` (the object
table: SD_PTR + 0x1EFB8, 4-byte entries at SD+0x1EFB9+2·idx). Master
RETURN vs clone RETURN: **96 words identical** (entry 1 = `1443 4C15`:
0x14 'C' 'L' 0x15 — the twin's `20 ‖ name ‖ 21` copied with count ac1 =
len+2 into the CHAR(4) field, as the master). The prompt's hypothesis is
refuted on the first capture.

### 2.2 Whole-state memory oracle at every turn — clean
Play leg (patient driver), book K=50, checker ON; reproduced the
divergence locally (3.70M pairs, FIND_OBJECT 7016A933 vs 7016A928, blk
ordinals ONE apart). `QUEST_CAPTURE=70177BED` (SIGNAL_TURN) with
`QUEST_CAPTURE_WINDOWS=@70000210+0:1F400,@70000212+0:8000` — the whole
shared-data region (128,000 words from 0x70180000) and the object pages
(32,768 words from OBJ_PTR = 0x70017C00) at each SIGNAL_TURN entry and
return, on both engines: **0 differing words** at turn 1 entry, turn 1
return, and turn 2 entry — the turn the divergence occurs in. No copy-out
(the other 7 are DISPLAY_SCREEN ×3, DISPLAY_CAVE, OBSERVE ×2, HELP —
frame/static screen buffers) and no twin put a different byte into game
state.

### 2.3 What the divdumps actually say
- 047 play: master `insns=153` (a full batch), clone **`clone_insns=0`**
  (its batch ran nothing), ordinals 28 apart, same pc.
- 047 play-st: same shape (`clone_insns=0`).
- My cap2 run: master `insns=5` (cut five instructions into block
  7016A92D, after `LWADD 1,[0x70000212]`), clone `insns=0`, ordinals one
  apart; both engines at loop iteration 254 (master ac1 = OBJ+9·254,
  clone's XWDO counter 0xFE) — the SAME state, sampled at different pcs.
- In every case `stderr` has `Interrupt - shutting down...` (SIGTERM)
  BEFORE `LOCKSTEP DIVERGENCE - details in stdout log` (cap2 err
  90271/90276; kt4 err 6634/6635). The kill precedes the divergence.
- The register "differences" are the two pcs' natural state (ac0 = the
  loop's 9, ac1 = OBJ+9i vs i); none is a string residue; claims 0/0.

### 2.4 The mechanism, from the source
`Launch.cpp:374` (P33-B) `signal(SIGTERM, sigint_handler)` → main loop
`OS::global.shutdown_all()` (Launch.cpp:380) → `halt_task()` on every
task (OS.cpp:167) → `Machine::run`: `if(halt_ptr && *halt_ptr) break;`
(Machine.cpp:257) inside the batch loop → the master's `QueueEntry` ends
with a partial batch; the clone half's `run()` breaks at once (0
instructions) → `Lockstep::compare_pair(master_half, entry)`
(MachineThread.cpp:209) with no halt/shutdown guard → `blocks_differ`
and `regs_differ` → DIVERGENCE. `Lockstep::aborting` (Lockstep.cpp:248)
is only set by an abort, not by shutdown.

(The listener task, C_A_LISTENER, is dead code under emulation — user
correction: the ctrl-C/ctrl-A move-interrupt listener; it sits in
`INTWT_call`'s 100 ms poll of `task->halt` (OSContextSystem.cpp:232) and
executes nothing until the shutdown throws `INTWT interrupted` on its
own thread. Its `.UKIL`/"68 instructions run" backtraces are teardown
noise printed at the kill, not an event during the game. The
"Waiting for your turn" / `RETURNING AC1` lines before every divergence
are the main task's own prompt write and its return from the mediated
turn wait — they mark where the driver stopped driving.)

### 2.5 Confirmations (user's (a) and (b))
| run | artifact | twins | driver / kill | result |
|---|---|---|---|---|
| cap2 | P33-B slice 7 | 57 | patient play, script kill after the driver | **DIVERGENCE** at the kill (§2.3) |
| cap3 | P33-B slice 7 | 57 | same, `-types lockstep,scalls` | clean (3.81M pairs, `end=clean` = killed mid-game) |
| kt1 | **P32 slice 6** | **0** | SIGTERM at 200 s | clean; last pair a completed one |
| kt2 | P32 slice 6 | 0 | SIGTERM at 194 s | clean; last pair **`insns=0 clone_insns=0`** — a halt-cut pair that compared equal |
| kt3 | P32 slice 6 | 0 | (login glitch, ended I.STOP at 6,766 pairs) | n/a |
| kt4 | **P32 slice 6** | **0** | SIGTERM at 228 s | **DIVERGENCE**: FIND_OBJECT 7016A928 vs 7016A923, ordinals 37 apart, `insns=147 / clone_insns=0`, err: `Interrupt - shutting down...` then `LOCKSTEP DIVERGENCE` |

kt4 is 047's signature with zero twins in the IR: the twins are
excluded. (a): the scalls trace shows no listener activity; the only
syscalls between the turn wait and the kill are the main task's. Whether
P33-B's checker work shifts the phase is moot — the kill lands wherever
the wall clock puts it; the outcome is the halt-cut race.

Also confirmed in passing: **every play leg since 034 has been killed
mid-game** (044b, 046: `end=clean`, `want=clean,I.STOP`; the verdict
accepts `clean`, which is what a kill produces) — the ESC never reaches
the prompt after the auto-move (REPORT-B §6). P33-B's graceful SIGTERM
turned that silent kill into a visible pair compare.

## 3. Design items (report, do not implement — user ruling)

1. **Gen-6.2 checker ruling: a shutdown must not be compared.** Either
   (i) `shutdown_all` sets `Lockstep::aborting` (or a `halting` flag
   `compare_pair` checks) so a halt-cut batch is never compared, or (ii)
   the halt is deferred to the next pair boundary (the batch completes,
   then both halves halt). (i) is one line at Lockstep.cpp:248 /
   OS.cpp:167; (ii) is the deterministic form. CheckerHistory entry: "a
   halt-truncated batch pair is not a divergence; before P33-B the kill
   was instant and the legs read clean". Evidence: §2.3–§2.5.
2. **The play legs' end condition.** The verdict must require `I.STOP`
   for play/play-st (today `clean` passes a killed leg), and the driver's
   post-auto-move keys must reach the prompt (REPORT-B §6 item). Until
   then the play legs verify 2–3.8M pairs and then die; that is what
   they have always done.
3. The listener teardown (`INTWT interrupted` on its own thread at the
   kill) is harmless under (1); if (ii) is chosen, the listener's halt is
   naturally synchronous with the pair boundary too.

## 4. Deliverables
- `094ad7a` Capture extension (debug tool only): per-machine arming so the
  IR clone gets its own ENTRY/RETURN pair (`check()` at every IR block
  entry), `QUEST_CAPTURE_DEST=@addr+off` (indirect base, e.g. a shared-data
  table), `QUEST_CAPTURE_LEN`, `QUEST_CAPTURE_WINDOWS=<base|@addr+off>:<len>,…`
  whole-region snapshots. This is the memory oracle StringsDesign §6 lists
  as optional; here it cleared the twins in two runs (§2.1, §2.2).
- This report; REPORT_worklog-C.md. No IR, arena, lowering, or checker
  change; no artifact change (Provenance unchanged). 047b not queued: the
  legs would show the same race until item 3.1 lands.

## 5. For the integrator (P33-B's owed notes, unchanged)
Mapper.md §1.4 / StringsDesign §6 for the two checker findings; "ground:
t@b.k; readable: p@b ≡ t@b.last" (REPORT-B §6). Plus the two items of §3
for the Gen-6.2 ruling.
