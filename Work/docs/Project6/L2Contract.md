# L2Contract.md — The L2 (Condition-System) Boundary Contract

Project 6, Phase 1 deliverable 2. Status: DRAFT FOR REVIEW.

This document is the BOUNDARY view: what any L2 implementation owes L1,
derived from the native L2 source (runtime/*.cpp, lockstep-certified
against the bytes) and the Project 1–5 DERIVATIONs. It is
implementation-agnostic; the internal view is docs/Project6/
NativeDesign.md, which this contract constrains and which must never
leak back into this document.

Governing principle (user-ratified, PROMPT.md): the contract specifies
what the L2 code DOES and RETURNS — full behavior, every branch,
including cold ones. Deviations exist only as entries in the Deliberate
Exclusions Register (§10). The scans (SESSION_REPORT_AUG13.md) license
individual exclusions; they do not define scope.

Evidence base per claim class:
- Behavior and exit state: the native wrappers (bit-faithful,
  continuously lockstep-verified) + Project 1–5 DERIVATIONs.
- Privacy classifications: scans 1–5 (SESSION_REPORT_AUG13.md §§5–6).
- Call-mechanism semantics: emulator source pins quoted in the
  DERIVATIONs (never intent).

## 0. Definitions

- **Crossing**: a control transfer between L1 and L2 by any mechanism
  (call, indirect call, transfer-to-pc, patched return, return into the
  other layer). Every crossing has a control component (pc) and a stack
  component (wsp/wfp effect). Interior L2→L2 calls are NOT crossings
  (rendezvous ruling, SESSION_REPORT §3) and are explicitly
  unclassified implementation.
- **Exit**: the point where an L2 entry's execution leaves L2 —
  return (WRTN to the caller), transfer (to an arbitrary L1 pc), or
  descent (to L3). One entry may have several exits; all are contract.
- **CONTRACT-PRIVATE**: state whose VALUE no conforming implementation
  must reproduce in real memory, licensed per cell by scan 1/2 (zero
  L1 readers). The register file is never contract-private (§11.3).
- **CONTRACT-OBSERVABLE**: state L1 reads or writes; preserved exactly.
- **SHARED-PROTOCOL**: state both layers read/write under a defined
  protocol (the B record, §2.3).
- **Address-as-landmark** (scan-1 finding b): a cell whose VALUE is
  contract-private but whose ADDRESS is load-bearing for L1
  (I.GINIT uses &chain_head arithmetically; T?AREA/O?AREA export
  addresses). Static-area geometry is preserved on the master by
  construction; the clone owes only that the exported ADDRESSES keep
  their arithmetic meaning to the L1 code that uses them (today: only
  I.GINIT's heap-bounds computation, which runs identically on both
  engines against the real static area).
- **ABORT-INTENDED** (second addendum + rulings A1–A3): an L2-internal
  defensive branch — a condition meaning the runtime's own invariants
  are broken. The contract specifies the detecting condition and the
  current raise behavior (full behavior), and annotates that a
  conforming implementation aborts (abort_world, save=false) instead of
  signaling. No recovery path is specified. Semantic raises (reporting
  legitimate external conditions) are NOT in this class and keep full
  signal contracts.
- **DEAD-GUARDED**: a branch statically dead in this binary behind a
  documented gate; the contract specifies the gate, a runtime re-check,
  and the fallback if the gate ever opens.

## 1. Entry mechanisms and staging conventions

All arg-cell arithmetic below, pinned from the emulator source via the
DERIVATIONs (P1 §2, P2 "Instruction semantics", P3 "Entry facts"):

| Mechanism | Pushes | ac3 at entry | ovr | In-stream words |
|---|---|---|---|---|
| LCALL [T],n | frame wide (psr<<16)\|n | call-pc+4 | cleared | none (wide target is instruction words) |
| XCALL a,argw,[EA] | frame wide (psr<<16)\|argw (bit15 clear; else argw&0x7FFF) | call-pc+3 | cleared | index word at pc+1, argument word at pc+2 — both instruction operands, read at execution from code space |
| LJSR [T] | NOTHING | call-pc+3 | NOT cleared | see per-entry (I.PROLOG only) |
| XJSR (interior only) | nothing | pc+2 | not cleared | — |

Frame-relative argument addressing after WSAVS (frame pointer F =
entry_wsp E + 10; entry_wsp = wsp after the caller's push):
argc = narrow at F−9 (the frame wide's low half); arg-N pointer =
wide at [F − 10 − 2N]. WSAVS image (5 wides): saved ac0 [F−8],
ac1 [F−6], ac2 [F−4], caller wfp [F−2], ret|c [F]; ovk:=1.
WSSVS/WSSVR image (6 wides, LJSR routines): pushed psr wide FIRST
(low half 0 ⇒ WRTN pops zero args), then ac0/ac1/ac2/wfp/ret|c;
ovk:=1 (WSSVS) / 0 (WSSVR).

**In-stream literal-word conventions (complete enumeration).** The
only L1-side call convention with data words the continuation skips is
I.PROLOG: the LJSR at pc is followed by FOUR data words at
pc+3..pc+6 — [ac3+0,+1] a wide (→ frame slot [+4]), [ac3+2] a narrow,
sign-extended (→ [+6]), [ac3+3] a narrow display count — and the
continuation is ac3+4 = **LJSR pc + 7**. Only I.PROLOG reads these
words; all 18 Quest sites pass 0 / 0 / {0|1}. O.ON, O.REVERT,
I.EPILOG, and I.GOTO are plain LJSR sites: no data words, continuation
pc+3 (used by O.ON/O.REVERT; never used on I.EPILOG/I.GOTO live
paths). Everything else on the surface is LCALL (continuation pc+4)
or XCALL (continuation pc+3, operand words at pc+1/pc+2). Interior L2
conventions (the EE38/EE3D XCALLs, XPSHJ, the skip-return) are
implementation, listed in the per-entry behavior only because their
effects appear in normative memory images.

## 2. Abstract state the contract obligates

### 2.1 The establisher chain (CONTRACT-PRIVATE representation)

An abstract per-task stack of **establisher records**, one per active
I.PROLOG bracket, innermost first. Each record carries, abstractly:
(a) an identity that is also I.GOTO's cut level — in the current
implementation the establishing routine's frame pointer; (b) the wsp
restore point recorded at establishment (entry wsp + 4); (c) two
opaque words from I.PROLOG's inline data (always 0 in Quest); (d) an
ordered list of **handler nodes**, each (type-key, key2, handler-pc,
active?), maintained by O.ON/O.REVERT with the reuse/backstop
discipline of §3.4.

Push: I.PROLOG. Pop: I.EPILOG (one record), I.GOTO (every record above
the cut target). Search: the select loop (per-record chain_search:
first node with key match; nodes with type-key 0 are inactive and the
LAST such node is the reuse backstop). Representation is free
(that is the point); the current bit-faithful representation (frames,
[wsb−0x40] head, on-stack nodes) is one conforming choice.

**Stack reservations are normative even where contents are private**
(consequence of the full register-file rule §11.3 — wsp is compared at
every rendezvous): I.PROLOG grows wsp by 4 (two wides: the head slot
and entry ac1); O.ON's allocate path grows wsp by 8 (the node);
I.GOTO restores wsp to the recorded snapshot. A conforming
implementation must reproduce these wsp effects exactly; the CONTENTS
of the reserved words are contract-private storage (Exclusions
Register E8/E9).

Privacy basis: scan 1 (zero L1 readers/writers of the wsb band);
frame-slot and on-stack-node privacy rests on the ratified provenance
argument plus the lockstep backstop — FLAGGED in §12 (scan 1 did not
enumerate per-frame slots).

### 2.2 The condition record C (CONTRACT-PRIVATE value; landmark address)

wsb−0x40 band (O?AREA exports its address). Fields and classes per the
scan-2 census: chain head [wsb−0x40] (value private; address is an L1
landmark for I.GINIT); recorded signal type/key2/code
[wsb−0x3E/−0x3C/−0x3A] (written by O.SET; read by the O?SIGNAL tail,
R?SIGNAL, DEF?ON — all L2 — and by L3 observers); walker outputs
[wsb−0x36/−0x38] (written by O.SET; NO L2 reader; L3-only observers);
resume flag [wsb−0x2A] (written by O?SIGNAL every raise; read by the
tail and DEF?ON — all L2). PRIVATE-with-L3-observer ratification:
contract-private for the stack-free clone; L3 observers (?FATAL,
P?SNAP, I?LINE) run master-only where authentic cells exist by
construction. Revisit only if an ABORT-INTENDED path ever routes live
into clone-side ?FATAL.

**The resume-flag convention** (P5 §1, normative): every O?SIGNAL
entry writes the flag — *arg4 (narrow, sign-extended to a wide) when
argc>3, else 0. The resumable test is bit15 of the narrow at
[C+0x16] = the sign bit of that wide. The flag is sticky until the
next raise; the SHORTHAND entries and O.SERROR never write it, so a
shorthand-raised signal's tail and DEF?ON read the PREVIOUS raise's
flag. A conforming implementation reproduces exactly this staleness.

### 2.3 The task error record B (SHARED-PROTOCOL)

B = T?AREA-result + 8 = wsb−0x21. Owned jointly with L1 (?LIB_ERROR
and ?LIB_ERROR_CODE are L1); it stays in real memory in any
implementation. Fields (P2 census, scan 2): [B+0] narrow signal latch
(?LIB_ERROR WBTO sets bit 0x8000; write-only); [B+1] last error code
(w ?LIB_ERROR; r ?LIB_ERROR_CODE, ?DEFAULT_ERROR_HANDLER); [B+3]
message-buffer pointer + the varying-string buffer it addresses
(?LIB_ERROR free-then-alloc protocol); [B+0x1E] handler address —
single writer in the whole system (?LIB_ERROR's lazy install, value
0x7017E3D2), so its value is always 0 or ?DEFAULT_ERROR_HANDLER; the
XCALL through it is the system's only data-held L1→L2 entry; [B+0x20]
companion word (installed 0; passed in ac1 at dispatch). L2's
obligations: ?DEFAULT_ERROR_HANDLER reads [B+1] (its only B access);
no L2 code writes B. The install invariant ([B+0x1E] ∈ {0, 0x7017E3D2})
is L1's to maintain; L2 may check it defensively but must not depend
on more.

### 2.4 The exported predicate rt::signal_has_handler (contract operation)

Scan-1 finding a, mandatory: "true iff a signal of type −1/key2 0
raised now would find a handler" — an L2 contract operation over
abstract chain state, exported to L1 (?LIB_ERROR's gate). It must
never be an L1 raw read of chain representation; re-hosting the chain
must not break it. Current definition: select-loop semantics with
(type=−1, key2=0); answers false when the O.SET walker gate (§3.8) is
open (prediction impossible ⇒ caller falls back — safe).

## 3. Per-entry contracts

Format per entry: mechanism; Inputs (consumed); Behavior (all
branches); Outputs per exit — registers AND flags, memory written,
continuation semantics; Ordering/state obligations; Edge/annotation
rows. "Entry values restored" means: from the frame image, i.e. the
values AT entry, except slots the body patches. E = entry wsp
(post-push where a frame word exists), F = E+10 for WSAVS routines.
c=carry, ovr sticky-unchanged unless stated. Citations: P<n> =
docs/Project<n>/DERIVATION.md; source file in runtime/.

Layer note: entries §3.1–3.15 are the registered L2 surface (scan 5
list 1); §3.16 is the indirect entry; §4 the untranslated surface.
?LIB_ERROR / ?LIB_ERROR_CODE are L1 (Layering census) — they appear
here only as the counterparties of §2.3/§2.4 and §3.16.

### 3.1 I.PROLOG (0x7017E733) — establish a condition frame  [P3; frames.cpp]

LJSR; no own frame. In-stream convention per §1 (the only one).
Inputs consumed: caller wfp (the establishing frame), wsp, wsb,
[wsb−0x40], caller's saved-ac1 slot [wfp−6] (static-link walk seed),
the four inline words, entry ac1 (pushed as residue only).

Behavior: push 0 (head slot) and ac1; record wsp snapshot
(= entry wsp+4) at [wfp+2]; [wfp+0xA] = address of the head slot
(= entry wsp+2); [wfp+4]/[wfp+6] = inline wide/narrow; chain-push:
[wfp+8] = old [wsb−0x40], [wsb−0x40] = wfp; display loop: count−1
iterations copying static links to [wfp+0xC..] (never iterates in
Quest — all sites pass count 0 or 1 — but its register/flag effects
differ by count and are normative); pop the pushed continuation EA;
transfer.

Outputs — single exit, transfer to entry_ac3+4 (= LJSR pc+7):

| | value |
|---|---|
| ac0 | old chain head (the pre-push [wsb−0x40]) |
| ac1 | −1 (count 0) / 0 (count 1) / count−1−iterations generally |
| ac2 | wfp+0xC + 2·iterations |
| ac3 | wfp (caller frame) |
| c | **1 for count 0 (WSBI borrow), 0 for count 1**; general: borrow of the final decrement |
| ovr | unchanged (sticky; all operands positive) |
| wsp | entry+4 |
| psr/ovk | unchanged (no frame op) |

Memory: [wfp+2/+4/+6/+8/+0xA] (+0xC.. if iterating), [wsb−0x40],
stack wides [entry_wsp+2]=0, [+4]=entry ac1, residue above final wsp
[+6]=entry_ac3+4. All of it contract-private storage EXCEPT the wsp
growth (normative, §2.1) and [wsb−0x40]'s landmark address.

State obligation: exactly one establisher record pushed, empty node
list, snapshot = entry wsp+4.

### 3.2 I.EPILOG (0x7017E77D) — disestablish and return from the caller  [P3; frames.cpp]

LJSR; no own frame; the LJSR return address is never used.
Inputs: caller wfp, [wfp+8], the caller's own WSAVS frame image and
frame word (consumed by the WRTN).

Behavior: [wsb−0x40] = [wfp+8] (chain pop), then WRTN against the
CALLER's frame — I.EPILOG returns FROM its caller.

Outputs — single exit, transfer to the caller's return address
(bits 0..30 of the caller's ret|c wide):

| | value |
|---|---|
| ac0/ac1/ac2 | the caller's saved values (its frame image) |
| ac3 | the caller's caller's frame pointer (restored wfp) |
| c | bit31 of the caller's ret wide |
| psr | high half of the caller's frame word (restores ovk/ovr/…) |
| wsp | caller frame − 12 − 2·(caller's argc) |
| wfp | the caller's caller's frame |

Memory: one wide, [wsb−0x40]. Shadow call stack: pops the caller's
entry (return address matches — silent).

State obligation: pop exactly the innermost establisher record.
Cold edge: no guard exists — I.EPILOG against a frame that is not the
chain head writes whatever [wfp+8] holds into the head (full behavior;
never observed mismatched; PL/1 codegen brackets guarantee pairing).

### 3.3 I.GOTO (0x7017EC7C) — non-local goto / unwind  [P3; frames.cpp]

LJSR. Inputs: ac0 = target frame pointer (the round-tripped token or a
game-captured frame value), ac2 = label pc, ac3 = LJSR return (dead on
live paths), wfp, the saved-wfp chain, [wsb−0x40], per-walked-frame
[frame+8], [target+2].

Token round-trip (scan 3, normative): the dispatcher hands each
handler the establisher token in ac1; every one of the 26 catalog
bodies holds/passes it (entry ac1 or the fp−6 reload) to I.GOTO's ac0
WITHOUT dereferencing it. L1's only right over the token is
round-tripping; machinery that treats it as a frame address is
L2-internal. Two game non-handler sites pass frame values captured
from their OWN frames (label variables) — I.GOTO's ac0 is therefore
"a cut level L2 itself minted or L1 legitimately captured", and both
must keep working.

Behavior — three shapes:

1. **Local** (ac0 == wfp): jump to the label. Outputs: pc = label;
   ac1 = entry ac3; ac3 = wfp; ac0/ac2/c/wsp/wfp/psr unchanged; no
   memory writes.
2. **Unwind** (live shape): walk the saved-wfp chain from wfp; each
   step, if the frame being left is the current chain head, advance
   the head candidate past it ([frame+8]); stop when the frame BELOW
   the cursor equals the target. Then: [wsb−0x40] = walked head;
   patch cursor's ret|c slot ([cursor]) = 0x7017EC9D (landing stub;
   bit31 clear ⇒ c=0 at landing); patch cursor's saved-ac2 slot
   ([cursor−4]) = label; wfp = cursor; WRTN through the cut frame;
   the landing stub restores wsp from [target+2] (the I.PROLOG
   snapshot) and jumps to the label. Outputs — transfer to label:

   | | value |
   |---|---|
   | ac0 | cursor's saved-ac0 value |
   | ac1 | cursor's saved-ac1 value |
   | ac2 | label (the patched slot) |
   | ac3 / wfp | target frame |
   | c | **0** (patched ret wide, bit31 clear) |
   | psr | cursor frame's pushed psr |
   | wsp | [target+2] = the establishment-time snapshot |

   Memory: [wsb−0x40]; the two patched cursor slots; two pushed wides
   (entry ac3, label) as residue above the restored wsp. Shadow stack:
   pops one entry with the documented benign mismatch notice
   (call return 7017EC9D vs stack 7017EE40 — identical on both
   engines).
   State obligation: every establisher record strictly above the
   target is popped; a record AT the target survives with its nodes.
3. **Anomaly branches** (never observed):
   - 0x7017ECBD/ECC1 — walk meets a non-positive link or the
     bad-chain test: **LCALL O.SERROR with code 0x11614 / 0x11635**.
     **ABORT-INTENDED** (the addendum's named site): condition =
     corrupt frame chain during unwind; current behavior = raise
     ERROR through the signal machinery; conforming implementation:
     abort_world(save=false), no recovery path specified.
   - 0x7017ECA2 — non-descending link (below ≥ cursor):
     segment-masking against the restart vector [0x70000124]
     (cross-stack/task dispatch; the vector is installed by I.GINIT
     to the data pair at 0x7017EB63 → R.SIGREC). Full behavior:
     specified by the emulated bytes; never executed.
     **FLAGGED-AMBIGUOUS** (A1: flag, don't decide): a designed
     cross-task restart mechanism, not obviously a corrupt-state
     detection — see §12 flag 3. Until adjudicated, conforming
     implementations fall back to bit-faithful emulation on this
     shape (the current behavior).

### 3.4 O.ON (0x7017ED9B) — register a handler  [O_ON.md; o_on.cpp]

LJSR, WSSVS 0x0004. Inputs: ac0 = condition type (≤0 = catch-all;
Quest always −1), ac1 = key2 (forced to 0 for catch-all), ac2 =
handler pc, implicit: caller wfp (the establisher), its [wfp+0xA]
head-slot pointer and chain, entry c (recorded in images; the helper
preamble clears it for catch-all searches).

Behavior: search the caller frame's node list (chain_search: first
node with [+2]==type && [+4]==key2 wins; every node with [+2]==0
overwrites the backstop, LAST zero node wins). Found or backstop →
REUSE: overwrite node [+2]=type, [+4]=(type≤0 ? 0 : key2),
[+6]=handler. Neither → ALLOCATE: the frame-extension trick — the
body relocates its own six-wide saved block up 8 words (relocated
image at [frame−2..frame+9], overwriting the untouched locals; ret|c
wide re-pushed with ac3 = LJSR return and entry carry) and the
abandoned block at frame−10 becomes the node with type/key2/handler
written; caller_frame[+2] := frame−4 (bookkeeping); node[+0] = old
head via @[caller+0xA]; head slot := node. (frame here = entry wsp+12,
O.ON's own would-be frame pointer.)

Outputs — single exit, return to LJSR pc+3, both paths:

| | value |
|---|---|
| ac0/ac1/ac2 | entry values (restored from the — possibly relocated — image) |
| ac3 / wfp | caller frame (restored) |
| c | entry carry |
| psr | restored from the pushed psr wide (WSSVS image) |
| wsp | **entry wsp (reuse) / entry wsp + 8 (allocate)** — normative |

Memory: WSSVS image + helper residue (six-wide helper image, scratch
wide, patched saved-ac1 slot, ret wide +1 on not-found — all
contract-private storage); the node writes; allocate-path relocated
image + caller_frame[+2] + head-slot link. State obligation: after
O.ON, a search for (type', key2') on this frame finds this handler iff
the keys match per §2.1's discipline; re-ON after REVERT reuses the
deactivated node (validated live).

Note: the pushed helper psr carries ovk=1 (O.ON's WSSVS precedes the
XJSR); catch-all searches record c=0 in the helper ret wide (the
preamble WSUB 1,1 side effect — the O_ON.md single-bit lesson).

### 3.5 O.REVERT (0x7017EDCB) — deregister  [O_ON.md; o_on.cpp]

LJSR, WSSVR 0x0000. Inputs: ac0 = type, ac1 = key2, caller wfp,
[wsb−0x40].

Behavior: no-op unless [wsb−0x40] == caller wfp (the caller is the
innermost establisher); then search (same helper, catch-all preamble)
and, if found, node[+2] := 0 — deactivate IN PLACE (never unlink;
O.ON's backstop reuses such nodes). Not-found → no-op.

Outputs — single exit, return to LJSR pc+3: all registers/c/psr = entry
values; wsp = entry wsp. Memory: WSSVR image (pushed psr has ovk=0);
helper residue when the gate passed; node[+2] when found. State
obligation: the handler stops matching; the node remains reusable.

### 3.6 T?AREA (0x7017ED93) — area accessor  [P3; t_area.cpp]

LCALL, 0 args, WSAVS 0. Inputs: wsb. Behavior: patch own saved-ac0
slot with wsb−0x29; WRTN. Outputs — return to LCALL pc+4: ac0 =
**wsb−0x29** (address export — landmark); ac1/ac2/c/psr = entry;
ac3 = entry wfp; wsp = entry−2. Memory: the five-wide image with the
patched ac0 slot (contract-private storage). Cold edge: argc ≠ 0 —
no argc-dependent behavior in the body (WSAVS/WRTN pop per the frame
word); all 12 sites pass 0.
Protocol note (normative for L1 clients): every consumer rebases to
B = result+8; words result+0..+7 are dead header (no accessor
anywhere, P3).

### 3.7 O?AREA (0x7017FC39) — condition-record accessor  [P4 §2; o_area.cpp]

Identical shape to T?AREA with result **wsb−0x40**. Callers are
L2 (DEF?ON) and L3 (?FATAL) only — the entry is on the registered
surface and is contract, but it has no L1 caller today.

### 3.8 O.SET (0x7017EE56) — record the signal  [P1 §§3–5; o_signal.cpp]

XCALL argw=0 (interior, from the shared body) and LCALL 0 args
(I.SFALT — frozen). Inputs: ac0 = type, ac1 = key2, ac2 = code; wsb;
the chain ([wsb−0x40], per-frame [+4]/[+6]/[+8]); the gate wide at
**0x7017EEA0** (read from code space on every execution).

Behavior: run the deep walker, then store: [wsb−0x36] = walker out1,
[wsb−0x38] = walker out2, [wsb−0x3E] = type, [wsb−0x3C] = key2,
[wsb−0x3A] = code (entry values — the walker's WRTN restored them).
Walker (live path): find the innermost establisher frame whose
[frame+4] points at a positive narrow word; out1 = [frame+6],
out2 = the pointer; else zeros.

**DEAD-GUARDED branch** (binding adjudication, PROMPT.md; overrides
scan 5's "LIVE" line): the walker opens with WLDAI of the wide at
0x7017EEA0 (value 0 in the .PR, byte-verified) and branches on >0
into the EEA4–EEE9 region — the [wsb−0x40] cache, the frame-identity
walk, and the **I?LINEID descent** — statically dead in this binary.
Contract: the gate wide is part of the machine's code image; a
conforming implementation re-checks it (wide read) at every execution
and, if it is ever positive, falls back to bit-faithful emulation of
the whole entry BEFORE any store (the gate precedes the walker's first
write). The I?LINEID subtree remains unspecified behind the gate.

Outputs — return exit (WRTN): ac0/ac1/ac2/c/psr = entry values;
ac3 = entry wfp; wsp = entry−2 (argc 0). Memory: own WSAVS image;
walker WSSVR image (six wides; five locals NEVER written — untouched
residue); the five wsb-band stores. All memory contract-private
(scan 1/2); the five stores' VALUES are semantically load-bearing for
later L2 reads (§2.2).

### 3.9 O?SIGNAL (0x7017EDED) — the raise  [P1 §§4–5; o_signal.cpp]

LCALL, argc 3 or 4. Inputs: *arg1 = type (wide), *arg2 = key2,
*arg3 = code, *arg4 = resume flag (narrow, sign-extended) when
argc>3; entry carry (survives to images only when argc>3 — the
argc≤3 path's WSUB clears it: c_x = argc>3 ? entry_c : 0); the chain;
the gate wide.

Behavior: store the resume flag per §2.2 (BEFORE anything else that
can fail — ordering is normative); record the signal (O.SET semantics
§3.8, laid inline); select a handler: walk establisher records
innermost-out, per record chain_search(type, type>0 ? key2 : 0);
found → dispatch; exhausted → DEF?ON.

Exit A — **handler dispatch** (transfer, the L2→L1 rendezvous
crossing):

| | value |
|---|---|
| pc | handler address (node[+6]) |
| ac0 | type |
| ac1 | **the establisher token** (the registering record's identity; today its frame pointer) |
| ac2 | handler address |
| ac3 | 0x7017EE40 (DISPATCH_RET — the tail's address; the L1 body WRTNs back into L2 through it) |
| c | c_h = (type>0) ? c_x : 0 (helper preamble clears carry for catch-all) |
| ovk/ovr | 1 / 0 |
| wsp / wfp | E+12 / F; frame wide (psr_body<<16)\|0 at [E+12], psr_body = (entry_psr \| 0x8000) & ~0x4000 |

Exit B — **exhaustion**: identical state with ac1 = 0, ac2 =
0x7017EF05 (DEF?ON), and control continuing at DEF?ON (§3.13). This
is an interior L2→L2 hand-off, NOT a crossing (rendezvous ruling);
its onward exits are DEF?ON's.

Memory at either exit (normative image, capture-validated —
authoritative offsets are o_signal.cpp's, see REPORT correction c1):
own WSAVS image [E+2..E+11]; [wsb−0x2A] = flag; O.SET frame word then
XPSHJ wide then dispatch frame word, all at [E+12] (last write wins:
(psr_body<<16)|0); O.SET WSAVS image [E+14..E+22] then key2 backup
overwriting [E+14]; walker WSSVR image [E+24..E+34] (locals
[E+36..E+44] untouched); helper residue — LAST search only —
[E+16..E+26] + scratch [E+28] (none when the chain was empty); the
five wsb-band stores.

**The handler-returned tail** (EE40–EE55; entered when the dispatched
code WRTNs to DISPATCH_RET — an L1→L2 return crossing; runs with
wfp = F of whichever signaling entry raised):
1. flag = wide [wsb−0x2A]; ≠0 → **WRTN: resume exit** — return to the
   raiser's caller with the raiser-entry register image restored
   (ac0/1/2 = entry values, c = entry c, wsp = E−2−2·argc).
2. else if ac0 (as returned by the dispatched code — a handler may
   patch its saved-ac0 slot) > 0 → same WRTN exit ("treat as
   handled").
3. else load ac2 = 0x11618; if type-at-return ≠ −1 → **escalate**:
   re-enter the shared body at EE35 in the SAME frame with (type=−1,
   key2=0, code=0x11618) — one full re-signal, overwriting the same
   image offsets.
4. type == −1: if [wsb−0x3A] (the code O.SET recorded) == 0x11618 →
   **LCALL I.STOP, 0** — L2→L3 descent (the recursion terminator);
   else escalate as in 3.
The loop bound is structural: at most one escalation to ERROR/0x11618
before the I.STOP descent.

Stale-flag edge (normative): shorthand/O.SERROR raises never wrote the
flag, so step 1 reads the previous raise's value (§2.2).

### 3.10 The shorthand entries — O.SCONVE, O.SSUBSC, O.SFIXED, O.SZEROD, O.SOVERF, O.SUNDER (0x7017EE2D/EE27/EE1F/EE17/EE0F/EE07)  [P1 §4; o_signal.cpp]

LCALL, argc 0, WSAVS 0. Fixed inputs (no args, no flag store):

| Entry | type | code |
|---|---|---|
| O.SCONVE | −1 | 0x11611 |
| O.SSUBSC | −1 | 0x11612 |
| O.SFIXED | −2 | 0x11606 |
| O.SZEROD | −5 | 0x11608 |
| O.SOVERF | −3 | 0x11607 |
| O.SUNDER | −4 | 0x11616 |

key2 = 0; c_x = 0 (the joining WSUB clears carry). Behavior/exits =
§3.9 from the shared body onward (dispatch state, tail, escalation),
with the memory image differing only in the absent flag store and
argc 0 (resume WRTN pops no args: wsp = E−2). Live caller: X.CB →
O.SCONVE (the CONVERSION signature). O.SSUBSC has zero callers
anywhere; O.SFIXED/SZEROD/SOVERF/SUNDER's only callers sit in frozen
I.FFALT. All six are registered, fully specified, cold-branch
contract.

### 3.11 O.SERROR (0x7017EE33) — raise ERROR with caller's code  [P1 §4; o_signal.cpp]

LCALL, argc 0, WSAVS 0. Inputs: **entry ac2 = the condition code**;
type = −1, key2 = 0, c_x = 0, no flag store. Behavior/exits = §3.9
from the shared body.

Caller inventory (informational, per A1 — the callers are L1/L3, not
contract surface): 9 static LCALL sites, all heap-corruption /
DERR paths (I.ALLOC/I.FREE defensive raises, DERR.TRP ×2, DERR.USR).
The L1 heap defensive raises are ABORT-class by Layering open-question
(a)'s closure but are wired when next touched, not through this
contract; DERR.TRP is terminal-with-ABORT already (ruling 7).

### 3.12 ?DEFAULT_ERROR_HANDLER (0x7017E3D2) — the installed handler  [P2; lib_error.cpp]

Entered ONLY via XCALL 0,0,[ac2+0] through [B+0x1E] (§3.16). WSAVS 3.
Inputs: ac0 = area (t_area value), ac1 = companion ([B+0x20], 0),
ac2 = its own address, entry c = c_x (?LIB_ERROR's message-copy
carry), [B+1] (the code just stored).

Behavior: read code = [B+1]; build locals (−1, 0, code); push their
EAs; LCALL O?SIGNAL,3 — i.e. **raise O?SIGNAL(type=−1, key2=0,
code=[B+1])** with argc 3 (⇒ resume flag stored as 0: a
?LIB_ERROR-mediated signal is never resumable via the flag).
Boundary state at the interior hand-off (normative because it is
recorded in images and is the staging O?SIGNAL consumes): ac0 = code,
ac1 = −1, ac2 = 0, ac3 = 0x7017E3EF, c = 0 (the WADC/WSUB pair ends
c=0 deterministically), wsp = F+30, wfp = F' = F+16, psr = psr_body.
Exits: O?SIGNAL's (dispatch / exhaustion / its tail). If O?SIGNAL's
resume WRTN returns (to 0x7017E3EF): **WRTN** — return to the XCALL
site (0x7017E3D0) with entry values restored; control is then back in
L1 (?LIB_ERROR's tail WRTNs to the original raiser). Memory: the P2
residue map (T?AREA idiom frames, the local triple, the three EA
pushes, the LCALL frame word) — contract-private storage.

### 3.13 DEF?ON (0x7017EF05) — default handling on exhaustion  [P4 §6; def_on.cpp]

XCALL shape, argw 0, entered from the shared body's exhaustion
dispatch (entry ac2 = 0x7017EF05, ac3 = 0x7017EE40). WSAVS 0x000A.
Inputs: [C+2] = recorded type, [C+4] = key2, [C+6] = code (via
O?AREA), the resume flag narrow [C+0x16], the saved-wfp chain (the
R?SIGNAL walk), argc.

Behavior by recorded type:

| type | action | exit |
|---|---|---|
| > 0, == 2 | LCALL C?INIT(&key2-local) — no-op body — then WRTN | **resume**: return to 0x7017EE40 (the tail, §3.9) with DEF?ON-entry registers restored, wsp = E−2 |
| > 0, == 6 | LCALL P?DEFON(key2, code, type) → resignal O?SIGNAL(−1, key2, code) | P?DEFON/O?SIGNAL exits (§3.14/§3.9); a handler resume unwinds WRTN-by-WRTN back through fd9f → ef26 → the tail |
| > 0, other | same via P?DEFON → O?SIGNAL(6, key2, code) | ditto |
| == −1 | LCALL R?SIGNAL,0 (§3.15); then resume test: bit15 of narrow [C+0x16] SET → WRTN to the tail; CLEAR → **LCALL ?FATAL,0 — L2→L3 descent** (the observed death path) | resume / descent |
| ≤ 0, ≠ −1 (−5..−2) | escalate: O?SIGNAL(−1, key2, code) with locals staged as in P4 §6.1, then the same R?SIGNAL + resume-test tail | O?SIGNAL exits, then resume/descent |

The resume test's MOV.L# is a pure test — NO register or flag is
written (NovaCompute N=1 path; normative nothingness).

Outputs at the resume exit: entry registers/c restored (image),
wsp = E−2, pc = 0x7017EE40, wfp = the signaling entry's F (the
restored saved-wfp). Memory: WSAVS image; local [F+12] = area;
O?AREA's LCALL frame word + patched image; per-branch locals, EA
pushes and LCALL frame words per the P4 §6 footprint maps —
contract-private storage. Ordering obligation: [C+2/4/6] and the flag
are read, never written, by DEF?ON — the record must still hold the
raise's values when exhaustion reaches it.

### 3.14 P?DEFON (0x7017FD7A) — default resignal policy  [P4 §3; p_defon.cpp]

LCALL, argc 3 (sole site), WSAVS 0x0007. Inputs: *arg1 = key2,
*arg2 = code, *arg3 = type (each by reference — key2/code pass
through by POINTER to the resignal).

Behavior: type==2 → LCALL C?INIT(arg1-ptr) (body = WSAVS 0; WRTN — a
no-op with residue) → **WRTN**: return to caller with entry values,
wsp = E−2−6. type==6 → new_type = −1, **c := 1** (WADC 0,0 —
deterministic, recorded in the O?SIGNAL images); else → new_type = 6,
c = entry carry (NLDAI writes no flags). Then local [F+12] = new_type;
push arg2-ptr, arg1-ptr, &local; LCALL O?SIGNAL,3 —
O?SIGNAL(new_type, *key2-ptr, *code-ptr) with staging ac0 = new_type,
ac1/ac2 = P?DEFON entry values, ac3 = 0x7017FD9F, wsp = E+32, wfp = F.
Exits: O?SIGNAL's; on a handler resume the WRTN chain returns to the
DEF?ON caller. Memory: P4 §3.5 maps — contract-private storage.
Cold today (needs an unhandled positive-type condition); every branch
above is contract regardless.

### 3.15 R?SIGNAL / ?ERROR (0x7017EF54)  [P4 §5; r_signal.cpp]

LCALL; argc-polymorphic (?ERROR is the same address — the public
raise alias). WSAVS 0. Inputs by argc: argc≤0 → code = [C+6],
type = [C+2]; argc≥1 → code = *arg1, type = −1 default; argc==2 →
type = *arg2. Then the saved-wfp walk from its own frame.

Exit A — **plain return** (walk reaches a zero link — the live path):
the ENTIRE footprint is the WSAVS image; every register, flag, and
psr bit is restored to entry values; wsp = E−2−2·argc. (The code/type
loads influence nothing on this path — normative uselessness,
capture-confirmed bit-for-bit.)

Exit B — **anomaly** (a saved-wfp link jumps UPWARD, signed): patch
the anomaly frame: saved-ac2 slot := code, saved-ac1 slot := its
original ret|c wide, saved-ac0 slot := type; read the restart vector
[segment | 0x124]:
- vector ≤ 0 → **LCALL ?FATAL — L2→L3 descent**;
- vector > 0 → restart: rewrite the frame's ret slot to [vector]
  (= R.SIGREC via the I.GINIT-installed pair at 0x7017EB63), CLEAR
  [wsb−0x40], wfp := anomaly frame, WRTN — **transfer** to the
  restart pc with ac0 = type, ac1 = original ret|c, ac2 = code and
  the chain head zeroed.

**FLAGGED-AMBIGUOUS** (with §3.3 shape 3, same flag): the upward-link
condition reads as "our chain is broken" (defensive ⇒ ABORT-INTENDED)
but the installed restart vector is a deliberate boot-time mechanism
(cross-task restart?). Per A1, flagged for review, not decided; the
conforming behavior until adjudicated is bit-faithful emulation of
the branch (current implementation falls back whole, decided on pure
reads before the first store).

### 3.16 The indirect entry — XCALL through [B+0x1E]  [P2; scan 5]

The system's only data-held L1→L2 entry: ?LIB_ERROR (L1) ends with
XCALL 0,0,[ac2+0] where ac2 = [B+0x1E]. Because [B+0x1E] has exactly
one writer (the lazy install, §2.3), the target is always
?DEFAULT_ERROR_HANDLER; the crossing's staging is §3.12's input row.
Contract: a conforming L2 must be enterable through this pointer
protocol unchanged (the pointer lives in SHARED-PROTOCOL memory; L1
owns the install; the value 0x7017E3D2 is an address-as-landmark —
the ENTRY at that address is the contract object, not its bytes).

## 4. The untranslated L2 surface (scan 5 list 3)

Ten census symbols are not in the registration table. Per the
governing principle each is specified or explicitly excluded:

| Symbol | Disposition |
|---|---|
| I.WPROLO, I.DISPLA | EXCLUDED — Register E1/E2 |
| R.GOTO | EXCLUDED — Register E3 (body = WBR alias into I.GOTO+2; the feared clone/master asymmetry is moot) |
| O.SIGNAL | EXCLUDED — Register E4 (WSSVS; ac2=0x11601; joins the shared body at EE38 — derivation exists, P1 §4) |
| R.SIGREC | EXCLUDED — Register E5 (WITH the dynamic-path caveat: reachable via the restart vector, §3.15 exit B; derivation P1 §1) |
| R.SIGNAL | EXCLUDED — Register E6 (WSAVS 0 + WBR into the R?SIGNAL walk; zero references) |
| I.SFALT, I.SFCON | FROZEN — Layering ruling 6 (never installed; vector-gated at 0x1BB; the wsp==wsl boot-probe ruling deliberately replaces the signal-flavored path; fix-if-it-fires) |
| I.FFALT | FROZEN — the emulator's FP unit throws; can never dispatch; tripwire at the throw site |
| O.SEARCH | transitively FROZEN (sole caller I.FFALT); derivation exists (P1 §4) should that change |

Frozen entries are not exclusions: they are governed by ruling 6's
fix-if-it-fires convention with existing tripwires (every ST symbol
carries a logging stub, so a first-ever dispatch is loud).

## 5. Crossing inventory (normative sync surface)

From scan 5 + the native return expressions. L1→L2: the 20 registered
entries (via their §1 mechanisms) + the §3.16 indirect entry + the
L1→L2 RETURN crossing (a dispatched handler's WRTN to DISPATCH_RET
0x7017EE40, and — if O?SIGNAL's resume chain runs under
?DEFAULT_ERROR_HANDLER — the WRTN to 0x7017E3EF). L2→L1: the handler
dispatch transfer (§3.9 exit A), I.GOTO's label transfer and
I.PROLOG's continuation transfer, I.EPILOG's caller-return transfer,
every WRTN return to an L1 call site, and O?SIGNAL/DEF?ON resume
returns (which land in L2's own tail first — interior — then WRTN to
L1). L2→L3 descents: DEF?ON → ?FATAL; R?SIGNAL → ?FATAL; the
O?SIGNAL tail → I.STOP; (frozen family → ?FATAL/syscalls). The named
return-pc constants inside the wrappers (A_RET_*, OSET_*, DISPATCH_RET
et al.) are L2-internal composite boundaries, not crossings —
EXCEPT DISPATCH_RET's role as the L1→L2 return-crossing address,
which is contract (the dispatch hands it to L1 in ac3).

## 6. Ordering and persistence obligations (cross-entry)

- The chain (§2.1) persists across calls; its net mutation per entry
  is exactly the per-entry state obligation. No other entry mutates
  it.
- The recorded signal [C+2/4/6] persists from O.SET's stores (the
  raise) through DEF?ON's reads (exhaustion) and the tail's EE4E read
  (escalation test) — nothing between them may clobber it. A nested
  raise (escalation, resignal) overwrites it — by design; the values
  are per-raise, last-raise-wins.
- The resume flag: per-raise via O?SIGNAL only; sticky through
  shorthand raises (§2.2).
- The B record: L1-written, [B+1] read by §3.12 within the same
  signal; L2 never writes it.
- wsp discipline: the only entries with a nonzero net wsp effect at a
  RETURN exit are the frame-word pops (−2−2·argc, uniform), I.PROLOG
  (+4), and O.ON's allocate (+8). I.GOTO's transfer sets wsp to the
  snapshot; I.EPILOG's to the caller's caller frame level. Everything
  else restores entry wsp before the pop.

## 7. VALIDATION REGIME

- **The pair gate is the enforcement mechanism.** A conforming
  implementation is A/B'd against the bit-faithful native L2 under
  lockstep: the master runs the original bytes; the clone runs the
  candidate; at every rendezvous the harness compares pc and the FULL
  register file.
- **A rendezvous IS an L1↔L2 crossing (§5) — and nothing else.**
  Interior L2 structure (which routines exist, who calls whom, every
  A_RET_* composite boundary) is invisible implementation. One signal
  produces SEVERAL rendezvous: raise entry (L1→L2), handler dispatch
  (L2→L1), the handler's I.GOTO (L1→L2), the unwind landing (L2→L1),
  plus any DISPATCH_RET return crossing. Sync at every one; at none
  in between.
- **Harness consequence (named, per SESSION_REPORT §3)**: today every
  registered entry breaks a batch, so interior entries (O.SET from
  the shared body, P?DEFON/R?SIGNAL/O?AREA from DEF?ON, T?AREA from
  L2 callers) are pairing events. Once the stack-free L2 lands, the
  batch/ordinal accounting must count only L1↔L2 crossings — the
  break sites in Machine::run_steps (rt_sync entry blocks) and the
  span accounting in compare_pair must key on layer transition, not
  entry address (T?AREA is a crossing when ?LIB_ERROR calls it and
  interior when ?DEFAULT_ERROR_HANDLER does) — or pairing goes
  structurally asymmetric before a single register is compared.
  Phase 2 inherits this definition; mechanism sketch in
  NativeDesign.md §7.
  **RESOLVED (Aug 13 2026)**: implemented ahead of Phase 2, against
  the bit-faithful L2, replacing the old checker outright — see
  docs/CrossingsChecker.md (as-built mechanism, characterization,
  recalibration-gate evidence) and docs/CheckerHistory.md. One
  refinement to the wording above discovered at Step 0: "today every
  registered entry breaks a batch" was true only latently — with all
  live L2 translated, interior entries were already absorbed by
  composite spans and pending machinery in every live path (0 entry
  pairs at any L2 symbol in a measured full session); the checker
  change made that invisibility a layer-keyed RULE and added the
  entry-side crossing rendezvous.
- **Exit-register fidelity (ruling, SESSION_REPORT §4)**: the Outputs
  tables in §3 are NORMATIVE for every register and flag at every
  exit, INCLUDING apparent scratch/residue (I.PROLOG's ac1/ac2 walk
  values, the c=1 borrow, the dispatch c_h, the WRTN-restored
  images). An implementation that computes results differently must
  still stage the same exit-register image. No "contract-private
  register" concept exists.
- **The only narrowed compare is L2-private STORAGE footprints**,
  licensed entry-by-entry by the Exclusions Register (E7–E9). With
  that license, the footprint-capture protocol (QUEST_CAPTURE
  NATIVE-vs-RETURN diffs over private residue) is RETIRED for
  conforming implementations; captures remain the tool for
  bit-faithful work.
- **ABORT-INTENDED is a distinguished THIRD result class (ruling
  A3)**: an ABORT-INTENDED branch firing on EITHER side terminates
  the A/B run as neither pass nor compare-failure, and mandates
  investigation before further runs — the branch firing at all means
  corrupted state was detected: the A/B just found something real (a
  game bug faithfully reproduced, or an implementation bug that
  corrupted the chain). Mechanically this composes with built
  machinery: the conforming implementation's branch calls
  abort_world(save=false); the `aborting` flag silences the checker;
  the world stops with the named reason — no divergence spam, one
  banner.
- **PRIVATE-with-L3-observer** (ratified): L3 observers of C-record
  cells run master-only, against authentic cells that exist by
  construction. Revisit only if an ABORT-INTENDED path ever routes
  live into clone-side ?FATAL (it must not — that is what ABORT
  prevents).

## 8. STACK-SURGERY clause

Non-local wsp/wfp manipulation — I.PROLOG's snapshot, O.ON's frame
extension, I.GOTO's cut and the landing stub's STASP, I.EPILOG's
return-from-caller — is **L2's exclusive right** (Layering's law).
The contract's statement of what a stack-free implementation owes:

To **the clone's own stack** (which, in the staged M3b design, still
exists and carries L1 frames/args/locals): exactly the normative wsp/
wfp effects of §3 and §6 — the reservations, the cut, the snapshot
restore — because L1 code lives on that stack and the full
register-file compare sees wsp at every rendezvous. The CONTENTS of
L2's reservations are private (Register E7–E9).

To **the MASTER's stack**: nothing, post-detach — and here is the
subtle part, stated carefully. The master's stack continues to exist
and to be walked by L3 (?FATAL's traceback, P?SNAP, I?LINE — all
master-only). One might think the stack-free clone therefore owes the
master's L3 a bit-faithful stack image. It does not, for a reason of
construction rather than of policy: **the master never runs the
stack-free implementation.** In the A/B design the master emulates
the original bytes — its L2 IS the bit-faithful one — so every frame,
chain node, and residue word L3 will ever walk is laid by the
original code on the engine where L3 runs. The clone detaches at the
L2→L3 crossing and executes no L3 instruction; its re-hosted chain
and retired residue are visible to nothing that outlives the detach.
The contract therefore imposes no L3-facing obligation on a
conforming implementation — not because L3's needs were waived, but
because the A/B design routes every L3 need to an engine whose stack
is authentic BY that design. (Corollary: if the deployment model ever
changes so that L3 could run against a stack-free L2 — e.g. master
retired, clone standalone — this clause is the first thing to
reopen; §9 Q4.)

## 9. OPEN QUESTIONS (flagged loudly)

- **Q1 — I.GOTO shape-3 / R?SIGNAL anomaly classification** (§3.3,
  §3.15; §12 flag 3): defensive (⇒ ABORT-INTENDED) or designed
  cross-task restart (⇒ full signal contract)? The restart vector is
  deliberately installed at boot, which argues "designed"; nothing
  has ever executed it. Needs a ruling; until then both branches are
  specified as bit-faithful-emulate.
  **RESOLVED (Aug 13 2026)**: REVIEW's bit-faithful-emulate ruling
  vetoed by the user → ABORT-INTENDED. See the THIRD ADDENDUM.
- **Q2 — per-frame slot privacy**: scan 1 covered the wsb static
  band; the condition-frame slots ([frame+2..+0xA], display words,
  on-stack nodes) rest on the provenance argument + lockstep
  backstop, not on an enumeration (§12 flag 2). Accept, or fund a
  frame-slot scan?
- **Q3 — the O?SIGNAL tail's ac0 semantics** ("handler returned
  ac0>0 ⇒ treat as handled", §3.9 step 2): no Quest handler returns
  (all 24 non-fatal bodies I.GOTO out), so the value observed at
  EE45 has never been a deliberate handler result. The contract
  specifies the mechanism (the value in ac0 after the return
  crossing); whether any PL/1 idiom ever patches it is unknown and
  does not affect conformance.
- **Q4 — standalone-clone future**: §8's corollary. Out of scope for
  M3b/M4 (the master is a permanent participant by design); recorded
  so the assumption is never silent.
- **Q5 — ?ERROR's public-alias arity**: argc 1/2 paths (§3.15) have
  zero callers in Quest but are the alias's documented shape;
  specified as contract (cold branches). If an external caller ever
  appears (it cannot, in this closed binary), the arg-cell reads are
  the spec.

## 10. Deliberate Exclusions Register

Each entry: what is excluded / evidence / risk if wrong.

- **E1 I.WPROLO (0x7017E750, wide-prolog variant)** — no
  implementation required. Evidence: zero references of any kind —
  code, data/vector words in every memory_data dump, fall-through —
  in either image (scan 5 tally). Risk: a dynamic caller would reach
  the registered logging stub — loud, then implement.
- **E2 I.DISPLA (0x7017E766, display-chain walker)** — same evidence
  class and tripwire as E1.
- **E3 R.GOTO (0x7017EC7B)** — alias head (WBR → I.GOTO+2), zero
  references. Risk profile as E1; additionally harmless if wrong
  (the alias body is I.GOTO's).
- **E4 O.SIGNAL (0x7017EDE7)** — zero callers anywhere (P1 extent
  table). Full derivation exists (WSSVS; code 0x11601; joins EE38)
  should the evidence ever break.
- **E5 R.SIGREC (0x7017EE02)** — zero STATIC references, but
  **dynamically reachable**: it is the restart-vector target
  ([0x70001... 0x124] → 0x7017EB63 word0), reached only through
  §3.15 exit B / §3.3 shape 3 — themselves never executed and
  Q1-flagged. Evidence: the vector path has never fired in any
  recorded session; the anomaly branches guard it. Risk: if the
  anomaly ever fires with vector>0, the conforming behavior is
  already "bit-faithful emulate the branch" (Q1 interim), which
  executes the real R.SIGREC — the exclusion cannot be silently
  wrong. (This entry CORRECTS scan 5's "none" reference count; see
  REPORT.)
- **E6 R.SIGNAL (0x7017EF51)** — alt entry (WSAVS 0 + WBR into the
  R?SIGNAL walk), zero references. As E1.
- **E7 C-record walker outputs [wsb−0x36]/[wsb−0x38]** — a
  conforming clone need not host these VALUES in real memory.
  Evidence: scan 1/2 — writer O.SET only; zero L1 AND zero L2
  readers; sole observers ?FATAL (L3, master-only). Risk: a
  computed-address reader invisible to the scans would surface as a
  lockstep divergence the first time it read garbage — loud, not
  latent (the ratified backstop argument).
- **E8 L2 residue images (frame images, helper residue, untouched
  locals, XPEF/LCALL frame words) inside L2's stack reservations** —
  footprint compare retired; the reservations' wsp effects stay
  normative (§2.1). Evidence: scan 1 (no L1 access to the band);
  provenance argument for frame-hosted residue (Q2). Risk: as E7 —
  divergence-loud.
- **E9 I.PROLOG's two pushed wides' contents and O.ON's node-word
  contents on the clone** — same license and risk as E8; the head
  slot / node data are re-hosted (NativeDesign).
- **E10 The remaining C-record cells' VALUES in clone memory**
  ([wsb−0x40], −0x3E, −0x3C, −0x3A, −0x2A) — re-hosted natively;
  addresses remain landmarks (master untouched; I.GINIT identical on
  both engines). Evidence: scan 1 zero-L1-access + the L2 reader set
  being exactly the re-hosted implementation. Risk: as E7.
- (Frozen entries are NOT register entries — ruling 6 governs them;
  §4.)

Empty additions welcome; corner-cutting is argued here or not at all.

## 11. Conformance summary (the three sentences that matter)

1. At every L1↔L2 crossing, a conforming implementation presents the
   exact §3 machine state — full register file, flags, psr, wsp/wfp —
   and the exact SHARED-PROTOCOL and CONTRACT-OBSERVABLE memory.
2. Between crossings it may do anything at all, except write
   L1-observable state or skip a normative wsp reservation.
3. Where §3 says ABORT-INTENDED, it aborts the world instead of
   signaling; where §3 says DEAD-GUARDED, it re-checks the gate and
   falls back; everything else it reproduces, cold branches included,
   unless a Register entry says otherwise.

## 12. Review flags carried into this contract

1. PRIVATE-with-L3-observer — RATIFIED (SESSION_REPORT scan-2 flag 1),
   restated in §7 with its revisit condition.
2. Frame-slot privacy rests on provenance + backstop, not a scan
   (§9 Q2) — NEW flag from this drafting.
3. I.GOTO shape 3 / R?SIGNAL anomaly: AMBIGUOUS under A1's classifier
   (§9 Q1) — NEW flag; interim behavior specified.
4. Project 1 correction (scan-2 flag 2: pointer-based readers of
   [wsb−0x3C/−0x38/−0x36]) — folded into §2.2's accessor lists.
5. The prompt's "?LIB_ERROR touches [+0x16]" seed error (scan-2
   flag 3) — the +0x16 access is DEF?ON's; folded into §2.2/§3.13.


## THIRD ADDENDUM (Aug 13 2026) — ABORT-INTENDED inventory amendment

Two sites JOIN the ABORT-INTENDED class (definitions and A1–A3
composition unchanged). Both were promised or pended by the Phase-1
review; recorded here so the contract carries no IOUs. Conforming
behavior: abort_world(save=false), the detecting condition, offending
values, and pc named in the banner; the mv_error_handler (the living
attic, Project 8) is the forensic reference if either ever fires.

1. **I.EPILOG mismatched-frame pop** (§3.2): wfp is not the innermost
   establisher at disestablish. Bit-faithful behavior was the
   unconditional head-write [wsb−0x40] = [wfp+8] (silently tolerant
   of the impossible). Adjudicated F3 (REVIEW: A1 applied — broken
   codegen bracketing = defensive), re-confirmed by user ruling
   Aug 13 2026: "the native version gets an extra check on PL/1
   correctness."
2. **I.GOTO shape 3 / R?SIGNAL upward-link anomaly** (§3.3 shape 3,
   §3.15 exit B, §12 flag 3): the non-descending/restart shape.
   REVIEW Q1's bit-faithful-emulate-permanently ruling is VETOED by
   the user (Aug 13 2026, the veto the ruling reserved): post-Stage-C
   there is no coherent stack-hosted state to fall back to, and an
   abort IS the "reclassify only if it ever fires" mechanism, with
   full state captured at the moment of firing. The deliberately-
   installed restart vector's design-intent argument is preserved in
   the record; if the mechanism ever fires, reopen with the abort's
   evidence in hand.

§9 Q1 is thereby RESOLVED. The H7 bad-token injection (Project 8)
exercises the third result class end to end; these two sites remain
never-fired by design.
