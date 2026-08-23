# M4b Notes — the push_map / decorated-call protocol (discussion record)

*Status: DISCUSSION RECORD, not a design of record. Original
conversation forked immediately after Project 12; writeup received and
reconciled by the session of record on Aug 16 2026. The verbatim
writeup follows the reconciliation.*

## Reconciliation with the post-P12 record (P13/P14 landed since)

1. **Vocabulary drift** (mechanics unaffected): hijack -> redirect
   (ruled); T/T_any/T_inv -> the Mapper module, hw/Mapper.{hpp,cpp}
   (P14 deleted the accretion); LiveArea -> LiveRecord; base is
   0x74000000, not 0x78000000 (P13 ruling — prefix separability, I3).
2. **The writeup's open Q1 is ANSWERED** by the session-of-record M5
   discussion (docs/M5Notes.md): the driver is STATICALLY-ADDRESSED
   CALLER WRITES for static analysis ("callers should be writing the
   routine area and then doing the lcall" — user ruling in discussion).
   Copy/pop removal is the side effect. Consequence: game->game sites
   are the target; RT arg pushes are out of scope (the RT becomes
   intrinsic summaries under the M5 direction).
3. **Its issue 1 (open-window shadow accounting) is Mapper.md §3b made
   concrete**: the closed-form shadow_wsp is wave-one-scoped and dies
   here; windows become a new record kind in the SAME Mapper (A gains
   window intervals; W ties become legal; record-list order replaces
   address order as the LIFO authority; parent-end/child-base seam
   fan-ins take the Q2 overlay treatment). Answer to its Q4: this is
   **Checker Generation 5** — the accounting model changes.
4. **Its Q5 (compare/serialize the flag?) — no, by architecture**: the
   redirect is clone-only, so decorated calls, the push_map, and the
   written-vs-pushed flag are clone-side machinery like the Mapper.
   The master pushes stock. Mid-window register pairs are the Mapper's
   job (open-window records), exactly as the writeup proposes.
5. **Consume-and-clear at every WSAVS, set+non-redirect -> abort** is
   house style (the QUEST_INJECT fail-loud lesson generalized) — to be
   ratified when M4b is planned for real, along with the riding
   proposals (window tripwires, clear-before-overflow ordering, abort
   at WRTN/WPOPB/mediation on a set flag, dataflow-unambiguous-only
   conversion).

## Census answer (Project 15, Aug 16 2026)

Open question 3 is ANSWERED: **the "never" bucket is empty.** All 754
game-target call sites classify CLEAN (566) or CLEAN-EMPTY (188);
zero PROBLEMATIC, zero inner-call windows. quest.argmap (1352 lines)
is the push_map's direct ancestor and covers every site. Two findings
that reshape M4b: (1) the compiler evaluates nested-call arguments
into temporaries BEFORE the push run — windows are straight push runs
with only stack-neutral compute inside, so open-window accounting
never has to survive a mid-window call in practice (tripwires kept as
fail-loud backstops); (2) the property is program-wide (all 1749
sites classify CLEAN with RT targets included), relevant if the goal
question ever pulls RT pushes into scope. Mixed arity confirmed
per-site convertible. See docs/Project15/REPORT.md.

**Sequencing**: M4b planning starts after M4a lands completely (P14
Phase B roll-call + the user's live play session). First-project
granularity (one SITE, then stop — by analogy to P12's one routine) is
the open question its Stage 0 should settle.

---

# M4b — Design Discussion (User + Claude, Aug 15 2026) [verbatim writeup]

*Context for the receiving session: this is a faithful writeup of a design
conversation held immediately after Project 12 landed (M4a: READ_IN's
WSAVS frame in a fixed area on the clone, 0 divergences, Checker
Generation 4 — see docs/Project12/REPORT.md). It is a DISCUSSION record,
not a design of record: nothing here has been ruled into M4aDesign.md,
and no code has been written. Format follows the actual exchange;
positions are attributed. Vocabulary is Project-12's: "book" =
quest.addrbook, "hijack" = the WSAVS area-frame path, "W" = the clone's
real wsp at the hijacked WSAVS, "window"/"open-window record" defined
below.*

---

**User:** You know how we have a bunch of PUSH instructions to get the
args onto the stack before an LCALL/XCALL? [...] What I'm thinking is we
have a "push_map", which is a map based on PC, which points to a store
address. When we're about to run a push instruction, if the pc is in the
map, instead we store it at the map target.

**Claude:** So M4b: for calls targeting a book routine, the arg pushes
(and the call's frame word) write directly into the callee's fixed area
arg slots — the caller's wsp never moves, WSAVS does no copy, WRTN pops
nothing. Under M4a the caller still pushes args on the real stack, WSAVS
copies them into the area, the WRTN fixup pops them; the push/copy/pop is
pure overhead, and args are the last reason the *caller's* wsp has to
move at all for a migrated call.

The mechanism is clean — pc-keyed, checked in the push path only,
tool-generated alongside the book. Design issues visible from here:

1. *Arbitrary code runs between pushes.* An arg can be computed by a
   function call — syscalls, L2 crossings, other book routines — so the
   arg-build window isn't atomic. (a) The checker needs a record from
   the FIRST redirected push: the master's wsp climbs through the window
   while the clone's doesn't, so shadow_wsp/T must carry the partial
   push count dynamically, not just per live frame. (b) Compare pairs
   WILL land mid-window and must pass.
2. *Arg-build re-entrancy of the same callee.* One arg-slot set per
   routine: if a call to X computes one of its args by (transitively)
   calling X — DIST(DIST(...)) shapes — the inner call clobbers the
   outer call's already-landed args. This is a new, EARLIER window than
   the execution re-entrancy M4a already trips on. The tool catches
   direct cases; through-call chains need a runtime tripwire: opening an
   arg window for X while X has one open → abort.
3. *The map needs static dataflow.* Per site: which pc feeds which arg
   index, multi-wide pushes (WPSH 0,2 = three wides), branches inside
   the window, and proof each push pc feeds exactly one call site.
   tools/dataflow.py may be a starting point.
4. *LCALL/XCALL need entries too.* The frame word (psr<<16|argc) is
   pushed by the call instruction — that write redirects to wfp−10, wsp
   doesn't move, WSAVS stops reading [wsp] and stops copying, and the
   WRTN fixup becomes wsp = W (unchanged) instead of W−2−2argc. XCALL
   sites are fine — the static link travels in a register.

**User:** We don't have to do this for every call. We can convert one
function at a time. The LCALL also needs to be decorated with this map
address, and WSAVS needs to know if the client pushed or wrote.

**Claude:** Per-site conversion makes the arrival-mode question fall out
naturally. Three coordinated pieces, all tool-generated per call site:
push_map[pc] → area address; a decoration on the call instruction; and a
WSAVS that knows which caller style it's receiving. Both caller styles
coexist against one routine — decorated sites write, undecorated sites
push and get the M4a copy. Consequences: LiveArea grows a mode bit; the
WRTN fixup in write mode is wsp = W; shadow accounting starts at the
first decorated push (open-window record: pushes landed so far, added
into shadow_wsp and T); tripwires for window-vs-window conflicts; and
the tool only converts sites where the whole push-run is unconditional
or all paths agree — anything ambiguous stays M4a.

Two open questions flagged: (1) signals raised MID-WINDOW — the master's
partial pushes unwind for free, the clone has stale words in the area
and an open record to discard; the I.GOTO cut plausibly handles it if
windows are dropped like live frames, but it needs the same audit
treatment as Project 12's four walks. (2) What's driving M4b — just
removing the copy/pop, or making the caller-side writes statically
addressed, which the copy approach never gives you?
*(Neither was answered this conversation — first items for the next
session's Stage 0.)*

**User:** Actually, maybe LCALL could just set some processor state to
tell the WSAVS if the args have been written or pushed.

**Claude:** Sound, because the gap between flag-set and flag-consume is
exactly one instruction boundary — LCALL transfers directly to the
entry, WSAVS is the entry's first instruction, nothing else from this
task runs in between. A per-machine bit ("args written, not pushed")
that the decorated call sets and WSAVS consumes; no identity payload
needed for correctness. Hygiene: a stale flag (decorated call whose
target isn't a hijack WSAVS) would dangle and poison the next book
call — hardening proposed at that point was an expected-pc payload plus
tool discipline (only decorate calls whose target's first instruction is
a book WSAVS/WSAVR).

**User:** Maybe every WSAVS clears the bit. If the bit is set and WSAVS
wasn't a hijack we blow up.

**Claude:** Simpler and strictly stronger — consume-and-clear at every
WSAVS/WSAVR (and WSSVS/WSSVR, same save path) collapses the payload into
three unconditional rules:
- bit set + book hijack WSAVS → write mode, clear;
- bit set + non-hijack WSAVS → abort_world (a decorated call landed
  somewhere the tool didn't promise: map corruption, self-modified
  target, or a tool bug);
- bit clear + hijack WSAVS → M4a copy mode (undecorated caller, legal).

The dangle dies structurally: no path from a decorated LCALL to any
later frame-building instruction that doesn't pass through the very next
WSAVS. One exception to route: handle_overflow fires AT that WSAVS — so
clear the bit first thing in the WSAVS case, before the overflow test,
one clear point. For symmetry, also abort if the bit is set at any other
wsp-consuming boundary a call can't legally reach (WRTN, WPOPB, mediated
syscall arrival) — unreachable if the tool is right, which is exactly
why they should abort if reached; cheap since the bit is almost always
clear. *(These extra abort points are Claude's proposal, not yet ruled.)*

The flag was the easy part — now the clean easy part. The hard parts of
M4b are unchanged: push_map dataflow, open-window shadow accounting,
arg-build re-entrancy, mid-window signals.

**User:** So then I think the only thing on the stack is the dynamic
stack allocation, and some register store/restore calls. Does that sound
right?

**Claude:** Close; the residue list for a fully-converted world is:
1. Dyn routines (WMSP/STASP/LDASP — 16 segments in the census) and
   push/pop routines (WPSH/WPOP — 19 flagged), excluded from the book,
   fully stacked; plus QUEST's main frame itself (push-flagged).
2. The entire RT/L2 range: game→runtime calls push args undecorated, and
   RT frames (RTBridge emulated frames, native-leaf residue) are
   real-stack. The book scopes game routines only.
3. L2 machinery even for converted routines: I.PROLOG's head slot goes
   on the REAL stack (wsp += 4) even when the frame is in an area —
   Project 12 audit fact; O?SIGNAL's dispatch frame E; DERR's two wides;
   XPSHJ/LPSHJ wides.
4. Mid-window arg pushes for undecorated sites — mixed-argc or
   ambiguous-dataflow sites left on M4a.

**User:** I'm hoping 4 is "not yet converted" vs. "never going to be
converted."

**Claude:** Splitting 4: mixed argc IS convertible — arity is a
caller-side property and conversion is per-site, so a 2-arg site and a
3-arg site to the same routine get different push_map entries and
different frame words to the same wfp−10; the callee reads argc from the
frame word as always. The book's `mixed` flag was M4a conservatism, not
an obstacle. The honestly-hard residual is sites where dataflow doesn't
resolve: a push pc feeding different call targets on different
executions, or paths disagreeing about push count or slot assignment.
Whether those exist in QUEST is an empirical question for the tool —
1988 compiler output from structured source, so the prior is push-runs
are compact, unconditional, site-local, and the answer is few or zero.
Conditional ARGUMENTS (F(x ? a : b)) compile to a computed value then
one push — fine; it would take a conditionally-pushed argument COUNT to
defeat the analysis, which the language probably can't express except
through the mixed-arity pattern already handled.

So: with per-site conversion, plausibly every game→game call site
converts and category 4 is "not yet" with an empty "never" bucket —
pending the dataflow census. Category 1 is also "not yet" if a future
M4c gives dyn routines fixed areas for their dynamic allocations.
Categories 2 and 3 are the structural "never" — or at least "not by this
mechanism."

---

## State of play for the receiving session

**Agreed in discussion (informal — not yet rulings):** push_map keyed by
pc redirecting pushes to area arg slots; per-site conversion with both
caller styles coexisting; a per-machine written-vs-pushed flag set by
decorated calls; every WSAVS consume-and-clears it, set+non-hijack →
abort_world.

**Claude's proposals riding along (explicitly not ruled):** open-window
shadow accounting from the first decorated push; window-conflict and
same-callee arg-build re-entrancy tripwires; clear-before-overflow-test
ordering; abort on flag-set at WRTN/WPOPB/mediation; write-mode WRTN
fixup wsp = W; tool converts only dataflow-unambiguous sites.

**Open questions, in rough priority:** (1) the M4b goal question —
copy/pop removal vs statically-addressed caller writes (drives how hard
to chase RT arg pushes and category 1); (2) mid-window signal unwind —
needs a Project-12-style audit of the walks/cut against open windows;
(3) the dataflow census — does the "never" bucket exist; (4) checker
generation question — is open-window accounting a Gen 4 extension or
Gen 5; (5) does the flag itself need to be compared/serialized at pairs
and mediation, or is it derivable.

**Standing constraints from METHOD.md that shaped this conversation and
bind the next:** boundary rulings before code; the design doc is not
edited by working sessions; corrections to design facts are recorded,
not silently fixed; one routine (here: one SITE?) then stop —
granularity for M4b's first project is itself an open question.

---

## WPSH arg-push sites (Aug 22 2026 analysis)

WPSH is NOT primarily a push/pop-pair op — its dominant role is
argument marshalling (one instruction pushes a contiguous register
run). In QUEST: 59 WPSH total, only 23 WPOP → most WPSH are drained by
a call's WRTN (args) or a frame pop, not a matching WPOP.

**No wrapping cases**: every WPSH/WPOP has XX<=AA (verified over
quest.dis), so width = AA-XX+1, a contiguous ascending run. The
emulator's mod-4 wrap path is dead code for this program.

**25 WPSH instructions push game→game call args** (in quest.argmap;
the only multi-slot-per-pc case). They feed three game routines:
- TERRAIN     — `WPSH 0,2` → 3 slots (~13 sites)
- TERRITORY   — `WPSH 0,1` → 2 slots (~8 sites)
- RETURN_MESSAGE — `WPSH 0,1`/`0,2` → 2–3 slots (~5 sites)
IMPORTANT (corrected): TERRAIN (0x7017C877), TERRITORY (0x7017CD71),
UPDATE_SCREENS (0x7017D635), RETURN_MESSAGE (0x70176FDD) all sit in the
0x7017xxxx range but ARE game routines — in the book, migrated, in the
census. The game/RT split is the BOOK, not a raw address cutoff.
EXACT boundary: last game routine = WRITE_OBJECT (0x7017D859); first
runtime routine = ?CHAR_TO_UNSIGNED (0x7017D99B); from there up all
`?`/`MT?` system routines (?UNSIGNED_TO_CHAR, ?UMUL32, ?UDIV32,
?OPEN_FILE, ?READ, ?CREATE_TASK, ... and further up ?WRITE_SCREEN
0x7017E27A, B.MOVE 0x7017E5CB, C.TRANS 0x7017E64A). So game < 0x7017D99B,
runtime >= 0x7017D99B, marked cleanly by the `?` naming. (?CHAR_TO_UNSIGNED
being first RT is apt — its failure, bad numeric input, is the only
organically-observed signal source; see M5Notes.) Only WPSH feeding those 0x7017Exxx RT targets is
out of scope (game→RT stays stock); WPSH feeding TERRAIN/TERRITORY/
UPDATE_SCREENS/RETURN_MESSAGE is IN scope and correctly censused.

**M4b implementation consequence:** the push_map must handle a WPSH pc
mapping to MULTIPLE area slots (write AC0..AC[k] to slots base..base+k
in one shot), not the one-pc→one-slot model that XPEF/LPEF sites use.
quest.argmap already encodes this as repeated `<routine> argN at <same
pc>` lines — the push_map builder reads slot count per pc from there.
All 25 are clean contiguous runs (0,1 or 0,2), so no cycle walk needed.

---

## WPSH/WPOP population fully characterized (Aug 22 2026)

The 59 WPSH / 23 WPOP split into TWO disjoint, fully-understood uses:

**(1) 25 WPSH = argument marshalling** (game→game calls; in quest.argmap;
drained by the callee's WRTN, never by a WPOP). Multi-slot per pc:
TERRAIN `WPSH 0,2` (3), TERRITORY `WPSH 0,1` (2), RETURN_MESSAGE 2–3.
These are M4b's concern (push_map, pc→multiple slots).

**(2) 23 WPSH = local frame-pointer-borrow save/restore brackets**
(these are the ones that DO pair with the 23 WPOPs). Every one is the
identical 3-instruction idiom, verified across all 23:
```
WPSH 3,3          ; save AC3
LDAFP 3           ; load frame pointer into AC3
X{W,N}STA/XSTB … [ac3+off]   ; store one register into a frame slot
WPOP 3,3          ; restore AC3
```
23/23 contain LDAFP, 0/23 contain a call, span is EXACTLY 3
instructions, only variation is the store opcode (XWSTA ×19, XNSTA ×2,
XSTB ×2), operands 3,3 (once 2,2). The compiler emits this when it must
write a frame slot but AC3 is live. NOT block-move/WCMV related (those
don't clobber AC3 this way); NOT around calls.

**Remaining 11 WPSH** (59−25−23) are the single-register arg pushes
already counted in (1)'s pc set / other single-slot arg sites.

**Consequence:** WPSH/WPOP are NOT an open problem. Population (1) is
M4b (censused). Population (2) is a trivial, self-contained, stack-
symmetric local idiom — both engines execute it identically on the real
stack, the mapper's compression leg already handles the transient
AC3-save word, and it spans 3 instructions with no call/alloc — so it
needs NO special M4b/M4c handling. If M4c ever relocates in-body stack
residue, these brackets are the easiest possible case (fixed shape,
local pairing, no wrap).

---

## All 59 WPSH accounted for (Aug 22 2026) — complete partition

- **25** — game→game arg marshalling (in quest.argmap; push_map handles
  pc→multiple slots).
- **23** — frame-pointer-borrow brackets (WPSH 3,3/LDAFP/store/WPOP 3,3;
  local, trivial, no handling needed).
- **8** — game→RUNTIME arg pushes (?OPEN_SHARED_IO_FILE ×3, B.MOVE ×2,
  C.TRANS ×3). Out of scope: game→RT stays stock (RT → M5 intrinsics).
- **3** — stack-temporary construction for a PASS-BY-REFERENCE call
  (RETURN_MESSAGE,3 at 70169B82). NOT arg pushes. Full window traced:
  `WPSH 0,0; LDASP 0` (build temp, addr→AC0), `WPSH 1,1; LDASP 1`,
  `WPSH 2,2; LDASP 2` — three temporaries pushed, each addr loaded —
  then `WPSH 0,2` pushes AC0/AC1/AC2 (the three addresses) as the args.
  Stack depth at the call = **6 wides**: 3 arg-pointers on top of the 3
  temporaries they reference. Census correctly maps ONLY the final WPSH
  (arg1-3 at 70169B81), call classified CLEAN, temp-building excluded.
  Verified 0 WPSH miscategorized. (The temporaries are never popped
  because RETURN_MESSAGE is [[noreturn]] — see below — so there is no
  "after the call" cleanup; the whole stack is abandoned at process
  termination.)

**RETURN_MESSAGE is a NORMAL compiled PL/I game routine that happens to
be [[noreturn]].** It has a standard WSAVS 0x0003 frame and by-reference
args like any game routine (it IS in the book and census), but its body
ends in SYSCALL 0310 (process-terminate) with an inline message table
("A mighty blow has been struck..." — game death/combat messages) and
has NO WRTN anywhere. The C++ port is an unconditional
terminate_process(message). So "noreturn" is a behavior property, NOT
evidence it is runtime — a game routine whose job is to kill the process
with a message. Consequences: correctly censused (args redirect
normally); roll-call correctly lists it LIVE-UNEXERCISED-unless-fatal
(reaching it means the process dies); arg temporaries need no cleanup
(process death reclaims the stack). This is the general lesson that
in-body stack temporaries are reclaimed by FRAME TEARDOWN at WRTN (or by
process death for noreturn routines), never by per-call pops — so
redirecting arg pushes cannot leak stack.

**Flag for M4b (pass-by-reference) — sharpened:** some game→game args
are ADDRESSES of caller stack temporaries. At such a site the stack
holds MORE than argc wides (RETURN_MESSAGE,3 example: 6 wides = 3
pointer-args + 3 referenced temporaries). M4b redirects ONLY the
argc pointer-pushes into the callee area; the temporaries MUST remain
on the stack (they are what the pointers reference). So after redirect:
pointer-args live in the area, referents live on the real stack, callee
dereferences area-resident pointers to reach stack-resident temps — the
mapper handles the stack addresses. Correct and sufficient, but the
implementer must NOT try to also relocate the referent temporaries, and
must not be surprised that a CLEAN arg window leaves >argc wides on the
stack. The census already scopes the window to just the pointer-push.


---

## RULING (Aug 22 2026): P16 first-slice — mid-window pairs → stack_offset in LiveRecord

**P16 outcome:** the one-site mechanism (DIST,4 @ 70166E1C) is PROVEN
(84 clean write-mode calls, args to correct slots, marker
written-and-pushed, flag consumed at every WSAVS, copy mode coexisting,
ratified arithmetic exact). Stage-2 hit the anticipated mid-window
condition; the session made a clean Boundary-2 stop.

**Finding (verified):** the checker's 500-instruction quantum boundary
can stop inside the 6-instruction arg window. There the count-matched
master pushed k args (wsp +2k) while the clone WROTE them to the area
(wsp flat), so shadow is off by exactly 2k. Verified 70001FD6 vs
70001FD0 = 6 = 2k, k=3. Source-agnostic (a signal mid-window does the
same).

**RULING — a per-frame `stack_offset` carried IN the LiveRecord.** (Both
C1 open-window-record-kind and C2 quantum-alignment rejected — see end.)

Mechanism:
- **`stack_offset` is a new field in `LiveRecord`**, initialized 0 when
  WSAVS pushes the record. It is clone-side checker state (the master
  never redirects); never compared or serialized.
- **Decorated push** (XPEF/LPEF/XPEFB/LPEFB/WPSH at a mapped pc):
  `records_.back().stack_offset += 2 * words_written` (2 per word; 2k
  for a WPSH writing k words).
- **Decorated pop and decorated LCALL:** `records_.back().stack_offset
  -= 2 * words` (the LCALL subtracts its own 2*argc).
- **Checkpoint comparison:** `delta = records_.empty() ? 0 :
  records_.back().stack_offset;` then `clone.shadow_wsp + delta ==
  master.wsp`. Empty → delta 0 → exactly the current closed-form check;
  no sentinel record, no change to the ~11 existing `records_.empty()`
  stock-passthrough sites.

Record-push gate — book membership, NOT decoration (clarified). Every
WSAVS whose routine is in the book pushes a LiveRecord (copy mode OR
write mode — both call push_record; the args-written flag only selects
which). A non-book WSAVS pushes nothing. So `records_.back()` is the
currently-executing book-live frame's record. A decorated arg-push's
`stack_offset += ` therefore lands on the CALLER's record precisely when
the caller is book-live — which it always is for a real call site (the
load-time guard enforces it). This also means stack_offset is a new
field on records that ALREADY get pushed for all 101 (M4a discipline) —
not new record-push behavior, just a new field.

Why IN the record (this is the key insight, from the record stack the
mapper already keeps):
1. **Frame-scoped for free.** The arg window runs entirely while the
   CALLER's record is on top (pushes + LCALL all fire BEFORE the
   callee's WSAVS pushes its record — verified against EagleStack.cpp:
   LCALL sets the flag, WSAVS pushes the record). So the window's
   increments/decrement all land on the caller's record; the callee's
   WSAVS then starts a fresh record at offset 0. No save/restore logic.
2. **O.ON / I.GOTO unwind handled automatically.** stack_offset rides
   in the record, so `unwind_to`'s existing suffix-pop
   (`while records_.back().W > cut_W: pop`) discards the offsets of all
   cut frames and leaves the target frame's offset intact — the
   non-local unwind restores stack_offset for free. A mid-window signal
   that abandons a window drops that window's partial offset when its
   frame's record is popped. No new abandonment machinery.
3. **Only decorated ops move it.** MSP, the tombstone push, non-
   decorated pushes move both wsps equally and touch stack_offset NOT
   AT ALL — it measures divergence, not absolute wsp.
4. **No blind windows.** Every instruction boundary stays a valid
   compare point (unlike C2's forbidden regions and C1's special
   open-window identity mode). Full lockstep coverage kept.
5. **Generalizes to M4c** — the WPSH/WPOP frame-borrow pairings use the
   same field (decorated push up, decorated WPOP down).

Empty-records handling → **decorate QUEST as the base record** (primary).

QUEST boot is nocall/stock today, so `records_` starts empty. Rather
than guard the empty case, ELIMINATE it: **add QUEST to the book so it
migrates in ordinary M4a COPY MODE.** This needs NO special handling and
no new base-record logic — it works because the flag rules already route
it correctly:

QUEST is entered by the LOADER, not by a decorated LCALL. So the
"args written" flag is CLEAR when QUEST's WSAVS fires. The
consume-and-clear rule for (flag clear + book redirect) is already
"M4a copy mode" — so QUEST just takes the normal copy-mode migrated-frame
path: frame → area, stock stack behavior, exactly like the other 100
routines. It is NOT a write-mode/decorated-call frame; it is an ordinary
copy-mode routine that happens to be the OUTERMOST frame, so its record
is the base of the stack and `records_` is never empty. "Decorate QUEST"
= "add QUEST to the address book"; nothing more.

Supporting detail (why it's ordinary, not why it's special): QUEST's
WSAVS (0x0022 @ 7015c005) and WRTN (@ 7015c5e0) are structurally normal
(the `nocall` flag is about the caller side — the loader jumps in,
nothing LCALLs it — not the frame-save side), so:

- **WSAVS**: QUEST is the FIRST record, so it hits the existing
  `records_.empty()` first-record branch (latches the I2 diff, starts
  shift_after from 0) — that branch already exists for "I am the base
  record." QUEST is a COPY-MODE, argc-0 record (no preceding decorated
  LCALL, no args): the simplest possible record. `fwd(W)` on an empty
  chain is identity, so master_wfp = W + 10 — the degenerate base of the
  normal formula, no special case.
- **WRTN**: `wrtn_fixup` (Mapper.cpp:449) touches ONLY `records_.back()`
  (the returning frame's own record); its sole caller-record reference
  (records_[size()-2]) is trace-only and already `size() > 1`-guarded.
  new_wsp = W − 2 − 2·argc = W − 2 (argc 0) restores the boot wsp; pop →
  empty. Correct for the base frame with no new code.

Result: `records_` is never empty once boot runs; the empty case
vanishes with no special code — QUEST is just a copy-mode routine in the
book, and the flag-clear→copy-mode rule does the rest.

This FOLDS QUEST out of the M5 nocall set into M4b (a scope addition vs.
M4a, which excluded it). Two things to VALIDATE (not special-case):
  (1) QUEST is entered by the loader with boot-time ac3/wsp state, not
      caller-set-up state — confirm the WSAVS redirect's ac3→area-wfp
      assumption holds from boot (same slotpatch-style check the DIST
      report did).
  (2) QUEST's frame migrates to area 0x74000000 — the boot frame is now
      in the area. Lockstep either boots clean or is INSTANTLY red (it's
      the first frame — trivial to validate: any error diverges at
      instruction 1).

Fallback (only if boot-frame migration hits a wrinkle): the three-layer
guard — load-time (refuse to decorate a site whose containing routine is
not book-live), checkpoint READ (empty → 0), decorated WRITE (empty →
abort, fail-loud). In QUEST today no game→game arg-push site sits
directly in QUEST (its arg-push direct calls are all RT: ?OPEN_FILE,
?WRITE_SCREEN; or zero-arg: INIT_SHARED_DATA, READ_IN — verified), so
the guard would also be correct — but the base-record approach is
cleaner and preferred.

Deferred (NOT M4b): MSP interleaved inside a redirected window is a
stack-LAYOUT question, but NO arg window in QUEST contains an MSP/wsp-
mover (verified; the only in-window wsp motion program-wide is the
RETURN_MESSAGE ref-arg temp-build, separately handled). Expected to fold
into the same decorated-offset system when M4c handles it.

Rejected: C1 (open-window record KIND — heavier; a record/identity
restructuring where one field suffices) and C2 (quantum alignment —
partial: leaves mid-window signals fail-loud; bends batch segmentation).
Keep C2 only as a possible later PERF note, never a correctness
mechanism.

**Checker generation:** this is a comparison-term change (one record
field + one checkpoint line), NOT a mapper-identity change — no Gen-5
record restructuring. Note in CheckerHistory when it lands; the closed
form is recovered exactly when the offset is 0.

Next: add `stack_offset` to LiveRecord + the three hooks + the checkpoint
line, on the DIST site. Re-run task-018 (expect div=0 on all
site-reaching legs), report.
