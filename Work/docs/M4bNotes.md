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
  (RETURN_MESSAGE at 70169B82). NOT arg pushes: the idiom is
  `WPSH r,r / LDASP r` repeated to materialize temporaries on the stack
  and load their ADDRESSES into ACs, then a final `WPSH 0,2` pushes
  those three addresses as the actual args. Census correctly maps only
  the final WPSH (arg1-3 at 70169B81) and excludes the temp-building
  WPSHes. Verified: 0 WPSH miscategorized.

**Flag for M4b (pass-by-reference):** some game→game args are ADDRESSES
of caller stack temporaries (built via WPSH+LDASP). Redirecting the arg
PUSH to the callee area is correct and sufficient — but the pushed value
is a pointer into the real stack, and what it points AT is not
relocated. The callee dereferences the pointer; the mapper handles the
stack address. No special handling needed, but the M4b implementer
should know an arg can be a live stack-temp pointer, not just a value.
