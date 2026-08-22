# Finding B — the I2 heap-fence fix (implementation task)

*Ruled Aug 22 2026 (M4aDesign §12, from REPORT_FINDING_B.md). A Mapper
change: I2 currently forbids legitimate heap allocation during signal
handling. Fix I2, do NOT touch handlers or the book. Validate TOGETHER
with the Finding-A `>=` fix on the fo leg.*

## What's true (established by the investigation)

wsl is dual-purpose on this runtime: stack limit AND stack/heap fence.
I?ALLOC (real STASL 0x7017E903 / native i_alloc.cpp:191) and I?FREE move
BOTH wsl and the heap break (rt::HEAP_BREAK, 0x700001F0) by the same
size, same direction, at every live alloc/free. The fail-open path
allocates a 14-word message buffer this way; I2's "wsl constant while
records live" wrongly aborts on it.

## The fix (two parts, both in the Mapper's I2 check)

1. **Latch `wsl − heap_break`, not `wsl`.** At the latch site
   (Mapper.cpp ~:343) record `latched_diff = wsl − read_wide(HEAP_BREAK)`.
   At each i2_assert (push_record / wrtn_fixup / unwind_to touchpoints)
   assert the DIFFERENCE is unchanged, not wsl itself. Legitimate
   alloc/free moves both together → difference constant → no abort. A
   real slip (wsl moved without heap_break) → difference changes →
   abort, exactly as I2 intends.

2. **Stack-clearance bound at each i2_assert:**
   `wsl > max(live wsp, max over live records of master-side extent hi)`.
   The reclassified band [new_wsl, old_wsl) must lie strictly above all
   stack-leg activity so the leg's identity/compression split stays
   well-defined.

## Do NOT build or run — hand back the change; godspeed tests it

Container builds/batteries are slow; testing happens on a fast server
(godspeed) after you hand back. Your job is the CODE CHANGE plus a clear
report of what you changed and how to verify it. Do not spend time
building or running the battery.

Confirm the current tree already has the Finding A fix (`grep 's >= it->W'
Work/c_src/hw/Mapper.cpp` → present); your I2 change sits alongside it.

## What godspeed will run to verify (describe expected results in your report)

- **fo leg in `failopen` DRIVER MODE** (login → L→P; drive.py mode
  `failopen`), NOT `m` mode — this is the condition that keeps a game
  record (LIST_PLAYERS / GET_INPUT) live across the ?LIB_ERROR heap
  allocation and ARMS I2. Before the fix it aborts (latched 7001715A →
  7001714C); after the fix it should be GREEN, 0 div, the fail-open
  handler running to its handled recovery.
- **m / inj / abort / play** on the 101 book — no regression (Finding A
  stays green).

State in your report the exact expected before/after so godspeed's run
is a clean confirmation. If your reasoning says the stack-clearance
bound (2) could be crowded by DISPLAY_SCREEN's post-Finding-A extents on
some path, flag it as a thing to watch in the godspeed run.

## Deliverable

hw/Mapper.{hpp,cpp} (I2 check + latch), a short REPORT_FINDING_B_FIX.md
(the change, the fo-green result, the no-regression table), updated
Work.tgz. Do NOT edit the design-of-record docs beyond the report.
Mapper.md's I2 wording gets updated by the planning session on review.
