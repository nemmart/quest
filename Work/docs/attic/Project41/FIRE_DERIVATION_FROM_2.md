# FIRE's frame layout, derived from FIRE.2 alone

Step 1 of the P41 Item 1 mutual check. Written from the book BEFORE
`game/declarations.json` was opened and before FIRE.1 was read.

Source: `emulation/quest.ir2.book` and `Disassembled/quest.dis`.
FIRE.2 @7016A461, addrbook range [7016A461, 7016A4F8), frame 0x0A, `nested`.

## Extent check

FIRE.2 runs from its `WSAVS 0x000A` at 7016A461 to `WRTN` at 7016A4F7,
entirely inside its addrbook range; the next entry (FIRE.3) begins at
7016A4F8 with its own `WSAVS`. No block of another family lies inside the
range and nothing branches in. So unlike DROP.1 (METHOD §16) this `.N@`
range is NOT mis-sized, and every uplevel reference below is FIRE.2's own.

## Method

`WSAVS` stores the enclosing frame pointer (passed in ac1) at `wp(fp, -6)`.
Every uplevel reference therefore begins `XWLDA r,[ac3+0x7FFA]`. FIRE.2
does this four times, always into ac2:

| PC | instruction |
|---|---|
| 7016A477 | `XWLDA 2,[ac3+0x7FFA]` |
| 7016A4A4 | `XWLDA 2,[ac3+0x7FFA]` |
| 7016A4D0 | `XWLDA 2,[ac3+0x7FFA]` |
| 7016A4E6 | `XWLDA 2,[ac3+0x7FFA]` |

(Four loads for five references — consistent with R43, re-loaded per block:
the load at 7016A477 serves both 7016A479 and 7016A485, which are in the
same block split only by the P27 DERR fold. ac2 is verified unwritten
between them: 7016A47F `NLDAI ...,0`, 7016A481 `WMUL 0,1`, 7016A482
`LWADD 1,[...]` all target ac0/ac1.)

## The five uplevel references

| PC | instruction | link displacement | form | width |
|---|---|---|---|---|
| 7016A479 | `XNLDA 1,@[ac2+0xFFF4]` | **−12** | indirect | 16-bit, sign-extended |
| 7016A485 | `XWLDA 0,[ac2+0xC]`      | **+12** | direct   | 32-bit |
| 7016A4A6 | `XNLDA 0,@[ac2+0xFFF4]` | **−12** | indirect | 16-bit, sign-extended |
| 7016A4D2 | `XWLDA 0,[ac2+0xC]`      | **+12** | direct   | 32-bit |
| 7016A4E8 | `XNLDA 2,@[ac2+0xFFF4]` | **−12** | indirect | 16-bit, sign-extended |

`0xFFF4` = indirect bit (0x8000) | `0x7FF4`, and `0x7FF4` is −12 in the
15-bit signed displacement field. `0x7FFA` is −6 likewise.

All five are READS. FIRE.2 writes nothing through the link.

## The argument-slot convention (derived, not assumed)

From OWNS @70175CBF, a two-argument routine whose source is proven
(152/152 MATCH): `*p` (argument 1) is `XNLDA 0,@[ac3+0xFFF4]` at 70175CC1
and `*item` (argument 2) is `XNLDA 2,@[ac3+0xFFF2]` at 70175CE3. So

> **argument k lives at word displacement −12 − 2(k−1)**, and the slot
> holds a by-reference POINTER, not the datum.

Displacement −12 is therefore **argument 1**. FIRE's addrbook argc is 2,
so −12 is in range. Independent corroboration inside FIRE's own body:
FIRE reads its own slot −12 indirectly at 7016A387 (`XNLDA 0,@[ac3+0xFFF4]`),
which is the same slot and the same indirect form — so −12 is an argument
slot of FIRE on FIRE's own evidence too.

## The derivation

| parent slot | kind | width | witness PCs (FIRE.2) |
|---|---|---|---|
| **−12** | FIRE's **argument 1**, by reference | datum **16-bit** signed | 7016A479, 7016A4A6, 7016A4E8 |
| **+12** | FIRE's **local variable** | **32-bit** | 7016A485, 7016A4D2 |

Nothing else. FIRE.2 reaches exactly two slots of its parent.

## Corroborating detail (why the widths are believable)

Both values are used identically: each is immediately bounds-checked
`1..10` (`WSGTI r,10` / `WSGT r,r` / `DERR 17`, the P27-folded assert) and
then multiplied by 686 and added to `M32[0x70000210]` — i.e. both are
subscripts into the same 686-word-stride array based at 0x70000210. Two
values with the same use and the same range, but stored differently:
one 16-bit and reached by reference (an argument), one 32-bit and reached
directly (a local). The width difference is not an artifact of the use;
it is a property of the storage, which is what makes it evidence.

- `XNLDA` is the narrow (16-bit) load, sign-extending into the 32-bit
  register; `XWLDA` is the wide (32-bit) load. The compiler's choice of
  mnemonic is the width witness.
- A 16-bit local would still be read `XNLDA` (R2: it occupies a whole
  2-word slot but is loaded narrow). +12 is read `XWLDA` at both sites,
  so the datum there is genuinely 32 bits wide.
