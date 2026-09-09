# FIRE's frame layout, derived from FIRE.1 alone

Step 2 of the P41 Item 1 mutual check, done from the book by the same
mechanical procedure as `FIRE_DERIVATION_FROM_2.md` and still before
`game/declarations.json` was opened.

FIRE.1 @7016A3BD, addrbook range [7016A3BD, 7016A461), frame 0x04,
`push,nested`, argc 0.

## Extent check

`WSAVS 0x0004` at 7016A3BD to `WRTN` at 7016A460, wholly inside the
addrbook range; FIRE.2's `WSAVS` begins at 7016A461. Not mis-sized.

## One wrinkle FIRE.2 did not have

FIRE.1's prologue is

    7016A3BD  WSAVS 0x0004
    7016A3BF  WMOV 1,2

so the FIRST uplevel reference uses the link **still in ac1 from entry**,
copied to ac2, with no `XWLDA 2,[ac3+0x7FFA]` in front of it. A scan that
keys only on the link-load instruction would miss it. It is a link
reference on R42's own terms (ac1 holds the enclosing frame pointer at
entry) and it is self-confirming: 7016A3C0 reads displacement +10 off the
ac2 from `WMOV`, and 7016A424 and 7016A44F read the SAME displacement +10
off an ac2 loaded from `[ac3+0x7FFA]`. Same slot by two routes.

A second wrinkle: at 7016A3DA the link is loaded into **ac3**, displacing
the frame pointer, so 7016A3DC's `[ac3+0xE]` is link-relative and not
frame-relative. (This is one of the three `ac3 = <address>` sites P38
cites as evidence for R41.)

## The link references

| PC | instruction | link displacement | form | width |
|---|---|---|---|---|
| 7016A3C0 | `XWLDA 0,[ac2+0xA]`  | **+10** | direct | 32-bit read |
| 7016A3DC | `XWSTA 0,[ac3+0xE]`  | **+14** | direct | 32-bit **write** |
| 7016A3F2 | `XPEF [ac2+0xE]`     | **+14** | direct | address taken (arg 3 of `UPDATE_SCREENS`) |
| 7016A401 | `XWLDA 0,[ac2+0xE]`  | **+14** | direct | 32-bit read |
| 7016A424 | `XWLDA 0,[ac2+0xA]`  | **+10** | direct | 32-bit read |
| 7016A44F | `XWLDA 0,[ac2+0xA]`  | **+10** | direct | 32-bit read |

## The derivation

| parent slot | kind | width | witness PCs (FIRE.1) |
|---|---|---|---|
| **+10** | FIRE local | **32-bit** | 7016A3C0, 7016A424, 7016A44F |
| **+14** | FIRE local, written and read uplevel | **32-bit** | 7016A3DC (w), 7016A401 (r), 7016A3F2 (&) |

Nothing else. FIRE.1 reaches no argument slot at all.

## Corroborating detail

+10 is bounds-checked `WUGTI 0,100000` / `WSGT` / `DERR 17` at all three
sites and then multiplied by **9** and added to `M32[0x70000212]` — a
subscript into a different array from the one FIRE.2 indexes (686-stride
at 0x70000210). So +10 and FIRE.2's slots are not the same variable seen
twice; they are different variables.

+14 is the only slot in either sibling that is WRITTEN uplevel, which is
what makes its 32-bit width a `XWSTA`/`XWLDA` pair rather than a single
load: store at 7016A3DC, read back at 7016A401 across an intervening
`LCALL UPDATE_SCREENS`. Both instructions are wide. Its address is also
passed to `UPDATE_SCREENS` at 7016A3F2. The three `XPEF`s are emitted
right-to-left, so 7016A3F2 — the FIRST push — supplies the LAST argument,
and `UPDATE_SCREENS`' matched source (72/72 MATCH, proven) declares
that parameter `const int32_t *cell`. That is an independent confirmation
of +14's 32-bit width from a routine outside the FIRE family entirely,
and it is the strongest single piece of width evidence in either
derivation, because it comes from a source that is already proven rather
than from a mnemonic reading.
