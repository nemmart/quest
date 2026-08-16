# Project 15 — ArgWindows: the call-site arg-push census (Java, Tools family)

Hi Claude! Solo implementation session; user reviews at the plan gate
and the landing. This is a PURE ANALYSIS project: one new Java tool in
the Tools/ family, no emulator changes, no book changes, nothing runs
under lockstep. Output feeds M4b planning (docs/M4bNotes.md, open
question 3: the dataflow census — does the "never" bucket exist?).

## The goal, in the user's words

The utility does NOT have to handle every LCALL. It gets the automatic
stuff done so we're not dealing with so much bulk that needs careful
analysis. Straight-line arg-push windows are classified CLEAN and
emitted mechanically; anything with real control flow is classified
PROBLEMATIC with the reason named, for human analysis later. Do not be
clever at the margin — a wrong CLEAN is expensive, a spurious
PROBLEMATIC is cheap. When in doubt, PROBLEMATIC.

## What it computes

For every call whose target is a GAME routine (LCALL absolute or XCALL
pc-relative, target in the game range — the book's entry set is the
authority, Disassembled/quest.addrbook or regenerate):

1. **argc** from the call instruction.
2. **The arg window**: walk BACKWARD from the call doing stack-depth
   accounting until 2*argc words of pushes are attributed:
   - XPEF/LPEF = 1 wide (2 words); XPEFB/LPEFB = 1 wide; WPSH s,d =
     (d-s+1) wides — credit true widths.
   - An INNER call encountered mid-walk accounts net backward as
     -(2 + 2*argc_inner) words (its args + frame word were consumed by
     its WRTN) and the walk continues backward to cover the inner
     call's own pushes as well. This makes f1(f2(x), y) fall out as a
     rule, not a special case.
   - The window = [first attributed push pc, call pc).
3. **Classification**:
   - **CLEAN**: depth accounting closes exactly; no branch, jump, skip
     (the SKIP_INSTRUCTIONS set in Follow.java), or dispatch
     instruction inside the window; no address inside the window
     appears in the global branch-target set (Follow's quest.targets
     is the starting authority — verify it covers skip fall-throughs,
     or compute the target set independently).
   - **CLEAN-WITH-INNER-CALLS**: as CLEAN, plus inner call(s) where
     the only in-window address in the target set is the word after
     each inner call, and its only static source is that call's
     return. (LCALL looks like control flow; confirm nothing else
     targets the return word.)
   - **PROBLEMATIC(reason)**: anything else — depth doesn't close,
     a conditional/skip/branch inside, a target lands mid-window, an
     inner call's return word has another source, a push pc that
     appears to feed two different call sites, indirect/unresolvable
     push widths. Name the reason; do not attempt recovery.

## Output format — TWO files (user ruling)

**File 1 — `quest.argmap`: the arg list.** Machine-consumable, the
future push_map's direct ancestor. Only CLEAN and CLEAN-WITH-INNER-CALLS
sites appear. One line per arg:

```
ATTACK arg1 at 70163A20
ATTACK arg2 at 70163A22
ATTACK arg3 at 70163A26
ATTACK arg4 at 70163A2A
FIND_OBJECT arg1 at 70164AF0
FIND_OBJECT arg2 at 70164AFC
```

`argN at xxxx` = the pc of the push instruction producing arg N (slot
numbering per the frame layout, arg N at [wsp-2N], derived from DEPTH
accounting, not push order). argc==0 sites contribute nothing here.

**File 2 — `quest.callsites`: every game-target call site, handled or
not.** One line per site, the complete census:

```
call ATTACK,4 at 70163A2E CLEAN
call FIND_OBJECT,2 at 70164B02 CLEAN-WITH-INNER-CALLS inner=DIST@70164AF8
call READ_IN,0 at 70162010 CLEAN-EMPTY
call STORE,1 at 70166C10 PROBLEMATIC target-lands-in-window 70166C0C
```

Every site classifies as exactly one of CLEAN / CLEAN-WITH-INNER-CALLS
/ CLEAN-EMPTY (argc==0) / PROBLEMATIC(reason). File 2 ends with the
summary census: totals per class, PROBLEMATIC grouped by reason. The
two files must agree: every arg line in file 1 belongs to a site
classified CLEAN or CLEAN-WITH-INNER-CALLS in file 2, and every such
site has exactly argc lines in file 1 — the tool verifies this before
exiting.

## Cross-checks (the tool must verify its own totals)

- Total game-target call sites found == the known census (1749 total
  call edges in quest.dis include RT calls; the tool derives its own
  game-target count and prints it beside a grep-derived count from
  quest.dis as a sanity line).
- All 63 XCALL sites appear, each with its static-link WMOV/XWLDA
  immediately before the call (report if the link instruction sits
  inside another site's window — it shouldn't).
- REFRESH_SCREEN (argc 0/1) and RETURN_MESSAGE (3/6): mixed arity is
  PER-SITE and should classify normally (M4bNotes: mixed argc is
  convertible; the M4a book flag was conservatism).
- argc==0 sites: window is empty, trivially CLEAN — count them
  separately in the summary.

## Build/run (Tools family conventions)

Tools/ builds with `javac *.java */*.java`; the new tool is
`ArgWindows.java` beside StartStop/Follow, reusing Memory, Instruction
decode, SymbolTable, and OldDisassembler word lengths (see Follow.java's
wordLength usage). Inputs: filesystem dir, PR name, quest.addrs (code
ranges), quest.targets, quest.symbols. Outputs written directly: `quest.argmap` and `quest.callsites` (paths
as arguments). Add the invocation line to Tools/README.md's
generation list.

## Boundaries

1. Analysis only. No emulator, book, or doc-of-record edits.
2. When classification is uncertain, PROBLEMATIC — never guess CLEAN.
3. If the .PR decode surprises you (an instruction Follow's tables
   don't cover in a window, an unresolvable width), that is a finding
   for the report, and those sites classify PROBLEMATIC(undecoded).
4. Deliverables: Tools/ArgWindows.java, quest.argmap, quest.callsites,
   docs/Project15/REPORT.md (the census summary, the PROBLEMATIC list
   with reasons, cross-check results, anything surprising about 1988
   codegen), README.md invocation line.
5. The report's headline is the census answer for M4b: how big is the
   CLEAN bucket, and does a "never convertible" class exist?

## Environment

Java toolchain: `javac`/`java` (verify availability first; if absent,
`apt`/container notes in docs/Run.md). ~49 s/turn budget applies; this
project is compute-light — most of the time is reading Follow.java's
decode patterns and getting widths right.
