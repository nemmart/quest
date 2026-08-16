# ?UNSIGNED_TO_CHAR (0x7017DA75) — Translation Derivation

Decoded from quest-rt.dis this session; implement next session. Body:
0x7017DA75..0x7017DB19, WSAVS 0x0019 (25-wide frame). Digit table at
0x7017DA6C (byte data BEFORE the entry — dump it: expected
"0123456789ABCDEF" or similar, indexed remainder+1, suggesting a
leading pad byte or 1-based indexing — VERIFY).

## Calling convention

- Stack args (by reference, standard): arg1 = &value (wide, the number
  to convert), arg2 = &base (narrow; clamped to [2,16]), arg3 = &width
  (narrow; only if argc>2; width 0 or argc<=2 → 32; else min(width,32)).
  argc==1 → base defaults to 10 (path at 7017DA9F).
- **Register argument (third "sick" pattern)**: the destination string
  address arrives in the CALLER'S ac2, read by the body from its own
  frame's saved-ac2 slot (7017DB13: XWLDA 2,[ac3+0x7FFC]). RTBridge
  already captures entry ac0-2 (saved_ac) — expose an accessor
  (e.g. entry_ac(n)) for the wrapper.
- Returns nothing in registers (no slot patch observed — WRTN restores
  entry regs; VERIFY no other stores to [ac3+0x7FF8..0x7FFF] beyond
  the READ at 0x7FFC).

## Decoded flow

1. Parse args per above → locals: [2]=argc, [3]=width-flag (raw arg3
   value or 0), [4]=base, [5]=width (32 or clamped arg3).
2. [8..9] = value (wide); [6] = 32 (buffer index, fills right-to-left);
   [0x1E] = width copy; [7] = 1 (XNDO loop counter).
3. Digit loop (XNDO at 7017DACB bounds it by width via [7]):
   - stage base as wide in [0x20..21]; call ?UDIV32(&[8], &[0x20],
     &[0xC]) — in the native subtree this becomes a direct
     rt::udiv32_3 call (quotient returned, remainder out).
   - [0xA..B] = quotient (wide store of returned ac0).
   - remainder reloaded as a WIDE from [0xC..D] (7017DADE) although
     ?UDIV32 only narrow-wrote [0xC] — **[0xD]'s high half is residue
     the code then feeds through CVWN**. Determine CVWN's exact
     semantics; the emulated result presumably masks/converts so the
     residue is harmless, but the native must reproduce the same
     final value AND the same memory bytes.
   - digit char = table[remainder+1] (byte load via
     XLEFB [pc+..] → 0x7017DA6C:1 byte space + WADD); stored at
     scratch byte [0x1B-area + index] (XLEFB [ac3+0x1B]); index [6]
     decrements.
   - value = quotient. Loop continues while quotient != 0 (WSEQ 0,0 at
     7017DAF2 — VERIFY skip sense: reads as skip-if-ac0==0), and when
     width-flag != 0, continues regardless until XNDO exhausts width
     (zero-padding to the requested width).
4. Finish (7017DAF8..): first-digit position = [6]+1; length =
   33 - position (VERIFY WSUB operand direction at 7017DAFD);
   byte-pointer to first digit; length clamped to 32 into [0x22];
   WCMV copies digits to staging at [0x46]-area; LDAFP restores ac3;
   then length+2 (WADI — VERIFY: +2 for the varying-length prefix
   word?), a WLSI shift (VERIFY semantics/amount), destination = entry
   ac2 from the saved slot, and a second WCMV from [0x44]-area into
   the destination — assembling the PL/1 CHAR VARYING (length word +
   chars). Decode this tail precisely against WCMV/WLSI/WADI
   implementations before writing any code.

## Instruction semantics to pin (read the emulator source, don't guess)

WCMV (EagleSpecial — char move: reg roles, direction, padding, what it
leaves in ac0-3), WLSI (shift left immediate: which reg, amount
encoding), WADI (add immediate: operand order), CVWN (wide→narrow
convert: masking/sign/ovr), WSEQ/WSGE/WSLE skip senses (verify my
clamp/exit readings above), WLDB/WSTB byte load/store reg roles, XLEFB
byte-address arithmetic (word addr ↔ byte addr factor), XNDO loop
bound encoding ("44" operand at 7017DACB).

## Residue map (replicate write-for-write)

WSAVS image (bridge emulate_frame) + every local the body writes:
[2],[3],[4],[5],[6] (final value after decrements),[7] (final XNDO
counter),[8..9] (final quotient=0 or last),[0xA..B],[0xC] (+[0xD]
untouched — do NOT write it),[0x1E],[0x20..21],[0x22], the digit
scratch bytes actually written (only positions index+1..32 — untouched
scratch bytes keep prior residue: do NOT clear the buffer), the
staging-area bytes WCMV writes, and whatever WCMV/loop leave in
scratch beyond the copied length. Empirical captures are the safety
net: instrument the stub to snapshot the frame region at entry and
after the emulated WRTN for several live calls (store visit produces
plenty), then diff the native footprint against the captures before
registering the translation.

## Validation plan

- Captures first (frame-region diff, as above), implementation second,
  then scripted smoke, then a user store session: every displayed
  price/quantity exercises it, including multi-digit values; the
  purchase flow also feeds ?CHAR_TO_UNSIGNED for later.
- The subtree note: master runs the emulated body INCLUDING its
  emulated ?UDIV32 call (run-to-return ignores nested entries); the
  clone's wrapper must NOT dispatch — call rt::udiv32_3 directly.
  ?UDIV32 stays registered for direct callers (there are none today,
  but the registration is harmless and keeps it validated).

## Open questions

1. Digit table contents/indexing (remainder+1 → leading byte?).
2. CVWN exact behavior on the residue-bearing high half.
3. The finish-tail arithmetic (length word format, WLSI amount, both
   WCMV configurations).
4. Whether any path writes the saved-reg slots (slot-patch scan came
   back clean on a first read — re-verify).

## RESOLUTION (implementation session) — TRANSLATED AND VALIDATED

Implemented as `runtime/unsigned_to_char.{hpp,cpp}`; registered;
validated under lockstep (9 native calls across 9 sites, 0
divergences) and by empirical footprint diff (master emulated RETURN
snapshots vs clone native footprints: **0 differing words** over all
pairs, 110 words each — frame region + destination). Two derivation
corrections and all four open questions resolved:

1. **[0xC..0xD] carries no residue — the derivation above is wrong.**
   The emulated ?UDIV32 body wide-stores the remainder through the
   arg3 pointer (`7017db48 XWSTA 0,@[ac3+0xFFF0]`), so BOTH words are
   written every iteration. The wide reload at 7017DADE reads
   (0<<16)|remainder (big-endian wides: word[addr] is the high half);
   CVWN keeps the low half = the true remainder, never sets ovr
   (remainder < 16). The existing native udiv32 (set_arg_wide) was
   already exactly faithful.
2. **Missed residue surface: the inner ?UDIV32 calls.** Each digit
   iteration leaves 9 wides above the outer locals at [fb+52..fb+69]:
   three XPEF'd arg pointers (&[0xC], &[0x20], &[0x8]), the LCALL
   frame word ((psr|0x8000)<<16)|3 — ovk set by the outer WSAVS, ovr
   always 0), and ?UDIV32's WSAVS image with its saved-ac0 slot
   patched to the quotient. Final iteration's values persist.
   Varying pieces, empirically confirmed: patched slot = last
   quotient; saved-ac1 = base; saved-ac2 = 1 on iteration 1 (NLDAI at
   7017DAC1), thereafter the PREVIOUS iteration's scratch byte
   pointer fb*2+0x1B+(34-k); return wide = 0x7017DADC | carry<<31
   where carry = entry carry if (k==1 && argc>2) else 0 (the argc>2
   parse path skips the WSUB 0,0 at 7017DAA3).
3. Open questions: (1) digit table = 00 00 "0123456789ABCDEF", XLEFB
   base at byte 1, so remainder+1 → TABLE[remainder]. (2) CVWN: keeps
   sext16 of the LOW half, ovr |= high half not a pure sign
   extension — moot per correction 1. (3) Finish tail: len =
   33-(index+1); WADI 2,0 adds the 2-byte length prefix; WLSI 1,2
   doubles the saved-ac2 word address to bytes; WCMV #1 copies digits
   scratch→staging (fb byte 0x46), WCMV #2 copies len+2 bytes from fb
   byte 0x44 ([0x22] read as the big-endian length word + staging) to
   the destination. WCMV semantics: ac0=dstcount, ac1=srccount,
   ac2=dst, ac3=src byte addrs, space-pads, positive counts move
   forward. (4) No slot patch — only the saved-ac2 READ at 0x7FFC.
4. XNDO final counter (local [7]): k on quotient-exit, width+1 on
   width-exhaustion — uniformly "the loop variable i at exit". With a
   width flag, digits are zero-padded AND high-truncated to width
   (faithfully ported).

Empirical coverage: argc=1/base 10 only (status-panel and weight
sites). argc=2/3 paths ported from the disassembly, no game callers
observed yet.
