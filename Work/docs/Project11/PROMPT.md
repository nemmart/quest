# Project 11 — The Probe Suppression List: Get Login Working Under -zero=clone

Hi Claude! Solo session; the reviewer verifies. Small, sharp scope:
build the exception-list mechanism for probe mode and drive the
garbage expedition through LOGIN — catalogue every specimen you hit,
suppress it, go again, until the login sequence completes under
`-zero=clone` and the game reaches the input prompt. That working
login IS the deliverable. NOT in scope: Project 10 step 2 (the
native-alone matrix) — a separate task; do not run it.

Read IN ORDER: docs/METHOD.md (esp. §15 — no long matrices, red =
stop-and-report); docs/Layering.md ruling 8 IN FULL (the zeroing,
ruling (ii), specimen #1, the probe program and its three payoffs —
your charter); docs/Project10/REPORT.md (the switch semantics, the
probe checker, specimen #1's full characterization, §5.3's F1
cascade analysis); docs/NextSession.md "NEXT TASK" Part A;
hw/Lockstep.cpp (the probe-mode register relaxation + the mediation
compare — find where world-facing call arguments are compared);
docs/CheckerHistory.md (you are NOT changing checker generations —
the suppression list is probe-mode-only instrumentation).

## The mechanism

A suppression table consulted ONLY in probe mode (-zero=clone;
refuse/ignore otherwise — shipping and attic semantics must be
provably untouched):

- Entry: (syscall number, site pc, register set) — specimen #1 seeds
  it: (0303, 0x7017E2F4, {ac0}). Site pc = the game-side caller pc
  the mediator already knows (verify exactly which pc the mediation
  compare has in hand at comparison time — characterize before
  building; if the natural key differs, say ordinal+call+register,
  derive and document the honest key, but it must be SITE-specific,
  not a global register exemption).
- Semantics (USER AMENDMENT — collect, don't halt): in probe mode,
  register-VALUE mismatches at the mediation compare NEVER halt —
  master stays authoritative (its garbage-authentic arguments
  execute; the clone consumes the master's results, keeping the
  world 1988-true and both engines on the same downstream branch,
  incl. the F1 cascade — VERIFY that composition empirically, it is
  the load-bearing design claim). Instead:
  - KNOWN specimen (in the table): one-line log (id, pc, values).
  - NEW specimen (not in the table): a FULL FORENSIC RECORD (spec
    below), then continue. The user may never be able to reach this
    code path again — the record alone must suffice to author the
    catalogue entry and its suppression, cure-without-retest.
  - Halts remain ONLY for the real alarms: pc forks and count skews
    (the engines disagree on control flow — continuing is
    meaningless there), and those still stop the world loudly.
- THE FORENSIC RECORD (one block per new specimen, stderr AND
  appended to a probe-specimens log file next to the data files so
  it survives the session): specimen auto-number; syscall number +
  name; site pc (the suppression key exactly as implemented); BOTH
  engines' full register files (mismatching registers marked);
  wsp/wfp/psr/carry both sides; instruction counts; ordinal + tid;
  the mediated packet contents from BOTH engines (word dump, diffs
  marked); a master-side call-stack backtrace (CallStack has it —
  the caller chain is the provenance trail a human cures from); and
  the last few rendezvous pcs if cheaply available (locates the
  game phase). Everything needed to author the suppression entry
  and argue its safety WITHOUT reproducing the run.
- Source: a simple built-in table in one file (probe_suppressions),
  each entry with a dated comment naming its specimen. No config
  files, no env parsing beyond what exists.

## The expedition (the actual work)

1. Build; verify -zero=both login-fast regression green (2 runs:
   login, FAIL_OPEN) and -zero=none untouched (1 run) — the
   mechanism must be inert outside probe mode.
2. `-zero=clone -handler=check`, login. It halts at specimen #1
   (known). Confirm the suppression admits it (banner + log line, no
   halt), and record how far the run then gets.
3. With collect-don't-halt, the login expedition is ONE run that
   accumulates records (re-run only after adding table entries, to
   confirm they demote to one-liners). Author a catalogue entry +
   suppression per record. A pc fork or count skew is a FINDING of
   a different class — STOP and report, never suppress (those are
   the probe's real alarms). Packet-CONTENT diffs: log them in the
   record and flag for reviewer classification — they are
   higher-signal than register cargo.
4. SUCCESS = the full login sequence (CL/Claude/quest/Y/any/F)
   completes under -zero=clone and reaches the game's input prompt,
   0 unsuppressed halts. Try ONE move (S) as a bonus; catalogue any
   hit; do not chase further — the deeper expedition is the USER'S
   free-play program with this instrument.

## Deliverables

The mechanism (probe-mode-gated, specimen-commented);
docs/Project11/REPORT.md (SharedProtocol format) containing THE
CATALOGUE: one section per specimen — id, site, syscall, registers,
values, provenance-if-derived, and the suppression entry — plus the
exact expedition sequence (halt-by-halt), the F1-composition
verification result, and the login-working evidence (exact commands
+ outputs per METHOD §10). Update Layering ruling 8's catalogue
pointer to this REPORT (one line). Checkpoint per the environment
protocol: tar to /mnt/user-data/outputs/p11-ckpt.tgz + journal line
after EVERY green step — the container WILL reset on you.

## Gotchas

The standing set (NextSession.md): ~49s turns, login
CL/Claude/quest/Y/any/F, scratch QUEST/, stdbuf, port-8781 zombies
(pkill -9 -f emulator between runs), rm /tmp traces + scratch
between runs, make -j2, one emulator run per tool call,
grep-warning false positives. Expect the expedition loop to be
SHORT runs — login-only, ~2-3 min each; a dozen specimens would
still be under an hour. If the specimen count explodes past ~15,
stop and report the shape of what you're seeing instead of grinding
the list out.
