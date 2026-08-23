# CheckerHistory.md — The Lockstep Checker, Generation by Generation

Each milestone reshapes what "verified" means, because each milestone
changes what the clone is allowed to do differently from the master.
This file tracks every checker generation: what it compared, where it
compared it, what it could not see, and why it was superseded. One
section per generation; add a new section when the checker changes,
never rewrite an old one (METHOD §11 — the wrong turns and old shapes
are load-bearing).

The invariant that never changes, across all generations: **the master
runs the original bytes and is the oracle; the clone runs the thing
under test; anything the clone does differently must be invisible to
the comparison surface, or it is a divergence.** Each generation is a
deliberate statement of what that comparison surface is.

---

## Generation 1 — the Milestone 2 checker (entry-keyed era, pure emulation)

*Built for: proving two identical emulations stay identical, and
catching the first translations. Reference: LockstepHarness.md,
EmulationVerification.md.*

**What the clone was allowed to do differently:** nothing at first
(both pure emulation), then — as M3 translations landed — run
per-routine native C++ whose stack/memory footprint is BIT-IDENTICAL
to the emulated original.

**Sync surface:**

- **Batch pairing**: client batches (500 insns) admitted in
  master/clone pairs, same task ordinal, master first, server never
  running during a client slice. Every pair compared: ending pc, full
  AC set + carry, instruction counts, exception text (strcmp), trap
  site for syscall-ended batches.
- **Syscall gate**: clients rendezvous at every trap. Same call, same
  ACs, same packet contents, or divergence. LOCAL calls execute in
  both; MEDIATED calls execute once on the master and replay into the
  clone (same error, same result ACs, same caller-memory side
  effects). Principle: master executes, clone gets checked copies.
- **Shared pages**: clone runs against private copies; the server's
  MirrorPages write both and COMPARE-ON-READ at every consumption;
  the pair-boundary page audit byte-compares all (real, copy) pages
  every 16 pairs to catch drift in unconsumed bytes.
- **RT entry breaks (the M3a refinement)**: batches additionally
  break when pc arrives at ANY of the ~130 runtime entry addresses,
  so every runtime call became a verified rendezvous with argument
  state compared. The sync identity was the ENTRY ADDRESS as a
  logical event.
- **Translated routines** (as they landed): the clone dispatches
  native at the call site; the master arms rt_pending_return at the
  entry and runs the emulated body to the return (or RT-range exit
  for transfers); the pair rendezvouses at the POST-CALL point with
  the instruction-count compare exempted (native_span). Fallback
  spans re-emulate symmetrically (nested-in-fallback guards at all
  four dispatch sites).
- **Terminal machinery** (late M3a): one final verified pair at
  terminal entries, then DETACH (clone halts, master finishes death
  alone), ABORT (abort_world, save suppressed — DERR ruling), or
  RETIRE (?RETURN 0310 keyed at dispatch).
- Plus the footprint-capture protocol OUTSIDE the checker
  (QUEST_CAPTURE NATIVE-vs-RETURN diffs) for what pairing cannot
  see: private memory residue.

**Blind spots (known and accepted):** anything both engines share —
emulator instruction bugs, wrong syscalls, shared wrong assumptions
(METHOD §2). Heaps could differ while registers agreed between
audits; the capture protocol covered that during translation
bring-up.

**Why superseded:** the entry-keyed identity breaks the moment L2
stops being bit-faithful. Once a native L2 no longer calls its
interior routines at their old entry addresses (O.ON calling FOOBAR
in C++, not by LCALL), entry-keyed breaks would fire on the master's
emulated interior with no clone counterpart — structural asymmetry
before a single register is compared. Interior invisibility held in
practice only because every LIVE L2 entry happened to be translated
(measured: 0 entry pairs at any L2 symbol in a full M-trigger
session), an accident of coverage, not a rule. M3b's
contract-fidelity license required making it a rule.

**Validation record:** carried Milestones 2 and 3 end to end — the
entire 616-word lift plus the DEF?ON cluster, two natural triggers,
three injected shapes, every play session, at 0 divergences; caught
the ?FILL_WORDS shallow-dereference in-session at the exact byte,
the WSUB carry residue, the wrong-sign heap gate, and the two
M3-merge integration bugs.

---

## Generation 2 — the Milestone 3b checker (crossings-only, CURRENT)

*Built for: licensing a contract-faithful (not bit-faithful) L2.
Implemented Aug 13 2026, replacing Generation 1 outright — no flag,
no modes (user ruling: one sync model; a Generation-1 build lives in
HISTORY if a fine-grained microscope is ever needed). Reference:
CrossingsChecker.md (design, characterization, evidence),
L2Contract.md §5/§7 (the crossing inventory it implements).*

**What the clone is allowed to do differently:** everything INSIDE
L2. Interior structure, interior calls, and L2-private storage are
contractually invisible; only the state handed across the L1↔L2
boundary (and the L3 door) is compared.

**Sync surface** ("check the game's fabric continuously, check the
handler machinery at its skin, check death at the door"):

1. **L1 fabric — unchanged from Generation 1**: heartbeat, syscall
   gate, shared-page mirroring/audit, L0/L1 entry pairs, translated
   leaf post-call pairs, terminal machinery.
2. **L1→L2 crossings pair AT the entry pc**: translated L2 targets
   are dispatch-DEFERRED (Machine::pending_native) so the clone
   breaks at the entry with argument state compared under the strict
   count rule, and runs the native code on resume; the master breaks
   at the same pc and arms its run-to-return. Untranslated L2
   entries pair at the entry and then arm rt_pending_return on BOTH
   roles, making the whole emulated subtree one absorbed span.
3. **L2→L1 crossings pair at the target**: returns (post-call
   point), handler dispatch transfers, unwind landings,
   continuations — the Generation-1 span machinery, now the
   normative exit rendezvous. Plus the return-crossing arrivals
   (handler WRTN to DISPATCH_RET 0x7017EE40 / E3EF) as explicit
   rendezvous when no span is pending.
4. **Interior L2→L2 is invisible BY RULE**, keyed on the layer map
   (RTStubs::l2_bits, 30 entries from the Layering census), not on
   translation coverage.
5. **L3 unchanged**: one verified pair at the door, then
   DETACH/ABORT/RETIRE.

**Rulings embedded:** crossings inside a native composite are
subsumed by the composite's boundary pairs (METHOD §7 whole-subtree
principle); an escalation raise from the emulated dispatch tail is a
fresh crossing chain, not suppressed interior; non-L2 translated
leaves keep exit-only pairing (fabric, "same as today").

**Blind spots (known and accepted):** everything Generation 1
accepted, plus — by design — L2 interior state. The license: scans
1–5 proved L1 never reads L2-private cells, so damage cannot hide
(the first L1 read of a damaged cell diverges at the next pair or
heartbeat), and shared pages remain server-compared. The register
file is never private (Contract §7); footprint captures of
L2-private memory are contractually retired for conforming
implementations, retained as a tool for bit-faithful work.

**Validation record (recalibration gate, Aug 13 2026, against the
unchanged bit-faithful L2):** M-trigger session (I.STOP detach +
retire + write-back; 24 new L2 entry pairs exactly at
I.PROLOG/O.ON/I.EPILOG/O.REVERT/I.GOTO; identical rtcalls chain to a
Generation-1 baseline run), FAIL_OPEN double signal (?FATAL detach,
crossing pairs at strict equal counts), all three QUEST_INJECT
shapes including the RESUME (entry pair 11/11 + resume exit span),
and the :ABORT test terminal (verified pair, banner, save
suppressed, clean self-termination) — 0 divergences everywhere; the
two anomalies found (shape-1 post-handler death, plain L→P ESC
non-detach) reproduced bit-for-bit on the Generation-1 baseline
binary, proving them environmental.

**What it must survive next:** Phase 2's stack-free L2 (the reason
it exists) — dispatch at the same 20 entries pairs at the door
before the new code runs; every traversal must end at the
contractual L2→L1 exit state; the DISPATCH_RET re-entry rendezvous
is already waiting for the native tail handling (hazard H6).

---

## Generation 3 — the Milestone 4 checker (anticipated, NOT built)

Placeholder so the shape of the next change is on record (premise
ratified in Plan.md M4): de-stackify the STORAGE, keep the
ACCOUNTING. The clone will maintain shadow stack arithmetic —
wsp/wfp/frame-address values computed exactly as the master's, with
no memory behind them — so the full-register-file compare and the
Generation-2 rendezvous survive M4 unchanged. Self-enforcing: a
stray stack dereference reads unbacked memory and diverges
immediately. Details when Step 2 starts; record the actual shape
here when it lands.

---

## Generation 4 — the M4a checker (shadow stack accounting + T(), CURRENT)

*Built for: licensing game routines whose WSAVS frame lives in a fixed
memory AREA (0x78000000+) on the clone instead of on the MV/8000
stack. Implemented Aug 15 2026 (docs/Project12/REPORT.md §2; design
docs/M4aDesign.md §5). Supersedes the Generation-3 placeholder; the
Generation-2 crossings surface is unchanged underneath.*

**What the clone is allowed to do differently:** hold ANY address where
the master holds a stack address, provided it is translatable — its
stack may look completely different (area frames; the real stack above
a hijacked frame shifted by the words the master pushed and the clone
did not). Code, pc streams and instruction counts stay identical.

**Sync surface additions:**
- **Live table + T():** per-Machine LIFO of hijacked frames (area wfp,
  the clone's real wsp at the WSAVS, argc, frame size, the master's wfp,
  cumulative shift). `T(v)` maps a live area address onto the master's
  frame, a real-stack address above a live frame by the cumulative
  shift, else identity; `T_any` also reads @-flagged (bit 31) and byte
  addresses; `T⁻¹` maps master addresses back for mediation.
- **Register rule at every pair:** pass iff `clone == master` or
  `T_any(clone) == master`, all four ACs; carry strict.
- **wsp:** `shadow_wsp == master wsp` at every pair (new — wsp was
  never compared before; parity in the stock world is now proven at
  every pair). shadow_wsp is T of the clone's wsp with a ≥ threshold —
  the design's "run the master's stack arithmetic in parallel" in
  closed form; any missed accounting fails every subsequent pair.
- **Mediation:** ACs through T_any; the master's packet reads are
  verified against the clone at T⁻¹(address) and differing pointer-
  shaped values pass iff T_any translates them; write replay lands at
  T⁻¹.
- **L2 door (ruling A):** the L2's own frame-chain ordering tests
  (R?SIGNAL walk, DEF?ON pre-walk, I.GOTO walk, native cut) compare in
  master coordinates through T — the same rule applied inside L2.
- Hijacked WSAVS/WRTN/unwind logged (`-types hijack`); re-entrancy and
  out-of-order area returns abort_world.

**Blind spots (known and accepted):** everything Generation 2 accepted;
plus a legitimate integer that happens to equal a shifted stack address
would pass under the OR-rule (no game integer lands in 0x78xxxxxx; the
real-stack shift window is a handful of words).

**Validation record (Aug 15 2026):** empty book → M-trigger + FAIL_OPEN
0 div with the wsp check armed; READ_IN live → login/M-trigger/ESC
detach (8 hijacked pairs), FAIL_OPEN, and an injected signal INSIDE
READ_IN (on-unit → I.GOTO unwind through the area frame → I.EPILOG),
all 0 divergences. Two mediation corrections caught in-session by the
mediated-input compare (byte and @-flagged pointer forms).

**Stage 0b amendments (Aug 15 2026, Project 13 §6):** base moved to
0x74000000 (0x78000000 collides with T?AREA heap growth); "hijack"
renamed "redirect" throughout (trace type `redirect`,
`area_redirect_enabled`); `T_any` is now a PREFIX DISPATCH on the top
byte (0x70/0x74 word, 0xE0/0xE8 byte, 0xF0/0xF4 @-word — M4aDesign §9)
instead of ordered guessing. Two boundary-2 rulings landed:
(1) **end-inclusive T** — a one-past-the-end pointer (WCMV cursor
residue) belongs to the frame it walked off (`v <= base+size`),
unambiguous because the book's bases now stride size+16 so
`block_end < next_base` always; pointers MORE than one-past-end remain
untranslatable by design. (2) **@-form on the inverse path** —
mediated handlers dereference packet pointers with bit 31 still set;
`clone_word_address` and write replay now mask, `T_inv`, re-encode
(the inverse of `T_any`'s 0xF0/0xF4 case). The blind-spot note above
should read 0x74xxxxxx. Validation: batch-1 battery re-proven identical;
batch-2 book (45 live) full battery at 0 divergences, 26 routines
exercised in free play (31k verified redirects).

## Generation 4/5 — M4a closed (Aug 22 2026)

M4a mechanism complete and closed. Frame redirect (WSAVS/WRTN → 0x74
areas) + Mapper (bijection A, codec E, equivalent/frame_precedes/
clone_location, invariants I1–I6) validated across 101 migrated
routines, 44 exercised in live play at 0 divergences over 1.3M redirect
events, plus scripted battery green. Findings A (s>=W stack leg) and B
(I2 as wsl−heap_break fence latch + live-wsl bound; clearance clause
removed) both fixed. Live signal dispatch with area frames confirmed
clean. 57 routines migrated-but-unexercised = coverage backlog.

## Generation 4/5 addendum — mid-window checkpoint term (P17, Aug 22 2026)

A COMPARISON-TERM change, not a mapper-identity change (no Gen-5 record
restructuring; A/E/T and the record identity are untouched). The wsp
checkpoint is now `master.wsp == clone.shadow_wsp() + checkpoint_offset()`
where the offset is `LiveRecord.stack_offset` of the top record (empty →
0, recovering the closed form exactly). Only decorated M4b ops move it:
redirected arg pushes +2/wide on the caller's record; the write-mode
WSAVS consumes −2·argc (P17 Stage-0 ruling: at the WSAVS, NOT the
decorated LCALL — the post-LCALL/pre-WSAVS boundary is a valid compare
point and the args are still elided there; battery pair evidence at
pc=70166E1C off=8 confirms). unwind_to's suffix-pop discards cut frames'
offsets for free. Validated: task-020 battery, div=0 all legs, 33
mid-window pairs passing at off ∈ {0,2,4,6,8}.

## Generation 4/5 addendum — M4b widened to all flat-LCALL sites (P18, Aug 23 2026)

No checker or mapper-identity change — the P17 comparison term carries
unmodified to 535 decorated sites (tranche A: 515 flat single-word;
tranche B: 20 WPSH multi-slot to TERRAIN/TERRITORY). Two emulator-side
completions of the EXISTING design: (1) the caller_write hook replicated
to XPEFB/LPEFB (P16 hooked only XPEF/LPEF; DIST's window never used the
B-variants — a decorated site with a B-variant push then ran write-mode
WSAVS with an un-elided arg → shadow +2·argc, captured in task 023 at
GET_INPUT/701760C4); (2) the WPSH multi-slot hook per the P18 spec:
AC[XX] → the map's base (lowest) slot, ascending, note_arg_write(m,
wides), fail-loud if map wides != AC group size. Loader grammar extended
to 3-field `push <pc> <base_slot> <wides>`, every wide's slot validated
in the callee arg region. Validated: task-024 (A alone) and task-025
(A+B) batteries, div=0 all five legs, 0 i2/probes/aborts, write-mode
WSAVS == WRTN counts (±in-flight at kill), WPSH windows exercised at 6
sites with offset += 2·wides mid-window (TERRAIN off 2→10 across the
WPSH, closing at 2·argc=18) and distinct per-slot values confirming the
ascending order. Remaining undecorated: 26 XCALL/nested + 5
RETURN_MESSAGE (tranches C/D, next project).
