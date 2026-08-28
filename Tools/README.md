## Build:
```
javac *.java */*.java
jar -cvf tools.jar *.class */*.class
```


## Generate all components:

```
java -cp tools.jar debug.SymbolTable ../QUEST/QUEST.ST >quest.symbols
java -cp tools.jar StartStop ../QUEST QUEST "SQR31?3" "?CHAR_TO_UNSIGNED" "SQR31?3" >quest.addrs
java -cp tools.jar Follow ../QUEST QUEST quest quest.addrs
java -cp tools.jar DisassembleBlocks ../QUEST QUEST quest.targets quest.tags >quest.blocks
java -cp tools.jar Disassemble ../QUEST QUEST quest.addrs >quest.dis
java -cp tools.jar Disassemble ../QUEST QUEST quest.addrs nocode >quest.mem
java -cp tools.jar Disassemble ../QUEST QUEST quest.addrs nomem >quest.code

java -cp tools.jar StartStop ../QUEST QUEST "?CHAR_TO_UNSIGNED" "?NTOP" "?NBOT" >quest-rt.addrs
java -cp tools.jar Disassemble ../QUEST QUEST quest-rt.addrs >quest-rt.dis

java -cp tools.jar ArgWindows ../QUEST QUEST quest.addrs quest.targets ../Work/c_src/quest.addrbook quest.dis quest
```

## StartStop Utility � Reachability Analysis

**Purpose:** Starting from known entry points, follow all reachable code paths
to determine which addresses contain executable code vs data.

**Input:**
- `<dir>` � filesystem root directory
- `<PR file>` � program file (e.g. `quest`)
- `<Start Symbol>` � first function symbol (e.g. `SQR31?3`)
- `<Stop Symbol>` � symbol marking end of code (e.g. `?CHAR_TO_UNSIGNED`)
- `<Const Last Symbol>` � last constant/memory symbol (e.g. `SQR31?3`)

**Output:** `quest.addrs` � address file with tagged ranges:
```
mem 70000000 70010000      # memory/data region
code 7015BD20 7017DA75     # executable code region
mem 7017DA75 7017E000      # more data
disp 70160190 701601A0     # dispatch/jump table
```

**How it works:**
- Seeds with all symbol addresses in the [start, stop) range
- Follows all branches, calls, jumps, skip instructions, dispatch tables
- Handles special cases: LJSR to I.EPILOG/I.STOP (no return), I.PROLOG
  (returns to pc+7), XCALL (discovers internal call targets), LDSP (dispatch tables)
- Marks each reachable word address
- Outputs contiguous ranges tagged as `code`, `mem`, or `disp`

**Key details:**
- WRTN, DERR, WPOPJ are terminal (no successors)
- SYSCALL 0310 is terminal (process exit)
- XPSHJ (push-jump) returns to pc+2
- SWAT.REX has special entry points at +0x23 and +0x3E
- Dispatch tables (LDSP) are auto-discovered and their jump targets followed

---

## Follow Utility � Control Flow Analysis

**Purpose:** For every reachable instruction, determine its successor addresses
and identify block boundaries (targets).

**Input:**
- `<dir>` � filesystem root directory
- `<PR file>` � program file
- `<SS file>` � address file from Stage 1 (e.g. `quest.addrs`)

**Output:** Two files:
- `quest.targets` � block start addresses (one hex address per line, `%08X`)
- `quest.tags` � control flow tags for every instruction (`%08X <tag>`)

**Tag format:**
```
7015BD20 n 7015BD22              # simple fall-through
7015BD24 n 7015BD25 7015BD26     # conditional: two successors
7015BE74 c 70176FDD n 7015BE78   # call: target + return address
7015C068 j 7017FCE8 n            # JSR to I.STOP: no return
7015D064 j 7017ED9B n 7015D067   # JSR to O.ON: returns to pc+3
7015D055 j 7017E733 n 7015D05C   # JSR to I.PROLOG: returns to pc+7
7015BD46 n                       # terminal: WRTN, DERR, SYSCALL 0310
```

**Tag types:**
- `n [addr...]` � next addresses (0 = terminal, 1 = unconditional, 2 = conditional)
- `c <target> n <return>` � LCALL/XCALL: call target, execution resumes at return
- `j <target> n [return]` � LJSR: jump-subroutine, optional return
- `u` � unresolvable (shouldn't appear in practice)

**How it works:**
- Loads code locations from the Stage 1 address file
- Walks every code address, decodes each instruction
- For each instruction, computes successor addresses based on instruction type
- Adds branch/call targets and fall-throughs to the targets set
- Symbol addresses within code regions are also targets (function entry points)

**Special cases handled:**
- I.EPILOG, I.STOP: LJSR with no return (`j <target> n`)
- I.PROLOG: LJSR returns to pc+7 (exception handler setup)
- O.SERROR: LCALL that doesn't return (throws)
- LDSP: dispatch table with multiple jump targets
- novaCompute format: skip codes 0 (no skip), 1 (always skip), 2+ (conditional)
- XNDO/XWDO, LNDO/LWDO: loop instructions with two successors


## DisassembleBlocks Utility � Block Disassembly

**Purpose:** Disassemble code into basic blocks with human-readable output,
absorbing ASSERT patterns (bounds check + DERR) inline.

**Input:**
- `<dir>` � filesystem root
- `<PR file>` � program file
- `<targets file>` � block starts from Stage 2
- `<tags file>` � control flow tags from Stage 2
- `[<symbols file>]` � optional additional .SYM file

**Output:** `quest.blocks` � basic blocks with instructions and terminators:
```
# DISPLAY_INVENTORY
701674B5:
WSAVS 0x009E;
XNLDA 0,@[ac3+0xFFF4];
WSGTI 0,10 (0x000A);
WSGT 0,0;
DERR 17;
n 701674BD

701674BD:
NLDAI 686 (0x02AE),1;
WMUL 1,0;
LWADD 0,[0x70000210];
...
n 701674C7 701674C8
```

**ASSERT pattern absorption:**
The disassembler detects 6 bounds-check patterns and keeps them inline
(no block split) instead of treating the skip instructions as block boundaries:

| # | Pattern | Meaning |
|---|---------|---------|
| 1 | `WSGTI R,N; WSGT R,R; DERR` | assert(0 < R = N) signed |
| 2 | `WUGTI R,N; WSGT R,R; DERR` | assert(0 < R =u N) unsigned |
| 3 | `NLDAI N,Rx; WSLE Rx,Ry; DERR` | assert(Ry = N) |
| 4 | `WSLT R1,R2; WSLE R1,R3; DERR` | assert(R3 = R1 = R2) |
| 5 | `WSLT R,R; WUSGE R,R2; DERR` | assert(R = 0u) |
| 6 | `WSUB R1,R2; WULEI R2,N; DERR` | assert(R2-R1 =u N) |

When absorbed, the block terminates with `n <next_addr>` (fall-through past the DERR)
instead of DERR's dead-end `n`.

**Symbol labels:** Function names from the .ST symbol table appear as comments:
```
# DISPLAY_INVENTORY
701674B5:
```

---
## ArgWindows Utility — Call-Site Arg-Push Census

**Purpose:** For every call whose target is a GAME routine (the address
book's entry set is the authority), identify the arg-push window and
classify it. Straight-line windows are CLEAN and emitted mechanically;
anything with real control flow or stack-state-touching instructions is
PROBLEMATIC with the reason named, for human analysis. Feeds M4b's
push_map (see docs/Project15/REPORT.md).

**Input:**
- `<dir>` filesystem root directory
- `<PR file>` program file (e.g. `quest`)
- `<addrs file>` code ranges from StartStop (e.g. `quest.addrs`)
- `<targets file>` block starts from Follow, cross-checked against an
  independently recomputed target set (e.g. `quest.targets`)
- `<addrbook file>` game entry set (e.g. `../Work/c_src/quest.addrbook`;
  all entries count, #-commented or not)
- `<dis file>` disassembly for the sanity count (e.g. `quest.dis`)
- `<argmap out>` `<callsites out>` output paths

**Output:** Two files that the tool verifies against each other before
exiting (every arg line belongs to a CLEAN/CLEAN-WITH-INNER-CALLS site;
every such site has exactly argc lines):

`quest.argmap` — one line per arg for CLEAN sites only. `argN at xxxx`
is the pc of the push producing arg N. Slots are by DEPTH, not push
order: arg N lives at [wsp-2N] at the call == [wfp-10-2N] in the callee,
so arg1 is the LAST-pushed wide. A multi-wide WPSH yields several slots
at the same pc.
```
RETURN_MESSAGE arg1 at 7015BE72
RETURN_MESSAGE arg2 at 7015BE6F
```

`quest.callsites` — every game-target call site (the complete census),
one line each, ending with a summary (totals per class, PROBLEMATIC by
reason, sanity lines):
```
call AUTO_MOVE,2 at 70160A94 CLEAN
call READ_IN,0 at 70162010 CLEAN-EMPTY
call STORE,1 at 70166C10 PROBLEMATIC stack-op-in-window WPOP@70166C0C
```

**How it works:**
- Decodes the code ranges forward (Follow's decode rules, including the
  SYSCALL 3-word special case), then walks BACKWARD from each call doing
  stack-depth accounting until 2*argc words of pushes are attributed
- Push widths: XPEF/LPEF/XPEFB/LPEFB = 1 wide; WPSH s,d = ((d-s+4)%4)+1
  wides. An inner returning call accounts net -2*argc_inner (its WRTN
  consumed its args and the frame word the call pushed), and the walk
  continues backward to cover the inner call's own pushes
- The window is [first attributed push pc, call pc)

**Classification (conservative — a wrong CLEAN is expensive, a spurious
PROBLEMATIC is cheap):**
- `CLEAN` — accounting closes exactly; nothing inside the window but the
  attributed pushes and stack-neutral compute; no branch, skip, jump,
  dispatch, or syscall; no branch target strictly inside the window (the
  window-start pc itself is exempt — control arriving there still
  executes every push)
- `CLEAN-WITH-INNER-CALLS` — as CLEAN plus inner returning call(s) whose
  return word's only static source is that call
- `CLEAN-EMPTY` — argc==0, no window
- `PROBLEMATIC(reason)` — everything else, no recovery attempted: any
  instruction touching stack state inside the window (WPOP, WMSP, WFPSH,
  STASP, LDATS, ...), skips/branches, mid-window targets, unresolved
  widths, non-returning inner calls, discontinuous code, a push pc
  feeding two different call sites

**Cross-checks performed:** total call edges vs the dis-derived count;
all 63 XCALL sites with their WMOV/XWLDA static link immediately before
the call (reported if a link sits inside another site's window); the
recomputed target set vs the targets file.
