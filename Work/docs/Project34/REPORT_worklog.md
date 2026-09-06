# Project 34 — worklog (Sep 6 2026)

Branch `p34-readable` from main 4837d5c; a docs + tool branch.  The ir 5
book was read out of `origin/p31-located-strings` @
b4384b177bed8229c50b43235b2c08d62a954fd8 (sha256 281fd501…; P31 not yet
merged when this ran — the header of every rendering names the book it
read).  Nothing under Work/c_src except tools/readable.py was touched.

## Sequence

1. Read METHOD.md, RESEARCH.md, PICK_X_Y_reading.md, IR.md §2–§6 (ir 5),
   quest.addrbook layout, quest.symbols, RTConventions.md, the WRTN arm
   (EagleStack.cpp:471–495).  Confirmed the uploaded Work archive is ir 4
   (stale) and main has no ir 5 book; took the book from P31's branch.
2. Wrote tools/readable.py: tokenizer + recursive-descent parser for the
   ir 5 grammar (statements, string statements, rt_call/call/goto/assert),
   loaders (book header, addrbook incl. stacked `#` entries, symbols,
   blocks.split edges), routine attribution by reachability from each
   addrbook entry.
3. Rewrites in the order they are PROVABLE, not the order listed in the
   prompt: register folding first (structural, in-block, with the
   symbolic env), then naming at render time (frame / static / record),
   then literal folding (needs the folded routine + liveness), then the
   sketch.  Each is a switch and reports.
4. PICK_X_Y as the calibration: after fp tracking (WSAVS sets fp; the
   string library clobbers ac3), reaching-shape analysis for reused
   locals, and the env for register-held record pointers, the rendering
   matches the hand reading line for line.
5. DIED: player-record varying assignments, `call UPDATE_SCREENS(…)`
   folded from the book-mode slot stores; the WMSP/WCMV/STASP/LJSR area
   (P32/P33 territory) stays as instruction lines.
6. Cross-block register liveness for the strict-surface rule (RT
   conventions + WRTN reload as declared beliefs); adjacent effectful
   nesting (`add(cvwn(div(…)))`).
7. Literal folding: may-live fixpoint; refusals categorised by the shape
   of the first reader (census §4).  Time-boxed per the user: the
   remaining 428 are described, not chased.
8. Attribution bug found through the frame census (`local_534?` in an
   18-word frame): the blocks-file edge regex ignored `j … n …` lines,
   and I.GOTO's fall-through edge walked ON-unit bodies into their
   parents.  Fixed; census §1 records the finding (ON-units are inlined
   separate frames).
9. Declared beliefs made explicit as switches: heapdisjoint, argdisjoint,
   envinherit (joins included); fp-preserving LJSR helpers.
10. Rendered PICK_X_Y, DIED, DISPLAY_INVENTORY, ATTACK, LOGON (+ its two
    ON-units), UPDATE_SCREENS, ALLY_PLAYER (+ unit), and the two routines
    with the most distinct static references (OP_EDIT 29, DISPLAY_INVENTORY
    19); wrote Census.md.

## Runtime

15.5 s whole book (parse 3 s; the rest is the per-routine analyses:
analyse_fp with env inheritance to a fixpoint, reaching shapes, liveness
scans).  Flagged: over the 10 s bar.  Profiled once; the obvious next
step is caching per-statement classifications instead of re-walking the
symbolic env per liveness query.  Not done — a research tool.

## Loud failures / refusals kept

- Any unparsable IR line aborts the load (no silent skips).
- 24 blocks unreached from any entry after the edge fix (fallback
  attribution, counted in the census header).
- 2 frame references beyond the addrbook frame, 1 beyond argc, 53 odd
  offsets (byte-form `bp(ac3, d)` with odd d is handled; the 53 are
  `wp(ac3, odd)` reads of the ac3|c word's neighbour) — rendered with a
  `?` and listed.

## Not done

- Census "compared-with constants" is empty (the comparison sits behind
  the register fold) — the tool needs to look through the fold.
- Bit-field decomposition (`lsh(16*i*stride + c, -4)`), 1,152 sites.
- P32/P33 instruction lines (WCMV append chains, WMSP temps) are opaque
  to every rewrite here, by construction.

## Commits (p34-readable)

- d924f86 WIP: tools/readable.py
- 46c1951 WIP: attribution fix, env inheritance at joins, static arrays,
  frame arrays, literal census categories
- (final) renderings, Census.md, this worklog
