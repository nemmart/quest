# Project 10 — REPORT (Stack-Claim Zeroing + the Garbage Probe)

Per SharedProtocol.md REPORT format, adapted to a hw/-side project
(no translation_table entries; the "routine" is the claim machinery).
Solo session, Aug 14 2026. Code changes: hw/EagleStack.cpp,
hw/Machine.{hpp,cpp}, hw/RTBridge.{hpp,cpp}, hw/Lockstep.{hpp,cpp},
os/OSProcess.cpp, Launch.cpp. No checker-generation change (the probe
relaxation is a mode, not a generation; the full checker is untouched
in both/none). No doc edits beyond this report and the ruling-8 note.

## 1. Status

| Piece | Status |
|---|---|
| Prerequisite check | **Project 9 APPROVED** (SessionPlan verdict, Aug 14). Reviewer's B2 reproduction was DELEGATED to this project's baseline — **discharged, green** (§6 step 0). |
| Scope 1 — claim zeroing | **DONE** (§3): EagleStack sites + RTBridge clone-side symmetry, per-machine `zero_claims` gate, tripwire at WSAVS. Warning-free build. |
| Scope 2 — `-zero=both\|none\|clone` | **DONE** (§4): one switch; clone implies the probe checker + loud banner; clone requires -lockstep; bad values rejected. |
| Scope 3 — validation | **STOPPED RED at step 1** (§6): default-mode regression is 0-divergence but BEHAVIORALLY red — the FAIL_OPEN second signal vanishes under zeroing. Steps 2 (B1 vanish) and 4 (attic-only pass) NOT run as gates; the attic run happened as the A/B control and is green. Step 3's scripted probe ran early as the localization instrument and produced **the project's first probe specimen** (§7). |
| Ruling 8 IMPLEMENTED note | Added to Layering.md, marked "validation red, ruling needed" — see §8. |

**Bottom line: the implementation is mechanically sound and the
user's probe design worked on its first run — it located a live,
load-bearing 1988 read-of-uninitialized bug at login. That same
dependence means the both-zeroed default CHANGES GAME BEHAVIOR
(silently, symmetrically), so shipping it needs a user ruling
(§7 options). Per METHOD §15 the session stopped and reported.**

## 2. Claim-site audit (Scope 1 mandate)

Every wsp modification in hw/ + runtime/, classified. A CLAIM = a wsp
increase exposing unwritten words; claimed words for old→new are
[old+2, new+2) — wsp points AT the top live wide (wide_push
pre-increments; LDATS reads read_wide(wsp)).

| Site | wsp effect | Class | Zeroed? |
|---|---|---|---|
| EagleStack WSAVR/WSAVS | +frame_size*2 after 5-wide push | CLAIM | YES (tripwire here) |
| EagleStack WSSVR/WSSVS | +frame_size*2 after 6-wide push | CLAIM | YES |
| EagleStack WMSP | ±2*ac[AA] | CLAIM iff positive | YES (after the limit checks, so fault behavior is unchanged) |
| EagleStack STASP | := ac[AA] | CLAIM iff raise | YES (defensive; known live use lowers) |
| EagleStack WRTN, WPOPB | decrease | pop | — |
| EagleStack LDATS/STATS/ISZTS/DSZTS | none | — | — |
| EagleStack WPSH (+XPSH family) | increase, writes every word | push | — (not a claim) |
| EagleStack handle_overflow | wsp \|= 0x80000000 | boot-fault MARKER — copy_segment strips bit 31, so the masked address is unchanged and no space is exposed | — |
| Machine::wide_push | +2, writes | push | — |
| EagleIntegration wrtn/wrtn_void | wsp=wfp then pops | return path | — |
| runtime/frames.cpp push_wide / wrtn | push replica / WRTN replica | push / pop | — |
| RTBridge native_return | decrease | pop | — |
| RTBridge native_return_ss(final_wsp) | wrapper-chosen exit wsp | contract exit state (restore/lower on all audited paths) | — (see §5 residual) |
| runtime/def_on.cpp wsp=E+38 / E+32 | interior-state replication | wrapper residue (cells already written by the wrapper) | — (see §5 residual) |

No unlisted claim site found.

## 3. As-built: the zeroing

- `hw::Machine`: static `zero_mode` (ZERO_BOTH default / ZERO_NONE /
  ZERO_CLONE) + per-machine `bool zero_claims` (constructor: mode ==
  BOTH; clone-keyed override at task registration in OSProcess.cpp —
  the same place lockstep_role lands).
- `EagleStack.cpp zero_claim(machine, old_wsp, new_wsp)`: zeroes words
  [old+2, new+2) via `machine.memory->write_word(copy_segment(
  machine.pc, w), 0)` — the SAME addressing wide_push uses, so the
  writes land where the mirror/audit machinery already looks (the
  prompt's gotcha). Guards: no-op unless new > old; **no-op if old
  and new differ in bit 31** — a sign-crossing "raise" is the boot
  marker or I.INIT stack re-basing, not a claim, and under 28-bit
  masking a sign-crossing loop would sweep the segment.
- **Clone-side symmetry (design finding, implemented)**: the prompt
  scoped zeroing to EagleStack, but the clone's native wrappers never
  EXECUTE the claim — `RTBridge::emulate_frame/_ss` writes only the
  frame image. Any translated routine that reads a never-written
  frame local (the documented ?WRITE_SCREEN class, RTBridge.hpp
  "Dead-stack residue fidelity") would read master=0 / clone=stale —
  B1 re-created one layer down. So `RTBridge::zero_frame_claim(fp)`
  now zeroes the same words the master's WSAVx/WSSVx zeroes, gated on
  the same flag. frame_size comes from the entry instruction itself
  (machine.pc IS the entry pc at dispatch — Machine::run_steps runs
  the wrapper in place of fetch+decode there), and the raw opcode is
  verified first (WSAVR/WSAVS/WSSVR/WSSVS = fixed encodings
  A729/A739/8729/8739; a non-frame entry logs and skips, never zeroes
  a garbage range). Audited: every wrapper calls emulate_frame*
  exactly once, at its own entry (20 sites, 13 files). This is
  COMPLETION of ruling 8's "both sides read 0 for the whole class",
  not an extension — flagged for reviewer ratification.

## 4. As-built: the switches

`-zero=both|none|clone` in Launch.cpp (one switch, per the user
ruling — the asymmetric configuration IS the experiment):

- **both** (default) / **none** (bit-faithful attic, precedent
  -handler=mv): normal full checker, no checker interaction at all.
- **clone**: requires -lockstep (rejected otherwise); master
  unzeroed + clone zeroed via the registration-time override; sets
  `Lockstep::probe_relax_regs`, which relaxes ONLY the
  register-VALUE comparison in compare_pair. Armed and verified
  still armed: pc/address, instruction counts under the existing
  Gen-3 exemption rules, terminal/span structure, trap sites,
  exception text, and syscall mediation (untouched upstream — it is
  what caught the specimen). A loud multi-line GARBAGE PROBE banner
  prints at launch.

## 5. Open questions / integration hazards (residuals)

1. **Interior frames written manually as wrapper residue** (e.g.
   mv_error_handler's inner routine images, def_on's E+32/E+38
   states) are NOT claim-zeroed. Exposure: a bit-faithful wrapper
   replicating a read of an inner never-written local would go
   master=0/clone=stale. No regression tripped it (all green runs
   §6), so no live translation does this today; first occurrence
   surfaces as an ordinary divergence. Carried, not fixed.
2. **native_return_ss(final_wsp)**: a wrapper COULD raise wsp at
   exit; all four current callers restore/lower. A future raiser
   would be a claim. Carried.
3. **Project9/REPORT.md is missing from the work archive** (only
   PROMPT.md); the SessionPlan verdict references its §2. Baseline
   was reconstructed from the P10 prompt's explicit set + Project8
   REPORT §1 semantics. Also the P10 prompt's pointer to a
   NextSession.md "IN FLIGHT" section — no such section exists;
   the SessionPlan verdict was used as the in-flight state.

## 6. Validation evidence

All runs: fresh scratch QUEST copy, `stdbuf -o0 -e0`,
`-lockstep -silent`, driver docs/Project1/drive.py
(login CL/Claude/quest/Y/any/F + L→P + ESC), 1-core container.
Build warning-free; pre-change binary size 959896 = the reviewer's
clean-rebuild number.

**Step 0 — pre-change baseline (the world as found; discharges the
reviewer's delegated B2 reproduction):**

| Run (pre-change tree) | Result |
|---|---|
| -handler=check QUEST_FAIL_OPEN=USER_DATA_FILE | 0 div; **?FATAL DETACH at 7017F036** — the delegated B2 case, green on a fresh container |
| -handler=check QUEST_INJECT=70176AA7:-1:0x2006 (shape 2) | 0 div; ?FATAL detach 7017F036 |
| -handler=check QUEST_TERMINAL=7016EC74:ABORT | 0 div; WORLD ABORT banner, save suppressed |
| -handler=mv QUEST_FAIL_OPEN=USER_DATA_FILE | 0 div; ?FATAL detach 7017F036 |

**Step 1 — post-change default (-zero=both) regression: RED
(behavioral), and the A/B that proves it:**

| Run (post-change) | Result |
|---|---|
| -handler=check FAIL_OPEN, -zero=both (default) | 0 divergences BUT **NO ?FATAL** — signal 1 handled, then the game returns to input; shutdown finds it in GET_INPUT above ?LIB_ERROR. Reproduced 2/2. |
| same, -zero=none (attic) | 0 div; **?FATAL detach 7017F036** — bit-for-bit the baseline outcome. The attic works (step 4's question answered en passant). |

Interpretation: zeroing is symmetric, lockstep agrees on both sides —
this is the ruling's own residual-risk class (load-bearing garbage =
behavior change invisible to symmetric lockstep) observed in the
first regression run. Per METHOD §15, red = stop; the probe run
below is the CHARACTERIZATION of this red, not iteration on it.

**Step 3 (run early as the localization instrument) — the probe:**

```
./emulator -handler=check -zero=clone -lockstep -silent <QUEST-copy> \
    QUEST_SERVER @QUEST @QUEST > log
env: QUEST_FAIL_OPEN=USER_DATA_FILE   driver: drive.py
```

Result: banner printed; run halted at the FIRST syscall rendezvous
after login input, with the full specimen (§7). This validates the
probe machinery end-to-end: asymmetric zeroing wired per-machine,
register relaxation active (the run got PAST the first zeroed frames,
which a full checker could not), mediation armed and decisive.

**Steps NOT run** (blocked behind the ruling): step 2 (-handler=native
B1-vanish + native-alone headline) and the formal step-4 attic pass
(though the A/B above is that run in all but name). The shape-2 and
:ABORT post-change regressions were also not run — with FAIL_OPEN
already red, more matrix rows prove nothing the ruling needs.

## 7. FINDINGS (the headline)

**F1 — the FAIL_OPEN→?FATAL cascade is garbage-dependent (1988
behavior).** Under -zero=both the second, unhandled signal never
raises; under -zero=none it does, exactly as documented since
Project 5. Symmetric, deterministic, 2/2 reproducible, invisible to
lockstep by construction.

**F2 — probe specimen #1 (the mechanism, located to the pc):**

```
================ LOCKSTEP DIVERGENCE ================
rendezvous mismatch (call or arguments):
  master: call=0303 ac0=0000FFFF ac1=00000000 ac2=700010F2
  clone : call=0303 ac0=00000000 ac1=00000000 ac2=700010F2
SYSCALL 0303 issued from 7017E2F4, during LOGIN (frame: QUEST+0x42)
```

This is the documented ?WRITE_SCREEN dead-locals read caught live:
"loads never-written locals [4..5] into ac0 for SYSCALL 0303"
(RTBridge.hpp, derived in the M2 era from the disassembly). Per
METHOD §13's ?ERMSG convention, ac0=0x0000FFFF is two 8-bit fields —
255-byte buffer, channel 0377. Zeroed, it becomes buffer 0 /
channel 0. The screen/error machinery therefore runs on stack
residue on EVERY login, and downstream the changed channel/length
plausibly starves the "invalid channel number" cascade that
produces FAIL_OPEN's signal 2 (the F1 mechanism — consistent, not
independently proven; proving it means tracing the 0303 result into
the signal-2 raise site, a post-ruling task).

Note the fired pc 7017E2F4 sits in the same packet-builder
neighborhood as Project 8's B1 trace (7017E2C8 / stub 7017FDED):
B1 and these findings are one family — L1 code whose dataflow
passes through cells it never initializes.

**Needs a USER RULING before further work:**
(i) zero-with-exceptions — keep ruling 8, carve out the
    ?WRITE_SCREEN-class cells (candidates: preserve residue
    semantics at the syscall boundary for the known packet cells, or
    exempt specific frames), keeping M4's determinism argument
    intact for everything else;
(ii) accept the behavior change as part of infidelity #4 and
    re-baseline the FAIL_OPEN expectations — NOTE: signal 2 is
    load-bearing validation machinery (the B2 case, terminal
    matrices), so this quietly retires a trigger the project uses;
(iii) hold zeroing at -zero=none default until M4, where the
    prologue design can address the class wholesale;
(iv) something else. The specimen makes any of these an informed
    choice — which is what the probe was for.

## 8. Deliverable notes

- Layering.md ruling 8: dated IMPLEMENTED-with-caveat note appended
  (one line, per the prompt; the reviewer integrates the rest).
- The probe launch line for the user's long free-play session is in
  §6 step 3 verbatim (drop QUEST_FAIL_OPEN, play at leisure). What a
  hit looks like: `rendezvous mismatch` naming call + ACs (syscall
  mediation, as in F2), a `LOCKSTEP DIVERGENCE` with differing
  result pcs (pc fork), or a count skew at a non-exempt pair — each
  report names its pc; each hit is a located 1988
  read-of-uninitialized, F2 being the type specimen.
- run_one.sh (session scratch, /home/claude) wraps the standard
  scratch-copy/stdbuf/driver choreography; commands above are exact.
