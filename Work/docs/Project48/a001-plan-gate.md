# a001 — P48 plan gate: APPROVED. All eleven rulings granted, one boundary widened.

Integrator, Sep 12 2026. **Proceed to Stage A.**

R1a–R1e, R2, R3, R4, R6, R7: **granted as recommended.** R5 granted with a
boundary extension — see below. Nothing in the plan needs changing before you
start.

Two things in this gate are better than what I asked for, and one piece of
your own analysis is more pessimistic than it needs to be.

---

## The boundary widening (R5)

**Granted, and `game/quest_rt.h` is yours.** Nothing else is running.

You are right that leaving `SUB(i, n)` as a native identity means the
`assert` we emit is compared against nothing — the three DERR sites in
UPDATE_SCREENS and every generated subscript check would be untested. Make
it a real trapping check on the native side and teach the driver to treat
"both trapped" as agreement and "one trapped" as a disagreement.

Two conditions:

1. **The C view and the translator view must stay the same object.** The
   header serves both; do not fork it.
2. The trap must be faithful in *kind* — the original raises DERR 17 on a
   bounds violation, so trapping is the faithful behaviour, not an
   embellishment. Say in the REPORT what you made it do.

---

## On the two best things here

**R2 — pure operators only — is the ruling I would have most wanted and did
not think to ask for.** "The C source says nothing about carry, so emitting
an op that writes flags would be the compiler inventing semantics" is exactly
right, and it is the same discipline as refusing rather than approximating.
Flag conversion becomes a rewrite with a flag-liveness precondition, which is
where it belongs.

Your consequence is correctly stated and I want it in the REPORT too: **a
naive routine's IR will differ from the book on every arithmetic statement,
not just on allocation.** That is the expected starting distance, and someone
seeing it for the first time at L2 would read it as a defect. It is not.

It does require one refinement to the design, which I have made — see the
last section.

**§2.3's checksum is the best idea in the gate.** Threading an ordinary
`chk = chk * 1000003u + v` through the program gets you order-sensitivity and
path-sensitivity with *no instrumentation on either side*, because it is just
a statement in the subset that both back ends already handle. It closes the
"final state alone is not enough" gap without either back end having to grow
a hook. Keep it exactly as designed.

---

## One correction to your own §2.5

**Item 10 is less dangerous than you think, and I want to say why so you do
not over-invest in guarding it.**

You write: *"If I have the C semantics of a construct wrong in the same way
in both, the corpus agrees."* That does not follow, because **the generator
does not implement C semantics — it emits C text.** gcc supplies the
semantics. If you believe `uint16_t / uint16_t` is an unsigned divide and
emit `/u`, gcc still performs the signed divide C mandates, and the corpus
**disagrees**. Your model is the thing under test; gcc is not downstream of
it.

A shared misunderstanding can only hide when it causes the generator to
**not emit** a construct, or to emit **UB**. So item 10 is not an independent
risk — it collapses into item 2 (constructs never generated, answered by your
census) and item 4 (UB, partly answered by the gcc-vs-gcc discard counter).

This matters practically: it means **the construct census in §2.4 is the
central instrument of the whole project**, not a reporting nicety. Every zero
in that table is a real hole; everything else is covered by gcc being an
honest oracle. Give the census prominence in the REPORT.

**Item 1 stands as you wrote it and is the real residual.** The rig runs the
real `IRExec`, so you prove the lowering faithful to the IR *as implemented*,
not *as specified* — and a v-form `IRExec` bug is invisible here and
invisible in lockstep, because lockstep only ever exercises the book's IR.
P46's vform self-test is the only other thing that has ever executed these
paths. Nothing in this project can fix that; I am recording it in DESIGN as a
named residual rather than letting it live only in your report.

---

## Smaller notes

**§3.1 — your UPDATE_SCREENS reading closes and the cross-checks are the
right ones.** `WSGE 2,2` as the compare-against-zero form making
`WSUB;WSGE;WNEG` an ABS diamond rather than a self-compare; `−611 = −587 −
22 − 2` placing `screen` immediately below `fm588`; and the consistency of
the two ABS bounds (4, 5) with the two subscript bounds (9, 11). That last
one is a genuine independent check rather than a restatement, which is what
makes it worth something.

**§1.4a — signed `>>` by pure decomposition: granted**, and your instinct to
name it a suspect is right. Six statements for one operator is fine. Put
`n = 0` and `n = 31` in the hand suite, as you planned, and also
`x = 0x80000000`.

**§1.7 — construct-anchored provenance from the start.** Good. DESIGN §7.4
warned that position-keyed sites make oracle files write-once; carrying the
anchors from day one costs nothing now and saves a migration later.

**Corpus target ≥ 2,000 against a floor of 200:** fine, but do not let corpus
size substitute for census coverage in the REPORT's headline. 2,000 programs
with a zero in the table is worse than 300 with none.

---

## The design change your R2 forces

Your R2 exposes a flaw in DESIGN §7.2a as written. The tripwire says that if
~20 general rewrites cannot close a routine, suspect the C — and that a
growing rewrite count means something is being laundered.

But flag conversion is **one rewrite rule applied to every arithmetic
statement in the program**. On a 72-statement routine that is dozens of
applications of a single, sound, well-understood rule. Under the metric as
written that reads as an alarm, and it is not one.

**So the metric is split, and I have amended §7.2a:**

- **Rewrite-RULE count** — how many distinct rules exist. This is the
  tripwire. ~20 is the target; growth is the reportable event.
- **Rewrite APPLICATION count**, and oracle length — how much supplying a
  routine needed. This is the progress metric, expected to be large early.

One rule applied 200 times is a compiler model working. Two hundred rules
applied once each is fitting. The old wording could not tell them apart.

Proceed to Stage A.
