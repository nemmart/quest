# Salvage — what survives the compiler line (P35–P43)

Project 45, Sep 12 2026. Tree: `main` @ `7279d22` (post-P44 design, post-P41
merge), branch `p45-salvage`. Rulings of record: `docs/Project45/a001-plan-gate.md`.

**The test applied to every item:** *does this statement describe Quest, or
the machine that compiled Quest?* The first kind is here. The second kind is
listed in §4 only to say it is gone.

**Status vocabulary.** *Kind*: `PROGRAM-FACT` / `PHENOMENON` / `MEASUREMENT`
/ `METHOD-RULING` / `COMPILER-CLAIM`. *Confidence*: **Verified** = re-checked
by P45 this session against `Disassembled/quest.dis`, `quest.mem`, the
addrbook or `crossings.py`; **High** = ≥2 independent witnesses or a live
corroborating source; **Single**; **Reported** = asserted in an attic report
with nothing P45 could check. A fact with **live corroboration** is being
*confirmed*, not salvaged — that is a stronger status and is marked as such.
PCs written `block@XXXXXXXX` are IR block labels (the attic's rule citations
are block addresses, not instruction addresses); bare PCs are instructions.

---

## §1 Program facts — SALVAGED

### 1.1 Calling interface (a001 Q1: ABI facts are program facts)

| # | fact | evidence | witnesses | conf | live corroboration | depends |
|---|---|---|---|---|---|---|
| F1 | **Static link.** A nested procedure (`.N@` entry) receives the enclosing frame pointer in **ac1**; `WSAVS` saves it at `wp(fp,-6)`; it is reloaded as `XWLDA n,[ac3+0x7FFA]`. Independent of argc (args stay at `wfp-10-2N`). | 63/63 game XCALL sites are preceded by a link load into ac1: 44 `WMOV 3,1` (parent calls child), 18 `XWLDA 1,[ac3+0x7FFA]` (sibling calls sibling), 1 `WMOV 2,1` (fp held in ac2 at that site). FIRE.1's prologue `WSAVS; WMOV 1,2` uses ac1 straight from entry. | 63 sites + 4 FIRE-family prologues | **Verified** | `M4aDesign.md` (restore image: ac1 @ wfp-6 = static link; call site `WMOV 3,1`), `ON_ERROR_CATALOG.md` §B (`XWLDA 0,[ac3+0x7FFA]` = load enclosing fp) | DESIGN §9.2 rendezvous; any C spelling of uplevel access (P44 project 3); `game/quest_rt.h` UPLINK/UP/UPARG |
| F2 | **Uplevel reference shape.** Link-load-then-displace; the parent's by-reference *parameters* are reached as `@[link+0xFFFx]` (triple indirection: link → arg slot → datum), its locals as `[link+d]`. | FIRE.2 `XNLDA 1,@[ac2+0xFFF4]` ×3; FIRE.3 ×12 + ×1 (`0xFFF2`); direct forms in all three children (§3 table) | 47 refs in the FIRE family | **Verified** | same as F1 | same as F1 |
| F3 | **Nesting is exactly one level deep program-wide.** No child ever reloads a grandparent link; every XCALL site's link is either the caller's own fp or the caller's own saved link (F1). The addrbook's five apparent double-nestings are mis-attributed `nocall` ON-units. | the 63-site census above (no second-level link load exists) | 63 | **Verified** (consistency; no counter-instance) | METHOD §16 (nested flag conflates called procedures and `nocall` units) | call-graph extraction (P44 project 7) |
| F4 | **Value return (slotpatch).** A value-returning PL/I function stores its result into the saved-ac0 image of its own frame so `WRTN` restores it: 32-bit → `wp(ac3,-8)`, 16-bit → `wp(ac3,-7)`. The addrbook's `slotpatch` flag marks exactly these (16 entries). | OWNS 70175DA2 `XNSTA 2,[ac3+0x7FF9]`; DISTANCE_TO_PLAYER 701687DB `XWSTA 0,[ac3+0x7FF8]` (in block@701687D9); 701703AC `XNSTA 0,[ac3+0x7FF9]` after an X.CB | 3 sites, both widths | **Verified** | addrbook header (image ac0 @ wfp-8); DESIGN §9.2 already relies on it | rendezvous contract; every C routine that returns a value |
| F5 | **Optional arguments have two spellings, and there are exactly two `mixed:` routines.** REFRESH_SCREEN (`mixed:0/1`) reads a frame *marker word* at `wp(ac3,-9)`; RETURN_MESSAGE (`mixed:3/6`) tests the argument *slot* `M32[wp(ac3,-16)]` for null. No third instance exists, so which spelling PL/I picks is unfalsifiable in this program. | addrbook grep: 2 `mixed:` entries; RETURN_MESSAGE 70176FDD..E0 `XWLDA 2,[ac3+0x7FF0]` + null test | 2 | **Verified** | — | any C for these two routines (P44 project 3 needs an optional-arg spelling) |
| F6 | **RETURN_MESSAGE's `mixed:3/6` arity is an artifact of hand assembly, not PL/I.** Five call sites: four pass 6 args (7015BE74, 70175EC8, 70175EFF, 7017D7D9); the 3-arg site 70169B82 is inside LOCK_FILE's hand-built tail (`LDASP/WPSH` sequence 70169B78..B81). | listing | 5 sites | **Verified** | METHOD §16 | the addrbook's arity flag for RETURN_MESSAGE is **misleading** (§3) |
| F7 | **`WSAVR` is not a compiler entry variant.** Exactly 2 of 102 live entries use it, both hand assembly (LOCK_FILE 70169B0F, UNLOCK_FILE 70169B86). | addrbook grep | 2/102 | **Verified** | METHOD §16 | any entry-prologue model |

### 1.2 Hand assembly and multiple ENTRY (the facts under R39/R40)

| # | fact | evidence | conf | live corroboration | depends |
|---|---|---|---|---|---|
| F8 | **LOCK_FILE + UNLOCK_FILE (70169B0F..70169D69) are one hand-assembly unit.** They branch into each other's ranges (UNLOCK→LOCK 5 edges e.g. 70169B94→70169B75; LOCK→UNLOCK 70169B73→70169BB8), share an error tail at 70169B78, use raw `SYSCALL 0245/0246`, `WSAVR`, `LDASP`. It is the **only** mutual pair in the program. | `compiler/crossings.py` run by P45: one MUTUAL pair | **Verified** | METHOD §16 (R39); `crossings.py` live | evidence admissibility for every rule; translation scope (these two are reconstructed as assembly, not PL/I) |
| F9 | **PL/I multiple ENTRY.** CREATE_MAP 7016509F / DISPLAY_MAP 701650AC and TRANSPORT_TERRAK 7017D48F / TRANSPORT_SUNDAR 7017D492: same `WSAVS` frame (0x6AA / 0x0C), same argc (2 / 2), each prologue initialises the same slots with different constants (CREATE_MAP 8:=19, 9:=19, 12:=0x8000; DISPLAY_MAP 8:=10, 9:=19, …; TERRAK 8:=2, SUNDAR 8:=7) then branches **one-way** into the shared body (7016509F→701650B9, 7017D48F→7017D495). | listing + `crossings.py` (two one-way edges) | **Verified** | METHOD §16 (R40) | reconstruction shape: **one function, two entry prologues**; every per-routine statement count for these four entries (§3) |
| F10 | **ON-unit bodies interleave into their parent's addrbook range**, so `.N@` ranges mis-size the parent. Verified instances: DROP.1 (unit DROP.2@701696FA is `WSAVS 0; …; I.GOTO`, DROP.1's body resumes at 70169702 and runs to I.EPILOG at 70169723 and beyond); READ_IN (7 statements, `WBR` at 701766FD over READ_IN.1@701766FE, body resumes 7017670A → I.EPILOG 7017671A); **LIST_PLAYERS.1@7016EC57 — new instance**: the unit is 0x1D words (WSAVS 0x09 … I.GOTO at 7016EC71), the parent resumes at 7016EC74, and the remaining ~0x55C words of the `.1` range are LIST_PLAYERS' own body. | listing | **Verified** | METHOD §16 (P39 addendum) | `Census.md` per-routine figures (§3); P44 §11 ON-condition CFG closure; the L1 harness's notion of "a routine's blocks" |

### 1.3 Data and literals

| # | fact | evidence | conf | live corroboration | depends |
|---|---|---|---|---|---|
| F11 | **RETURN_MESSAGE @70176FDD is the fatal-error exit.** Tail `7017700F SYSCALL 0310` (?RETURN, never returns); no `ret` path. Default message when arg 3 is null: literal at **0x70000CCD = "Unexpected error"** (16 bytes, read from `quest.mem`), length constant `NLDAI 16` at 70176FE8 — recovered independently and agreeing. Packing: `len \| (*code \| 0x8000)`, `\|= 0x1000` if `*severity != 0`; ac1 = text, ac2 = packed. | listing + `quest.mem` | **Verified** | `ERROR_PROCESSING.md` (names it as the terminal path for the fatal LOGON handlers and INIT_SHARED_DATA), `HeapSignalPlan.md`, `Layering.md` | translation of every fatal path; the L1 harness's terminal-path handling (METHOD §13) |
| F12 | **PL/I BIT literals are not constant-folded.** `'001'B` / `'1'B` are rebuilt at run time by **`X.CB @7017E708`** (ac2 = destination word address, ac0 = byte pointer to the character form, ac1 = its length, then an undecorated `LCALL [0x7017E708],0`). Two game sites: GET_INPUT 7016AA41 (`"001"` at 0x7016A9B9, len 3) and 701703A6 (`"1"` at 0x7017024D, len 1). Literal bytes confirmed in `quest.mem`. | listing + `quest.mem` | **Verified** | — | P44 lowering of any BIT literal (a runtime call, not a constant); the second site is also an F4 witness |
| F13 | **PL/I CHARACTER is an unsigned byte.** GET_INPUT 7016AA63..69: `WLDB 0,2` (zero-extend) then `WSGTI 2,128`, then `WANDI 2,255`. A signed `char` makes the test dead (gcc `-Wall` caught it). | listing | **Verified** | — | every C declaration of a CHAR datum (`unsigned char`) |
| F14 | **GET_INPUT's frame.** `WSAVS 0x29` (41 wides = 82 slots); a 144-byte `CHAR` buffer at slots 4..75; temps at 76/78/80/82 (`XLEF 2,[ac3+0x4C]`, `XWSTA 2,[ac3+0x4E]`, …); `?READ$6(chan=0x70000262, buffer, one, count&, options=0x1000, flags)` from the six `XPEF`s at 7016AA4D..55 into `LCALL [0x7017DE5F],6`. | listing | **Verified** | — | GET_INPUT's C |
| F15 | **DIED's signature.** Arg 1 (`@[ac3+0xFFF4]`) is read 12× in 7016603D..70166487 — a CHAR VARYING death message whose length sizes the message temps; arg 2 (`@[ac3+0xFFF2]`) is read **0×**. | listing (counts by P45) | **Verified** (the never-read half; the VARYING reading is P36's) | — | DIED's C. *(a001: the C file is not a source; this fact is re-derived from the listing, not from `DIED.c`.)* |
| F16 | **Twin (string-temp) sizing arithmetic is derivable from the concatenation chain.** Claim count = `ceil(bytes/4)` = `+3; lsh -1; lsh -1` over the running byte length; the final VARYING twin adds 2 for its length word (`+5`). DIED group 70166144: `WNADI 1,23` / `NLDAI 3`; group 701661AE: `WNADI 0,76` / `NLDAI 3` — 3 and 23, 3 and 76 bytes are the literal piece lengths. | listing (constants confirmed; the arithmetic is P36's) | **High** | `IR.md` §5.9 / `quest.arena` (P33-B) | P44 `s@` twins; no `LEN()` sizing intrinsic is needed — only the length of a VARYING *parameter* is unknowable from the C |
| F17 | **Record/field facts established by the line and already live in `compiler/gen_declarations.py`:** `PLAYER.fm590` (K=−590, bits 0..6, 8, 11 seen), `PLAYER.fm591` (K=−591, bits 7,8,9,12,13,14,15), `PLAYER.fm63` (bits 0,1,2,5), `PLAYER.fm390[10]` (K=−390, inner stride 1, 1-based, bound 10), `PLAYER.fm629`, `OBJ_PTR->f911504` (the DO limit over CASTLE), the CASTLE table (CAS_PTR, stride 23, bound 1000). Bit numbering is from the MSB; the bit address is `16*scaled + (16*K + n)` folded into one WNADI (DIED 70166046..4D: `NLDAI 686,1; WMUL 1,0; NLDAI 16,2; WMUL 0,2; WMOV 2,1; WNADI 1,0xDB24` = word −590, bit 4). | listing for the DIED sequence; the field table is live | **Verified** (sequence) / **High** (table) | `gen_declarations.py` (live) | every C that touches those fields |
| F18 | **No world-coordinate offset.** The game computes in the raw 0x3Bxx space; PICK_X_Y's spawn gates are `x > 0x3BF5`, `x <= 0x3FAC`, `y > 0x3B73`, `y <= 0x3FDE` (70176267..72). **Correction:** the attic text says the y-low gate is 0x3B77; the listing says **0x3B73**. | listing | **Verified** | `Project34/PICK_X_Y_reading.md` (the table origins only — the no-offset ruling is **not** in any live doc) | any world-render or C that reads coordinates |
| F19 | **VARYING length word written as its own statement.** DIED 70166096 `XNSTA 0,[ac2+0x7D8F]` (K=−625) stores the length of a `VARYING(32)` field separately from the 32-blank assignment two statements earlier. | listing | **Verified** (instruction) / **High** (reading) | `IR.md` §5.8 `assign_varying` | how `.len` is spelled in C |

### 1.4 Frame layouts (see §3 for the taint status)

| # | fact | conf | depends |
|---|---|---|---|
| F20 | **FIRE's parent frame** as reached uplevel: args 1 (−12, 16-bit datum) and 2 (−14, 16-bit datum, DERR-17 bound 100 vs arg 1's bound 10 — different variables); locals +8 (16-bit), +10 (32, REGION subscript), +12 (32, PLAYER subscript), +14 (32, written uplevel by FIRE.1 and FIRE.3, **never touched by FIRE itself**). Every slot reached by ≥2 independent classes of {FIRE.1, FIRE.2, FIRE.3, FIRE's body}, zero width/displacement disagreements. FIRE.1 is called from 7016A08C, FIRE.2 from 7016A309, FIRE.3 from **7016A1E5** (P41 wrote 7016A309 for FIRE.3; that site calls FIRE.2). | **Verified** (`docs/Project45/verify_frames.py`) | `game/declarations.json`, `gen_declarations.py` PARENT_FRAMES |
| F21 | **LIST_PLAYERS slot 13** is a 16-bit local: LIST_PLAYERS.3 7016F5A2 `XNLDA 1,[ac2+0xD]`, plus the parent's own 7016ECA6 `XNSTA 0,[ac3+0xD]` and 7016ECD7 `XNADI 1,[ac3+0xD]` (parent code inside the `.1` range, F10). P41 recorded it single-witness; it is not. | **Verified** | same |
| F23 | **The base-register class {ac2, ac3} is an ISA fact.** The Eagle's wide addressing modes admit ac2 or ac3 as the index register and no other, so a compiler targeting this machine has exactly TWO general offset registers and must manage them as a scarce resource. This is not a derived compiler rule. | Measured over `quest.ir2.book`: `[acN±d]` sites — ac0 **0**, ac1 **0**, ac2 3,463, ac3 15,881; `wp(acN,d)` — ac0 0, ac1 0, ac2 3,337, ac3 13,376. 19,344 base uses, none on ac0/ac1 | universal (19,344) | **Verified** | the emulator's own decode tables (independent of the attic entirely) | DESIGN §5.2 base-register dataflow (two-state space); P5/P6 below have their MECHANISM here | salvage |
| F22 | **Array locals span real words.** A `CHAR(n)` local occupies `ceil(n/2)` words rounded to a whole even slot (F14: 144 bytes → slots 4..75). | **Verified** (one routine) / Single | C declarations of buffers |

### 1.5 Measurements over the book/listing (numbers survive; conclusions do not)

| # | measurement | P45 re-count | attic count | conf |
|---|---|---|---|---|
| M1 | Loads of `SD_PTR` (0x70000210) by destination register, whole listing | ac0 55 / ac1 251 / ac2 248 / ac3 9; of the 306 ac0/ac1 loads, **302** are immediately followed by `WSUB`/`WBTO`/`WBTZ` | same (P41) | **Verified** |
| M2 | Static-link loads `XWLDA n,[ac3+0x7FFA]` by destination, game range | ac0 3 / ac1 18 / **ac2 288** / ac3 12 (+3 via `[ac2+0x7FFA]` when fp is in ac2) | 3 / 18 / **276** / 12 (P39, over the book) | **Verified** except ac2 (see §5) |
| M3 | Game XCALL sites and the instruction that supplies ac1 | 63 sites: 44 `WMOV 3,1`, 18 `XWLDA 1,[..7FFA]`, 1 `WMOV 2,1` | 63/63 (P39) | **Verified** |
| M4 | Addrbook: `WSAVR` entries; `mixed:` entries; `slotpatch` entries; `nested` uncommented entries | 2 / 2 / 16 / 23 | 2 / 2 / — / 23 | **Verified** |
| M5 | Cross-family control-flow edges (`crossings.py`) | UNLOCK→LOCK 5, LOCK→UNLOCK 1, CREATE_MAP→DISPLAY_MAP 1, TERRAK→SUNDAR 1; one mutual pair | same (P40/P41) | **Verified** |
| M6 | Immediate-after-call register choice | 12 ac0 / 3 ac1 of 15 (P12) | P38 | Reported |
| M7 | Blocks with two element-address constructions that build both addresses before either field load | 163 of 242 | P40 | Reported |

---

## §2 Phenomena — RECORDED, not ruled

Observations about the binary that carry no rule. P44 §7.2 needs these as
`keep` (do-not-eliminate) test cases or as lowering expectations.

| # | phenomenon | site | conf |
|---|---|---|---|
| P1 | **Self-move at a join.** The join of RETURN_MESSAGE's message diamond opens with `WMOV 0,0` (70176FF5); both arms already leave the length in ac0. | 70176FF5 | **Verified** |
| P2 | **Redundant loads across a join are the norm, self-moves the exception.** Every block of the four matched routines addresses `wp(ac3,d)` with no preceding LDAFP (349 statements) — i.e. the frame register is *not* reloaded at joins while value registers are. | P38 measurement | Reported |
| P3 | **Loop-invariant element address hoisted into a frame slot.** QUEST.1: `LWADD 0,[0x70000210]` at 7015C5ED, `XWSTA 0,[ac3+0x6]` at 7015C5FC (before the loop's entry test), body reloads `XWLDA 2,[ac3+0x6]` at 7015C611. The parent QUEST has the same shape with registers permuted. (P40 cites 7015C5F9/FB/60E; those are off by a few words.) This is DESIGN §4.2's example of a transformation-created `v`. | 7015C5E1.. | **Verified** |
| P4 | **The loop counter of an XNDO loop can live in ac0 or ac1 but never ac2/ac3** in every matched loop; the DO limit is reloaded only when its register was taken (LIST_PLAYERS.3 7016F563 reloads, QUEST.1 does not). | LIST_PLAYERS.3, QUEST.1, OWNS, P35 loops | Reported |
| P5 | **`WPSH 3,3` / `LDAFP 3` / … / `WPOP 3,3`** around a frame reference when ac0–ac2 are all live and ac3 holds a base. **Mechanism (F23): this is SPILLING under a two-register constraint, not a quirk.** | FIRE.1 7016A3EA..EE; INIT_OBJ_TBL 7016DF8F (block@7016DF68) | **Verified** |
| P6 | **The frame pointer is displaced and re-materialised by `LDAFP` into ac2 or ac3** — **mechanism (F23): one of two bases usually holds the frame, leaving ONE free for every record access, string pointer and static link**; FIRE.1 has 3× `LDAFP 2` and 3× `LDAFP 3`; DIED holds the frame in ac2 for ~10 blocks (22 `wp(ac2,8)` refs) while ac3 holds a record base. | FIRE.1 block@7016A3C7; DIED 70166376..3B3 | **Verified** (FIRE.1) / Reported (DIED counts) |
| P7 | **Bit reference materialises 0/−1**: `WSUB v,v; WSZB base,off; WADC v,v; MOV.L# v,v,SNC` (DIED 70166054..57); `WCOM` for `^`. Bit assignment is set-then-undo: `WBTO` unconditionally, sign test, `WBTZ` on the zero arm (DIED 70166376..82, one witness). QUEST.1 has bare `WBTO`/`WBTZ` for constant assignments. | DIED, QUEST.1 | **Verified** (70166054..57) / Single (set-then-undo) |
| P8 | **`SUB(i,n)` bound check idiom**: `WUGTI r,n; WSGT r,0; DERR 17` (`>u` when n > 0x7FFF, else `WSGTI` `>s`), e.g. FIRE.1 7016A3C2..C6 (n=100000), QUEST.1 7015C5E6..E9 (n=10). Already folded by P27. | many | **Verified** |
| P9 | **`COM.# 1,1,SZR`** — a 16-bit test against −1 (FIRE.1 7016A417); **`WADC 0,0`** as "materialise −1 in a register that holds anything" (FIRE.1 7016A41E, `x + ~x`). | FIRE.1 | **Verified** |
| P10 | **Constant zero by `WSUB r,r`**, never by immediate (OWNS 70175DA5 `WSUB 0,0`; FIRE.1 **7016A3D0** `WSUB 1,1` — the attic cites 7016A3D2, mid-instruction). No `WSUB 1,1` exists in INIT_OBJ_TBL despite the citation. | OWNS, FIRE.1 | **Verified** (2 of 3 citations) |
| P11 | **A scaled-subscript CSE temp and an element-address temp both exist** (PICK_X_Y slots 4/6; UPDATE_SCREENS 6/8; DIED slots 8/10 for `16*i*686` vs `i*686`), and DIED 70166110 creates **no** element-address temp across five consecutive stores to the same element — the "when" is unexplained. | P35/P36 | Reported |
| P12 | **Immediate-after-call register choice**: of 15 sites that load a packed immediate right after a decorated game→game call, 12 use ac0 and 3 use ac1 — and all three ac1 sites call GET_INPUT (HIT_ANY_CHAR 7016DEAD `WLDAI 1,0x20D0B`, 701618A9, 7016F411). Two readings fit; nothing separates them. | 15 sites | **Verified** (7016DEAD) / Reported (census) |
| P13 | **Dummy-argument temp order at GET_INPUT's ?READ call**: slot 76 goes to argument 6 (the BIT literal), slot 78 to argument 2, yet argument 2's store is emitted first (7016AA37..3B). Neither "left to right" nor "call-materialised first" explains both. | GET_INPUT | **Verified** |
| P14 | **`assign_varying` capacity is visible only for variable-length sources** (min diamond `WSGE/WSLE + WMOV`); constant sources fold it. Frame slot `wp(ac3,12)` shows 27/17/16/47 across sites. | DESIGN §10 | Reported (already in DESIGN) |

---

## §3 Live data and live docs — TAINTED, or carrying stale text

### 3.1 `game/declarations.json` / `compiler/gen_declarations.py` PARENT_FRAMES

**Marked in place** (a001 Q2/Q3): a `_taint` dict per frame in `PARENT_FRAMES`,
carried to the `.json` by a one-expression addition to the table emission,
regenerated, and checked byte-identical to the prior file except for the
markers (`declarations.h` differs only in its table digest). Shape:

```
"_taint": { "w12": { "status": "derived", "witnesses": 12,
                     "independent": ["FIRE.2", "FIRE"],
                     "note": "no sibling second witness; parent body x10",
                     "verified": "P45 against quest.dis, Sep 12 2026" } }
```

| entry | P41 said | P45 re-derivation (`verify_frames.py`) | status |
|---|---|---|---|
| FIRE args.1 (−12) | FIRE.2 ×3, FIRE.3 ×12, body ×10 | same | derived |
| FIRE args.2 (−14) | FIRE.3 ×1, body ×2 | same | derived |
| FIRE w8 | FIRE.3 ×8+XPEF, body ×11 | same | derived |
| FIRE w10 | FIRE.1 ×3, FIRE.3 ×2, body ×1 | same | derived |
| FIRE w12 | FIRE.2 ×2, body ×10 | same | derived (no sibling second witness) |
| FIRE w14 | FIRE.1 ×3, FIRE.3 ×6, body 0 | same | derived |
| LIST_PLAYERS w13 | **single** | LIST_PLAYERS.3 ×1 **+ parent body ×2** | derived |

**No layout value changed. No count disagreed with P41.** The carried-in
ruling 4 ("every slot rests on a single witness") was true of the pre-P41
file and is not true of the file on `main`; the markers say what is true.
What remains single-witness in this file: nothing. What remains
*unconfirmed by a source outside the attic*: the reading of each slot's
*meaning* (REGION subscript, PLAYER subscript) — those come from the DERR-17
bounds and are plausible but are comments, not layout.

### 3.2 `docs/Project34/Census.md` — per-routine figures that depend on addrbook ranges

The header caveat names only the two R40 pairs. METHOD §16 (P39 addendum)
already widens it: **every `.N@` statement count is suspect**. Concretely:

| figure | why wrong | replacement |
|---|---|---|
| CREATE_MAP 7 / DISPLAY_MAP 902; TRANSPORT_TERRAK 2 / TRANSPORT_SUNDAR 138 | R40 shared body falls in one range | compare against the union of the pair's ranges (F9) |
| READ_IN "17 statements, one rt_call" | union of READ_IN + READ_IN.1; the parent is 7 statements and uses I.PROLOG/O.ON/I.GOTO/I.EPILOG, none on the census's marker list | struck at the P40 gate; re-count with the ON-unit excised |
| LIST_PLAYERS / LIST_PLAYERS.1 (`#`, nocall) | LIST_PLAYERS.1's range is 0x579 words of which 0x1D are the unit (F10) | LIST_PLAYERS' count is under by the parent body inside the `.1` range |
| DROP / DROP.1 / DROP.2 | DROP.1's body continues past DROP.2's start (F10) | same |
| "130 addrbook entries", "24 blocks unreached by any entry", "4,312 blocks mis-owned … with the edge cut" (§1) | these are the *symptoms* of F10 and are correct as measured; they are not per-routine counts | keep; cite F10 as the cause |
| the readable renderings in `Project34/readable/*.txt` for any routine with an embedded `nocall` unit | block ownership by nearest-preceding entry | rendering artefact; not a data error |

**Recommended replacement caveat text** is in `docs/Project45/REPORT.md`
(the file is outside P45's write boundary).

### 3.3 `docs/CURRENT_STATE.md`

Landing-history entries for P35–P41 cite R-numbers as live results. The
integrator banner-marked the section in the a001 push; nothing further.

### 3.4 `game/quest_rt.h` (P44-owned; report only)

Carries `UPLINK(P)` / `UP(P,name)` / `UPARG(P,k)` (a spelling for F1/F2 that
P44 project 3 will re-decide), `BITS("001")`, `BIT_SET/BIT_CLR/BIT_PUT`, the
`cvwn/sx16/trunc16` intrinsics and the withdrawn bit-assignment ruling. The
*facts* those spellings encode (F1, F2, F12, F13, P7) are here; the
spellings themselves are compiler-line design and are not salvaged.

### 3.5 `Work/emulation/quest.addrbook`

Two flags are **misleading**, not wrong: RETURN_MESSAGE `mixed:3/6` (F6 — the
3-arg caller is assembly) and the `nested` flag on `#`-lines (F3/F10 — those
are `nocall` ON-units, a different construct). The file is not in P45's
boundary; recorded for P46/M4a owners.

### 3.6 `game/routines/*.c`

Not a salvage source (a001). They are P35–P43 output. `OWNS.c`, `PICK_X_Y.c`,
`UPDATE_SCREENS.c`, `REFRESH_SCREEN.c` matched the book register-exact
through the void translator, which (DESIGN §1) is still a real proof of
recovery **given the translator was sound at those sites** — a condition P44
project 4 is what establishes. Treat them as strong drafts for L1, not as
sources of record.

---

## §4 Compiler claims — VOID

By rule token, one line each, no restatement. **CR** = census-recoverable
under P44 §13 project 8 (a book-wide count could re-derive it as an *oracle
statistic*, never as a rule carried forward).

| rule | one-line reason it is gone | CR |
|---|---|---|
| R1, R2, R2a | slot assignment order = allocator policy; P44 places `v`s by oracle | yes (slot bijections over the book) |
| R3, R3b (voided by P38), R3b′ | temp-slot reuse timing; two witnesses on opposite sides of a statement boundary | yes |
| R4 | field K folding — **not void; this is IR.md §5's spelling, live** | — |
| R5, R41, R41′ | **PARTIALLY VOID — see F23.** The base-register CLASS {ac2,ac3} is an ISA fact, not a compiler claim (19,344 base uses in the book, ZERO on ac0/ac1). Void: the PREFERENCE ORDER "ac2 then ac3", which really was read off three routines. The SD_PTR census (M1) survives as a measurement | order: yes (M1 is the census) |
| R6, R6′, R7, R7a, R7b, R7c (falsified), R7c′, R7d | the protection-cost model; the whole register allocator | partly (R7d's loop-header table is a 3-row census) |
| R8, R8a, R8b, R8c, R8d | register-knowledge lifetime across labels/joins; P2 is the phenomenon | yes |
| R9, R9a, R10 | CSE temp creation/consumption; P11 records the unexplained counter-instance | yes |
| R11 | multiply-by-constant register choice | yes |
| R12 | marker-word test — the *fact* is F5; the rule (which spelling when) is unfalsifiable | no |
| R13, R13b, R13c | if/then shape and branch inversion; DESIGN §6 makes block order an oracle decision | yes (`quest.blocks.split`) |
| R14, R14b, R15 | Nova skip forms — these are **instruction semantics**, lifted in IR.md; not void, not a rule | — |
| R16, R16b | when `cvwn` is emitted — DESIGN §10 derives the min-diamond rule from the spec instead | no |
| R17 | DERR-17 fold — live in P27 (`assumed-foldable`), spelling not rule | — |
| R18 | dummy-store deferral; P13 records the counter-instance | yes |
| R19 | long-jump stubs at routine end; block placement is an oracle decision | yes |
| R20 | operand evaluation order; P40 measured 163/242 and could not settle it | yes (statement-level, not block-level) |
| R21, R21a, R21b, R21c, R21c′, R21d, R21e, R21e′, R22 | DO-loop register/limit/entry-test shape; P4 records the observations | yes |
| R23, R23a, R23r | indexed store/read address order; R23a (stride 1 → no scaling) is arithmetic, not a rule | yes |
| R24 | lazy LDAFP after WCMV | yes |
| R25 | CHAR argument temp building | yes |
| R26, R26a, R26b, R27 (amended), R28 | bit-address arithmetic register choices; the *address formula* is F17 | yes |
| R29, R29a | bit materialise / set-then-undo — recorded as P7 | yes |
| R30 | game→game call slot order — the *slot layout* is the addrbook's (`wfp-10-2n`, live); the store order is void | — |
| R31, R32 | located-string address term order — a rendering port of `string_sites.py`, which is live; nothing to salvage separately | — |
| R33, R34 | frame-pointer displacement mechanism; P5/P6 are the phenomena | yes |
| R35, R35a | slotpatch return — the *fact* is F4; R35a's "not a one-word THEN" is shape | no |
| R36, R36′, R36a | the hoist and its placement — P3 is the phenomenon; DESIGN §4.2 already owns it | yes |
| R37 | zero via `WSUB r,r` — P10 | yes |
| R38 | X.CB literal rebuild — the *fact* is F12 | no |
| R39, R40 | METHOD-RULINGs, live in METHOD §16 | — |
| R42, R43, R44, R45 | the static link — F1/F2/F3 are the facts; R44's "R41 base when a base" is allocation | no |
| FP_COST = 2 | a cost-model parameter with only a lower bound | no |
| "R3 falsification by DIFF(reg), never refused" | a working rule of the comparator; obsolete with it | — |
| P36 rulings 1/B, P37 amendment (literal fits 16 bits → no CVWN) | translator policy; the CVWN observation itself is DESIGN §10 | — |

---

## §5 What was not read, and what might be in it

- **`translate.py` (3,148 lines)** — grepped for R-numbers and PCs only.
  Its docstrings hold the `string_sites.py` port and two bad R28 PCs (P41);
  nothing else in it can be a program fact that the reports do not already
  state. Risk: low.
- **`docs/attic/Project35/PLAN.md`, `Project36/PLAN.md`, P35/P36 REPORT
  bodies beyond the findings sections, `REPORT_worklog.md`** — not read
  line by line. Possible content: per-slot bijection tables for the three
  P35 routines (a MEASUREMENT of the book, CR-relevant), the P35 corrections
  list (PICK_X_Y type polarity, the PROMPT's instruction-count transposition,
  RANDOM_NUMBER$3 argument order) — the last two are stated in P35 §5.10 and
  taken as Reported.
- **`Project36/*.ir`, `*.cmp*.txt`, `Project35/results/`, `Project37/results/`**
  — comparator outputs; nothing to salvage.
- **`Project40/QUEST1_PREDICTIONS.md`, `QUEST1_ABANDONED.md`, `Project41/R41_AUDIT.md`,
  `FIRE_DERIVATION_FROM_2.md`** — read only for the facts cited above; their
  bodies are translator diagnostics.
- **`REWRITE_PLAN.md`, `Project42/43` PROMPTs** — skimmed; design, no
  program facts.
- **`NextSession.pre-P44.md` lines 283–829** — pre-attic history (P24–P34
  hand-offs), superseded by live docs; the non-compiler items in its brief
  (play driver, Gen 6.2, floats, M5b) are all in live `NextSession.md`.
- **Not verified**: the P38 349-statement "frame survives a join" count
  (P2), the P40 163/242 block measurement, the P39 `M32[wp(·,-6)]` census's
  **ac2 = 276** (P45 counts 288 `XWLDA 2,[ac3+0x7FFA]` in the listing —
  the difference is presumably blocks not in the book; not chased).
