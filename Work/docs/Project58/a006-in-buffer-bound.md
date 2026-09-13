# a006 — `IN_BUFFER`'s capacity is 132, established end to end. Check it, then take the 30.

Integrator, Sep 13 2026. Your a005 report says:

> the biggest remaining class IS an inline invariant — of a static, not a frame
> slot — and it is **blocked by one declaration this book does not carry**

**The book does carry it.** It is a literal at the single call site. Verify
each link below independently, then apply it.

---

## The chain

**1 — `IN_BUFFER` is a static varying at `0x7000021C`** (`quest.symbols`
line 32), data at `0x7000021D`. The next symbol is `OUT_CHAN` at `0x70000260`,
so the object spans `0x44` words = 136 bytes, i.e. **134 bytes of data** after
the length word. *(An adjacency bound, and only a cross-check — the real bound
is below.)*

**2 — exactly one routine fills it.** Program-wide, `0x7000021C` appears as a
call argument in only two shapes:

```
rt_call ?READ_SCREEN(0x70000262, 0x7000021C, wp(ac3, 12))     ×1   ← the writer
rt_call ?CHAR_TO_UNSIGNED(0x7000021C)                          ×10  ← readers
```

**3 — the `max_length` argument is a LITERAL at that call site**, block
`7017670A`:

```
ac2 = 0x00000084                    ; 132
M16[wp(ac3, 12)] = trunc16(ac2)
rt_call ?READ_SCREEN(0x70000262, 0x7000021C, wp(ac3, 12)) site=70176716
```

One call site, one constant, no path dependence.

**4 — `?READ_SCREEN` honours it, structurally**
(`emu_types/OperatingSystem.cpp:284`):

```cpp
std::vector<uint8_t> bytes(static_cast<size_t>(max_length));  // exactly 132
int32_t amount = ch->read(bytes, true);                       // cannot overrun
if(amount < 0) amount = 0;                                    // floored
result.assign(std::string(bytes.begin(), bytes.begin() + amount));
```

The buffer is **sized at `max_length`**, so `amount ≤ 132` by construction, and
the negative case is explicitly floored. `result.assign` then writes that count
into `IN_BUFFER`'s own length word.

**5 — it does not return a length.** `emu_rt/read_screen.cpp` constructs a
`VaryingString` over `arg_addr(2)` and returns `ei.wrtn_void()`. The count goes
into the header in place; nothing is passed back. (`max_length` is read as
`read_word(...) & 0xFFFF` — 16-bit, matching the caller's `M16` store.)

### Therefore

> **`0 ≤ len(IN_BUFFER) ≤ 132`**, and 132 < 134, so a copy into its data
> cannot reach `OUT_CHAN` or anything above it.

That is the fact your `StaticBounds` pass was defeated for — *"a copy into
IN_BUFFER's data whose count is a chain total the slot model cannot bound,
which, with no declared size for IN_BUFFER, the pass must treat as possibly
reaching every static above `0x7000021D`."* It has a declared size. It is 132.

---

## Tier, and the provenance to record

Not "the compiler is correct" and not an adjacency inference. The bound comes
from **the program's own stated limit**, honoured by an implementation written
from the DG syscall manuals (user, Sep 13) — so it models the documented
interface, and **clone and master execute the same path through it**.

Recommended `quest.assumptions` row:

```
in-buffer-cap-132   0x7000021C   -   compiler/countflow.py
#   IN_BUFFER (static CHAR VARYING, data at 0x7000021D) has length <= 132 whenever it is read.
#   Sole writer: ?READ_SCREEN at 70176716, max_length a literal 132 (block 7017670A, NLDAI 0x84).
#   ?READ_SCREEN sizes its read buffer AT max_length and floors a negative amount to 0
#   (emu_types/OperatingSystem.cpp:284-296; implementation written from the DG syscall manuals).
#   Cross-check: the object spans 0x7000021C..0x70000260 (OUT_CHAN) = 134 data bytes; 132 fits.
#   FALSIFIED BY: a second ?READ_SCREEN call site targeting 0x7000021C, or a max_length that is
#   not the literal 132, or any write to 0x7000021D.. that is not through this call.
```

---

## The work

1. **Verify all five links.** Two readings of the earlier site were wrong; do
   not inherit this.
2. **Re-run `StaticBounds` with the bound** and report the movement on the 30
   sites (DISPLAY_CAVE ×9, GET_QUEST ×4, LIST_PLAYERS.2 ×3, SEIGE ×3, …).
3. **Check the falsifier yourself**: is `70176716` really the only writer of
   `0x7000021D..`? Your own tool can answer that better than a grep.
4. **Then keep slimming.** After these, a005's table leaves the `M8[ac2]`
   cursor class (5), record-field capacities (8), argument cells (8),
   DISPLAY_SCREEN's mutual invariant (4), and the small list (9). **Ask the
   same question of each: is the bound a literal somewhere nobody looked?**

## The method finding, now seven for seven

This one was not found by a tool at all. The user asked *"IN_BUFFER is used for
reading from the user?"*, then *"is it passed to ?READ?"*, then *"?READ_SCREEN
has a max input length, right?"* — three questions, and the constant was four
greps away.

Every one of the seven has been a **model** fix, and every one was found by
asking a concrete question about a concrete object. The reflex when a tier
looks too large is to reach for more analysis; **on this project that reflex
has been wrong seven times out of seven.**
