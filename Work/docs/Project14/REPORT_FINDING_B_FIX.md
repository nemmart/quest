# Finding B fix — I2 as a heap-fence invariant (implementation report)

*Implements docs/Project14/FINDING_B_MAPPER_FIX.md (ruling: M4aDesign
§12, from REPORT_FINDING_B.md). Code change only, per the task — NOT
built or run here; godspeed verifies. Files touched:
c_src/hw/Mapper.{hpp,cpp}. No handler, book, driver, or
design-of-record edit.*

## 1. The change

**(a) The latch is now `wsl − heap_break`, not `wsl`.**
`latched_wsl_` → `latched_diff_` (Mapper.hpp). At the first-push latch
site (`push_record`) we record
`(wsl & 0x7FFFFFFF) − (read_wide(rt::HEAP_BREAK) & 0x7FFFFFFF)`
(rt::HEAP_BREAK = 0x700001F0, from runtime/i_alloc.hpp — hw→runtime
include precedented by RTStubs.cpp). `i2_assert` (called from
push_record / wrtn_fixup / unwind_to, unchanged touchpoints) recomputes
the difference and aborts if it changed:

    MAPPER I2: wsl moved without the heap break while records live
    (latched diff …, now …; wsl …, break …)

Legitimate I?ALLOC/I?FREE motion (STASL 0x7017E903 / 0x7017E9D2-D5,
native i_alloc.cpp:191/271) moves both by the same size, same direction
→ difference constant → no abort. A wsl write with no matching break
motion — every slip the old form was written for — still changes the
difference and still aborts.

**(b) Stack-clearance bound at each i2_assert.** Current
`wsl > max(shadow_wsp(wsp), max over live records of master_wfp + 2 +
2*frame_wides)`, else abort
(`MAPPER I2: stack clearance violated …`). Notes on the two terms:

- `shadow_wsp(wsp)` is the master-side live wsp; shifts are positive so
  it is ≥ the clone's real wsp — one term covers both engines' stack
  activity.
- The per-record term is the ToClone leg's own extent end `hi`
  (master_wfp + **2 +** 2*frame), i.e. the exclusive end of the mapped
  band — two words *stricter* than the ruling's "extent hi" reading of
  master_wfp + 2*frame (the merge point). Deliberate: it bounds the
  whole band the leg can ever map, and on the fo geometry the margin is
  ~0x15C00 words, so the extra strictness costs nothing.

**(c) One forced consequence the task file didn't spell out (flagging
for the planning session's Mapper.md pass):** the latched wsl was ALSO
the stack-leg domain bound in `map_word` (both directions) and
`probe()`. With wsl legitimately moving −14, a *stale* latched bound
would keep classifying the reclassified band [7001714C, 7001715A) — the
live message buffer — as stack addresses: `clone_location` of a master
read/write into the buffer (?ERMSG, the handler's screen write) would
apply the compression shift and land wrong, i.e. I2 would go green and
the run would go red one syscall later. The three bound sites now read
the **live** wsl (`stack_bound()` = owner's current wsl), which under
(a) is trustworthy: I2 guarantees its motion is only ever
fence-legitimate, and (b) guarantees the leg stays strictly below it.
Addresses above live wsl are heap and take the identity — which is
exactly right for the buffer. Mapper.md's "bounded above by the latched
wsl" wording is the line this changes; wording update is yours.

Nothing else changed: touchpoints, record fields, shift arithmetic,
Finding A's `>=` stack leg and Q2/A merge-point handling are all as
they were (`grep 's >= it->W' hw/Mapper.cpp` still hits).

## 2. What godspeed should see

**fo leg — `failopen` DRIVER MODE** (drive.py mode `failopen`,
QUEST_FAIL_OPEN=USER_DATA_FILE via run.sh; NOT `m` mode — only failopen
keeps a game record live across the ?LIB_ERROR allocation and arms I2):

- **Before the fix (confirm reproduction):** abort at pc 7017EC7C,
  `MAPPER I2: wsl moved while records live (latched 7001715A, now
  7001714C)` — byte-identical to the standing evidence
  (evidence/b2/finding_B_failopen_I2.txt).
- **After:** GREEN, 0 div. FAIL_OPEN denial banner, ?LIB_ERROR runs
  (wsl 7001715A → 7001714C, break in lockstep, diff unchanged), the
  game's ON handler prints and takes its I.GOTO recovery
  (label 7016F1C4, HIT_ANY_CHAR) with `unwind_to`'s i2_assert passing;
  handler runs to its handled recovery. wsl stays 7001714C
  (buffer deliberately retained at B+0x3) — later touchpoints keep
  passing, no transient-window dependence.

**No-regression, 101 book** (same profile as the Finding A
verification):

| run   | expected                                                        |
|-------|-----------------------------------------------------------------|
| m ×2  | green, 0 div, anchors exact, I.STOP; DISPLAY_SCREEN through WRTN both GTOD variants |
| inj   | ?FATAL 7017F036 (QUEST_INJECT=7016A896:-1:0x2006, play mode, normal speed — timing-sensitive) |
| abort | both-engines banner                                             |
| play  | green for the 39-routine tour                                   |

m/inj/abort/play never take the fo path, so for them the change is: the
latch computes one subtraction, each i2_assert reads one extra wide and
does a depth-length max loop, and the bound sites read live wsl — which
equals the old latched value whenever no heap motion has happened.
Behaviorally identical unless something moves wsl, which is exactly
I2's business.

## 3. Things to watch in the godspeed run

- **Clearance crowding (task asked):** the Finding-A family is the case
  that crowds bound (b) — DISPLAY_SCREEN's post-A extents reach
  shadow_wsp 70001FD0 against wsl 7001714C+. That holds with ~0x15100
  words of margin, and the clearance is checked at touchpoints where
  those extents are live, so a green m/play run IS the confirmation. If
  a clearance abort ever fires with `high water` in the 70001Fxx range
  and depth including DISPLAY_SCREEN, that's this bound meeting that
  family — report it as a finding, don't loosen the bound.
- **First fo on a heap that has never allocated:** the latch reads
  HEAP_BREAK at first push (boot steal precedes any redirect, per the
  existing latch ruling); if the break cell were ever uninitialized at
  that point the latched diff would be garbage and the first legit
  alloc would abort with a diff message whose `break` field looks
  wrong — none seen in the trees, just listed so a weird first-abort is
  diagnosable from the message alone.
- Repeated fo signals free the old buffer (+14) then alloc the new one
  (−N by message length): diff returns to latched at both commits;
  touchpoints between free and alloc (none exist on this path — both
  happen inside one ?LIB_ERROR dispatch batch) would be the only way to
  see a transient diff. If a future handler path ever interleaves a
  touchpoint there, the diff abort will say so precisely.

## 4. Verification I ran here (per the task: no build/battery)

`g++ -std=c++17 -fsyntax-only -I. hw/Mapper.cpp` — clean. Finding A's
`>=` leg confirmed present pre- and post-edit. Nothing else run.
