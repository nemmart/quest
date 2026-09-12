# INTEGRATOR.md — standard operating procedures for the reviewer/integrator

*Written Sep 9 2026, from the practice that emerged over P26–P37. The
integrator is the session (or person) that writes prompts, rules on plan
gates, verifies deliveries, merges to main, and keeps the docs true.
METHOD.md is the law for implementation sessions; this file is the law
for the one reviewing them.*

---

## 1. The loop

```
write prompt → session runs Part 1 → PLAN GATE → rulings → session builds
   → delivery (branch or tarball) → VERIFY → merge → update docs → next prompt
```

Two gates, and only two: the plan gate (before code) and the landing
(before merge). Everything else is the session's own business.

---

## 2. Writing a prompt

A prompt is a contract. It states the goal, the design of record, the
files the session may touch, what it must refuse, and what it must
deliver. Sections that have earned their place:

- **GOAL** in one paragraph, with the number that defines success
  (embeds, statements matched, legs green). If success can't be stated
  as a number or a named artifact, the project isn't ready to prompt.
- **Context of record**, listed by path, in reading order, saying what
  each is for. Sessions read what you list and little else.
- **Carried-in rulings** from prior sessions and from chat discussion —
  a session cannot know what was decided in conversation.
- **Part 1 (plan gate) → STOP AND REPORT**, then Part 2 (build), Part 3
  (report). The gate items should be the questions you actually want
  answered, not a ritual.
- **Boundaries — BINDING**, numbered: which directories, which
  invariants (e.g. "the strict surface is untouched"), what is
  STOP-and-report rather than judgement.
- **Deliverables**, explicitly including *how* to deliver (below).

Sizing: one project ≈ one construct family, or one artifact, or one
census + one implementation. If the prompt has two "and also"s, it is
two projects. P36 is the cautionary case: a single 495-statement target
that interleaved five constructs produced no measurable result until all
five landed. P37's answer — several small routines, each isolating one
construct — is the pattern to prefer.

**Estimate honestly in the prompt.** State the expected size ("≈780
sites", "19 groups") so the session can tell you at the gate that your
census was wrong. It often is; see §4.

---

## 3. Delivery mechanics

- **Prefer a pushed branch.** Sessions with the clone URL push
  `pNN-topic` and nothing is ever lost. Sessions given only tarballs
  must send a **Work.tgz of the whole tree** — P35 crashed mid-delivery
  having sent a 5-file zip, and half the project had to be recovered
  from a second zip the next day.
- Say in the prompt: *commit and push at every stage boundary*, not
  only at the end.
- Task scripts for the runner go on **main**, never only on a branch —
  the runner polls main's `tasks/`.
- A task's three attempts are consumed by kills too; re-queue under a
  new name (`047b`) rather than expecting a retry.

---

## 4. Ruling on a plan gate

Read the whole report before answering. Then:

1. **Take corrections gratefully and say so.** Sessions census the
   ground; integrators write prompts from memory and prior reports. When
   the gate says your count was wrong, the gate is usually right — P37
   corrected a routine's address, its entry variant, the meaning of
   `wp(ac2, …)`, and the project's ordering, all in one report. Record
   your error in the reply so it lands in the report too.
2. **Answer every ruling request explicitly**, by its label (A, R1, S2).
   An unanswered request becomes a guess.
3. **Rule on what was asked, not on how to build it.** If the session
   proposes a mechanism you'd not have chosen but which is sound, say
   yes; it has read the code and you have not.
4. **Push back on scope creep and on unfalsifiable claims**, not on
   style.
5. **Prefer derivation to assertion.** Where a session offers a rule or
   a directive (a pragma, a mask, a hand-listed exception), ask whether
   it can be derived first, and require any assertion to be recorded
   with its evidence and *counted* in the report.
6. If a gate is right and needs nothing, say **"go"** and nothing else.
   Long agreement wastes the session's context.

**Reply format**: put everything meant for the session in a blockquote,
so the user can copy it at a glance.

---

## 5. Verifying a delivery — do not take the report on trust

The report says what the session believes. Check the claims that matter,
in the repo, before merging:

- **Re-run the self-tests and the comparator/gates yourself** on the
  branch. `--selftest` is NOT sufficient: it compares the book to itself
  and never invokes the translator, so a broken header or emitter passes
  it. **Translate at least one routine end to end.** (P40 shipped a
  `quest_rt.h` with an unterminated comment: nothing translated on the
  branch at all, while `--selftest` still passed and the report quoted
  349/349 from before that commit.) (P36: `ircmp.py --selftest` PASS and the P35 three still
  197/197 — verified on merge, not assumed.)
- **Reproduce the headline number** with the session's own commands
  (`lower.py` reproducing an artifact byte-for-byte; `translate.py` →
  `ircmp.py` reproducing a match).
- **Read the battery legs, not the verdict line.** Verdict scripts carry
  stale `want`s from the task they were cloned from; a RED task with 15
  green legs and two stale expectations is a green project (P32/046,
  P33-B/047b). Say so in the merge commit.
- **Check the diff is the claimed diff**: `git diff --stat main branch`,
  and for artifact changes confirm nothing outside the stated scope
  moved.
- **Confirm the branch's base**: a branch cut before a recent merge may
  need a rebase, and its provenance headers may be stale.

---

## 6. Merging

- Merge with an explicit merge commit whose message states the result,
  the battery/verification evidence, and anything carried as a known
  gap.
- Resolve doc conflicts by **keeping both** sides and demoting the older
  one (`## [merged] …`), never by deleting history.
- Immediately after the merge, fold the session's integrator notes into
  the **designs of record** (IR.md, the design docs, Mapper.md,
  CODEGEN_RULES.md, Provenance.md, EmulatorDivergences.md). A finding
  that lives only in a REPORT is lost.
- Mark the project's status in CURRENT_STATE and NextSession in the same
  commit.

---

## 7. Keeping the docs true

- **Designs of record** are amended when reality contradicts them, with
  the correction dated and attributed to the project that found it
  (StringsDesign's corrections log is the model).
- **Provenance.md** gets a new dated table whenever an artifact
  regenerates, with the old prefix alongside the new.
- **EmulatorDivergences.md** gets a row whenever a manual page is read
  against the source — fixed / open-benign / open-guard, with the
  argument.
- **NextSession.md** carries exactly one current re-entry brief at the
  top (tree of record, how to build and launch, where things stand, the
  ordered next items, operational reminders); everything older is
  demoted to `[merged]` history in the same file.
- When the project sleeps, write a **pause note**: state of main, what
  is in flight and where, the *first* thing to do on return, and a
  bolded TODO for anything known-broken.

---

## 8. Standing judgements (accumulated; add to this list)

- **A green battery is not proof of correctness** — only of the paths
  the legs walk. Ask for a "statements/sites never executed by any leg"
  count. 17 wrong IR statements once sat on main under a green battery
  because no leg reached those blocks.
- **Totality beats coverage**: a lowering that refuses loudly and leaves
  the original embedded is always preferable to one that guesses.
- **A rule that was proposed, regressed something, and got narrowed is
  worth more than a rule that always worked** — require the
  falsification to be recorded, not just the final form.
- **Confidence levels on rules** (A/B/C by number of independent
  witnesses) keep the model honest; do not let a one-instance rule be
  promoted by argument.
- **Never let a session invent a number.** No match percentage for a
  partial translation, no verdict for an untested rule; "not measured"
  is a valid report line.
- **Tools drift apart**: when two tools implement the same analysis
  (Follow vs StartStop), expect one to lag and plan a shared
  implementation.
- **The runner is a shared resource**: one battery at a time, JOBS=3 on
  the 4-core box, and check whether a task is queued on main before
  waiting for it.

---

## 9. Parallelism

Two sessions may run at once only if their **file sets are disjoint**.
The pattern that worked: a checker/Lockstep session alongside a
lower.py/IRExec session; a research session (Python tools + docs) beside
anything. State the shared-file rule in both prompts, naming the other
project. Phase A (census, design) is almost always parallelisable even
when Phase B is not.

---

## 10. The question channel (SOP, Sep 12 2026)

Questions used to travel as copy-pasted conversation text between a worker
session and the integrator session. Slow, lossy, and the answer landed
somewhere the worker could not re-read. **Questions, plan gates, and answers
now go through the repo. The user carries only a nudge, never content.**

### Files

    docs/ProjectNN/q001-short-title.md      written by the WORKER
    docs/ProjectNN/a001-short-title.md      written by the INTEGRATOR

Matching numbers and titles so a pair sorts adjacent. **Worker owns `q*.md`;
integrator owns `a*.md`.** Neither edits the other's files, so parallel
sessions never conflict. Everything on **main** — a question on a branch is
invisible.

### The loop

1. Worker writes `qNNN`, commits, pushes to main, **STOPS**, and tells the
   user it is there.
2. User tells the integrator.
3. Integrator pulls, writes `aNNN`, pushes, tells the user.
4. User tells the worker.
5. Worker pulls, reads `aNNN`, resumes.

**The worker waits.** It does not work ahead on other fronts while a
question is outstanding: a ruling can invalidate work done in parallel, and
reviewing work built on a guess costs more than the idle time saves.

**The plan gate uses this channel.** "Part 1 — STOP AND REPORT" means write
the gate report as `q001-plan-gate.md` and push it. Not chat.

### What a question must contain

The integrator has none of the worker's context — no scrollback, no
reasoning, no half-formed hypotheses:

- what you were doing and what you found
- the decision needed
- the options you see, with your read on each
- **your RECOMMENDATION** — what you would do if it were your call, and why

The recommendation is mandatory. It is often the whole answer ("yes, do
that"), it makes disagreement specific rather than vague, and writing it
frequently dissolves the question.

### What an answer must contain

The ruling, the reasoning, and whether it changes a design doc. **If it
changes a design doc, change the doc in the same push** — an answer file is
not a place for design to live, or the next session will not find it.

The channel also carries **unprompted guidance**: an `aNNN` with no `qNNN`
is legitimate.

### The user's role

Two nudges per exchange: *"there's a question"* to the integrator,
*"the answer is ready"* to the worker. No content, no copy-paste.
