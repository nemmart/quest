# P25 worklog (Aug 29 2026)

1. Read METHOD.md, prompt, IR.md, P23/P24 reports. Plan-gate census
   replayed lower.py's own gate over the hash-matched P24 inputs:
   443 lowered + 78 bform + 18 parse-blocked (15 fold-regex incl.
   all 6 XCALLs/OP_EDIT + 3 LPEF-@) + 25 WPSH + 2 borrow = 566.
2. Byte-EA semantics from Machine::eagle_{x,l}_byte_indexed (both
   engines identical): per-mode table; L masks NOWHERE, no ii=0
   arm, no indirection. QUEST.PR raw words: the lone ii=0 LPEFB
   displacement is 0xE0001998 → the "indirect hard case" is a
   disassembler rendering artifact. §8 formula corrected.
3. User rulings (in-session): borrow pair IN (args as stores,
   bracket as @addr instruction pairs — user's explicit sequence);
   wp/bp two-arg pointer builders with masking in the executor
   (spelled masks rejected); `*` over `<<`; M8 index raw (ring-7
   observation: wrap-at-use is identity on real values; raw is what
   the source executes and faults loudly on garbage); full scope
   tier; disassembler fix stays with the user (184 lines, counted
   correctly on the second attempt — first count was a one-constant
   grep, §10 class, recorded).
4. lower.py: fold regex, reconcile_at (X keeps the indirect bit in
   the number w/ decorative @; L strips it into @ — both accepted,
   bit-without-@ refuses; the first regen tripped exactly this on
   XNLDA @[ac3+0xFFEC] and the reconciliation was rebuilt from the
   dis evidence), byte_ea, wp/bp, push_stores (WPSH x,a order
   pinned from Disassembler.java bit layout — ((a-x)&3)+1 is
   ambiguous for group 3), borrow veto dropped, XLEFB/LLEFB + 6
   byte ld/st ops. WPSH group==wides check mirrors the emulated
   hook's throw.
5. IRExec: MEM8/WP/BP/MUL + M8 lvalue, anchored to
   Memory::read_byte/write_byte and Machine::set_byte_segment.
   Clean build.
6. Regen book+stock: 566/566, skips = 3 standing exclusions.
   Embeds −3,516. Spot-checked every form incl. the borrow block
   (@WPSH/@WPOP pairs among lowered stores at 7015F795) and the
   0xE0001998 constant store.
7. Negative test: malformed bp line refuses at load, exact token.
8. Local gates: k1fo 0 div/1,607 blocks; k1play 0 div/2,284;
   st-fo 0 div/1,580. Predicted blocks stated first; INIT_OBJ_TBL
   (LPEFB) live at startup, LOCK_FILE live, TERRAIN+5 WPSH live,
   7/78 bform live, borrow 0/2 (census-carried, recorded as-is).
9. Docs: IR.md §5/§6/§8/§9 amended; ByteEA.md census; this report.
10. Repo: branch p25-byte-addressing (Work: lower.py, IRExec.cpp,
    ir2.book/stock, docs); task 035 + 035-bform-blocks.txt to main.
11. Battery verdict: 12/13 green, all IR legs clean; the one red
    (inj-emu — all-emulated, zero P25 code, injection pc unreached,
    green in 034's proof run) reported per boundary 6 and RULED
    FLAKE by the user; retries left to ride (user: "green tells us
    something, red tells us something else") — attempt 3: 13/13
    GREEN, DONE. Flake confirmed empirically.
12. Post-landing polish (user requests): IR.md referenced from
    lower.py + IRExec.cpp headers; DISASSEMBLER_BYTE_OPERANDS.md
    (fix deferred by ruling, shim standing); byte-pointer literal
    0xW:b (dis fold notation, word-addressed — dumps grep; b=byte
    select 0/1 EXCLUSIVELY, bitp(w,n) reserved for M1 so bit
    addressing can never overload the colon); wp/bp confirmed
    register-relative-only. Regen + K=1 re-gate green (0 div,
    LPEFB block live: the :b literal round-trips under strict
    pairing).

Wrong turns kept: the "3 lines" undercount (§10); the first
strictness pass refusing X-convention @ operands (evidence, not
tuning, chose the reconciliation).
