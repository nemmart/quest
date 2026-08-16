> NOTE (Aug 15 2026): before executing this program, read docs/M5Notes.md —
> the exceptional-edge treatment here has a known context problem and
> candidate alternatives (raise census + fatalize; runtime as intrinsic
> summaries; invoke-style edges; static handler dispatch via M4 areas).
> Not yet ruled; M5 planning starts from those notes.

# The Flat-Graph Program — post-M3b direction (user + reviewer, Aug 14 2026)

The plan of record for M4/M5 and the reconstruction's second half.
Status: DIRECTION (ratified in discussion); project specs to follow.

## The premise (why Quest permits this)

No recursion, non-re-entrant routines (M4's stated premise — to be
MACHINE-VERIFIED, see analysis 1). Therefore: every frame slot is a
global ("ROUTINE.slot_3"), every call is a jump that first stores a
constant into the callee's (single, global) return slot, every WRTN
is an indirect jump with a statically enumerable target set ("if
WRTN can go back to five call sites, it's just a jump with 5
targets" — user). One flat CFG over one flat store: ONE GIANT
ROUTINE THAT PLAYS THE GAME. Classical whole-program dataflow
(FORTRAN-era, revived by the program's 1960s shape) runs over it in
seconds at this scale (~10K blocks, 83+ symbols in quest.blocks).

## The IR: quest.blocks, CLOSED

quest.blocks is already an interprocedural CFG (basic blocks; `n`
branch successors; `c` call edges, 1749). It is INCOMPLETE in three
edge classes, and the completion data is this project's existing
artifacts:

1. **Runtime call summaries** — L2Contract per-entry control
   behavior: returns, pc+k continuations (I.PROLOG pc+7 past its
   literal words — the naive `n` successor points into DATA; O.ON
   pc+3), transfers, NO-RETURN terminals (?FATAL, I.STOP, 0310
   sites).
2. **Exceptional edges** — ON_ERROR_CATALOG (26 registration sites +
   body addresses) + the raise census: registration→body edges,
   raise→handler edges, I.GOTO landing edges, DEF?ON resume edges
   (4-arg raises), terminal edges, escalation (fresh-chain) edges,
   ABORT-INTENDED edges to the terminal node.
3. **Indirect targets** — the census's enumerations (B+0x1E entry,
   vectors, XJMP tables) + the ONE annotated self-writing-code site:
   the boot-installed jumps at 0x1B8/0x1BB → static edges to I.INIT
   / I.SFCON (derivation in Layering ruling 6).

## The handler system DISSOLVES into the graph

The condition system's only dynamic ingredient is the chain, whose
contents depend on the call path — but with no recursion, possible
call paths per point are finite and enumerable, so POSSIBLE CHAIN
CONTENTS PER RAISE SITE ARE A STATIC QUANTITY (a gen/kill dataflow:
registrations gen, reverts kill). Then: raise = jump to the catching
body; unwind = jump to the landing; unhandled = edge to terminal.
Path-ambiguous raise sites (different catching handlers on different
paths): first try return-slot constant propagation (poor-man's
context sensitivity, free from the representation); the O(1)
fallback is ONE global current-handler variable written at
establishment sites — a chain of length one, itself just a variable
the analysis tracks. Expectation: most sites resolve to a singleton
statically (26 handlers, per-command establishment).

**And the addressing insight (user): no wfp, no token.** The
establisher token exists only to FIND the frame (restore wfp, cut
wsp); fixed global layouts mean handler bodies and landing sites
address their globals ABSOLUTELY — context baked into the code at
the jump target. I.GOTO's whole contract reduces to: pop (update
current-handler), jump. Dispatch staging, ac1, door state — all
compile away. The condition RUNTIME was an artifact of relative
addressing; absolute addressing leaves only its DECISIONS, and
decisions are edges. (The native L2 remains the master's fidelity
machinery and the M3b clone's runtime — the FLAT PROGRAM is what
needs none of it.)

## The analyses (one representation, the whole second half)

1. **Re-entrancy census** = call-graph cycle check — M4's foundation
   VERIFIED by machine, not assumed.
2. **Reaching definitions / slot lifetimes** — drives the
   slot→static rewrite (M4 executes a machine-checked plan instead
   of exploring).
3. **Read-before-write census** — cross-checked EMPIRICALLY by the
   probe specimen catalogue (Project 11+): static and dynamic
   instruments corroborating, per project tradition. Feeds M4's
   zero-init prologue design (matches the zeroed master, ruling 8).
4. **Global liveness → slot packing by graph coloring** = M5
   delivered whole.
5. **Chain-content dataflow** (above) = the handler lowering.
6. Free corroboration of every DEAD verdict (I?LINEID gate, the six
   untranslated entries) from an independent method.
7. Infeasible-return phantom paths cost PRECISION never SOUNDNESS
   for these may-analyses; refine with return-slot constprop only
   where packing precision pays.

## The expressions insight (the last stage: analysis → SOURCE)

Most register traffic is TEMPORARIES (~90%, user estimate; the
4-accumulator machine + the '88 codegen's template shapes force it —
values born, combined, dead within a handful of instructions).
Liveness on the flat graph partitions register uses:
single-def-single-use temporaries vs the small residue of real
carriers. The temporary class IS expression structure, recovered
mechanically: chase use→def, substitute — load/load/op/store
collapses to `Z = X + Y`. Registers were the compiler's pencil marks
for walking expression trees; dataflow reads the trees back out. The
VARIABLES (things deserving names) are the frame slots and statics —
exactly where the M5 analysis already lives.

The full recovery pipeline, each stage feeding the next: flat graph
→ liveness splits temporaries/carriers → temporaries fold to
expressions → blocks become assignment sequences over named slots →
condition system already edges → structural analysis (loops/ifs,
classical) → PL/1-SHAPED SOURCE: assignments, calls, ON-units,
gotos where 1988 really had gotos. The resistant ~10% (carry idioms,
FRH/FEXP float surgery, division helpers) sits largely in leaf
routines ALREADY hand-translated — the residue has names and C++
bodies waiting.

The honest hard part: recovered source is only PROVABLE if the
translation back preserves evaluation order and width semantics
(16-bit narrows, sign-extension quirks, SNC/SZC skips) — which is
where lockstep earns its keep once more: every recovered routine
gets the L2 treatment. DECOMPILATION AS A VERIFIED TRANSFORMATION —
the oracle referees the whole ladder, source included.

**Status note (user, Aug 14): "I'm sure we'll hit some serious snags
on the way. But this is the vision at least."** This document is the
vision, not a promise; snags get rulings when they arrive, per
project custom.

## Sequencing

Finish M3b (P11 probe instrument; P10 step 2 native-alone = Stage C
green) → **"Close the Graph"  project: quest.blocks + summary/exceptional
edge synthesis + validation against the census** → the analyses →
M4 rewrite per plan → handler lowering → M5 coloring. The oracle
regime persists throughout (M4 premise in Plan.md: de-stackify the
storage, keep the accounting).

## M4 entry design (user headspace, Aug 15 — record for the M4 kickoff)

**The WSAVS hijack**: change the machinery, not the code. Hijacked
WSAVS copies args off the stack into the routine's per-function
global area (LCALL word carries the arg count), sets wfp/ac3 to the
area's CONSTANT base (LDAFP 3 then re-loads it after every call —
the convention works for us), optionally zeroes the area (= the M4
zero-init prologue, same site). Per-routine live flag set/cleared at
WSAVS/WRTN = a dynamic re-entrancy tripwire on every call. Game code
untouched; M4 becomes an emulator transformation, lockstep-verified.

**The checker consequence**: code unchanged ⇒ pc streams and
instruction counts stay IDENTICAL on both engines; only
frame-address VALUES diverge (wfp/ac3, escaped XLEF pointers,
pointer fields in packets). Therefore the M4 checker is NOT
syscall-output-only — it is the ALREADY-PROVEN probe-class regime:
pc armed, counts armed, mediation armed, register values relaxed
(the -zero=clone expeditions are the live precedent — fork-exact
localization retained). Spend the extra effort in ONE place:
POINTER NORMALIZATION at the mediation compare — the hijack's live
frame table translates clone global-addresses back to
master-equivalent stack addresses at compare time, keeping the
world surface strictly checked. Spectrum recorded: full shadow-MMU
(max compare, most machinery) > probe-class + normalized mediation
(RECOMMENDED) > world-surface-only (the floor; avoid).

**The linkage insight (user, closing the design)**: only WSAVS is
hijacked. The five-word return block still pushes on the real stack —
now a pure CALL-LINKAGE stack (ten words per depth, no locals/args) —
and 'previous wfp' is just a value the convention round-trips: WRTN
pops it and restores the CALLER's global-area constant without
knowing anything changed; LDAFP 3 refreshes ac3 stock. Args copy to
a MIRRORED layout below the global base so negative ac3
displacements work unchanged. WRTN/LDAFP/the whole return convention
run unmodified — 'wfp gets restored to the global area, just like it
would on a real stack.' wsp values diverge from the master (in the
probe-class relaxation); instruction counts stay identical.

**The opt-in table (user, the keystone refinement)**: WSAVS consults
a PC-KEYED MAPPING TABLE — in the table: global layout; absent:
stacked exactly as today. This makes 4a the M3 campaign shape
reborn for storage: routine-by-routine migration under lockstep,
with the stacked path as the per-routine attic. Mixed mode composes
with NO extra machinery: hijacked callees copy args off the stack
regardless of caller mode; stacked callees dereference
global-address arg pointers fine (memory is memory); previous-wfp
round-trips constants and stack addresses alike, so linkage records
and full frames interleave coherently. Buys: bisection debugging
(halve the table → the guilty migration names itself); stragglers
may stay stacked indefinitely (frozen-L2 precedent); and the table
IS the new world's address book — routine → area base/size/argc,
the storage symbol table the analyses and named source need anyway.

**4a prerequisite — the address-space census (user, the last design
piece; CORRECTED)**: the global areas MUST live in emulated address
space (compiled loads/stores dereference them; pointers escape into
packets). Sizing is small (non-reentrancy ⇒ one area per routine ⇒
sum of frame sizes, a few thousand words). ADDRESS-SPACE MODEL,
CORRECTED BY THE USER: in a 32-bit MV word address the HIGH BIT IS
THE INDIRECTION FLAG (the @ mechanism — the DG manual's 'bits 1–31
= AC3' is exactly this); pointers are 31-bit. The reviewer's
'byte address = word address ×2 aliasing' reading of specimen F3c
was WRONG — E0002974 is an indirection-flagged VALUE, not a second
view of memory. Bits 1–3 are the RING NUMBER (the MV protection
architecture; user, second correction) — 0x7xxxxxxx = ring 7, the
user program's world; 0x3xxxxxxx = ring 3, the OS, reached only via
gate calls (?G.SYSCA at 0x30000000). Therefore the global areas MUST
be ring-7 addresses (0x7xxxxxxx), judged as unused within ring 7's
28-bit space — a smaller, well-bounded search (image, heap, stack
extents all live there). A global-area pointer that ever needs @
semantics carries the high flag as usual — no conflict, but no
shortcuts: candidates are 0x7-prefixed, 31-bit, indirection-flag
clear. Finding method, two
instruments: DYNAMIC — a page-map census (log every page Memory ever
creates across the trigger battery + long play; the complement is
untouched-in-practice); STATIC — preamble extents + a disassembly
sweep for absolute addresses outside them (the flat-graph
indirect-target enumeration covers computed ones). Where both agree
never-touched, the address book gets its base; the hijack maps pages
lazily so only migrated routines consume any.

**Ring 6 as the address book (user, Aug 15) — likely the answer, one
characterization pending.** Since ring-7 code can never form a
ring-6 address on the real machine, 0x6xxxxxxx is EMPTY BY
ARCHITECTURE — the census evaporates. Emulator check (reviewer):
Memory keys pages purely by page number, no ring/permission model
anywhere — a page at 0x6xxxxxxx reads/writes like any other. BUT
Machine::copy_segment (a & 0x0FFFFFFF | pc-ring) is the faithful
intra-ring addressing mechanism and may RE-RING a ring-6 base back to
ring 7 on some effective-address paths (it already stars in the P10
zeroing note). 4a STEP ZERO: characterize which EA paths apply
copy_segment and whether a full-32-bit register base ([ac3+d] with
ac3=0x6...) survives them. If register bases pass untouched and only
pc-relative forms re-ring → ring 6 works cleanly. If register-based
accesses re-ring too → either a tiny hijack-only emulator adjustment
or fall back to the ring-7 census (method recorded above). Do not
allocate on a hunch; one afternoon of reading EagleMemory settles it.

**The page-table readout (user, the guaranteed option)**: the OS
side already keeps the authoritative page table (Memory::map_page,
keyed by page number; 1K words/page). Dump the mapped-page set after
a LONG session with the trigger battery folded in (pages appear over
time — heap growth, ?CREATE_TASK stacks — so log cumulatively), take
the complement inside ring 7, and pick ~32 unregistered pages: the
address book's home by FACT, not by proving a negative. This makes
ring 6 the ELEGANT option and ring-7 blank pages the GUARANTEED one;
4a step zero chooses between two known-good answers after the
copy_segment characterization. Lazy mapping in the hijack keeps a
safety margin cheap.

**Page-set dynamics (user hunch, reviewer-verified, then user-
corrected)**: THREE mechanisms map pages. (1) Load-time image pages,
fixed. (2) ?MEMI (0116, extend memory) — ONE site in the whole
program (0x7017E134, the RT's heap-arena growth), monotonic upward.
(3) The SHARED-MEMORY SYSTEM — ?SOPEN/?SPAGE/?SCLOSE (OSContextShared)
map data-file pages into the process at addresses THE GAME NAMES in
the syscall packet (memory_page_number), and unmap on close:
genuinely dynamic AND game-controlled placement, so a naive
heap-high-water census could pick a hole a later ?SPAGE window is
meant to occupy. Therefore the ring-7 readout must be the CUMULATIVE
union over a long session (all three mechanisms) before taking the
complement, plus heap headroom — provable by observation, with a
residual. This tips the balance toward RING 6: architecturally empty
regardless of where the game parks its windows, immune to all three
dynamics — IF copy_segment cooperates. That characterization is now
THE deciding fact of 4a step zero.