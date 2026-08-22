# Project 16 — M4b first slice: one converted call site (REPORT)

**Session:** Aug 22 2026. **Verdict: Stage 1 mechanism PROVEN; Stage 2
battery surfaced the Boundary-2 stop condition (open-window shadow
accounting is needed). Stopped per boundary 2 — no Mapper or design-doc
changes made. Awaiting a ruling between two candidates (§5).**

## 0. Stage 0 ratifications (user, this session)

1. **Write-mode WRTN fixup is `wsp = W − 2`**, not `wsp = W`. W (wsp at
   WSAVS, as the code defines it) INCLUDES the pushed tombstone; the
   invariant is post-return wsp = caller's pre-window wsp. The M4bNotes
   "wsp = W" wording predates the call-marker ruling.
2. **Mode-aware push_record (+2·argc)**: a write-mode record's clone
   stack elides the args as well as the WSAVS image, and the tombstone
   sits 2·argc lower than the master's marker. `master_wfp =
   fwd_prev(W) + 2·argc + 10`; `shift_after = shift_prev + 2·argc + 10 +
   2·frame`. Ratified as within-scope: "NO mapper change" is scoped to
   the §3b structural items (which genuinely don't trigger — the
   tombstone keeps ≥1 wide per live call, W strictly increasing, no
   ties); the mode bit is the one M4bNotes anticipated.
   The write-mode overflow test is `shadow + 2·argc + 10 + 2·frame >
   wsl` (argc read first) for master symmetry.

Stage-0 addendum (per the concretized prompt ruling): **(a)** caller-side
pcs live in a SECOND table — one `pc → area address` map (`caller_map` in
AddressBook, loaded from `QUEST_PUSH_MAP`) — not merged into the book;
the book's key domain is callee entry pcs (WSAVS side), the caller map's
is call-site pcs (push/LCALL side). **(b)** per-pc EXACT destination
addresses (direct transcription of argmap + book layout; zero runtime
arithmetic; load-time validation against the book catches drift).

## 1. The site

`call DIST,4 at 70166E1C CLEAN` (callsites:249; argmap arg1@70166E19,
arg2@70166E14, arg3@70166E10, arg4@70166E0E — two interleaved `XWLDA 2`
register setups untouched). DIST book entry: area 74003950, wfp
74003962, max_argc 4, frame 0, WSAVS, slotpatch.

`Work/c_src/quest.pushmap`:
```
push 70166E0E 74003950   # arg4   push 70166E10 74003952   # arg3
push 70166E14 74003954   # arg2   push 70166E19 74003956   # arg1
call 70166E1C 74003958   # marker slot (wfp-10)
```

**Slotpatch watch (Stage-0 answer): clean.** DIST's `XWSTA 0,
[ac3+0x7FF8]` patches area wfp−8 — the saved-AC0 wide of the AREA
restore image, which write mode doesn't move. No interaction.

## 2. Stage 1 implementation (branch p16-m4b-first-site, merged to main)

- **AddressBook.{hpp,cpp}**: `caller_map` + `load_push_map_from_env()`
  (validates push slots inside the target's arg region; call slots ==
  some entry's wfp−10; requires a book). Loaded in Launch after the book.
- **Mapper.hpp**: `caller_write(pc)` accessor, gated on `book_` exactly
  like `entry_for_pc` — the master's mapper has no book, so the master
  pushes stock by CONFIGURATION (no role query). `LiveRecord.args_written`
  (the mode bit). `push_record` takes the mode.
- **EagleStack.cpp**:
  - XPEF/LPEF: caller-map hit → `write_wide(slot, resolved)`, wsp
    untouched, traced (`ARGWR`). (XPEFB/LPEFB/WPSH not hooked — this
    site needs none; the prescribed map covers them when a site does.)
  - LCALL: caller-map hit → marker STILL pushed (tombstone), marker
    value also written to the mapped slot, `machine.args_written = true`;
    resolved target verified against the slot's book routine (mismatch =
    abort_world before transfer).
  - WSAVS/WSAVR: consume-and-clear FIRST (before book lookup + overflow
    test). Three rules: set+book → write mode; set+non-book →
    abort_world; clear+book → M4a copy mode byte-identical. Write mode
    reads argc from the AREA marker copy (authoritative per ruling — the
    stack tombstone is never read for content), tests overflow with
    +2·argc, writes only the five restore wides, `push_record(mode=W)`.
  - Tripwires (riding proposal, INCLUDED — not separately ratified;
    ~6 lines, trivial to strip): flag-set abort at WSSVS/WSSVR, WRTN,
    WPOPB. Mediation-arrival abort NOT included (os layer, out of
    one-site scope).
- **Machine.{hpp,cpp}**: `bool args_written` (clone-only machinery,
  never compared or serialized — Q5 answered by architecture).
- **Mapper.cpp**: mode-aware `push_record` arithmetic (§0.2); WRTN fixup
  `wsp = args_written ? W−2 : W−2−2·argc`; `mode=W|C` in redirect traces.

In-container smoke: build clean (zero warnings), book 101/40 pages,
push map validated (4+1), decorated call resolved to DIST, server boots.

## 3. Stage 2 battery (runner task 018)

```
fo    div=1  i2=0  probes=0  m4b_aborts=0  writeWSAVS=97  argwr=392  end=clean+FATAL
m     div=1  i2=0  probes=0  m4b_aborts=0  writeWSAVS=84  argwr=337  end=clean+FATAL
inj   div=1  i2=0  probes=0  m4b_aborts=0  writeWSAVS=5   argwr=23   end=clean+FATAL
abort div=0  i2=0  probes=0  m4b_aborts=0  writeWSAVS=0   argwr=0    end=WORLD-ABORT+FATAL
play  div=1  i2=0  probes=0  m4b_aborts=0  writeWSAVS=5   argwr=23   end=clean+FATAL
```

**What is PROVEN by the trace** (results/018-p16-m4b-battery/):
- 84 complete write-mode DIST calls in the m leg before the divergence:
  4 ARGWR writes to the correct slots + marker (80000004) written AND
  pushed, per call; flag consumed at every WSAVS (m4b_aborts=0
  everywhere); copy mode coexists (18 mode=C frames, other routines).
- The ratified arithmetic holds at every write-mode pair of events:
  WSAVS `shadow_wsp == master_wfp + 2·frame` exactly (70001FE4 ==
  70001FE4, frame=0); WRTN lands at `W−2` (700010E2 from W=700010E4)
  with clean depth. The W−2 fixup and the +2·argc offset interact
  correctly across call 2 (and calls 3..84) — the specific pair the
  user asked to see: m_dist_first_calls.txt, seq 2439/2440 (call 1)
  and 2447/2448 (call 2), identical shadow geometry.
- abort leg: the terminal pc 7016871D sits INSIDE DIST's body and fires
  on the first DIST call from ANY site, before the decorated site is
  reached — that leg gives the conversion zero coverage (pre-existing
  leg shape, not a regression; note for the battery matrix).

## 4. THE FINDING — mid-window pairs are structural (boundary-2 stop)

**div=1 on every leg that reaches the site.** The tell: m-leg `argwr =
337 = 84·4 + 1` — the divergence hit after exactly ONE arg write of
call #85, i.e. with the arg window OPEN.

**Root cause:** client batches are a **500-instruction quantum**
(`Machine::run`: `batch = 500` under lockstep). Compare pairs therefore
land at arbitrary pcs — not only at crossings/syscalls/terminals. When a
quantum boundary lands inside the 6-instruction window, the count-matched
master has pushed k args (wsp climbed 2k) while the clone wrote them
(wsp flat); no record is open yet, so `shadow_wsp` maps the clone wsp to
the master's PRE-window wsp → off by exactly 2k → `wsp_differs`. 84
calls passed only because the quantum phase took ~500-instruction steps
to drift into a window. Reproduced in all four site-reaching legs.

This is M4bNotes issue 1(a) verbatim ("compare pairs WILL land
mid-window and must pass"). The design bet that no pair lands mid-window
holds against crossings (census: windows are straight-line pushes, no
calls/syscalls) but NOT against the quantum. Per boundary 2 this session
stopped: **no Mapper change, no design-doc change**.

**The raw dump (task 019, fresh m-leg reproduction — div after 5 write
calls this run, argwr 23 = 5·4+3, i.e. THREE writes into window #6;
quantum phase is timing-dependent run to run):**
```
master: result_pc=70166E19 insns=500 ... ac3=70001B8A wsp=70001FD6 shadow_wsp=70001FD6
clone : result_pc=70166E19 insns=500 ... ac3=7400333C wsp=700010E2 shadow_wsp=70001FD0
```
Both engines stopped AT 70166E19 (the arg1 push — the 4th push pc) on an
`insns=500` batch: the quantum boundary, mid-window, three pushes done.
Registers identical (ac0/ac1/ac2; ac3 equivalent under the standing area
map — T() shows 70001B8A both; identical 4-frame backtraces: the site
lives in DISPLAY_SCREEN → GET_QUEST → QUEST). The ONLY mismatch is
master wsp 70001FD6 vs shadow 70001FD0 — **off by exactly 2k = 6 for
k = 3 partial pushes**. Q.E.D.

## 5. Candidate rulings (presented neutrally — planning session to rule)

**C1 — Open-window shadow accounting** (the M4bNotes riding proposal /
Checker Gen-5 path): from the first redirected push, an open-window
record carries k = args written so far; `shadow_wsp` adds 2k while the
window is open; the window closes at the decorated LCALL (tombstone push
realigns both stacks' deltas; WSAVS then converts window → live record).
+ General and correct for every future converted site; the accounting
  model change is exactly the one M4bNotes/Mapper §3b already name.
− It IS the accounting-model change (Gen 5): the shadow becomes
  stateful between records; window abandonment (signal unwinds
  mid-window, GOTO cuts) needs defined semantics; touches the Mapper.

**C2 — Quantum alignment** (forbid mid-window pairs): if a batch would
end with pc inside a decorated window, keep stepping until past the
LCALL. Keyed by pc against the process-global caller map (AddressBook),
so BOTH engines extend at the same pcs by the same rule and instruction
counts stay matched; windows are straight-line and short (≤ ~14
instructions incl. register setups), so the extension is bounded and
deterministic.
+ Keeps the closed-form shadow (no Gen 5); touches Machine/scheduler
  batch segmentation, not the Mapper; small.
− Bends "master runs stock" in batch SEGMENTATION (though not in
  machine semantics — batch boundaries are checker infrastructure);
  needs the window's pc-extent representable (start pc per site — the
  caller map has the member pcs; the extent between first push and
  LCALL includes non-mapped setup pcs, so "inside a window" needs the
  site's [first-push, LCALL] range, a small derived table);
  mid-window signals remain possible (signal dispatch is not a batch
  boundary choice) and stay fail-loud under C2, silent-correct under C1.

Interaction with rules already ruled: neither candidate disturbs the
three unconditional flag rules, the tombstone ruling, or the ratified
record arithmetic — all of which the battery validated up to the
mid-window pair.

## 6. State of the tree / how to resume

- Repo main = Stage-1 code + quest.pushmap + tasks 018 (run, DONE with
  the finding) and 019 (evidence capture, queued/DONE). Branch
  p16-m4b-first-site preserved. This tarball mirrors repo main.
- To reproduce: `QUEST_ADDRESS_BOOK=…/quest.addrbook
  QUEST_PUSH_MAP=…/quest.pushmap`, m leg per task 018. To disable the
  conversion: unset QUEST_PUSH_MAP (M4a exactly as before — copy-mode
  path byte-identical).
- After the ruling: implement C1 or C2, re-run task-018 battery
  (expect div=0), then the Stage-2 report addendum + landing
  (CheckerHistory append if C1).
