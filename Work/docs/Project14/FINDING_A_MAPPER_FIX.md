# Finding A — the stack-leg least-upper-bound fix

*Ruled Aug 22 2026. A real Mapper stack-leg bug, not a routine to
exclude. This is a boundary-2 mapper change: implement carefully, verify
both directions, re-run. Supersedes the earlier "exclude DISPLAY_SCREEN
to M4c" draft.*

## The bug (verified arithmetic)

DISPLAY_SCREEN: WSAVS N → clone executes WSAVS 0 (linkage on real stack,
N-word frame in area at 74003xxx), then MSP grows a dynamic tail below
its linkage marker on the real stack. Crash record: master_wfp=7000115A,
frame=547, master extent [lo=7000114E, hi=700015A2). The MSP'd
real_wsp=700010E6 sits 52 wides below lo.

Current stack leg (Mapper.cpp, ToMaster):
```
for records innermost-first:
    if (s > it->W) return s + it->shift_after;
```
This picks the wrong frame and applies a blanket compression shift:
700010E6 + 0x6A = 70001150, which is INSIDE DISPLAY_SCREEN's own master
extent. ToClone's `lo <= s < hi` branch then sends 70001150 to the AREA
(74003332). Round trip 700010E6 → 70001150 → 74003332 ≠ start. I4 fires.
0 divergences: the game is fine, the mapper's self-check is what trips.

## Why it's a bug, not a limit

The MSP tail belongs to DISPLAY_SCREEN's activation. On the master it
sits contiguously just below DISPLAY_SCREEN's frame (same activation,
nothing interleaved — the routine is running; nobody else pushes until
it calls out, and a callee's frame goes below the tail, still ordered).
So the tail HAS a correct, unambiguous master address: the corresponding
offset below DISPLAY_SCREEN's master marker. The mapper just wasn't
computing it — it applied the compression shift (a "stack lifted out
below me" quantity) to an address that is part of the lifted frame's own
activation, the one case where that shift is wrong.

## The fix — least-upper-bound attribution

Stack grows down ⇒ a frame's MSP tail is at LOWER addresses than the
frame's marker ⇒ the owning frame is the nearest marker ABOVE the
address. So:

- **Attribute a stack address `s` to its LEAST-UPPER-BOUND live frame**:
  the record whose marker is the smallest one still >= s. (Not the
  current "largest W below s".) This is a SEARCH over live records, not
  scalar shift arithmetic.
- **Translate `s` as its offset within that frame's activation**, to the
  corresponding offset below that frame's MASTER marker.
- **ToClone uses the same attribution** so the round trip closes: a
  master tail address resolves back to the stack, not the area.

Get the exact offset arithmetic right against the emulator's real
WSAVS-0 + MSP layout (what W, master_wfp, and the tail offset actually
are in the record) — do not eyeball it from this one dump. The record
may need to carry enough to distinguish "my own activation's tail" from
"a genuinely-inner frame"; add fields if needed.

## Verify

- DISPLAY_SCREEN m leg: 101 live, 0 div, I4/round-trip clean, anchors
  exact, I.STOP detach.
- The other large dyn/push routines the m/play drivers reach (the report
  §4 latent suspects) — same clean.
- Re-run the full battery legs that were green (m/inj/abort) to confirm
  no regression from the stack-leg change.
- This likely subsumes Mapper §3b's "record-order not address-order" for
  the stack leg — note it in Mapper.md when the fix lands.

## Boundary

This is a Mapper change = boundary 2 (design care, not a quick patch),
but it is IN SCOPE for finishing M4a — the alternative (excluding
routines whose dynamic tail trips the closed form) would leave M4a
permanently unable to migrate large dyn routines, which defeats the
"all callable routines" goal. Fix the mapper.
