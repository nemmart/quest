# Project 9 — Generation 3: the Terminal Pair Is Just a Crossing (B2 fix)

Hi Claude! Solo session; the reviewer verifies (and will re-run your
recalibration). Checker-core work: characterize, change minimally,
prove against known-good. You MAY edit hw/ checker code + the mode
gates in runtime/ that exist only to serve the old rule; you may NOT
edit the contract, NativeDesign, or the mv attic's bit-faithful
behavior. CheckerHistory.md gains a Generation-3 entry (you write it).

Read IN ORDER: docs/METHOD.md; docs/Project8/REPORT.md §6 (B2 — the
finding this fixes; B1 is OUT OF SCOPE, separately ruled);
docs/CrossingsChecker.md + CheckerHistory.md; docs/Project6/
L2Contract.md §3.13 (DEF?ON's ?FATAL-entry exit row — the contracted
door state), §5, §7, §11; docs/SharedProtocol.md "Terminal targets
INSIDE the RT range" (the rule you are RETIRING) + SessionPlan Aug-11
(the P1 discovery that spawned it); hw/Lockstep.cpp compare_pair +
hw/Machine.cpp terminal checks IN FULL; runtime/o_signal.cpp /
def_on.cpp / p_defon.cpp terminal-bound fallback gates.

## The incoherence being fixed (user-found, Aug 14)

L2→L1 exit pairs exempt instruction counts (native_span both sides) —
interior work is contractually invisible. But the TERMINAL pair
(L2→L3) still compares counts strictly and cannot accept a
native-transfer arrival (the clone's batch ends native_break, no
terminal flag → structural mismatch). Those are Generation-2 relics:
the fallback-whole-for-terminal machinery exists ONLY to march both
engines to the door in emulated lockstep to satisfy them. Same code,
invisible interior, but suddenly counts matter — incoherent with the
crossings philosophy. B2 (Project8 REPORT §6) is not a fact about the
program; it is a fact about this rule.

## The amendment (three pieces, all in the checker)

1. **Terminal flag on ARRIVAL**: pc landing on a terminal point sets
   terminal_reached however control got there — native_transfer
   included (today only emulated arrival sets it). Find every arrival
   mode; the native-break path is the missing one.
2. **Count exemption at terminal pairs** exactly as at exit pairs:
   exempt iff native_span on the arriving side(s); strict counts
   remain when BOTH arrived emulated (mv attic, DERR, :ABORT test —
   all unchanged and must stay green).
3. **pc + full register file remain compared** — the door state is
   contracted (Contract §3.13 for ?FATAL via DEF?ON); a native
   arrival must present it exactly.

Then RETIRE what the old rule forced: in native/check modes the
terminal-bound fallback gates (o_signal/def_on/p_defon) are replaced
by the native death path running TO the ?FATAL door and transferring
there with contracted state (native_transfer to ?FATAL becomes LEGAL —
amend SharedProtocol's composition rule with a dated correction
note). The mv attic keeps its fallback verbatim (bit-faithful
reference; passes under the amended rules since both-emulated ⇒
equal counts anyway — state this backward-compatibility argument in
CheckerHistory and PROVE it in the matrix). The ABORT kind (DERR.TRP)
and detach/retire tails are untouched.

## Recalibration (the acceptance bar; reviewer will re-run)

Full terminal matrix, BOTH -handler=mv and -handler=check: FAIL_OPEN
double signal (the live B2 case — check mode must now take the native
path to a ?FATAL detach, THE new machinery's proof), inject shape 2
(same door), shape 3 RESUME (must stay green — no terminal), shape 1,
M-trigger + ESC (I.STOP detach + retire + write-back),
QUEST_TERMINAL=<pc>:ABORT (both-emulated, strict counts STILL
enforced — prove the exemption did not leak), QUEST_BAD_TOKEN (the
abort composition). Every run 0 divergences, all
detach/abort/retire/write-back lines correct. Plus one NEGATIVE test:
demonstrate the strict-count check still catches a real skew
(temporary deliberate perturbation, then removed — precedent: the
Project 8 rig).

## Deliverables

The checker amendment + retired gates; CheckerHistory.md Generation-3
entry (the incoherence, the amendment, the backward-compat argument);
SharedProtocol correction note; docs/Project9/REPORT.md
(SharedProtocol format: as-built decision table for terminal pairs,
matrix evidence with exact commands per METHOD §10, and ONE section
"What Stage C still needs" — B2 resolved, B1 untouched, native-alone
still blocked on B1 only).

## Gotchas

The standing set (NextSession.md): ~49s turns, login
CL/Claude/quest/Y/any/F, scratch QUEST/, stdbuf, drivers in
docs/Project1/, port-8781 zombies, ≥120s ESC waits, grep-warning
false positives. Plus: -handler flag semantics (Project8 REPORT §1);
the check-mode mismatch throw is your friend — if your retirement
breaks outcome parity you will hear about it before lockstep does.
