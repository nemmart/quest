// src/runtime/unsigned_to_char.hpp
//
// ?UNSIGNED_TO_CHAR (0x7017DA75): convert a 32-bit unsigned value to a
// PL/1 CHAR(32) VARYING string (2-byte big-endian length word + digit
// bytes) at the destination passed in the CALLER'S ac2 as a word
// address — the register-argument pattern (read by the body from the
// WSAVS saved-ac2 slot at [ac3+0x7FFC]).
//
// Stack args (by reference): arg1 = &value (wide), arg2 = &base
// (narrow, clamped to [2,16]; argc==1 -> 10), arg3 = &width (narrow,
// only read when argc>2). The width flag governs both the digit cap
// and zero padding: when ((flag*2)&0xFFFF)==0 (the body's ADD.O# SEZ
// test, so 0 and 0x8000 both count as "no flag") the width is 32 and
// conversion stops at quotient 0; otherwise width = min(flag,32)
// signed, and the digit loop pads with '0' until the XNDO bound
// exhausts width (also TRUNCATING high digits when the value needs
// more than width digits — faithful to the original).
//
// First native SUBTREE: the emulated body's only call is ?UDIV32,
// which the native version makes as a direct rt::udiv32_3 call (no
// registry dispatch). The master runs the whole emulated body,
// including its emulated ?UDIV32 calls, under run-to-return.
//
// Derivation: docs/UNSIGNED_TO_CHAR.md, corrected this session against
// quest-rt.dis and empirical captures (capture-QUEST.txt):
// - ?UDIV32 wide-stores the remainder through arg3, so frame words
//   [0xC..0xD] are BOTH written every iteration (no residue there).
// - Digit table at 0x7017DA6C is 00 00 "0123456789ABCDEF", indexed
//   remainder+1 past the leading pad byte, i.e. TABLE[remainder].
// - The inner ?UDIV32 calls leave 9 wides of residue above the outer
//   locals per iteration (3 XPEF'd arg pointers, the LCALL frame word
//   (psr|0x8000)<<16|3, and ?UDIV32's WSAVS image with its saved-ac0
//   slot patched to the quotient); the wrapper replicates the final
//   iteration's values, tracking the inner-entry ac2 and carry.
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace rt {

// Digit-loop result, everything the residue replication needs.
struct UnsignedToCharResult {
  int32_t digit_count;     // k: digits produced (0 when width < 1)
  int32_t final_counter;   // the XNDO counter's final value (local [7])
  char digits[33];         // positions [33-k .. 32] valid, MSB first
                           // (mirrors the scratch byte layout at
                           // frame byte 0x1B+position)
  int32_t last_quotient;   // q of the final iteration (0 unless truncated)
  int32_t last_remainder;  // remainder of the final iteration
  bool error;              // ?UDIV32 escape fired (unreachable: base>=2)
};

// Exact port of the digit loop (0x7017DAC5..0x7017DAF7). width and
// padded are the resolved values ([5] and the flag!=0 sense); base is
// already clamped. Calls rt::udiv32_3 directly (the native subtree).
void unsigned_to_char_3(int32_t value, int32_t base, int32_t width,
                        bool padded, UnsignedToCharResult& out);

} // namespace rt

namespace emu_rt {
// NativeRegistry entry: full calling-convention and residue-fidelity
// wrapper (see the derivation comment above).
uint32_t unsigned_to_char(hw::Machine& machine);
}
