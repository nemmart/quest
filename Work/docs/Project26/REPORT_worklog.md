# P26 worklog (Sep 5 2026)

1. Read METHOD, PROMPT, IR.md, MathDesign, P25 ByteEA/REPORT/PROMPT,
   the emulator sources for every mnemonic in the embed list
   (EagleCompute/EagleInstruction/EagleStack/EagleGeneral/NovaCompute)
   and the Java twins for the §3 diff. Disassembled.tgz was missing
   from the first upload — flagged, re-uploaded, hashes matched the
   book's provenance (vintage confirmed).
2. Census from quest.ir2.book: 27,600 embeds bucketed to the mnemonic
   (Census.md §1, sums checked by script). Verified structurally
   against blocks.split: all 7,030 skip-class embeds are block
   terminators, all skip blocks list [no-skip, skip] ascending
   (6,822/6,822), XNDO lists [fall, target], XJMP lists a spurious
   fall-through (finding). All 23 borrow brackets sit in one block
   each. The 11 non-borrow WPSH embeds classified (8 rt-call, 3
   pass-by-ref temps).
3. Nova shapes censused (1,043: 1,000 no-load tests / 43→67 loads);
   bit-op X/Y shapes (22 of 624 are XX==YY, base 0).
4. Plan-gate report: Census.md draft (scratch), R1–R10 presented.
   User rulings: all recommendations accepted; R9 ash/lsh only; bar
   ≤ 8,600; go-ahead.
5. Environment: repo cloned (runner tasks/034 template read), emulator
   rebuilt on Linux (archive .o files were MinGW), local gate.sh
   mirroring leg(); baseline k1fo on the shipped tree 0 div/1,576.
6. Slice 0: div/cvwn hoisted verbatim into EagleInstruction (family
   made static); stock K=1 failopen 0 div. METHOD §2 caveat recorded.
7. lower.py rewritten: BlockCtx (t allocator, borrow slot map,
   successor checks), lower_one → (stmts, term), nova_test derived
   from NovaCompute's tables, effectful family, word layer, skips,
   XJMP, bit ops via ind(), loops, one-liners, canonical goto [L] 0,
   ir 3 header. First emission 8,837 → WUGTI/WULEI/WANDI/WIORI
   immediates unparsed (registerWideImmediate prints `dec (0xHEX)`,
   not bare hex) → fixed → 8,529.
8. Reconciliation: 8,529 vs predicted 8,504 → my Nova-load sum was
   wrong (43 vs 67); with 67 + LNDO the prediction is exactly 8,529.
   Corrected in Census.md. LNDO stays embedded: the dis drops its
   register field (finding).
9. IRExec: Parser rewritten (class-homogeneous chains, new primaries,
   refusals), loader (ir 3, goto label lists, effectful root parse,
   lvalue classes, t definite assignment), executor (Ctx t[], faults,
   effectful dispatch to the shared helpers, goto table). Clean build.
10. Artifacts regenerated (book/stock). Spot-checked emitted forms at
    7015C2A6 (mul/add/WBTZ via ind), 7015C2BB (MOV.L# test), the
    SQR31 NEG.L#/WULEI blocks.
11. Gates: k1fo 0 div/358,235 pairs/1,584 blocks; k1st 0 div; k1play
    0 div/10.19M pairs/2,276 blocks. Coverage script: 65/77 classes in
    k1fo alone, 66/77 combined; all predicted blocks live; 4/14
    borrow blocks live as t-places.
12. Negative loader tests: 16 refusals with the exact token, 3 valid
    forms load. Noticed the Nova SNC/SZC prose in the draft was
    inverted while writing the nova_test table — corrected (emission
    derives from the source table, never the prose).
13. Docs: IR.md → ir 3; Census.md into the tree with [CORRECTED]
    marks; MathDesign banner; REPORT + this worklog; CURRENT_STATE /
    NextSession entries; lower.py/IRExec header comments.
14. Task 037 drafted from 034 (JOBS=3, embed-count + coverage lines).
    Held for the user's launch coordination, per instruction.
