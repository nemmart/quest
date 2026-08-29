# Project 24 — the wide-carry correction (prove, fix master, fix L2)

Hi Claude! Solo implementation session; the user reviews at the plan
gate and at the landing. Background of record:
**docs/Project23/WideCarry.md** (the finding, the parked patch, the
WADC open item) and Project23/REPORT.md §2b/§8.1. Read METHOD.md
first, as always — §2 (lockstep cannot see this bug: both engines
share the helpers), §8 (do not guess at silent-error semantics), §10,
§11, §15 all bear directly on this project.

## The finding, restated

`EagleInstruction::add()/sub()` compute CARRY as `>>31` (the result's
sign bit), not the ALU carry-out (bit 32), contradicting the DG manual
("CARRY set according to value of ALU carry"). Every wide arithmetic
instruction that goes through the helpers writes a wrong carry: WADD
WSUB WNEG WADC WINC WADI WSBI WNADI WADDI XWADD LWADD XWSUB LWSUB
XWADI XWSBI. The narrow (Nova ALC, `>>16`) path is correct. OVR is
correct. Because master and clone share the helpers, lockstep has
been green over a defect the whole time — only the manual caught it.

A prior session's (unrecorded, off-rails — REDO everything) census
concluded: carry is only ever READ by Nova 16-bit instructions;
wide instructions write carry (and WSAVS/WRTN copy it through frame
images everywhere) but nothing wide-produced is consumed — with a
short exception list. The user's candidate exception list (verify,
don't trust — it was cut on an earlier state of the listings):

```
quest.code:70160e64 ADC.C 0,0,SNC
quest.code:70160e73 ADC.C 1,1,SNC
quest.code:7016e75b ADC.C 1,1,SNC
quest.code:7016e76a ADC.C 0,0,SNC
```

P23 guessed these are multiple-precision math. Guessing is not the
deliverable — part 1 proves what they are.

## The project, in three parts + battery

### Part 1 — PROVE the carry-consumption claim; ship the exception list

On the CURRENT regenerated listings (post-LNADI/LNSBI; quest.dis sha
1f9153c0…, quest.blocks.split CFG) and the current emulator source:

1. Enumerate every instruction that READS carry: Nova ALC carry-field
   users (blank/C forms), all SNC/SZC-family skips, and any
   instruction whose emulator implementation reads `machine.c`
   (extract the reader set MECHANICALLY from the emulator source —
   P23's skip-table lesson: hand lists lie, source extraction
   doesn't). Do the same for writers. Remember METHOD §4: XCT sites
   and data-region holes hide instructions from grep; use the tools,
   not the text, where reachability matters.
2. For every reader site in game + runtime range, classify the
   reaching carry writer(s) along actual CFG paths (quest.blocks.split
   + tools/dataflow.py as a starting point): NOVA-reached
   (fix-invariant), WIDE-reached (behavior changes under the fix), or
   AMBIGUOUS (both/unknown — treat as wide until proven otherwise).
   WSAVS/WRTN carry restores count as TRANSPORT, not writes: trace
   through to the value's producer.
3. Deliverable: docs/Project24/CarryCensus.md — the method, the
   mechanically-extracted reader/writer sets, the classification
   table, and a per-site write-up of every WIDE/AMBIGUOUS reader
   (expected: the four sites above; any delta from the user's list is
   a FINDING to report, not silently reconcile). For each exception
   site: what the code computes, which wide writer reaches it, what
   the old vs fixed carry value is on that path, and whether the
   difference is game-observable (the user believes at most one site
   may matter — settle it with evidence, empirical where reachable:
   these blocks appear in normal-play IR coverage, so a targeted
   trace or capture should be attainable).
4. **Plan-gate ruling to collect (with the census in hand): WADC.**
   WideCarry.md's conservative default routes WADC around the fix
   (keeps today's c=1 for `WADC x,x`) because real-hardware evidence
   is lacking. If the census proves no wide-produced carry is consumed
   anywhere that WADC's carry can reach, the routing question becomes
   unobservable-to-gameplay and the user may rule to fix WADC with
   the rest; if any exception site is WADC-reached, the evidence bar
   from WideCarry.md stands. Present the options; the user rules.

STOP AND REPORT at the plan gate with CarryCensus.md before touching
any code. Parts 2–3 proceed only on the user's go-ahead, and their
shape may be adjusted by what part 1 finds.

### Part 2 — fix the emulator (master becomes right)

Land docs/Project23/wide_carry_fix.patch (or its re-derivation if the
tree has drifted — verify it still applies cleanly and still encodes
the manual's semantics; the patch header's sanity vector: 0xFFFFFFFF+1
→ c=1; 5−3 → c=1; 3−5 → c=0; x−x → c=1). Honor the WADC ruling from
the plan gate. Because the IR `#`-ops call the same helpers (P23
ruling), the fix covers emulated master, emulated clone, and IR clone
in one place — that was the point.

Also owed here: the METHOD §5 correction note ("WSUB x,x clears
carry" documents the OLD behavior — annotate per METHOD §11, do not
silently rewrite), and a correction pass over any other doc statement
the census shows to be old-behavior-derived.

### Part 3 — fix L2 native residue (clone matches the fixed master)

The hand-derived carry residue in native translations bakes in the old
behavior and WILL diverge at the first carry-comparing rendezvous
after the helper fix:

- Audit Project1 (O.SEARCH cluster) and Project2 (?LIB_ERROR)
  DERIVATION.md carry reasoning end-to-end (WideCarry.md names the
  known spots: "WSUB x,x → c=0" leaned on repeatedly; "WADC sets c=1"
  at Project2 DERIVATION ~line 310).
- Update runtime/lib_error.cpp's three staged carries (~lines
  172/331/350) and any implied exit residue in either cluster.
- Sweep ALL other native runtime/ translations for carry staging or
  carry-dependent reasoning, not just the two named clusters — the
  audit scope is every file under runtime/, with the derivation docs
  as the map.

**Parts 2 and 3 land ATOMICALLY — one tranche, never split across a
battery.** A tree with the helper fixed but residue unre-derived is
known-divergent by construction; it must never be a committed state.

### Battery — on the server, strict shape

Local gates first per METHOD §15 (minimal sufficient, ≤~30 min:
K=1 legs that provably execute the exception-site blocks and the
lib_error staged-carry paths — FAIL_OPEN/injection triggers are the
fast route into ?LIB_ERROR). Then the runner-box battery via the repo
tasks/ path: base it on the hardened strict-gate template parked in
**tasks/hold/031** (single runner loop — task 029's two-concurrent-
loops lesson; pairs floors + pinned endpoints), legs covering book IR,
stock IR, and all-emulated, plus the natural triggers. Landing bar: 0
divergences everywhere, exception-site blocks demonstrably executed
(coverage evidence in the report), and — since master==clone by
construction even when both are wrong — an explicit non-lockstep
argument in the report for why the fixed behavior is RIGHT (the
manual citations + the census), per METHOD §2.

## Boundaries — BINDING

1. **Scope = carry correctness, nothing else.** No t-places, no
   `save`, no B-form extraction, no @/bit-15 regeneration, no grammar
   changes. Findings that tempt scope go in the report. (t-places
   move to P25.)
2. **Part 1 before any code; plan gate with census in hand; WADC is
   a user ruling, not a session decision.**
3. **Parts 2+3 atomic** (above). A red run is STOP-and-report, never
   a solo iteration loop.
4. **Design-vs-reality: STOP AND REPORT** — in particular, any carry
   reader the census cannot classify, any exception site whose
   behavior change IS game-observable, or any divergence the residue
   audit cannot explain. Do not tune around evidence.
5. **Implementation bugs: fix and record** (METHOD §11).
6. Deliverables: CarryCensus.md, the landed fix + residue
   re-derivations, updated doc corrections, REPORT.md in house form
   (worklog + final), CURRENT_STATE/NextSession updates or an
   explicit handoff note that the integrator will do them.
