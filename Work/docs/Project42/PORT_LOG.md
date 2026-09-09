# P42 Stage 3 — the port log, per production

**The rule this table exists to enforce** (user ruling): *a template that no
bed routine exercises cannot be verified by translating the bed.* A green run
on the four matched routines says nothing about a production they never fire.

**But behaviour preservation is not "matches the book" — it is "emits exactly
what it emitted before the port".** So a routine that does not match the book,
or that refuses partway, is still a perfectly good behaviour-preservation
witness for any production it fires. `compiler/witness.sh` is that harness:
eight routines, byte-identical output required across all of them, plus
349/349 + 242/242 on the four that are supposed to match.

Witness classes:

- **BED** — fired by at least one of the four matched routines. Verified by
  byte-identical output *and* by the match.
- **EXTRA** — fired only by an unmatched routine (DIED, HIT_ANY_CHAR,
  LIST_PLAYERS.3@7016F556, QUEST.1@7015C5E1). Verified by byte-identical
  output only; these routines are not expected to match and are not asked to.
- **NO WITNESS** — fired by nothing available. **Not ported.** Left on the old
  path with the reason recorded, per the ruling.

| production | witness | ported | witnesses |
|---|---|---|---|
| `abs_builtin` | BED | no | UPDATE_SCREENS×2 |
| `address_of` | BED | no | REFRESH_SCREEN×1, HIT_ANY_CHAR×1 |
| `assign_indexed` | BED | no | UPDATE_SCREENS×1 |
| `assign_local` | BED | no | PICK_X_Y×2, UPDATE_SCREENS×1 |
| `assign_rt` | BED | no | PICK_X_Y×1 |
| `assign_uplevel` | **NO WITNESS** | no | — |
| `binop_const_addi` | BED | yes | PICK_X_Y×4, UPDATE_SCREENS×2 |
| `binop_const_inc` | BED | yes | PICK_X_Y×1 |
| `binop_const_scale` | BED | yes | PICK_X_Y×1 |
| `binop_reg_mem` | **NO WITNESS** | no | — |
| `binop_reg_reg` | BED | no | UPDATE_SCREENS×4 |
| `bit_address` | BED | no | OWNS×7, QUEST.1×2, LIST_PLAYERS.3×2, DIED×1 |
| `bit_base` | BED | yes | OWNS×7, QUEST.1×2, LIST_PLAYERS.3×1, DIED×1 |
| `bit_stmt` | EXTRA | no | QUEST.1×2 |
| `bit_value` | BED | no | OWNS×7 |
| `condition_bit` | EXTRA | no | LIST_PLAYERS.3×2, DIED×1 |
| `condition_cmp` | BED | no | OWNS×8, QUEST.1×2, PICK_X_Y×6, REFRESH_SCREEN×2, LIST_PLAYERS.3×1, UPDATE_SCREENS×2 |
| `const_materialise` | BED | yes | OWNS×2, PICK_X_Y×1, REFRESH_SCREEN×9, LIST_PLAYERS.3×2, HIT_ANY_CHAR×1 |
| `convert` | BED | no | PICK_X_Y×2 |
| `do_loop` | BED | no | OWNS×1, QUEST.1×1, REFRESH_SCREEN×1, LIST_PLAYERS.3×1, UPDATE_SCREENS×1 |
| `element_address` | BED | no | QUEST.1×4, PICK_X_Y×6, LIST_PLAYERS.3×1, DIED×2, UPDATE_SCREENS×4 |
| `field_direct` | BED | yes | QUEST.1×1, PICK_X_Y×1, LIST_PLAYERS.3×1, UPDATE_SCREENS×1 |
| `field_element` | BED | no | QUEST.1×4, PICK_X_Y×6, LIST_PLAYERS.3×1, UPDATE_SCREENS×4 |
| `game_call_stmt` | EXTRA | no | HIT_ANY_CHAR×1 |
| `goto` | BED | yes | PICK_X_Y×6, DIED×1 |
| `if_multi` | BED | no | OWNS×7, REFRESH_SCREEN×2 |
| `if_oneword` | BED | no | OWNS×1, QUEST.1×2, PICK_X_Y×6, LIST_PLAYERS.3×3, DIED×1, UPDATE_SCREENS×2 |
| `link_load` | EXTRA | yes | LIST_PLAYERS.3×1 |
| `marker_ref` | BED | no | REFRESH_SCREEN×1 |
| `prologue_epilogue` | BED | no | OWNS×1, QUEST.1×1, PICK_X_Y×1, REFRESH_SCREEN×1, LIST_PLAYERS.3×1, DIED×1, UPDATE_SCREENS×1, HIT_ANY_CHAR×1 |
| `return_value` | BED | no | OWNS×9, LIST_PLAYERS.3×2 |
| `return_void` | BED | yes | QUEST.1×1, PICK_X_Y×1, DIED×1 |
| `rt_call_stmt` | BED | no | PICK_X_Y×3, REFRESH_SCREEN×5, HIT_ANY_CHAR×2 |
| `scalar_ref` | BED | yes | OWNS×11, QUEST.1×8, PICK_X_Y×9, REFRESH_SCREEN×2, LIST_PLAYERS.3×4, DIED×1, UPDATE_SCREENS×10 |
| `temp_place` | BED | no | PICK_X_Y×6, REFRESH_SCREEN×10, HIT_ANY_CHAR×1 |
| `uplevel_arg_ref` | **NO WITNESS** | no | — |
| `uplevel_ref` | EXTRA | no | LIST_PLAYERS.3×1 |

## The three with no witness — none ported

| production | why no witness | disposition |
|---|---|---|
| `assign_uplevel` | needs a write through `UP()`/`UPARG()`. FIRE.1 writes wp(link,14) but has no `.c`; FIRE.2 has one and **refuses** at the cvwn/divide width question before reaching its uplevel statements. | left on the old path |
| `uplevel_arg_ref` | needs `UPARG()`. FIRE.2 is the only routine with one (7016A479) and it refuses first. | left on the old path |
| `binop_reg_mem` | needs a 32-bit memory right operand riding straight into XWADD/LWADD. No available routine produces one. | left on the old path |

`FIRE.2`'s refusal was **not** weakened to manufacture a witness. It refuses on
a genuine open question (the cvwn/divide width), that refusal is correct, and
P41 recorded it as unchanged. Weakening it would have bought a witness by
destroying the thing the witness was supposed to check.

## Orphan emissions — code belonging to no production

| site | instructions | why |
|---|---|---|
| `hoist_invariant_subscripts` | 2–3 | R36/R36a: the loop-invariant hoist. A **placement** decision, not a reduction. Fires in OWNS only. |

Deliberately **not** given a 38th production: an orphan is a finding about the
phase structure, and housing it would convert that finding into a modelling
detail. Every orphan is a candidate transform-phase decision, and this list is
what a phase split would be built from.

