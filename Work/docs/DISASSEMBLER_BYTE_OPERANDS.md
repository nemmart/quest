# Disassembler byte-operand rendering — standing defect, DEFERRED fix

Status (user ruling, Aug 29 2026): **known-defective, deliberately
unfixed.** The user examined the byteIndexed disassembler paths, found
the masking buggy in multiple ways, and is reluctant to change
anything for fear of breaking something load-bearing. Until that risk
calculus changes, the listings are wrong in the specific, bounded way
below, and lower.py's reconstruction shim is a STANDING compensator,
not a temporary one. (This is a recorded deviation from METHOD §14's
fix-and-regenerate default, by user ruling; the defect is bounded and
the compensation is loud about ambiguity, which is why the deviation
is tolerable.)

## The defect

For **L-form byte-EA operands** (LLEFB, LPEFB, LLDB, LSTB), bit 31 of
the 32-bit displacement is DATA — the top bit of a byte-pointer
constant, whose segment lives in bits 31:29 (set_byte_segment
packing). The disassembler applies the word-operand convention
instead: it treats bit 31 as an indirect flag, prints an `@` prefix,
and strips the bit from the printed number. Both halves of the
rendering are wrong for byte operands: the `@` implies an indirection
that does not exist (neither engine's eagle_l_byte_indexed has any
indirect handling, nor an ii=0 arm — Project25/ByteEA.md §2, read from
the source), and the printed value is missing bit 31.

Canonical example, verified against raw QUEST.PR words (offset
0x28FD68):

    7015c2b4 LPEFB @[0x60001998];      <- rendered
    raw displacement: 0xE0001998       <- actual (byte ptr, word 0x70000CCC)

## Scope (exact, from the pattern census)

**184 lines**: 183 LLEFB + 1 LPEFB — every L-form byte operand whose
displacement has bit 31 set (i.e. every seg-7 byte-pointer constant,
absolute or as the table base of an `[acN+const]` register-indexed
form). The 10 LLEFB lines WITHOUT `@` are genuine small relative
displacements (bit 31 naturally clear) and render correctly. X-form
byte operands are unaffected: their displacement is a full 16-bit
field with NO indirect bit. XLDB/XSTB/LLDB currently show no `@`
lines in quest.dis. Related but distinct word-form inconsistency,
also compensated in lower.py: X word forms KEEP the indirect bit in
the printed number with a decorative `@` (e.g. `XNLDA 1,@[ac3+0xFFEC]`),
while L word forms strip it (e.g. `LPEF @[0x70000210]` for raw
0xF0000210).

## The compensation (tools/lower.py)

- Byte L-forms: `@` -> reconstruct raw = printed | 0x80000000 (user
  instruction, Aug 29). `@` together with bit 31 already set refuses
  (can't happen under either convention — ambiguity is refused, not
  guessed).
- Byte X-forms: `@` refuses outright (unrenderable — no such bit).
- Word forms (reconcile_at): both conventions accepted; bit set
  WITHOUT `@` refuses.

The shim makes lower.py correct against BOTH the current defective
listings and a future corrected rendering (where the byte lines would
print the full value with no `@`), so a fix — if ever taken — does
not break the emitter.

## If the fix is ever attempted (METHOD §14 protocol)

Regenerate all listings, then diff old-vs-new and skim every changed
line. The expected diff is EXACTLY the 184 lines above; anything
more is itself a finding (and given the user's observation that the
byteIndexed disassembler masking is broken in several ways, a larger
diff is plausible — each extra line is a place where prior readings
worked from a wrong rendering). Precedent: the WLDAI
omitted-register-field bug (METHOD §14). The raw words are the
arbiter; QUEST.PR byte checks settle any dispute.
