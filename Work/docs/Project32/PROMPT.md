# Project 32 — append chains: the frame-scratch concatenation machinery

GOAL: lower the pieces of every concatenation that the compiler builds in
a frame scratch buffer (StringsDesign §5.1) — first pieces (COPY-STR/
LIT-EXACT 378), continuation pieces (CONCAT-PIECE 349), CALLRESULT
pieces (112), SUBSTR pieces (23), tail splits (17), and the copy-outs
P31 refused (P32-COPYOUT 86, P32-SUBSTR 15, P32-CAPACITY 9,
COMPUTED-OPERAND 4, OPERAND-DEAD 2, P32-RESIDUE/CALLRESULT 2) — as
located-string statements over the P30 library, with the residues of
§3. Still NO arena, NO checker change: every byte lands where the
master's does. Expected: embeds 1,670 → ≈700 (book); the remaining
WCMVs after P32 are the 57-claim `p@b` groups (P33-B) and whatever the
census refuses with a reason.

Hi Claude! Solo implementation session; the user reviews at the plan
gate and at the landing. Read docs/METHOD.md first. Design of record:
**docs/Project29/StringsDesign.md** (§2 the type and statements, §2.4
byte semantics incl. the SIGNED length word (F-B1), §3 residues, §4
timing, §5.1 the scratch-chain rule, §7 the idiom table). The ground:
**docs/Project31/Census.md** (§2's refusal buckets are your site list;
§2.1 the emitter's rendering rules; §1.2 the tool defects fixed; §10
Phase B findings), docs/Project31/p31.ledger + strings.ledger,
tools/string_sites.py (extend; it now carries the `[@0xW:b, "text"]`
literal form and `R[...]` rendering), docs/IR.md **ir 5 §5.8** (the
grammar you extend — version bump only if a production is added; state
it), docs/Project30/REPORT.md (the library: `copy` is the primitive;
`append` is copy at the caller's cursor, no separate entry),
docs/Provenance.md (verify FIRST). TREE VINTAGE: main after P31
(044 GREEN, merged) — state the commit.

**Shared-file rule**: P33-A may still be open on Lockstep/Mapper/
EagleStack hooks/frames.cpp; you do not touch those. Yours: lower.py,
IRExec (the executor), IR.md, string_sites.py, artifacts, docs/
Project32/.

## What "append" is, precisely (so the census can decide the form)

The master's chain: piece 1 `WCMV` with `dst = scratch base bp`,
`dst_count = src_count = len(piece)` (COPY-EXACT: no padding); piece k
`WCMV` with `dst = ac2` — the END POINTER RESIDUE of piece k−1, NOT
reloaded — `dst_count = src_count = len(piece_k)`. So a continuation
piece is already expressible with P31's register-form destination:
`[@ac2, len(piece)] = piece`. The design's `append([@a, n], piece)` is
the readable spelling of the same thing. Part 1 decides which the
emitter writes (LEAN: the register form — it is the ground; `append`
is a readable-layer rewrite, exactly like `fill()`), and what new count
expressions the grammar needs: `len(piece)`, `len(a) + len(b) + c`
(copy-outs), `min(len(x), k)` (SUBSTR/capacity), `ac0` (CALLRESULT),
`ac1` (tail split: the residue as the next count). None of these is a
memory write or a wsp write; all are word expressions the ir 4 grammar
can already spell EXCEPT `len()`, which ir 5 added.

## Part 1 — census + grammar (plan gate)

1. **Site list** (`string_sites.py --p32`): every WCMV not lowered by
   P31 and not in a `p@b` claim group (those are P33-B; list them
   separately with their block so the two projects' populations are
   disjoint and sum to the census). Per site: the chain it belongs to
   (scratch buffer address, capacity from the frame layout, piece
   index), the four operands rendered as IR expressions, the count
   expression and its provenance, EMIT/REFUSE with reason. Reproduce
   the Census §3 idiom counts; runtime line.
2. **Chains**: enumerate every scratch chain (buffer → pieces →
   copy-out → consumer), including the conditional-piece shapes
   (pieces on different blocks — the `IF … MSG = MSG ‖ …` case); verify
   per chain that the copy-out's count equals the chain's appended
   total (the compiler's `len(a)+len(b)+c` sum matches the pieces) — a
   mismatch is a FINDING. State how many chains, how many cross blocks,
   the longest.
3. **CALLRESULT**: the `?UNSIGNED_TO_CHAR` piece is `[@fp+k, ac0]` in
   the block AFTER the rt_call — confirm ac0 is unmodified between the
   return and the WCMV at all 112 sites (or list the exceptions).
4. **SUBSTR**: the `substr(piece, i, n)` cases and the "length of a
   different string" cases (`SUBSTR(x, 1, len(y)) = y`); grammar form
   for a byte offset that is an expression (`bp + expr`) — bring the
   concrete forms to the gate.
5. **Tail splits and the t-place cases** (COMPUTED-OPERAND 4,
   OPERAND-DEAD 2, P32-RESIDUE 1): the count or an address comes from
   a residue or from an effectful expression the grammar keeps at
   statement root; propose the t-place spelling (`t0 = …; [@t0, …] =
   …`) per site.
6. **Grammar draft**: the count/address expressions added (§ above),
   whether `append` is emitted or left to the readable layer, the
   refuse list, and the ir version statement.
7. **Residue plan** (§3): the copy residues are pure; every statement
   calls `residues_after_copy`; the tail-split site consumes ac1 from
   the previous statement's residue — confirm the ordering is preserved
   in the emitted text.
8. **Liveness + battery plan**: which chains the standing legs execute
   (the screen-message chains will be live; HELP's 3-piece chain is a
   named check); task 046 = 040's 15 legs + verdict lines (sites
   lowered by kind, chains complete, embeds, copy-out totals verified
   at first execution). Landing bar: 15/15, 0 div, embeds ≤ prediction.

STOP AND REPORT at the plan gate.

## Part 2 — implementation

lower.py `--strings-slice 4..6` (first pieces + continuations /
copy-outs + capacity + SUBSTR / CALLRESULT + tail splits + t-place
sites); slice 3 reproduces P31's artifacts byte for byte; IRExec for
any new expression forms; IR.md; artifacts + Provenance; K=1 book +
stock gates per slice; task 046.

## Boundaries — BINDING

1. Scope = scratch-chain pieces and their copy-outs. NOT `p@b` groups
   (P33-B), NOT any checker/Mapper/hook file, NOT string semantics in
   IRExec (the library only).
2. Part 1 before Part 2. Refuse-don't-guess; every refusal listed with
   its window and bucket.
3. The strict surface is untouched: residues on every statement; the
   chain's bytes at the master's addresses at the master's times (§4).
4. Design-vs-reality: STOP AND REPORT — a chain whose copy-out total
   does not equal its pieces; a CALLRESULT piece whose ac0 is clobbered
   before use; a conditional chain whose blocks the emitter cannot
   order; a residue the §3 table gets wrong.
5. Deliverables: the extended tool + ledger, docs/Project32/{Census,
   REPORT,REPORT_worklog}.md, emitter + executor + IR.md, artifacts +
   Provenance, **task 046 committed on MAIN** (the runner polls main's
   tasks/, not branches), CURRENT_STATE/NextSession, TREE VINTAGE, tool
   runtimes.
