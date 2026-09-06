# P33-B census — the 19 WMSP claim groups (tools/p33_census.py; raw: censusB_raw.txt)

Mechanised on string_sites.py's evaluator over main fa5b180's tree
(quest.dis 5c1db5fb…, blocks.split ba30a841…, synclist.p27 af1be42f…,
strhooks 9cc8c96c…). Reproduces docs/Project31/Census.md §1.5's 19/19
facts and P32's 96-site / 37-block population.

## 1. The groups

| row | block | routine | claims | WCMVs (into a temp / out of a temp / P32 pieces) | consumer | STASP | blocks in the bracket | boundary carrying a last-temp pointer | boundary carrying an INTERMEDIATE pointer |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 70166144 | DIED | 3 | 5 (5/0/0) | ?WRITE_SCREEN | 701661AA | 6144 6192 6193 61A7 | – | – |
| 2 | 701661AE | DIED | 3 | 5 (5/0/0) | ?WRITE_SCREEN | 70166215 | 61AE 61FD 61FE 6212 | – | – |
| 3 | 70166564 | DISPLAY_MAGIC | 4 | 7 (7/0/0) | ?WRITE_SCREEN | 701665F0 | 6564 65D8 65D9 65ED | – | – |
| 4 | 7016693C | DISPLAY_CAVE | 1 | 5 (2/1/2) | copy-out | 70166990 | 693C 6981 6982 | 693C 6981 | – |
| 5 | 70166E7C | DISPLAY_SCREEN | 3 | 5 (3/1/1) | copy-out | 70166EFB | 6E7C 6EE8 6EE9 6EF0 6EF1 | 6E7C 6EE8 | – |
| 6 | 70166F4F | DISPLAY_SCREEN | 3 | 5 (3/1/1) | copy-out | 70166FCE | 6F4F 6FBB 6FBC 6FC3 6FC4 | 6F4F 6FBB | – |
| 7 | 70166FE9 | DISPLAY_SCREEN | 3 | 5 (4/1/0) | copy-out | 70167059 | 6FE9 7048 7049 704D 704E 7050 | 6FE9 7048 7049 704D 704E | – |
| 8 | 70167744 | DISPLAY_INVENTORY | 2 | 3 (3/0/0) | ?WRITE_SCREEN | 70167797 | 7744 7771 7772 7794 | – | – |
| 9 | 7016B860 | GET_QUEST | 3 | 9 (5/0/4) | ?WRITE_SCREEN | 7016B8FA | B860 B8E2 B8E3 B8F7 | – | – |
| 10 | 7016BBE3 | GET_QUEST | 2 | 7 (3/0/4) | ?WRITE_SCREEN | 7016BC67 | BBE3 BC4F BC50 BC64 | – | – |
| 11 | 7016DBFC | HELP | 1 | 3 (2/1/0) | copy-out | 7016DC2E | DBFC | – | – |
| 12 | 7016DF68 | INIT_OBJ_TBL | 2 | 3 (2/1/0) | copy-out | 7016DFDB | DF68 DFB6* DFB7 DFC2* DFC3* DFC4 | – | **DF68, DFB6 (ac3 → temp 1)** |
| 13 | 7016ED93 | LIST_PLAYERS | 5 | 8 (8/0/0) | ?WRITE_SCREEN | 7016EE38 | ED93 EE20 EE21 EE35 | – | – |
| 14 | 70172DDE | OBSERVE | 1 | 11 (2/1/8) | copy-out | 70172E5F | 2DDE 2E4D 2E4E 2E54 2E55 | 2DDE 2E4D | – |
| 15 | 70172EB0 | OBSERVE | 1 | 11 (2/1/8) | copy-out | 70172F30 | 2EB0 2F1E 2F1F 2F25 2F26 | 2EB0 2F1E | – |
| 16 | 70175694 | OP_EDIT | 5 | 8 (8/0/0) | ?WRITE_SCREEN | 70175743 | 5694 571B 571C 5740 | – | – |
| 17 | 701757C5 | OP_EDIT | 5 | 8 (8/0/0) | ?WRITE_SCREEN | 7017586C | 57C5 5844 5845 5869 | – | – |
| 18 | 701758A0 | OP_EDIT | 5 | 8 (8/0/0) | ?WRITE_SCREEN | 70175951 | 58A0 5929 592A 594E | – | – |
| 19 | 7017A116 | STORE | 5 | 12 (8/0/4) | ?WRITE_SCREEN | 7017A1D1 | A116 A1B9 A1BA A1CE | A116 A1B9 | – |

(* = a folded DERR-cluster interior, unlisted; every other block is a
listed entry — a rendezvous at K=1.)

Totals: 57 claims; 128 WCMVs inside the brackets = **88 into a temp + 8
copy-outs = the 96 of P32's P33-B population** + 32 P32 scratch-chain pieces
(already lowered). 37 blocks = 19 claim blocks + 18 tail blocks (the
min-skip cut, the post-call / copy-out block). 11 `?WRITE_SCREEN` consumers
(`XPEF @[fp+slot]` pushes the last claim's base), 8 copy-outs.

## 2. Facts re-checked (§1.5's 19/19)

- Every WMSP (57/57) is preceded by an adjacent `LDASP r; WADI 2,r` — the
  temp's base word address. In DISPLAY_MAGIC's first claim (70166578) that
  register is stored to its frame slot and reused for the size BEFORE the
  WMSP — the reason the lowering targets the pair, not the WMSP.
- Every STASP restores from the FIRST claim's slot (`XWLDA r,[slot]; WSBI
  2,r; STASP r`): 10 groups use ac1, 9 use ac0 — the reason `release`
  names its register.
- The claims of a group all sit in the claim block; the last piece, the
  length-word store and the consumer sit in the tail blocks; no call other
  than `?UNSIGNED_TO_CHAR` (before the first claim) and the consuming
  `?WRITE_SCREEN` lies in a bracket.

## 3. The finding that ruled the shape (design-vs-reality, boundary 4)

Row 12 (INIT_OBJ_TBL): `temp2 = temp1 ‖ char(0x15)` leaves ac3 (the src-end
residue, a pointer INTO temp1) live at the listed entry 7016DFB7 — a
DERR-cluster continuation with no register write in between. Under a single
`p@b` (the Sep 5/6 ruling) the row maps to temp2 and that ac3 mismatches; the
group runs on every login. The other 18 hold §2.1's "residues overwritten
before any rendezvous" only by instruction ordering (the next claim's setup
or `LDAFP 3` clobbers them), not by construction. Ruling (Sep 6): per-claim
twins `t@b.k` — exact by construction; `p@b ≡ t@b.last` is the readable
layer's rendering.

## 4. Capacities (quest.arena)

5 claims have constant sizes (DISPLAY_MAGIC 8/8, DISPLAY_INVENTORY 5/6, HELP
8 wides) → `bound=exact`, cap = 4·size. 52 are `⌈(Σ lengths + k)/4⌉` over 21
atoms (frame varyings `N[fp+k]`, argument strings `N[W[fp−12]]`, statics,
record fields, `?UNSIGNED_TO_CHAR` digit counts) whose declared capacities
the census does not carry → `bound=unbounded`, 8192 B provisional; the
runtime `claim` check (and the master hook's twin) is the fault. Battery
datum: DISPLAY_SCREEN's twins peak at 688/688/692 bytes on k1fo (a whole
screen); everything else far below.

## 5. Liveness of the groups in the standing legs

k1fo (book K=1): rows 5, 7 (DISPLAY_SCREEN: 297 / 54–81 claims), 8
(DISPLAY_INVENTORY), 12 (INIT_OBJ_TBL). play (book K=50): the same four.
DIED / DISPLAY_MAGIC / LIST_PLAYERS / OBSERVE / HELP / GET_QUEST / OP_EDIT /
STORE are not reached by the drivers: the play driver's post-auto-move keys
(O/D/L/H) never land on the command prompt (a pre-existing driver defect —
046's HELP fail was its first observation; fix owed in NextSession). The
arena's hardest case (the screen-sized twins, the copy-out shape, the
`?WRITE_SCREEN` consumer inside the bracket, the INIT_OBJ_TBL survivor) is
live.
