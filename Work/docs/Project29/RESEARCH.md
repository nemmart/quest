# Project 29 — strings, Phase A: RESEARCH ONLY (idiom classification)

This is a research session, not an implementation session. The
deliverable is a taxonomy and an artifact set that a later DESIGN
session (StringsDesign.md, the MathDesign analogue) will rule on.
Nothing here touches lower.py, IRExec, IR.md, the emulator, or any
artifact the emulator reads. No battery. Python only (all inputs are
text; quest.mem is a text dump — literals decode in Python, verified
Sep 5: 7015BD9B, 27 bytes → 'Cannot set shared partition').

Hi Claude! Read docs/METHOD.md first. Context: docs/IR.md (ir 3, or ir
4 if P28 has landed — check CURRENT_STATE), docs/Project28/Census.md
(the census style to match; rt_sites.py the tool style), docs/
HWFindings_Sep5.md (manual-vs-emulator findings format), docs/
Provenance.md (verify your tree FIRST). TREE VINTAGE: state the main
commit you were handed. P28 (rt_call) may be in flight on its own
branch — you share no files with it.

## Why

After P28 the IR embeds are ≈2,300, of which the string / dynamic-
storage family is ≈1,770: WCMV 1,637, WMSP 57, WCMP 40, STASP 19,
WBLM 12, 3 pass-by-reference WPSH temps. These are PL/I character
operations (assignment with blank padding, concatenation into stack
temporaries, comparison, block moves) and PL/I temporary allocation.
They are the last big design job before the IR is "everything but
frames." We are not going to write that design from a prompt; we are
going to classify the ground first.

What is already known (Sep 5 planning session):
- WCMV (EagleSpecial.cpp:42–72): ac0 = dst byte count, ac1 = src byte
  count, ac2/ac3 = dst/src BYTE pointers; copies min, pads dst with
  spaces when src is exhausted; negative counts run backwards; c = 1
  iff src was truncated; leaves the four ACs at their end values;
  throws on segment crossing.
- The dominant WCMV preamble (504/1,637): `NLDAI n,1 / WMOV 1,0 /
  XNSTA 0,[ac3+d] / XLEFB 2,[dst] / XLEFB 3,[pc+…] / WCMV` = a PL/I
  varying-string assignment from a literal (length word stored, then
  the copy). 106 distinct 5-instruction preambles in total.
- WMSP (EagleStack.cpp:567): wsp += 2*ac, with upper/lower stack-limit
  faults and the ruling-8 zero-claim; typically preceded by `WADI /
  WMOVR / WMOVR` (round a byte length up to words) — PL/I temporary
  allocation for concatenation results. STASP restores wsp.

## Deliverables (all under docs/Project29/ and tools/)

1. **tools/string_sites.py** — text-only over quest.dis, quest.blocks.
   split, quest.tags, quest.mem, quest.symbols. For every WCMV / WCMP /
   WBLM / WMSP / STASP / pass-by-ref WPSH site: block, the setup window
   (the instructions that produce ac0–ac3 — walk back through the
   block, stop at the previous string op or block start; note windows
   that cross a block boundary), and for each of the four operands its
   PROVENANCE class:
     literal      — XLEFB/LLEFB of a pc-relative or absolute constant
                    into the code/data segment (decode the text)
     frame        — [ac3+d] local, or its length word
     static       — absolute address in the data segment
     temp         — a WMSP-claimed stack area (tie to its claim)
     computed     — anything else (say what)
   Runtime under 10 s; report the number.
2. **quest.strings** — the literal table: address (word:byte), length as
   used at the site(s), decoded text (escaped), the sites that use it.
   Provenance header with quest.mem's sha256. Literals used with more
   than one length are a finding (SUBSTR or a shared tail).
3. **Census.md** — the counts, and the IDIOM CATALOGUE: collapse the
   preambles into PL/I constructs with counts and one cited example pc
   each. Expected buckets (confirm or correct): `s = 'literal'`;
   `s = t` (same/different lengths — padding vs truncation, which is
   where c matters); `s = a || b` into a temp then out; `SUBSTR(s,i,n)`
   on either side; compare (WCMP) and what consumes its result;
   WBLM word-block moves (records?); the WMSP/STASP claim/release
   brackets with their lifetimes (paired? nested? released before the
   block ends?); the 3 WPSH pass-by-reference temps at 70169B77.
   Everything that fits no bucket, listed, with the window.
4. **Destinations** (the user's cut): for each WCMV/WBLM, where does the
   written data GO?  (a) a buffer that feeds a `?WRITE_SCREEN` /
   `?WRITE` in the same or a following block (ephemeral — a wrong byte
   is a wrong message, visible); (b) game state — static/record fields,
   names, anything that persists or is written to a file (silent until
   read back); (c) a temp consumed by a WCMP or another WCMV only.
   Counts per class; the (b) list in full — those are the fidelity-
   critical sites.
5. **Manual vs emulator**: if the DG pages for WCMV / WCMP / WBLM / WMSP
   are available (ask the user — they have the manual), diff them
   against EagleSpecial.cpp / EagleStack.cpp in the HWFindings table
   format. Direction semantics, padding character, c on truncation,
   what WCMP leaves in the ACs, and the WMSP limit checks are the
   points to read closely. A disagreement is a FINDING; do not touch
   the emulator.
6. **Open questions for the design session** — write them as questions,
   not decisions. At minimum: (i) these are the first IR statements
   that write MEMORY in bulk; lockstep compares registers + wsp + block
   ordinal, not memory — what gate proves a `wcmv(...)` wrote the same
   bytes (footprint capture? a rendezvous range compare? the (a)/(b)
   destination classes read back?); (ii) `wmsp` is a wsp WRITE — how
   does it fit the "no wsp arithmetic outside helpers" rule; (iii) byte-
   pointer expressions the grammar has (bp/0xW:b) vs what the sites
   need (byte pointer + computed offset); (iv) is the varying-string
   length word always written by the same idiom, so `s = 'lit'` can be
   one IR statement; (v) anything the census made you want to ask.

## Boundaries — BINDING

- No changes outside tools/string_sites.py and docs/Project29/. No
  emulator, lower.py, IRExec, IR.md, synclist, artifacts the emulator
  reads. No battery, no runner task.
- Semantics from the emulator source (cited) and the manual (via the
  user). Classifications you are not sure of go in "unclassified" with
  the window shown, never rounded into a bucket.
- Stop when the deliverables are written and committed on branch
  p29-strings-research; report with the counts table, the catalogue
  summary, the (b) destination count, and the question list. Ask the
  user for manual pages when you reach item 5.
