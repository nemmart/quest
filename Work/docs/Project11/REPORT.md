# Project 11 — REPORT (The Probe Suppression List: Login Under -zero=clone)

Per SharedProtocol.md REPORT format, adapted to a harness-side project
(no translation_table entries; the "routine" is the probe suppression
mechanism). Solo session, Aug 14 2026. Code changes:
os/ProbeSuppressions.{hpp,cpp} (NEW), os/LockstepMediator.{hpp,cpp},
os/OSTask.cpp, os/OSContext.cpp, hw/RTBridge.cpp, Makefile. No
checker-generation change (per CheckerHistory.md: the suppression list
is probe-mode-only instrumentation; Generation 2 untouched). Doc edits:
this report + the one-line ruling-8 catalogue pointer in Layering.md.

## 1. Status

| Piece | Status |
|---|---|
| Suppression mechanism | **DONE, lockstep-validated** (§3): table + site-pc plumbing + collect-don't-halt branch, probe-mode-gated. |
| Honest key | **CHARACTERIZED BEFORE BUILDING** (§2): master trap site (address-2); site-halt clone-trap-conditional. |
| Packet-content tier | **DONE** (§3.3): verify_read flag-and-continue in probe mode only; abort untouched otherwise. |
| Inertness (-zero=both / none) | **PROVEN** (§6 step 1): 3/3 post-change runs match pre-change; 0 PROBE lines; no specimens file. |
| Login under -zero=clone | **SUCCESS** (§6 steps 2–3): full CL/Claude/quest/Y/any/F to GET_INPUT, 0 divergences, 0 unsuppressed halts, 2 catalogued specimens. |
| F1-composition claim | **VERIFIED EMPIRICALLY** (§6 step 4): master-authoritative continuation keeps both engines on the 1988-true branch through the FAIL_OPEN cascade to ?FATAL. |
| Bonus one move (S) | **DONE, clean** (§6 step 5): move + full turn, 0 new specimens. |

## 2. The honest key (characterized before building, per the prompt)

The halt in P10's F2 fires in `LockstepMediator::verify_arrival`, which
had call number + ac0–2 in hand but NOT the site pc. The site is
available one frame up: `OSTask::dispatch_system_call` computes
`address - 2` — the game-side pc of the syscall issue site, the same
value "System Call %o, called from %08X" logs, and the pc specimen #1
was recorded under (0x7017E2F4). It is now plumbed into the Slot per
side at arrival.

**Wrinkle found during characterization, not after:** the mediator has
a SECOND caller — `RTBridge::syscall` (hw/RTBridge.cpp), the clone's
native-wrapper syscall path, whose logged "site" is the native
`entry_pc`, not a trap pc (deliberate; see the log-parity comment
there). The master has no stubs (Run.md: rtcalls is clone-only), so
`master_site` is ALWAYS trap-derived and uniform. Therefore:

- **The suppression key is (syscall, MASTER site pc, register set)** —
  site-specific, uniformly derived, matching specimen #1's recorded
  key. Never a global register exemption.
- **The site-mismatch halt applies only when the clone also trapped**
  (`site_native` flag from RTBridge). Inside a native span, control
  flow is already verified by the crossings checker's entry pair; a
  fork there breaks span/count structure and halts in compare_pair.

## 3. As-built: the mechanism

### 3.1 The table — os/ProbeSuppressions.{hpp,cpp}

One built-in table, each entry `(call, site_pc, reg_mask, dated note)`.
Lookup admits a mismatch iff call and site match and the
mismatching-register set is a SUBSET of the entry's mask. No config
files, no env parsing.

### 3.2 verify_arrival (os/LockstepMediator.cpp), probe branch

Gated on `Lockstep::probe_relax_regs` — set ONLY by `-zero=clone`
(Launch.cpp; requires -lockstep, rejected otherwise). Shipping
(-zero=both) and attic (-zero=none) never reach the branch; proven
inert in §6 step 1.

Halt taxonomy (user-ratified):
- **Call-number mismatch, or trap-site mismatch → HALT** (control-flow
  family: two engines issuing from different places is a pc fork
  wearing syscall clothing). The divergence report now prints both
  sites.
- **pc forks and count skews → HALT**, unchanged, in
  Lockstep::compare_pair (untouched by this project).
- **Register-VALUE-only mismatch → NEVER halts** in probe mode:
  - KNOWN specimen: one line, stderr + specimens file
    (`PROBE known specimen [note]: call site m=... c=...`).
  - NEW specimen: full forensic record (§3.4), then continue.
  - Master stays authoritative: for MEDIATED calls the clone consumes
    the master's results (existing mediator semantics — verified as
    the F1 composition, §6 step 4). For LOCAL calls see §5.1.

### 3.3 Packet-content tier (os/OSContext.cpp verify_read)

`verify_read` hard-aborts on a mediated-call INPUT mismatch — which
would have halted the expedition on the very residue class being
collected, arriving via handler memory reads instead of ACs. In probe
mode it now emits a loud `PROBE PACKET-CONTENT FLAG (HIGH SIGNAL —
reviewer tier)` block (range, width, both values, master pc + instr)
and continues with the master's garbage-authentic value. NEVER a
silent suppression — no table entry can admit it; every occurrence is
a full flag block for reviewer classification. Abort semantics
untouched outside probe mode.

### 3.4 The forensic record

One block per new specimen, stderr AND appended (unbuffered) to
`probe_specimens.log` in the host cwd. "Next to the data files" is
read as: next to the durable run artifacts — the scratch QUEST copy
is deleted per run, so a file inside it would not survive the session.
The log is swept into every checkpoint. Contents: provisional
specimen number; syscall number + name; the suppression key exactly as
implemented; both engines' pc/ac0–3 (mediation-compared regs marked
`*`), wsp/wfp/psr/carry; both instruction counts; ordinal; 16-word
packet dump at ac2 from BOTH engines' memory, diffs marked (page-probed
first — an unmapped ac2 skips with a note); the last 8 verified
rendezvous site pcs (Slot ring buffer); and the master call-stack
backtrace replicated INTO the record (CallStack::backtrace prints to
stdout only; the record must survive alone).

Record-reading note: the raw instruction counts differ legitimately
when the clone has run native spans (fewer emulated instructions);
the "skew" figure is raw, not an alarm. Count-skew alarms remain
compare_pair's under its exemption rules.

## 4. THE CATALOGUE

### Specimen #1 — ?WRITE_SCREEN dead locals (seeded from Project 10 F2)

- Suppression entry: `{ 0303, 0x7017E2F4, ac0 }` — "2026-08-14 #1
  ?WRITE_SCREEN dead locals".
- Syscall 0303 (?WRITE), site 0x7017E2F4 inside ?WRITE_SCREEN
  (RT range). Provenance: RTBridge.hpp "Dead-stack residue fidelity" —
  loads never-written frame locals [4..5] into ac0; per METHOD §13's
  ?ERMSG convention ac0 is two 8-bit fields (P10's 0x0000FFFF =
  255-byte buffer, channel 0377). Clone-zeroed → ac0=0.
- Live behavior this expedition: fires ~60–126 times per session with
  VARYING master residue (observed ac0: 0000FFFF, 80000000, 700010E8,
  E000371A, 030EE2F8, ...) — the residue is whatever the previous
  frame occupant left, confirming the P10 reading that the screen
  machinery runs on stack residue continuously, not just at login.
- Demotion evidence (before/after, verbatim): P10's halt was
  `rendezvous mismatch ... master: call=0303 ac0=0000FFFF ...` ending
  the run. Under the suppression, run clone_login1 line 217:

```
PROBE known specimen [2026-08-14 #1 ?WRITE_SCREEN dead locals]: call=0303 site=7017E2F4 m=0000FFFF/00000000/700010F2 c=00000000/00000000/700010F2 (master authoritative)
```

  — same call, same site, same values (m ac0=0000FFFF, c ac0=0), now
  one line and the run proceeds.

### Specimen #2 — game-side ?ISR dead pointer (NEW, this expedition)

- Suppression entry: `{ 0142, 0x70175F2E, ac0 }` — "2026-08-14 #2 game
  ?ISR dead ptr ac0".
- Syscall 0142 (?ISR, world-facing/mediated), site 0x70175F2E — GAME
  code (below the RT range), caller chain QUEST+0x27E → stub 7017FDED.
  Fires once, early login (master instr ~2042). First specimen located
  OUTSIDE the runtime: the game's own IPC setup consumes an
  uninitialized cell.
- Full forensic record (run clone_login1, verbatim — the
  cure-without-retest artifact):

```
=========== PROBE NEW SPECIMEN (provisional #2) ===========
syscall 0142 (?ISR)  site pc 70175F2E  [suppression key: (0142, 0x70175F2E, {ac0,})]
ordinal=0  master tid instr=2042  clone instr=1659  (skew 383)
master: pc=7017FDED  ac0=7000021C*  ac1=00000000   ac2=700010F8   ac3=7017FDF1  wsp=70001148 wfp=700010E8 psr=8000 c=0
clone : pc=7017FDED  ac0=00000000*  ac1=00000000   ac2=700010F8   ac3=7017FDF1  wsp=70001148 wfp=700010E8 psr=8000 c=0
packet[16w] @m:700010F8 @c:700010F8 (word: master/clone, * = diff)
  +00 0000/0000   +01 0009/0009   +02 0064/0064   +03 0001/0001
  +04 0001/0001   +05 0000/0000   +06 0000/0000   +07 0000/0000
  +08 0000/0000   +09 0000/0000   +10 0000/0000   +11 0000/0000
  +12 7000/0000*  +13 021C/0000*  +14 7000/0000*  +15 0262/0000*
recent rendezvous sites: 7017E2F4 7017DFEC 7017DD03 7017DBA3 7017DCB5 7017DD9B 7017DD9B 7017E2F4
master call-stack backtrace:
  frame  2 -- 7017FDED     guessing: .UKIL+0x8
  frame  1 -- QUEST+0x27E   [7015C283]
  frame  0 -- 7017EA51     guessing: I.INIT+0x23
================= END SPECIMEN #2 (continuing) =================
```

- Reading: ac0 carries a stack-region pointer (0x7000021C) the clone
  zeroed; packet words +12..+15 carry the SAME residue pointers
  (7000021C, 70000262). The ?ISR handler did NOT consume the packet
  tail this run (IREC return: length=0, data pointer=0), so no
  content-tier flag fired — the content tier stays armed for the day
  those words are read.
- Demotion: run clone_login2 logs exactly one
  `PROBE known specimen [2026-08-14 #2 ...] m=7000021C/... c=00000000/...`
  one-liner with values identical to the record. Cure-without-retest
  held: the table entry was authored from the record alone.

### Reviewer-tier flag F3c — the F1-mechanism candidate (NOT suppressed)

One packet-content flag, FAIL_OPEN path only (run clone_failopen):

```
***** PROBE PACKET-CONTENT FLAG (HIGH SIGNAL — reviewer tier) *****
mediated handler read differs: width 2 byte range E0002974..E0002975
  master=00000005  clone=00000000  (continuing, master authoritative)
  at master pc=7017FDED instr=1183772
*******************************************************************
```

Byte range E0002974 = word 0x700014BA — stack region. A mediated
handler consumed master=0x0005 / clone-zeroed=0 deep in the FAIL_OPEN
cascade, immediately before the ?FATAL detach. This is a LOCATED
CANDIDATE for the exact cell whose symmetric zeroing starves signal 2
under -zero=both (P10 F1); proving it means tracing this read to the
signal-2 raise site — explicitly a post-ruling task per P10 §7, not
chased here. Flagged for reviewer classification; no suppression
entry exists or should exist for the content tier.

## 5. Open questions / integration hazards

1. **LOCAL-call register mismatches** (design note, user-ratified
   treatment): a LOCAL-class call executes on BOTH engines with their
   own arguments — master-authority does NOT apply, so continuation
   lacks the mediated guarantee. The forensic record flags these with
   a loud `*** LOCAL-class call` banner for reviewer classification;
   probe mode's armed detectors (pc, counts, downstream mediated
   compares) catch consequential forks. Not silently promoted to the
   suppressible class. **No LOCAL-class specimen was observed this
   expedition** — both catalogued specimens are mediated.
2. **Per-run one-liner counts vary** (login1: 126, login2: 127,
   FAIL_OPEN: 61, S-move: 61) — driver path and screen-write cadence
   dependent. Expected; the count is not a gate.
3. **Standing stderr noise, pre-existing**: `RTBridge::zero_frame_claim:
   entry 7017E28C/7017E2BC/7017E302 opcode A6C9 is not WSAVx/WSSVx`
   lines appear in every run INCLUDING the pre-change baselines — P10's
   defensive skip-and-log on non-frame entries. Not introduced or
   changed here; left for the P10 lineage to classify.
4. **Specimens file location** deviates from a literal "next to the
   data files" reading — host cwd instead (scratch deletion; §3.4).
   Flagged in case the reviewer wants it inside a durable data dir.

## 6. Validation evidence (exact commands; expected values from these
   exact runs, METHOD §10)

All runs: fresh scratch QUEST copy, 1-core container, wrapper
`run_one.sh` (checkpoint tarball) around:

```
stdbuf -o0 -e0 ./Work/c_src/emulator <flags> -lockstep -silent \
    scratch_quest QUEST_SERVER @QUEST @QUEST > run_<tag>.log 2> run_<tag>.err
# driver: docs/Project1/drive.py (login CL/Claude/quest/Y/any/F + L→P + ESC)
# FAIL_OPEN runs add: QUEST_FAIL_OPEN=USER_DATA_FILE
```

**Step 0 — pre-change world-as-found** (tree at P10 delivery, binary
959896 → rebuilt 960216 here; behavior matches P10 REPORT §6):

| Run | Result |
|---|---|
| -handler=check -zero=both, login | 0 div; reaches GET_INPUT (shutdown backtrace: GET_INPUT+0x25 above START_TURN) |
| -handler=check -zero=both, FAIL_OPEN | 0 div; **no ?FATAL** (P10 F1 world); shutdown in GET_INPUT above ?LIB_ERROR+0x93 |
| -handler=check -zero=none, FAIL_OPEN | 0 div; **?FATAL DETACH 7017F036** |

**Step 1 — post-change inertness** (binary 969184, warning-free):
same three runs, same three outcomes line-for-line;
`grep -c PROBE run_post_*.{log,err}` = 0 everywhere; no
probe_specimens.log created. The mechanism is provably inert outside
probe mode.

**Steps 2–3 — the expedition** (`-handler=check -zero=clone`, login
driver):

| Run | known | new | content | div | Outcome |
|---|---|---|---|---|---|
| clone_login1 (table = #1 only) | 126 | **1** (#2) | 0 | 0 | GARBAGE PROBE banner; #1 demoted (§4); #2 full record; **login completes, GET_INPUT reached** |
| clone_login2 (table = #1,#2) | 127 | 0 | 0 | 0 | both demote to one-liners, #2 values = record; **SUCCESS: 0 unsuppressed halts** |

The expedition was ONE collecting run + one confirmation run — the
collect-don't-halt amendment did exactly what it was for.

**Step 4 — F1-composition verification** (`-zero=clone` + FAIL_OPEN,
run clone_failopen): 61 known one-liners, 0 new, **1 content flag
(§4 F3c)**, 0 divergences, then **?FATAL DETACH 7017F036** — the
master's 1988-true branch, matching the attic (-zero=none) and NOT
the -zero=both no-?FATAL behavior. The load-bearing design claim —
master-authoritative mediation keeps both engines on the same
downstream branch including the F1 cascade — is verified empirically.

**Step 5 — bonus one move** (driver drive_s.py: login, `S`, 60 s turn
wait, ESC; run clone_smove): move accepted, full turn processed
("Waiting for your turn" cadence), 61 known, 0 new, 0 content, 0 div,
no detach/abort. Nothing new to catalogue; the deeper expedition is
the user's free-play program. Launch line for it:

```
stdbuf -o0 -e0 ./Work/c_src/emulator -handler=check -zero=clone \
    -lockstep -silent <QUEST-copy> QUEST_SERVER @QUEST @QUEST > log
```

then telnet to 8781 and play; specimens accumulate in
probe_specimens.log (host cwd), one-liners for known, full records
for new, loud flags for content-tier, hard halts only for pc forks /
count skews / control-flow rendezvous mismatches.

## 7. Shared-doc corrections

1. **Project11/PROMPT.md** points at NextSession.md "NEXT TASK" Part A —
   no such section exists in the work archive; the label was edited
   away when the task split into Project 11 (user-confirmed). The
   whole-doc read is the correct re-entry.
2. **Project11/PROMPT.md** cites "Project10/REPORT.md §5.3's F1 cascade
   analysis" — the F1/F2 analysis is P10 REPORT **§7** (§5 is
   residuals, 3 items). User-confirmed pointer skew.

## 8. Deliverable notes

- Layering.md ruling 8: one-line catalogue pointer appended
  (→ this REPORT §4).
- Specimens logs in the checkpoint: probe_specimens.run1.log (the
  collecting run, containing #2's original record) and
  probe_specimens.log (subsequent runs).
- Checkpoint: /mnt/user-data/outputs/p11-ckpt.tgz — journal, run_one.sh,
  drive_s.py, all run logs, both specimens logs, changed sources,
  this REPORT.

## 9. Post-review amendment (reviewer, Aug 14): the SILENT tier

User request after expedition 1: suppression entries gain a
verbosity tier. **SILENT** = proven don't-care, no per-hit output at
all, counted for a one-line shutdown summary (hits + distinct master
ac0 values — accounting survives the silence). **NOTE** = the
existing one-liner, for specimens still under observation. New
specimens / content flags / halts unchanged. Promotion NOTE→SILENT
is a dated table edit. Specimen #1 promoted SILENT on expedition-1
evidence (445 hits, 72 distinct values, zero consequence); #2 stays
NOTE (one login hit/session, still observing). Implemented in
ProbeSuppressions.{hpp,cpp} + a summarize() hook at Launch shutdown;
build clean (975096). Live validation = the user's next expedition:
expected console = specimen-#2's single login line, then silence
until something NEW happens, then the summary line at exit.

## 10. Second amendment (reviewer, Aug 14): per-site input file

The catalogue moved OUT of the binary (user ruling): probe mode loads
probe_suppressions.txt from the launch cwd — authoritative when
present, built-in seed as fallback with a stderr note. Line format
documented in the file itself; canonical copy lives at Work/c_src/probe_suppressions.txt — the
launch cwd itself, no copy step (user ruling). Curing
a new specimen is now: read its forensic record, add one line,
replay — no rebuild. Tier edits (NOTE→SILENT promotions) likewise.
Build 980600, warning-free.

## 11. Third amendment (reviewer, Aug 14): the curator report at exit

Ctrl-C / shutdown now prints, in probe mode: silent-tier summaries
(hits + distinct values), NOTE-tier summaries (with a promotion hint
when they stayed boring), and — the user request — CATALOGUE ACTION
ITEMS: a ready-to-paste probe_suppressions.txt line for every NEW
specimen observed this session (provenance field left for the human;
full forensics in probe_specimens.log), or an explicit "no new
specimens — catalogue complete for played paths" line. Build 981008,
warning-free. Live validation = the user's next ctrl-C.

## 12. Fourth amendment (reviewer, Aug 14): RTBridge skip-log dedup

The §5.3 standing noise (zero_frame_claim on trampoline/alias
entries — first opcode a jump, A6C9/A6E9, ?LIB_ERROR/?ERMSG-region
aliases; no claim exists at such an entry, all sessions green) now
announces ONCE per entry per run; repeats silenced, genuinely new
shapes still loud. Classification stands as benign-by-evidence;
revisit only if a B1-class divergence ever appears near frames of
these routines.

## 13. Fifth amendment (reviewer, Aug 14): the ctrl-C wedge

User hit it live: ctrl-C mid-play → master IREC threw on interrupt,
but the CLONE was parked in the LockstepMediator Slot rendezvous for
that same call — a wait-point neither shutdown_all nor abort_all
covers — so its process never unregistered, Launch spun on
has_processes(), and FS::save_all (after the loop) NEVER RAN: the
shared files were not stored. Fix: LockstepMediator::release_all()
(atomic released flag; every Slot wait predicate gains a
released-escape; notify_all across slots), wired into BOTH shutdown
paths (sigint in Launch, abort_world in Lockstep). Ctrl-C is a
first-class exit (the curator-report flow depends on it); ESC
remains the in-game exit. Live validation = the user's next ctrl-C:
expect Unregistering for BOTH client processes, then the four
Writing lines, then the curator report, then exit.