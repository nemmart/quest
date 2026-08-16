# The Crossings-Only Checker (Aug 13 2026 — user-ratified, implemented)

The lockstep checker is keyed on LAYER TRANSITIONS, not entry
addresses. This REPLACED the entry-keyed checker in one session, with
no flag and no modes (user ruling: one sync model, permanently; the
old fine-grained behavior is not retained — a bit-faithful build in
HISTORY is the fallback microscope if one is ever needed again).

## The ratified sync surface

1. **L1 fabric, unchanged**: L0/L1 runtime-service entries pair as
   they always have (untranslated: at the entry; translated leaves:
   at the post-call point), syscalls pair at the gate, batch
   exhaustion (~500 insns) is the heartbeat.
2. **L1↔L2 crossings pair in BOTH directions**: every L1→L2 entry is
   a rendezvous AT the entry pc (argument state compared), and every
   L2→L1 exit (return, dispatch transfer, unwind landing,
   continuation) is a rendezvous at the target. Interior L2→L2 is
   INVISIBLE — no break, no pair, regardless of how it is reached.
3. **L3 pairs once at the door**: the terminal machinery
   (DETACH/ABORT/retire) is unchanged.

"Check the game's fabric continuously, check the handler machinery at
its skin, check death at the door." Rationale (user): what we care
about is the game/L1 world; L2's interior is nobody's business so
long as the state handed back to L1 verifies — and memory damage
cannot hide, because the first L1 read of a damaged cell diverges
against the master at the next pair or heartbeat.

## Step 0 — characterization (measured before changing anything)

A full M-trigger session under the OLD checker (2,699 pairs,
pair-classification script vs the Layering census):

| class | pairs |
|---|---|
| GAME heartbeat | 2,244 |
| SYSCALL gate | 112 |
| L1-fabric entry pairs (?WRITE_SCREEN, ?READ, X.CB, C?INIT, ...) | ~200 |
| L1-fabric post-call span pairs (native leaves in emulated bodies) | ~250 |
| GAME-range span pairs (translated L2 + leaf exits) | 34 |
| **entry pairs at ANY L2 symbol** | **0** |

Finding: the live structure was ALREADY nearly crossings-shaped —
every live L2 entry is translated, so every L1→L2→L1 traversal paired
once at its exit (native span) with the interior invisible. The gaps:

- no rendezvous AT the L1→L2 entry (exit-only verification);
- interior invisibility was an ACCIDENT of "everything live is
  translated", not a layer rule — an untranslated L2 routine calling
  L2 (the FOOBAR scenario) would have produced interior pairs on
  both engines and, worse, per-inner-call pairs when only some
  callees are native;
- the DISPATCH_RET/E3EF return-crossings had no explicit rendezvous.

## The implementation (four pieces)

1. **`RTStubs::l2_bits`** — the 30 L2 entries from the Layering
   census (20 translated + 10 frozen/dead), so every break decision
   can consult the layer. Entries in neither l2_bits nor
   terminal_bits are L0/L1 fabric.

2. **Deferred dispatch (`Machine::pending_native`)** — at all four
   dispatch sites (LCALL/XCALL in EagleStack, LJSR/XJSR in
   EagleGeneral) and in `RTStubs::inject_fire`: a native call whose
   target is TRANSLATED L2 is deferred — the site sets
   pending_native and returns the entry pc, the batch breaks AT the
   entry (the L1→L2 crossing rendezvous; strict count compare, both
   engines emulated identically to that point), and run_steps runs
   the native implementation first thing on resume, in place of
   fetch+decode. The master mirrors it: at a translated L2 entry it
   arms rt_pending_return AND breaks (previously it armed and
   continued silently). Non-L2 translated leaves keep today's
   immediate dispatch and exit-only pair ("same as today" ruling).
   The exit rendezvous is unchanged machinery: native_return /
   native_transfer on the clone, run-to-return termination
   (return-pc or RT-range departure) on the master.

3. **Untranslated L2 entries** (frozen/dead; never observed live):
   pair once at the entry, then arm rt_pending_return on BOTH roles
   — the whole emulated L2 subtree becomes one absorbed span ending
   at the L2→L1 exit. The four dispatch-site guards already suppress
   inner native calls inside a pending span, so absorption is
   symmetric by existing construction. Interior entries can never
   pair, even in the all-emulated world. Dormant-but-correct
   insurance: this is what makes interior invisibility a LAW rather
   than a coincidence of translation coverage.

4. **Return-crossing rendezvous** — arrival at DISPATCH_RET
   (0x7017EE40) or ?LIB_ERROR's O?SIGNAL-return (0x7017E3EF) with no
   pending span and no native_break is a break: the L1→L2 RETURN
   crossing (a dispatched handler's WRTN back into the signal tail;
   L2Contract §5). Gated on rt_pending_return==0 because both the
   clone's fallback spans and the master's run-to-return pass
   through these pcs as interior. Cold on live paths today (live
   handlers unwind via I.GOTO; E3EF at depth 0 is unreachable while
   ?LIB_ERROR is native) — same insurance class as item 3.

Untouched: heartbeat, syscall gate, terminal machinery
(detach/abort/retire kinds), compare_pair semantics, rtcalls,
coverage, captures, the injection staging, and all wrapper code
(runtime/ unmodified).

## Design rulings recorded

- **Crossings inside a native composite are subsumed** by the
  composite's own entry/exit pairs (e.g. native ?LIB_ERROR calling
  t_area/o_qsignal as C++): no shared architectural point exists
  mid-composite, and the composite's boundary pairs verify the whole
  traversal. Same principle as METHOD §7 "whole subtrees may go
  native together".
- **An escalation raise from the emulated dispatch tail** (EE4E test
  → LCALL O.SERROR) is treated as a fresh crossing chain, not
  suppressed interior — symmetric on both engines and verified at
  its own boundary pairs.
- **The entry pair costs one extra rendezvous per L2 crossing** (a
  handful per session — 24 in the M-trigger regression) and buys
  argument-state verification at the door plus a uniform rule for
  Phase 2: every L1→L2 crossing pairs at the entry pc, every L2→L1
  crossing pairs at the target pc, whatever the implementation
  behind the entry.

## Evidence (all runs this session, new checker, 0 divergences each)

| Run | Result |
|---|---|
| M-trigger (M+n+abc, ESC) | I.STOP detach + RETIRE + 4-file write-back; 24 new L2 entry pairs exactly at I.PROLOG/O.ON/I.EPILOG/O.REVERT/I.GOTO; rtcalls chain identical to the old-checker baseline run (X.CB ×7 → O?SIGNAL → I.GOTO → ?LIB_ERROR) |
| FAIL_OPEN double signal (L→P, continue) | signal 1 handled; cascade to ?FATAL; terminal pair at equal counts (340/340) then DETACH; the pair log shows every crossing as entry-pair (strict counts) + exit-span-pair, e.g. `7017EC7C I.GOTO 14/14` → `7016F1C4 span`, `7017ED9B O.ON 16/16` → `7016F1CF span` |
| QUEST_INJECT shape 1 (7016EC74:-1:0x2006, L→P) | INJECT ×2, O?SIGNAL(native) → handler dispatched ("Couldn't access user_data_file") → I.GOTO unwind. The session later died at ?FATAL via a real unhandled "invalid channel number" — REPRODUCED IDENTICALLY on the pristine baseline binary with the same driver (same trace, same death), so it is driver-timing/game-state variance (this session's 60 s post-P wait runs an extra game turn vs Project 5's 15 s), not checker behavior |
| QUEST_INJECT shape 2 (70176AA7, login-only) | exact documented outcome: O?SIGNAL(native) → terminal-bound fallback → DETACHED at 7017F036; entry pair verified at 7017EDED (11/11) |
| QUEST_INJECT shape 3 (:RESUME, login+M→n) | no detach; DEF?ON(native) ret=7017EE40 resume; play continued to the direction/turns prompts; entry pair 7017EDED 11/11 + resume exit pair at the raiser's post-call pc 70176AA7 (native_span, 106/4) |
| QUEST_TERMINAL=7016EC74:ABORT (L→P) | verified pair at the site, TERMINAL-ABORT banner, FS::save_all suppressed, world self-terminated, zero divergence spam |

Side findings, both reproduced on the BASELINE binary (not caused by
this change):

- The historical plain L→P→ESC driver does not reach I.STOP on this
  container: the first ESC leaves the list sub-menu / lands mid-turn,
  and the session ends only at socket close ("FSTerminal
  disconnect"). The M-trigger driver's ESC detaches cleanly and is
  the working ESC regression.
- ~1,100–1,200 "RESERVED ACCESS player[0]" stderr lines per session
  are standing noise on both binaries.

## What Phase 2 inherits

The rendezvous definition is now implementation-independent: a
stack-free L2 needs only to (a) be dispatched at the same 20 entries
(the deferral pairs at the door before its code runs), and (b) end
every traversal with native_return/native_transfer to the contractual
L2→L1 exit state. The DISPATCH_RET re-entry (hazard H6) already has
its rendezvous waiting (piece 4); what Phase 2 must add is the
native handling AFTER that pair (today the tail is symmetric emulated
code — verified by the shape-3 resume run).
