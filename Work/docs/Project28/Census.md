# Project 28 — Phase A: rt_call census, design findings, grammar draft (plan gate)

Session Sep 5 2026, solo. TREE VINTAGE: main @ 9972b85 (P27 merged
bd3369c; sync list of record c_src/quest.synclist.p27; artifacts
verified against docs/Provenance.md — quest.dis 5c1db5fb…,
blocks.split 1d3baaf6…, ir2.book 1dc6356a… with 6,258 embeds,
ir2.stock d5e4cb13… with 8,137). Branch p28-rt-call (rebased onto
main after the P27 merge, per the user). Phase A touches only
tools/rt_sites.py and docs/Project28/; Phase B has not started.

Tool: `c_src/tools/rt_sites.py` (text-only; imports lower.py's parsers
and `pef_value` so the argument expressions here are the ones the
emitter will render). Runtime 1.1 s. Raw output of record:
`rt_sites.out`; per-site table: `rt_sites.tsv` (987 lines).

    python3 Work/c_src/tools/rt_sites.py --dis Disassembled/quest.dis \
      --blocks Work/c_src/quest.blocks.split --rt-dis Disassembled/quest-rt.dis \
      --tags Disassembled/quest.tags --sites-out rt_sites.tsv > rt_sites.out

## 1. Site census — reproduces the Sep 5 numbers exactly

| measure | value |
|---|---|
| `LCALL [..],argc; # ?NAME` sites | **987** |
| argc == wides captured, in-block | **987/987**; cross-block 0 |
| distinct callees | **18** (prompt: "~15") |
| window shape: contiguous | **887** |
| interleaved: XLEF | **72** |
| interleaved: XLEF+XWSTA | **15** |
| interleaved: XWSTA | **11** |
| interleaved: XWSTA+XLEF+XWSTA | **2** |
| pushes | **2,879** = XPEF 2,126 + LPEF 750 + WPSH 3 (each 1 wide) |
| XPEFB / LPEFB at RT sites | **0** |
| refusals | **0** |
| arguments needing a t-place | **0** (all 2,879 inline at the call) |

By callee (argc sets; native = hw/RTStubs.cpp translation_table):

| callee | sites | argc | entry | native today |
|---|---:|---|---|---|
| ?WRITE_SCREEN | 723 | 2 (×436), 5 (×287) | 7017E27A | no |
| ?RANDOM_NUMBER | 111 | 3 | 7017DE33 | no |
| ?UNSIGNED_TO_CHAR | 89 | 1 | 7017DA75 | **yes** |
| ?DELAY | 18 | 1 | 7017DC63 | no |
| ?READ | 11 | 4 (×5), 6 (×1), 7 (×5) | 7017DE5F | no |
| ?CHAR_TO_UNSIGNED | 10 | 1 | 7017D99B | no |
| ?OPEN_FILE | 6 | 2 | 7017DD27 | no |
| ?CLOSE_FILE | 4 | 1 | 7017DB63 | no |
| ?OPEN_SHARED_IO_FILE | 3 | 5 | 7017DDBB | no |
| ?GET_SHARED_PAGE | 3 | 4 | 7017DC7D | no |
| ?WRITE | 2 | 3 (×1), 6 (×1) | 7017E1FC | no |
| ?CREATE_TASK | 1 | 2 | 7017DBB3 | no |
| ?AWAIT_CONSOLE_INTERRUPT | 1 | 0 | 7017DB4B | no |
| ?LOOKUP_PORT | 1 | 3 | 7017DCCB | no |
| ?LIB_ERROR_CODE | 1 | 0 | 7017DE25 | **yes** |
| ?CONNECT | 1 | 1 | 7017DB9B | no |
| ?CURRENT_PID | 1 | 0 | 7017E12B | no |
| ?READ_SCREEN | 1 | 3 | 7017DEEB | no |

Argument expression forms over all 2,879 wides (pef_value output):
`wp(ac3, d)` 1,985; word constant (L-absolute or X pc-relative fold)
752; `wp(ac2, d)` 122; `R[ac3 + d]` indirect 17 (all at the ?WRITE 6-arg
site 7017D7A1 and ?READ sites — the routine's OWN arguments passed
through, `XPEF @[ac3+0xFFF4]`; none has an interleaved store);
`acN` (WPSH value) 3 (the three ?OPEN_SHARED_IO_FILE `WPSH 0,0`).

Interleaved instructions — what they are (all 100 sites):

| callee | instruction | sites | role |
|---|---|---:|---|
| ?UNSIGNED_TO_CHAR | `XLEF 2,[ac3+d]` | 89 | register argument: ac2 = destination word address (the native body reads it as `bridge.entry_ac(2)`, runtime/unsigned_to_char.cpp:67; the emulated body at 7017DB13 `XWLDA 2,[ac3+0x7FFC]`) |
| ?UNSIGNED_TO_CHAR | `XWSTA 0/1/2,[ac3+d]` | 19 | compiler spill of a live register into a frame local before the call |
| ?RANDOM_NUMBER | `XWSTA 0,[ac3+d]` | 11 | same spill |

Every XLEF writes ac2; every argument in those windows reads ac3 (or is
a constant) — hence 0 t-places. XWSTA writes a frame local; wp()/bp()
arguments read no memory, and the 17 R[] arguments have no interleaved
store. Reordering the pushes after the XLEF/XWSTA (which is what
evaluating the arguments AT the call does) is therefore unobservable:
same values, same stack slots, same order of stack writes.

### Worked example (from the census; the grammar note's example)

    7015C042 XPEF [ac3+0x14];            ; pushed FIRST  -> arg 2 (text)
    7015C044 LPEF [0x70000260];          ; pushed LAST   -> arg 1 (channel)
    7015C047 LCALL [0x7017E27A],2; # ?WRITE_SCREEN
    =>  rt_call ?WRITE_SCREEN(0x70000260, wp(ac3, 20)) site=7015C047

    7015D92C XPEF [ac3+0x2C];
    7015D92E XLEF 2,[ac3+0x32];          ; register argument, lowers as
                                         ;   ac2 = wp(ac3, 50)   (P25 form)
    7015D930 LCALL [0x7017DA75],1; # ?UNSIGNED_TO_CHAR
    =>  ac2 = wp(ac3, 50)
        rt_call ?UNSIGNED_TO_CHAR(wp(ac3, 44)) site=7015D930

Reader sees PL/I order (arg1, arg2, …); the executor pushes argN first.

## 2. Design-vs-reality findings (boundary 5) — RULED Sep 5

### F1. rt_call is a TERMINATOR (ruled: agreed)

All 987 LCALLs are the LAST instruction of their blocks.split block:
every site's block ends with a `c <callee> n <site+4>` edge line and
site+4 is a block start 987/987 (rt_sites.py verifies; a `c` edge is
how Follow renders the call). The prompt's "never a terminator; the
fall-through continues in the same block" described a CFG that does
not exist for these sites. The embedded `@site LCALL` today is a final
instruction whose control transfer (to the runtime, outside
[game_start, game_stop)) is the block exit; the callee returns to
site+4, a block start. rt_call inherits exactly that: it mirrors P25's
`call` (a terminator carrying `site=`).

Ruling: `rt_call ?NAME(e1,…,eN) site=<hex8>`; successor = site+4 (must
be a listed block start); the LCALL instruction at `site` supplies the
target/argc from its own words and sets ac3 = site+4
(EagleStack.cpp:238–239, :273).

### F2. Variable argc callees (ruled: argc ∈ the callee's known set)

?WRITE_SCREEN {2,5}, ?READ {4,6,7}, ?WRITE {3,6}. Each reads its argument
count from the low word of the LCALL marker — `XNLDA n,[ac3+0x7FF7]` at
7017E27C / 7017DE71 / 7017E20E — and branches on it (rt/write_screen.hpp
already documents write_screen_2 / write_screen_5). Not a defect. The
RTConventions table records the SET; lower.py refuses a site whose argc
is outside its callee's set (a new argc would be a finding, not a
lowering).

### F3. Push mix (recorded; the prompt's counts were estimates)

2,879 = 2,876 XPEF/LPEF + 3 WPSH (all ?OPEN_SHARED_IO_FILE, `WPSH 0,0`);
the other 5 runtime-directed WPSH in quest.wpsh_wpop go to B.MOVE (2)
and C.TRANS (3) — not `?` callees, out of scope. No byte-pointer push
reaches a runtime routine (the 78 XPEFB/LPEFB in quest.dis are all
game→game, P25's ledger).

### F4. Right-to-left, from the source (cite in IR.md)

`RTBridge::arg_pointer(n) = M32[wsp − 2n]` with wsp at the LCALL marker
(hw/RTBridge.cpp:111; frame_word/argc at :105); the marker is
`(psr<<16)|argc` pushed by LCALL (EagleStack.cpp:239–244); WSAVS pushes
ac0 ac1 ac2 wfp ac3|c above it (:421–425), so at the callee arg n is at
`wfp − 10 − 2n` and the LAST push is arg 1. quest.pushmap.M4's site
comments (`# XPEF arg2` on the lower pc, `# LPEFB arg1` on the higher)
say the same for game→game.

### F5. Register conventions (recorded; hand-cited in RTConventions.md)

Only ?UNSIGNED_TO_CHAR reads an entry register (ac2, saved-ac2 slot
[ac3+0x7FFC] at 7017DB13; native `entry_ac(2)`). Four callees return a
value in ac0 by writing the saved-ac0 slot [ac3+0x7FF8] before WRTN:
?RANDOM_NUMBER (7017DE5B), ?CHAR_TO_UNSIGNED (7017DA13/DA39/DA69),
?LIB_ERROR_CODE (7017DE30; native `set_return_ac(0, code)`,
runtime/lib_error.cpp:473), ?CURRENT_PID (7017E146/E14A). Nothing in
the IR asserts any of it; the table is documentation and a check.

### F6. The stack-fault `pc=` text (ruled: set machine.pc = site; record why)

`Machine::wide_push` throws `Stack fault - upper limit - abort,
pc=%08X` naming `machine.pc` (Machine.cpp:87–95) and writes through
`copy_segment(pc, wsp)`. IRExec sets machine.pc only inside run_instr;
the rt_call executor must set `machine.pc = site` before its pushes so
the segment fold is the block's and the fault names a real pc.
RECORDED DIFFERENCE (honest correction to the Sep 5 flag): the master's
text would name the individual PUSH's pc (the XPEF/LPEF that overflowed),
the clone's names the LCALL site — the two strings differ in the pc
field. compare_pair compares exception text (METHOD §7). The fault
has never fired (wsl is far above every game frame; the game has no
recursion) and, if it ever does, it fires on both engines as an abort
whose only disagreement is that field. Not worth carrying push pcs in
the grammar; recorded here and in the IR.md note.

### F7. No EagleStack hoist is needed (recommendation)

"Perform the LCALL through the emulator's own LCALL path": the cleanest
callable form of that path is the instruction itself. `call` already
does exactly this for decorated sites — "executes the actual
instruction at `site` via the normal path" (IR.md §6). rt_call's
executor evaluates the arguments, pushes them via `machine.wide_push`
(argN first), then runs the LCALL at `site` through run_instr
(fetch/decode/execute, Capture hook, ovk/ovr check, instruction_count++,
IRExec.cpp:786–810) and returns its new_pc as the block exit — callee
entry, native return, or the syscall sentinel, exactly as the final
`@site LCALL` instruction does today. No change to EagleStack.cpp, so
the "K=1 stock gate for the hoist" has nothing to gate; the slice-(i)
gate covers the executor path.

## 3. Grammar draft (for IR.md — ir 3 → **ir 4**)

### §3 Block lines — new production

    rt_call <callee>(<expr>, ...) site=<hex8>
                                   Runtime call (P28). TERMINATOR. The
                                   LCALL at `site` (a `?`-prefixed
                                   callee) with its argument pushes
                                   folded into argument EXPRESSIONS in
                                   PL/I order: the first expression is
                                   argument 1. §6.

### §5.1 — additions

    stmt    := … (unchanged; rt_call is a terminator, not a statement)
    REFUSED AT LOAD (added): rt_call to a callee that is not a `?`
    symbol; a `site` whose word is not an LCALL; argc field at site+3
    (& 0x7FFF) != the number of argument expressions; site+4 not a
    listed block start; an argument expression that is not pure (any
    effectful op, any t-place — see the evaluation rule); rt_call
    anywhere but the last line of a block.

### §6 Operations — new entry

- `rt_call <callee>(e1, …, eN) site=<s>` — the game→runtime LCALL at
  `s` with its N argument pushes folded in. Executor: evaluate e1…eN
  (pure; order unobservable), then `machine.pc = s` (F6) and push eN
  first, e1 last, each through `Machine::wide_push` — the SAME helper
  XPEF/LPEF/WPSH call (EagleStack.cpp:665–701, :601), which owns the
  wsp==wsl overflow fault; then execute the LCALL instruction at `s`
  through the normal fetch/decode/execute path (as `call` does) and
  return its new_pc. The runtime then runs exactly as today (emulated,
  a logging stub, or a native body — the LCALL body's own registry
  lookup, EagleStack.cpp:284–303). **Evaluation order**: right to left
  — eN is pushed first and lands deepest, e1 last and nearest the
  callee's frame, because the callee reads arg n at wsp−2n from the
  LCALL marker (RTBridge.cpp:111) and the marker sits below the
  WSAVS-saved registers (EagleStack.cpp:421–425); the pushmap's arg
  comments and the M4a layout `arg n at wfp−10−2n` are the same fact.
  Example: `rt_call ?WRITE_SCREEN(0x70000260, wp(ac3, 20)) site=7015C047`
  pushes wp(ac3,20) then 0x70000260; the callee's arg 1 is the channel
  word at 0x70000260, arg 2 the text at ac3+20.
- **No wsp arithmetic outside the helper.** rt_call is the one
  construct that moves wsp, and it does so only inside `wide_push`.
  Stack-register WRITES stay refused (§5.1); `M32[wsp+2] = …; wsp =
  wsp + 2` spelled out is refused as it is today. The 23 P20 borrow
  brackets are unaffected (t-places, §5.4).
- **Arguments** (P25 grammar, nothing new): XPEF/LPEF → `wp(base, d)`,
  a word constant (L-absolute / X pc-relative fold), or `R[base + d]`
  (indirect operand); XPEFB/LPEFB → `bp(...)`/`0xW:b` (none occur at
  RT sites — kept in the rule for completeness); WPSH x,a → the
  register values `acx … aca` in ascending order (ac x pushed first,
  so it is the HIGHER-numbered argument). The census decides inline vs
  t-place per site: inline when the expression reads only state no
  interleaved statement writes (987/987 today); otherwise `tK =
  <expr>` at the push's position and `tK` as the argument (legal,
  unused); refuse if an interleaved store could reach an R[] operand.
- **Interleaved register arguments / spills** are ordinary preceding
  statements in program order (`ac2 = wp(ac3, 50)`; `M32[wp(ac3, 20)]
  = ac0`), before the rt_call. rt_call does not declare them; the
  per-callee table (RTConventions.md) documents which registers each
  callee reads on entry / writes on return.
- **Refuse list** (emitter; censused): window not [pushes + XLEF/XWSTA];
  argc != wides captured; window crossing the block start; argc not
  in the callee's known set (RTConventions.md); pef_value cannot render
  a push; an indirect argument with an interleaved store; the site not
  block-final; site+4 not a block start.
- Both modes: `rt_call` is stock-legal and book-legal (real stack in
  both; no book slots, no argpush). Block-ordinal accounting and the
  ovk/ovr check are unchanged.

### §4 Terminator rule — amended

"the last line of every block is an instruction, `call`, `rt_call`,
`ret`, or `goto`."

### §9 Version history — ir 4 entry

ir 4 (Project 28): `rt_call <callee>(args) site=<s>` terminator
(987 runtime call sites; right-to-left evaluation rule, real stack in
both modes, pushes only through `wide_push`); LNDO lowered as XNDO with
the L-form EA (fall-through pc+4); LDSP as range-assert + `goto`
table; the 67 Nova LOAD forms lowered pure with the high half
zero-filled as the emulator does (HWFindings_Sep5.md §3: UNDEFINED per
the manual — a don't-care, not a contract). Superset of ir 3; ir 3
files refused (regenerate together, as always).

## 4. Leftovers census

### LNDO 7015C0C7 — `LNDO 0,25,[0x70000216]` (block 7015C0C5, block-final)

Successors in tags/blocks.split: [7015C0CB, 7015C0E1] = [pc+4,
pc+1+25]. EagleGeneral.cpp:225–236 is XNDO (:167–178) with an L-form
EA and a 4-word length. Lowering = the existing XNDO shape with
`parse_memop(pc, "[0x70000216]", wide=True)` and `ctx.next_pc` = pc+4:

    t1 = nadd(M16[0x70000216], 1)
    M16[0x70000216] = t1
    t2 = (t1 >s ac0)
    ac0 = t1
    goto [7015C0CB, 7015C0E1] t2

(lower.py's XNDO path checks successors == [next, pc+1+arg]; LNDO
needs the same check with next = pc+4.)

### LDSP pair — tables decoded from the dis rendering, cross-checked against quest.tags

| site | block | index | table | range | entries | valid | −1 | tags |
|---|---|---|---|---|---:|---:|---:|---|
| 701604D1 `LDSP 1,…(0x70160191)` | 701604C4 | ac1 | 70160191 | [2, 66] | 65 | 22 | **43** | MATCH (23 listed, 21 distinct + fall) |
| 7016D704 `LDSP 0,…(0x7016BF1F)` | 7016D702 | ac0 | 7016BF1F | [1, 37] | 37 | 37 | 0 | MATCH (38 = 37 + fall) |

Semantics (EagleGeneral.cpp:251–260): L = M32[tbl−4], H = M32[tbl−2];
if L <=s idx <=s H and the entry is not −1 → jump to it; else fall
through to pc+3 = the `DERR 17` sink (701604D4 / 7016D707 — each a
one-instruction block whose ONLY predecessor is the LDSP, tags). Every
target is a listed block start. FINDING: the prompt's `assert(range);
goto [t1..tN] idx−lo` cannot express table 1 — 43 of its 65 entries are
−1 and also reach the DERR.

Options (both DERRs are terminal by ruling, P27 §"Why the fold is
faithful"):

- **A1 (recommended)**: `assert((lo <=s acX) && (acX <=s hi), "DERR 17
  @<sink>"); goto [L(lo) … L(hi)] acX − lo` where a −1 entry's label is
  the DERR-sink block. The sink stays listed and stays an embedded
  `@pc DERR 17` — a VERIFIED terminal pair (the master aborts at the
  same pc; the stronger F2-b-style pairing), 2 embeds remain, no
  grammar beyond rt_call. Out-of-range uses the P27 assert/detach
  pairing task 040 just proved (F2-a).
- A2: no assert — `goto [sink, L(lo)…L(hi)] ((acX − lo) + 1) *
  (((lo <=s acX) && (acX <=s hi)))`: every DERR path a verified pair,
  but the index expression is opaque. Not recommended.
- B (user's variant): the sink block becomes `assert(0, "DERR 17 @pc")`,
  retiring the last two embeds. It needs a grammar addition — an
  assert is not a terminator (§4), so either `assert(0, …)` becomes a
  permitted final line or the block needs a dead `goto`; and it trades
  the verified pair for the detach pairing. Acceptable per the user;
  A1 is judged cleaner. **Default A1 unless ruled otherwise.**

Loader belief check (cheap, recommended): at load, read L/H and the
table entries at `tbl` from memory and refuse if they disagree with
the label list (the entries are static data in QUEST.PR).

### Nova LOAD forms — 68 in blocks.split, **67** lowerable

The 68th is `SUB.ZR 1,1` at 7015BD6B, inside the permanently excluded
block (interior LJSR) — hence P26's 67. Shapes (op, CC, SS, skip):

| op | CC | SS | skip | count |
|---|---|---|---|---:|
| ADD | – | – | – | 23 |
| MOV | – | L | – | 19 |
| ADC | – | – | SKP | 6 |
| ADC | C | – | SNC | 4 |
| ADD | – | S | – | 4 |
| ADD | O | – | SBN | 4 |
| SUB | C | L | – | 4 |
| SUB | Z | R | SNC | 2 |
| MOV | – | S | – | 1 |
| SUB | Z | R | – | 1 (7015BD6B, excluded) |

16 carry a skip and all 16 are block-final (split-CFG invariant holds);
52 do not (23 happen to be block-final — the block ends for another
reason; they lower as plain statements). Reference: NovaCompute.cpp —
17-bit value :22–42, shift/carry :44–61, the load `machine.c = c;
machine.ac[YY] = src` when N == 0 :63–66 (src is masked to 16 bits by
every SS arm, so the high half is zero-filled), skip :68–77.

Ruled: per-shape PURE lowering, no `nova()` helper — the P26 no-load
decomposition (`nova_test`, ruling R3) plus two assignments:

    t1 = <17-bit ALU value>        ; as nova_test today
    c  = <carry bit of t1>         ; 0/1 by construction (lsh(..)&1)
    acY = <shifted 16-bit result>  ; high half ZERO — emulator's value;
                                   ;   UNDEFINED per the manual (don't-care)
    [goto [fall, skip] <test>]     ; iff a skip is present (block-final)

The test reads t1 (not the new c/acY) — identical values, and the
order of the two assignments is unobservable. Nova ops never touch ovr.
The 8 `ADC c,c,SKP` sites are unconditional skips (`goto [fall, skip]
1`), and the carry-consumer blocks 70160E64/E65/E73/E74,
7016E75B/E75C/E76A/E76B already have a coverage verdict in the 034/040
template.

## 5. Battery plan — task 041

040's script as the template (034's 13 legs + `derr` + `derr-emu`,
JOBS=3, source via `bin/task_source.sh p28-rt-call 041-p28-rt-call`),
IR paths → the regenerated ir 4 artifacts, `QUEST_SYNC_LIST` →
quest.synclist.p27 (unchanged by P28: no block is added or removed;
rt_call keeps every site's block and its site+4 successor). Verdict
lines added after the collation:

- `embeds_book == 2322`, `embeds_stock == 4201` (from lower.py's
  summary line, "instr"); `rt_call == 987` in both modes.
- `rt_sites: emitted=987 refused=0` — from lower.py's rt_call census
  (any refusal lists site + reason).
- rt_call coverage from the `IRExec: first execution of block <pc>`
  lines of every IR leg's .err, joined with rt_sites.tsv (site → block):
  per callee `executed/emitted`; **require ?WRITE_SCREEN > 0 and
  ?RANDOM_NUMBER > 0 in the play/fo legs** (login writes the screen;
  RANDOM_NUMBER runs on the first turn), report the rest.
- Leftover coverage: LNDO block 7015C0C5 (the init loop — expected
  LIVE at startup), the two LDSP blocks 701604C4 / 7016D702 (reach
  unknown — carried on census classification if not hit, per the P25
  precedent), Nova blocks = the existing carry-consumer list.
- Everything else as 040: 15/15 green, 0 div, blk_mismatch 0, the
  armed-pc drop counts, the derr pair at 7015C48E.

Landing bar: 15/15 green, 0 div, every censused site emitted or
listed as refused with reason, embeds == the prediction above,
?WRITE_SCREEN and ?RANDOM_NUMBER rt_call blocks LIVE.

## 6. Embed prediction (landing-bar basis, ruled)

| mode | now (post-P27) | − pushes | − LCALL | − LNDO/LDSP/Nova | **predicted** |
|---|---:|---:|---:|---:|---:|
| book | 6,258 | 2,879 | 987 | 1 + 2 + 67 | **2,322** |
| stock | 8,137 | 2,879 | 987 | 70 | **4,201** |

(The prompt's ≈4,600 was 8,529 − 3,866 − 70 against the pre-P27 book
baseline.) Under LDSP option A1 the two DERR-sink embeds remain in
BOTH counts (they are the 2 P27 left; the 2 LDSP instructions
themselves go). ir2 statements will rise by ≈ 987 rt_call terminators
+ 100 interleaved statements + Nova/LNDO/LDSP lines; `goto` count
unchanged except +1 LNDO, +2 LDSP.

## 7. Phase B slices (as prompted; all behind K=1 book + stock gates)

1. contiguous ?WRITE_SCREEN sites — 723 (all 723 are contiguous: the
   100 interleaved sites are 89 ?UNSIGNED_TO_CHAR (XLEF 72, XLEF+XWSTA
   15, XWSTA+XLEF+XWSTA 2) + 11 ?RANDOM_NUMBER (XWSTA));
2. the remaining 164 contiguous sites (17 callees);
3. the 100 interleaved sites (their XLEF/XWSTA already lower today as
   `ac2 = wp(...)` / `M32[...] = acN` — the slice is only the rt_call);
4. leftovers: LNDO, LDSP (A1), 67 Nova loads.

IRExec: parse `rt_call` (terminator kind), evaluate args, `machine.pc =
site`, `wide_push` right-to-left, run_instr(site), return new_pc;
loader refusals per §3. lower.py: window capture in emit_block (the
site is block-final, so the pushes and interleaved lines are the tail
of the block), argc-set table from RTConventions.md, refuse list.
IR.md → ir 4; artifacts + Provenance.md regenerated with headers.

STOPPED AT THE PLAN GATE. Phase B after the user's review.
