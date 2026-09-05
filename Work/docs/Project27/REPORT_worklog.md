# Project 27 — worklog (Sep 5 2026, solo session)

1. Read METHOD, BlockSyncDesign, P26 REPORT, HWFindings §6, Provenance.
   Tree verified (17/17 sha prefixes); repo main 29a1c24 == upload.
2. Part 1: tools/derr_clusters.py. Census reproduces the prompt's
   table exactly (1917/279/14/9/9/30/13 + 2 LDSP). Topology census:
   every two-skip cluster is G(fall=S,skip=D) S(fall=D,skip=K); every
   single-skip G(fall=D,skip=K). All K reached only from inside their
   cluster (2,271/2,271); 203 guard-is-block-start; 693 chained.
   Condition-system cross-check: 26 O.ON + 22 I.GOTO + 0 EF code
   operands, none inside a cluster. IR goto-label check 0 outside.
   Runtime 8.5 s (bisect for block_of; grow loop is the residual).
3. Three design-vs-reality findings at the plan gate (Census §2):
   F1 arrival counting (Machine.cpp:306) vs "absorb K"; F2 folded DERR
   is not a verified terminal pair; F3 QUEST_INJECT is an O?SIGNAL
   raise, cannot poke a register. Rulings: A / F2-a (+F2-b follow-up,
   two-line derr verdict) / QUEST_POKE with INJECT hygiene.
4. Part 2: lower.py fold (--assumed-foldable/--tags/--synclist-in/out).
   Baseline emission byte-identical to P26 without the flag (book and
   stock cmp). First fold: 203 folded / 2,068 refused — skip_test used
   skip pc as CFG key; guards are mid-block. Fixed (block-start key).
   Second run: 2,271 folded, 0 refused; 13,507 blocks / 6,258 instr /
   11,400 goto / 2,271 assert / synclist 13,510 — all on prediction.
   derr_clusters hexc aligned to lower.py's 0x%08X first (the artifact
   text is cross-checked against the emission).
5. QUEST_POKE: RTStubs.{hpp,cpp} (statics, env parse, armed line,
   refuse-on-malformed exit 2), OSProcess poke_armed (QUEST clients
   only), Machine.cpp fire-on-arrival before the INJECT check. Built
   clean.
6. Local legs (leg.sh adapted from tasks/039; no ss/fuser here):
   derr (POKE 7015C48B:0:11) — clone assert at 7015C48E, master
   DERR.TRP → ?FATAL → Unimplemented system call 0351, last pair
   7015C48B blk 269858 both; k1fo book 0 div; k1st stock 0 div.
   Negative: delisted goto label refused by the loader; stale tags sha
   refused by lower.py; malformed POKE refuses to launch.
7. k1play (book K=1 play) — first launch lost its process group (only
   the emulator was setsid'd; the driver died with the tool shell);
   relaunched detached; reaped again mid-turn by the sandbox (~10 min in).
   Partial trace: 6.94M pairs, 0 div, 0 mismatch, 1,961 IR blocks.
8. Docs: IR.md §4/§4a/§9; Census.md (plan gate); REPORT.md; this
   worklog; CURRENT_STATE + NextSession; tasks/040-p27-derr-clusters.sh
   (034 template + derr leg via bin/task_source.sh, P27 verdict lines).
9. Landing review: hypothesis that the DERR.TRP kind-2 terminal was
   dead code (frame showed +0x20). Checked the vector bytes (word 39 →
   700001DB LJMP pc-rel → 7017ED1C = entry; +0x20 is DERR.TRP's LDSP
   arm for code 17) and ran the all-emulated control (derr-emu):
   TERMINAL-ABORT at 7017ED1C fires. Refuted; recorded per METHOD §11.
   Control leg surfaced the readout off-by-one (C48E0000 00007015 for
   00000011 7015C48E) — bundled with F2-b. derr-emu added to task 040.
