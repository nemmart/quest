# Finding B — the fail-open −14-wide wsl motion: investigation brief

*Spun out of P14 B2 Stage 2 (see REPORT_B2 §3, M4aDesign §12). Analysis
only — read the disassembly/handler, do NOT change the Mapper or relax
I2. Output is a characterization + a recommended ruling for the planning
session.*

## The fact

On the widened (≥ ~99-live) book, the fail-open leg aborts:
```
FAIL_OPEN: QUEST1 denied :USER_DATA_FILE
MAPPER I2: wsl moved while records live (latched 7001715A, now 7001714C)
pc=7017EC7C   (a game record — LIST_PLAYERS, or GET_INPUT with it commented — live)
```
wsl drops by 0xE = 14 wides at pc 7017EC7C, in the L2 signal-delivery
region (O?SIGNAL 7017EDED, R?SIGNAL 7017EF54 just above), right after the
fail-open denial. Proven RECORD-AGNOSTIC: commenting the live routine
relocates the abort to the next live record with the identical move —
so it is the handler path, not any routine.

## Why it's newly visible

I2 ("wsl constant while records live") only arms when a game record is
live across the wsl adjust. On the batch-2 book the routines active
across the fail-open path were stacked, not area-live, so I2 never
armed and fo was clean (P14 §7). B2 keeps a record live across the same
path → the pre-existing motion trips I2.

## The question to answer

**What lowers wsl by exactly 14 wides on the fail-open path, and is it a
legitimate stack adjustment or an accounting slip?**

Trace, from the disassembly + the native handlers:
- `Work/c_src/runtime/o_signal.*`, `r_signal.*`, `mv_error_handler.*`,
  `native_error_handler.*`, `lib_error.*`, `def_on.*` — whichever runs on
  the ?OPEN fail-open → signal → handler path.
- Find the wsl write (or the wsp/wsl-coupled op) near the pc region
  around 7017EC7C / O?SIGNAL 7017EDED. 14 wides = 28 words — look for a
  handler frame of that size, a WMSP in the handler, or a wsl re-latch.

## The two outcomes (and their rulings)

1. **Legitimate handler stack adjustment** (the fail-open handler
   genuinely establishes a frame / lowers wsl as part of correct signal
   delivery): then I2 is too strict — it must tolerate a signal-frame
   wsl delta while records are live. Options: re-latch wsl across the
   handler window, or bound the stack leg by the post-adjust wsl. This
   is a Mapper/L2 contract change — hand back a precise proposal; the
   planning session rules, it is not patched here.
2. **Accounting slip** (the native fail-open handler moves wsl when the
   real machine would not, i.e. an emulator bug in the handler, not a
   real DG behaviour): fix it in the handler (boundary 3), I2 stays
   strict, fo re-runs clean.

Deliver: the identified wsl write (file:line, pc), the 14-wide account,
which outcome, and the recommended ruling. Nothing else changes.

## Note

Finding A (DISPLAY_SCREEN, I4) is RULED separately (M4aDesign §12,
excluded to M4c). This brief is Finding B only. They share a theme —
the widened live set puts records where M4a's closed form didn't have to
hold — but are mechanically distinct and independent.
