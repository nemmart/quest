# Mapper.md — the master↔clone address mapper, from first principles

*Ruled Aug 15 2026 (user + Claude) after Project 13 batch 2. Status:
DESIGN OF RECORD for the mapping layer. Supersedes the T/T_any/T_inv
prose scattered across M4aDesign §5/§8/§9 and the Project 13 §6
implementation notes — those remain as history; THIS is the spec.
Project 14 implements it as one module and deletes the accretion.*

## 0. Why this document

The checker's correctness rests on one function: the map between the
clone's addresses and the master's. Project 12/13 grew it by patches —
ordered guessing, then prefix dispatch, then end-inclusive containment,
then an @-flag guard on the inverse path — each correct, none derived
from a stated principle. Four patches were four instances of four
unstated invariants. This document states them; the implementation
must be readable as their transcription.

## 1. The object

The mapper is **a piecewise-affine bijection A between word-address
spaces, composed with an encoding codec E**, exposed as ONE public
function V. Nothing else.

### 1.1 The address bijection A (word addresses, per instant)

Defined at any instant by the live-record list plus one derived leg:

- **Area legs.** Each live redirected frame contributes the closed
  interval map `[area_alloc, area_alloc + size] ↔
  [master_alloc, master_alloc + size]` (affine, offset-preserving).
  CLOSED on the right BY PRINCIPLE: a one-past-the-end pointer is a
  reference to that object's extent (WCMV cursors, C-style end
  pointers) and belongs to the object it walked off — not a patch for
  HIT_ANY_CHAR, a definition. **Precision (P14 Q2 ruling): A is a
  bijection on interval INTERIORS; each closed right end is a
  deliberate forward-only two-to-one overlay** (see the §1.3 direction
  rationale). The inverse resolves an overlay master address to the
  STACK-LEG preimage, because dereference targets data. **Finding A
  extension (ruled Aug 22 2026, Project14/FINDING_A_MAPPER_FIX.md +
  REPORT_FINDING_A_FIX.md): one more two-to-one point per live record
  at the band's OPPOSITE edge — master_wfp + 2*frame (the master's wsp
  at that routine's base level) is the image of BOTH the stack anchor W
  (a position value; LDASP/MSP cursors hold it) and the area's
  last-local word (a data location). Same principle, mirrored: the
  inverse resolves to the DATA — the AREA this time, since the live
  locals are there — and I4/I6 assert the fixpoint at that point.**
- **Stack leg — the COMPRESSION map.** The clone's stack is SMALLER
  than the master's: every live redirected frame elides its
  `10 + 2*frame` words from the clone stack while the master keeps
  them. Everything above a redirected frame — stacked callees' whole
  frames, WPSH temporaries, pushed args, (later) WMSP allocs — sits
  at addresses differing by the cumulative compression below it. The
  leg is piecewise: each live record is a discontinuity; between
  discontinuities the map is a constant shift = sum of elisions
  below. The discontinuity is AT the anchor (Finding A, Aug 22 2026):
  s == W takes the record's OWN shift — it is the clone's wsp at that
  routine's base level and images to the master's wsp, the same `>=`
  threshold shadow_wsp always documented. Master-to-clone subtracts,
  clone-to-master adds — the SAME walk, two directions. Bounded above by wsl (I2); values above wsl
  are not stack addresses and take the identity. DERIVED from the
  record list — no independent state. (Wave-one validity of the
  closed-form shift rests on WSAVS images being the ONLY elision; see
  section 5 and the M4aDesign section-8 marker.)
- **Identity everywhere else.**

A is a bijection; **the inverse is the same structure walked the
other way**. There is no separate T_inv with its own bugs — one walk,
one direction flag.

### 1.2 The encoding codec E (stateless)

The machine encodes one word address several ways. `decode(v) →
(form, word_addr)` and `encode(form, word_addr) → v` with, today:

| form | real-stack prefix | area prefix | decode |
|---|---|---|---|
| word | 0x70 | 0x74 | v |
| byte | 0xE0 | 0xE8 | v >> 1 (low bit preserved through encode) |
| @-word (bit 31) | 0xF0 | 0xF4 | v & 0x7FFFFFFF |

This table must be TOTAL over forms that occur. Open question, settled
empirically (P14 probe): do @-byte or other composed forms occur? A
probe fires when decode fails but the raw value's shifted/masked
readings land in a mapped range — "unlisted form seen" is a finding,
not a silent identity.

### 1.3 The public surface

Internally: `map(v, direction) = encode(form, A_direction(decode(v)))`
when the decoded word is in the domain, else identity. Externally the
surface is exactly THREE calls, named by PURPOSE so a direction can
never be picked by accident; each direction of the bijection has
exactly one job:

```
equivalent(master_v, clone_v) -> verdict     # COMPARISON. clone->master inside.
  RAW      if clone_v == master_v
  MAPPED   if fwd(clone_v) == master_v       (verdict carries form + record)
  MISMATCH otherwise                         (verdict carries both decodings)

frame_precedes(a, b) -> bool                 # ORDER of two clone-side frame
  addresses in MASTER coordinates (signed, like the walks it serves).
  The Ruling-A chain walks' ONLY legal instrument (P14 Q1-a ruling).

clone_location(master_addr) -> clone_addr    # DEREFERENCE. master->clone.
  The ONLY address a caller may act on: mediation verify-reads and
  write replay into clone memory. Form-aware (codec applied); the
  interval walk in reverse.
```

**Why the directions are what they are** (forced, not chosen): at a
live block's closed end the forward map is TWO-TO-ONE — the area end
pointer and the clone's first stack word above the redirect map to
the SAME master address, because the master's frame-extent end IS its
next-push address. The compression re-splits what the master merged.
Comparison therefore MUST run clone->master: whichever preimage the
clone holds, forward-mapping it reaches the master's value with no
guess. Master->clone comparison would have to pick one preimage and
would mismatch legitimate states (re-creating the P13 §6.3 divergence
by architecture). The inverse is a function only BY RULING and only
for dereference, where "which data location" has a unique right
answer (the stack leg — data lives there; an end pointer is a
reference to extent, never a data location).

- equivalent is ASYMMETRIC BY DEFINITION: clone maps toward master,
  never the reverse — a master value that coincidentally maps
  somewhere must not rescue a mismatch.
- clone->master exists ONLY inside equivalent/frame_precedes;
  master->clone exists ONLY as clone_location. There is NO general
  direction-flagged public map. Code translating merely to compare,
  or comparing to decide where to write, is a smell.
- No comparison policy exists outside the module (the register rule,
  mediation packet fields, the L2 door, the four Ruling-A chain
  walks, and any future memory compare are all
  equivalent()/frame_precedes() callers). No per-site variants. No
  ordered guessing.

## 2. The invariants

- **I1 — disjointness.** Domain intervals of A are pairwise disjoint,
  INCLUDING their closed right ends: for consecutive areas,
  `alloc_end(k) < alloc_base(k+1)`. This is a LAYOUT obligation on the
  book tool; the stride rule (advance by size+16) is its current
  implementation. The book loader ASSERTS it at load.
- **I2 — bounded domain.** Every leg of A has explicit bounds; the
  stack leg ends at wsl, BOTH directions. Implementation (P14 ruling):
  wsl is LATCHED at the empty-to-nonempty push (the boot wsl-steal
  precedes any redirect, so configure-time wsl is not the operative
  value) and asserted constant at every mutation while records live.
  No open-ended "everything above W shifts" (the P13 §6.4 observation
  was a violation of this, unstated).

- **I3 — prefix separability.** The encodings of area addresses and
  real-stack addresses are separable by prefix in EVERY form (the
  table above). This is the real content of the base ruling:
  0x74000000 satisfies it, 0x78000000 did not (area byte = 0xF0 =
  real @-word). Any future base change re-proves this table.
- **I4 — round trip.** Strict `inverse(forward(a)) == a` on interval
  INTERIORS; on the overlay band (closed ends) the fixpoint
  `forward(inverse(m)) == m` — the two preimages agree on the master
  value (P14 Q2 ruling). Asserted on every non-identity mapping
  (checked builds). Plus the global drift check: shadow_wsp == master
  wsp at every pair (standing).

- **I6 — inverse agrees with the clone's own data.** When a mediated
  handler dereferences a pointer FIELD that itself passed
  equivalent(), `clone_location(master_ptr)` must equal the clone's
  own field value on the overlay band, agreement on the MASTER value (fixpoint form,
  P14 Q2). Bijectivity as a tested invariant, running on every MAPPED
  verdict.
- **I5 — LIFO nesting.** The live-record list nests with the stack:
  entry pushes, return pops the innermost, unwind-cut pops a suffix.
  Asserted at each mutation.

## 3. State and mutation

The mapper's ONLY mutable state is the live-record list. Mutation
sites, exactly three, all clone-role, all traced (`redirect`):

1. Redirected WSAVS/WSAVR — push record.
2. Redirected WRTN (incl. frames.cpp::wrtn, the I.EPILOG path) — pop
   innermost.
3. Unwind cut (I.GOTO / area_unwind_to) — pop a suffix. The one
   non-paired mutation.

Process death (?FATAL/?RETURN/ABORT) discards the world; no mapping
survives it. The AddressBook is immutable after load. The codec is
stateless.

**Placement and configuration (ruled).** The Mapper lives in hw/ —
it needs tight integration with the stack implementations
(EagleStack, frames) — as an INSTANCE owned by each Machine. It is
CONFIGURED, never self-discovering: launch code loads the AddressBook
and calls `configure(book_tables, main_stack_bounds, wsl)` on each
Machine's mapper; the Mapper performs no file I/O, reads no env vars,
and reaches into no os/-layer object. The master's mapper is
configured too, with no book — master-vs-clone is a property of the
CONFIGURATION, not of code paths querying roles.

**Tasks.** ?TASK (0500) IS implemented and fires twice per session:
the game creates the C_A_LISTENER task (entry 0x7017E784) with an
I.ALLOC'd stack at ~0x70017650 on its own std::thread. Mapper state
(record list, shadow accounting) lives on `Machine`, and each OSTask
owns its own Machine — so the listener's mapper is its own,
permanently empty; cross-task interleaving is impossible BY
CONSTRUCTION. The spec still demands it loudly: **assert at redirect
that the executing task is the main task** (equivalently: wsp within
the main stack bounds). The listener never LCALLs game routines today
(it parks in the runtime; CA interrupt delivery is not implemented);
if that ever changes, the assert converts a silent mistranslation
into a finding. Multi-stack A (per-task stack legs keyed by
containing allocation) is a named FUTURE extension, not wave one.

## 3b. Wave-scoped validity conditions (stated so successors inherit them)

- **Every live call leaves >=1 wide on the real stack** (currently
  2*argc+2: args + LCALL frame word), so record anchors W are STRICTLY
  increasing down any live chain — I5's strict `W > back().W` and the
  address-ordered stack leg both rest on this. The zero-stack call
  protocol (args written to the callee's area, no pushes — the
  M5-direction protocol) INVALIDATES it: ties become legal,
  record-list order replaces address order as the LIFO authority, the
  shift stays well-defined (sum over tied records), and new seam
  fan-ins appear (a parent's closed end vs its child's alloc base
  sharing a master address — the same two-to-one shape as Q2, same
  resolutions). Three changes, not a redesign — but CHANGES; a session
  adopting the protocol must make them deliberately. NOTE (Finding A,
  Aug 22 2026): the STACK-LEG portion is already done — the `>=` walk
  is record-order for ties (innermost-first hits the innermost tied
  record, whose cumulative shift_after is the sum over the tie). I5's
  strict nesting assert and the seam fan-ins remain the deliberate
  changes owed.
- **Closed-form shadow accounting** is valid while WSAVS images are
  the ONLY elision (M4aDesign §8 marker); WMSP/dynamic-local migration
  retires it for the per-instruction simulation.
- **Single populated mapper**: records exist only under the main task,
  asserted at push as TASK IDENTITY (P14 note: the .PR header ships
  wsb==wsl for the boot probe, so a stack-bounds form of the assert is
  not statically available). Multi-stack A remains the named future
  extension.

## 4. Obligations on Project 14 (the refactor)

- One module (`hw/Mapper.{hpp,cpp}`): A + E + the two-call surface +
  asserts I1–I6, owned per-Machine, records moved into it.
- DELETE T/T_any/T_inv and the scattered case logic
  (Machine.cpp, OSContext.cpp clone_word_address's inline guard,
  LockstepMediator's replay guard) in favor of V calls.
- Fix the stale Machine.cpp:484 comment (cites the abandoned
  "round exact multiples up another 16" wording; the invariant is the
  stride).
- The unlisted-form probe (§1.2).
- Pure regression: batch-1 book and batch-2 (45-live) book, full
  standing battery each, 0 divergences, coverage identical to
  Project 13 §6.6. No new routines migrate in P14.
- The two Project 13 signature files and the §6.5 @-flag signature
  remain the named regression targets.

## 5. Future (named so they are not bolted on)

- Multi-stack A (if CA interrupt / listener activity ever exists).
- Memory compare: the area frame and the master frame are the same
  bytes; V makes a wholesale compare a straight walk. When wanted, it
  is a consumer of V, not a new mapper.
- WMSP/dynamic locals migration: new record kinds with the same
  interval discipline; requires the per-instruction shadow simulation
  (M4aDesign §8 marker) — at that point the closed-form shadow_wsp is
  retired.
