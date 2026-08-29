# Project 24 — worklog (chronological; the report is the record, this is the path)

1. Read PROMPT/METHOD/WideCarry/P23 report + state docs. Sanity checks
   before starting: quest.dis sha matches 1f9153c0…; the four candidate
   ADC.C sites exist at the stated addresses in the current quest.code;
   dataflow.py and the parked patch present.
2. User confirmations: prior census untrusted (REDO), go-ahead for
   Part 1; quest.blocks(.split) noted as the CFG substrate.
3. Emulator-source extraction: every machine.c access in the tree
   enumerated and case-attributed. os/ has none; syscalls signal by
   skip-return. Sole consumer: Nova ALC CC∈{blank,C}. Immediately found
   F1 (XWDO/LWDO call add()) and F2 (frames.cpp emu_add/emu_sub
   replicate the buggy formulas; i_alloc.cpp add_c too).
4. p24/census.py: parsed both listings; ALC suffix grammar decoded from
   NovaCompute (carry field / shift / no-load / skip); dependence rules
   prune the MOV.L# bit-15 idiom. 47 game + 10 rt consumers.
5. Call-transparency verification: all 124 call targets framed; the one
   frame-word rewrite preserves bit 31; WIORI 0x80000000 sites are
   indirection-bit address building. Push-jump (XPSHJ/WPOPJ) is
   carry-opaque but game code has zero of them.
6. p24/classify.py: backward reaching-writer walk (KILL_WIDE /
   KILL_FIXINV / PASS / RESTORE transfer functions). All 47 game
   consumers → NOVA. rt: 7 MOV.R → NOVA (MOV.O/Z post-syscall);
   ?URTB×2 unreferenced-dead; 7017E4E3 dead behind an unimplemented
   Nova LEF inside the SWAT [0x8] trap handler (installer is
   ?FATAL-only per OSContextTask.cpp; §3 clean-pairs evidence).
   Bare-LEF rendering checked against §14: unimplemented-decode entry
   (`LEF*`, oper=-1), not a listing defect.
7. Holes/XCT (§4): both XCT operands are the known ENQH/ENQT builders
   (no carry effect; E9F6 aborts at XCT anyway); the four heap holes
   are recorded in I_ALLOC.md — one wide writer, zero consumers.
8. CarryCensus.md written; PLAN GATE. User ruled WADC (x,x → −1, c=0)
   and gave go-ahead for Parts 2+3.
9. Patch applied; sanity vector + WADC ruling verified by compiled
   execution. Residue audit file-by-file against the LISTING (not the
   old comments): lib_error latch e35D / tail e3C2 / WINC pair
   e394-e396; o_signal/o_on producers ee37/ee7B/edf8; p_defon fd8D +
   NLDAI path; unsigned_to_char loop (daA3 WSUB, daCB XNDO — the k>1
   zero survives for a fix-invariant reason); def_on's WADC path found
   at ef2C and confirmed fallback-gated. Edits: frames (forwards),
   i_alloc (formula + unlock bit), lib_error (×3 values + c_x),
   o_signal (×3), o_on (×2), p_defon (×1), unsigned_to_char (×1).
   Build clean.
10. Doc corrections (§11-annotated): METHOD §5, WideCarry, three
    DERIVATION headers, L2Contract normative rows, I_ALLOC carry
    chains, UNSIGNED_TO_CHAR.
11. Local gates: fo_book / m_book / play_book / stock_fo, K=1, split
    pair, all 0 div; ?LIB_ERROR+O?SIGNAL native on both fo legs;
    LOCK_FILE consumer block live in all legs. (~15 min, §15.)
12. Repo: branch p24-wide-carry pushed (P23-integrated base — main
    lacks it — plus the P24 tranche; commit message declares the base);
    task 032 pushed to main (031 template + IR-config legs + coverage
    report). REPORT.md written; battery verdict to be appended.

Wrong turns, recorded per §11: (a) an early grep classified WMOV as a
carry writer (substring match on "MOV 0,2") — caught before it entered
the census; the census regexes anchor mnemonics. (b) The first
unsigned_to_char re-derivation guessed the k>1 producer was WADD/XNSBI
before reading the loop head — the XNDO at daCB is the actual last
writer; guessing was replaced by the listing before any edit.
