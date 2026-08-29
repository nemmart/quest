# Project 25 — byte addressing (M8[]) + close the call-lowering ledger

GOAL (user ruling, Aug 29): push the decorated-call ledger from
443/566 lowered to **564/566 = 100% of the in-scope set** — B-form
(96) + WPSH multi-wide (25). The 2 borrow-adjacent sites are RULED
OUT of scope (below); back off the target ONLY where an issue needs
a user ruling, reported not decided.

Hi Claude! Solo implementation session; the user reviews at the plan
gate and at the landing. Read docs/METHOD.md first, as always. Context
of record: **docs/IR.md** (the normative IR spec — §8 already reserves
"M8/M1 byte addressing" for exactly this), Project23/REPORT.md §5
(the decorated-site ledger: 96 call sites blocked on B-form pushes,
incl. all 6 remaining XCALLs) and §8.2, Project24/REPORT.md (for the
battery world). Numbering note: t-places (formerly "P24", then "P25"
in older banners) shift to **P26**; this project is P25.

## The gap

566 decorated game→game call sites exist (quest.pushmap.M4); 443 are
lowered to `call` + plain arg-slot stores. The 123 remaining:
- **96 B-form**: lower.py cannot express a byte effective address, so
  every XPEFB/LPEFB arg push (78 occurrences in quest.dis) stays
  embedded and its call site stays unlowered.
- **25 WPSH multi-wide**: one instruction pushes several wides; the
  rev-2 grammar was SHAPED for one-instruction-to-many-statements
  (addressless statements — P23 REPORT §4), and the wides accounting
  already exists (note_arg_write scales; quest.wpsh_wpop has the
  data). This is expected to be grammar-free mechanical work.
- **2 borrow-adjacent: RULED OUT (user, Aug 29).** The borrow
  brackets are not call args at all — they're the slot-notation
  scratch-register idiom, and they are best solved with temps
  (t-places, P26). Their blocks stay embedded; do not attempt to
  lower these 2 sites or build any borrow machinery. Census them as
  the two deliberate exclusions; 564/566 is the 100% mark. DG byte pointers
are word addresses doubled plus a byte-select bit; IR.md §8 records the
parked value formula:

    byte_ptr = ((base<<1) + disp) & 0x1FFFFFFF | (seg<<29)

and the standing rulings: byte pointers are VALUES (an XPEFB lowers to
a plain arg-slot store of a computed value — no executor wrap, no
dereference), and absent=emulated stays the safe default for anything
inexpressible.

## The work

### Part 1 — census + grammar proposal (plan gate, before any code)

1. Ledger the 96 blocked sites from quest.pushmap.M4 + the dis:
   X vs L form, direct vs indirect (the indirect LPEFB at 7015C2B4 is
   the known hard case — its EA takes a memory dereference before the
   byte scaling), enclosing routine, and expected liveness under the
   standing battery legs (drive.py's play mode presses L →
   LIST_PLAYERS.3 holds XCALL/B-form sites; say in advance which sites
   the battery should demonstrate live).
2. Derive the byte-EA semantics FROM THE EMULATOR SOURCE
   (eagle_{x,l}_byte_indexed and friends), not from the formula above
   or apparent intent (METHOD §5) — confirm or correct IR.md §8's
   parked formula, including the indirect variant's dereference order
   and any segment/ring masking. Discrepancy = finding, report it.
3. Grammar proposal for the user's ruling. The user's opening
   suggestion: "maybe we need special R8[] and M8[] support?" Present:
   - **M8[<expr>]** byte memory accessor (matches IR.md §8's reserved
     M8/M1) — needed if byte LOADS/STORES are lowered;
   - byte-EA **value** expressions for the push sites (the `<<`
     computation) — the minimum that unlocks the 96 sites, since a
     pushed byte pointer is stored, never dereferenced, at the site;
   - whether **R8[]** is needed at all — registers hold byte pointers
     as ordinary word values, so a register byte-accessor may have no
     use; present the question with evidence rather than deciding.
4. Scope proposal for the user's ruling on byte LOADS/STORES: the
   push sites need only byte-EA values, but if M8[] is adopted,
   lowering XLDB/XSTB/WLDB/WSTB statements may come nearly free.
   Census the counts so the ruling is sized; recommend one scope.
   (The 100% call-ledger goal itself is already ruled — it is this
   project's target, not a plan-gate question.)
5. WPSH multi-wide plan: per-site mapping from quest.wpsh_wpop to
   N addressless stores + the call's wides accounting; identify any
   site that does not fit the expected shape (finding, not fix).
6. (Borrow pair: no analysis owed — excluded by ruling, see "The
   gap"; just census the two sites as deliberate exclusions.)

STOP AND REPORT at the plan gate with the census + proposals. Parts
2–3 proceed only on the user's rulings.

### Part 2 — implementation

- lower.py: emit the ruled grammar for B-form pushes (and byte
  load/stores if in scope), and the WPSH multi-wide expansion (one
  instruction → N addressless stores); TOTALITY unchanged — anything
  still inexpressible is omitted with a censused reason. Regenerate
  quest.ir2.book/.stock; ledger every newly lowered site (target:
  564/566; per-site census for any shortfall beyond the two ruled-out
  borrow sites).
- IRExec: evaluate the new forms; loader validation extended in the
  refuse-on-anything style (malformed byte-EA lines refuse, never
  skip — the P23 pushmap-parser lesson).
- docs/IR.md: spec update for the new grammar (version-history entry;
  spec-wins doc — the update itself is a deliverable the user approves
  at the landing).

### Part 3 — validation

- Local gates per METHOD §15 (≤~30 min): K=1 book leg that provably
  executes newly lowered B-form sites (the L-key path), plus one
  stock leg. Coverage evidence = IRExec first-execution lines for the
  predicted blocks, stated in advance (part 1.1).
- Server battery: COPY tasks/034-parallel-battery.sh — the parallel
  template of record (13 legs, JOBS pool; do NOT copy 031/032's
  serial shape) — as task 035, pointing at the new artifacts, with a
  B-form-site coverage line appended to the verdicts alongside the
  carry line. Landing bar: 13/13 green, strict gate, predicted
  newly lowered sites demonstrably live — the WPSH multi-wide set
  includes TERRAIN/TERRITORY, which every play leg hammers, so
  liveness evidence there should be abundant; honest reporting of any
  that the scripted legs cannot reach (the carry-coverage precedent: census
  classification carries the safety argument; liveness is
  demonstrated where the drivers go, recorded as-is).

## Boundaries — BINDING

1. **Scope = the call-lowering ledger + byte addressing per the
   plan-gate ruling, nothing else.** No t-places, no borrow/restore
   ops (standing ruling), no `save`, no @/bit-15 regeneration, no
   checker changes. Tempting adjacencies go in the report.
2. **Part 1 before any code; the grammar and byte-load scope are
   user rulings.** The 100% ledger target is pre-ruled; back off only
   via a reported issue (the borrow pair being the anticipated one).
3. **Semantics from the emulator source and, where it speaks, the DG
   manual** — never from the parked formula alone or from what a byte
   pointer "obviously" is. The indirect LPEFB's dereference order in
   particular must be read out of the source (METHOD §5) and proven
   empirically at a live site if one is reachable (§6 capture
   tooling).
4. **Design-vs-reality: STOP AND REPORT** — a site the grammar cannot
   express after the ruling, a byte-EA whose source semantics
   contradict the spec note, a lowered site that diverges. Do not
   tune around evidence.
5. **Implementation bugs: fix and record** (METHOD §11).
6. A red battery is STOP-and-report, never a solo iteration loop.
7. Deliverables: census/ledger doc (docs/Project25/ByteEA.md),
   implementation + regenerated artifacts, IR.md update, task 035 +
   green battery, REPORT.md + worklog in house form,
   CURRENT_STATE/NextSession updates or an explicit integrator
   handoff note — and STATE THE TREE VINTAGE the session was cut
   from (the P24 integration lesson: the reviewer diffs against the
   last integrated tree either way).
