# PICK_X_Y — a hand reading from the ir 3 book (Sep 5 2026 planning session)

Routine 0x701761E7 (96 instructions), read straight off the IR blocks
in a few minutes with nothing but the block dump. The target for
tools/readable.py: a renderer that gets most of the way here
mechanically, and a census of what it cannot.

```
PICK_X_Y(x*, y*):                          ; args by reference at [ac3-12], [ac3-14]
retry:
  n      = M32[base + 11502]               ; region count
  r      = ?RANDOM_NUMBER(&seed, &n, &1)   ; r in 1..n
  assert 0 < r <= 100000                   ; DERR 17 subscript check
  rec    = base + r*9                      ; region table, 9 words per record
  if M16[rec + 11495] == 0  goto retry     ; empty slot
  if M16[rec + 11497] / 100 + 1 == 3  goto retry   ; region type 3 excluded
  *x = ?RANDOM_NUMBER(&seed, &(cx + 20), &(cx - 20))   ; cx = M16[rec+11495]
  *y = ?RANDOM_NUMBER(&seed, &(cy + 20), &(cy - 20))   ; cy = M16[rec+11496]
  if !(15349 < *x <= 16300) goto retry     ; world bounds
  if !(15219 < *y <= 16350) goto retry
  return
```

Meaning: pick a random non-empty, non-type-3 region, then a random point
within ±20 of its centre, rejecting points outside the map rectangle.

What read cleanly from the IR as it stood: control structure (backward
gotos = loops, two-way gotos with the condition inline = ifs, assert =
the PL/I bounds check), arithmetic chains (`mul/div/add/cvwn`), the RT
calls (the callee name says what the block does), by-reference args
(`R[ac3 − 12]`).

What was gobbledygook: `M32[0x70000212]` (a static base), `wp(ac3, 4)`
(a frame local), `M16[wp(ac2, 11495)]` (a record field). Consistently
so: every one is `base + index·stride + constant`, i.e. exactly what a
typing pass needs.

Note on indexing: PL/I arrays are 1-based; `base + r·9 + 11495` with the
guard `0 < r` means the region table's origin is `base + 11504` and
11495/11496/11497 are field offsets 0/1/2 of a 9-word record — the
"element 0" slot does not exist. Rule for the renderer: `base + i·stride
+ K` with a `0 < i` guard is a 1-based array with origin `K + stride`.
