// hw/strings/EagleString.hpp — the string library (Project 30).
// Design of record: docs/Project29/StringsDesign.md §2.4 (byte semantics),
// §3 (residues). Semantics are TRANSCRIBED from hw/EagleSpecial.cpp
// (WBLM :20-40, WCMV :42-74, WCMP :76-109) and hw/EagleGeneral.cpp (WSTB
// :119); each function cites the lines it mirrors. The emulator's own
// arms are untouched — this library MIRRORS them (P30 ruling 5: mirror,
// not hoist), and tests/strings_selftest.cpp brute-forces the two against
// each other field by field. Residue-class divergences from the hardware
// (EmulatorDivergences.md B-1 ovr untouched, B-4 WCMP one-past pointers,
// G-2 WBLM indirect-bit throw, G-3 segment-crossing throw) are REPRODUCED.
//
// A PIECE (EagleString) is a SPAN over emulated memory — (byte pointer,
// signed count) — never materialised. copy() reads the source and writes
// the destination one byte at a time in program order exactly as WCMV
// does, so overlapping operands behave as they do on the master (P30
// plan-gate ruling). char(x) is the one inline piece (the WSTB idiom).
//
// The segment check (G-3) is the INSTRUCTION's: EagleSpecial compares
// get_segment(pc) with get_byte_segment(ptr) per byte, so the operations
// that touch memory take `site`, the pc of the replaced instruction. The
// pure residue functions (copy, blm) do not.
//
// P30 ships this DARK: the emulator links it and nothing calls it. P31
// (located strings) and later projects lower IR statements onto it.
#pragma once
#include <cstdint>
#include <string>

namespace hw {
class Machine;
class Memory;

namespace strings {

struct EagleString {
  uint32_t bp = 0;           // byte pointer (word_address*2 + byte index; ByteEA.md §2)
  int32_t  len = 0;          // signed count, as the instruction sees it
  bool     inline_byte = false;
  uint8_t  byte = 0;

  // "literal": address + length recorded from quest.strings.
  static EagleString literal(uint32_t bp, int32_t len);
  // [@a, n] — CHAR(n): n bytes at byte address bp, no length word.
  static EagleString fixed(uint32_t bp, int32_t n);
  // [@a, n varying] — length word at word address a, data at a+1
  // (StringsDesign §1.1): bp = (a+1)*2, len = read_word(a).
  static EagleString varying(Memory& memory, uint32_t len_word_addr);
  // PL/I SUBSTR(s, i, n) with a 0-based byte offset i: (bp+i, n).
  EagleString substr(int32_t i, int32_t n) const;
  // char(x): ONE byte from a word expression (the WSTB idiom). Inline.
  static EagleString chr(uint8_t x);
  // Source byte k (0-based, along the span's direction). Inline pieces
  // return their byte for every k.
  uint32_t byte_at(Memory& memory, int32_t k) const;
  // Materialised read-out, for tests and debugging only.
  std::string bytes(Memory& memory) const;
};

// ---- instruction mirrors ----------------------------------------------

// WCMV (EagleSpecial.cpp:42-74). dst = (dst_bp, n), src = piece.
// Copies min(|n|,|len|) bytes in program order, blank-pads the rest of
// the destination when the source is exhausted, truncates when the
// destination is; every byte read/written goes through Memory with the
// per-byte segment check against get_segment(site) (G-3 throws the same
// text). Then sets ac0-ac3 and c via residues_after_copy(). ovr untouched.
void copy(Machine& m, uint32_t site, uint32_t dst_bp, int32_t n, const EagleString& src);

// [@a, n] = piece  ≡  copy(dst_bp, n, src)   (StringsDesign §2.3)
void assign_fixed(Machine& m, uint32_t site, uint32_t dst_bp, int32_t n, const EagleString& src);

// [@a, cap varying] = piece: length word := min(len, cap) (the compiler's
// XNSTA before the copy, §2.4), then copy((a+1)*2, min(len, cap), src).
// Returns the transferred count.
int32_t assign_varying(Machine& m, uint32_t site, uint32_t len_word_addr, int32_t cap, const EagleString& src);

// WSTB (EagleGeneral.cpp:119): one write_byte at dst_bp; no residues (the
// compiler's WINC 2,2 follows as its own statement, §3).
void append_char(Machine& m, uint32_t dst_bp, uint8_t x);

// WBLM (EagleSpecial.cpp:20-40): k words (signed: + ascending, −
// descending) from src_word to dst_word, one word at a time, addresses
// stepped after each store — which is what makes the self-overlapping
// fills (Census F6) well-defined. Throws on bit 31 in either pointer
// (G-2) and on a segment change (G-3), same texts. Sets ac1-ac3 via
// residues_after_blm(); ac0, c, ovr untouched.
void block_move(Machine& m, uint32_t site, uint32_t dst_word, uint32_t src_word, int32_t k);

// Reference blank-padded equality (WCMP == 0), no residues, no segment
// check: the C++ end-state operator and the self-test's oracle.
bool pad_equal(Memory& memory, const EagleString& a, const EagleString& b);

// ---- residues (StringsDesign §3) --------------------------------------

// After a copy (WCMV :66-73). PURE: with t = min(|n|, |len|):
//   ac0 = 0; ac1 = len − sgn(len)·t; ac2 = dst_bp + n; ac3 = src_bp + sgn(len)·t;
//   c = (ac1 != 0).  ovr untouched (B-1).
void residues_after_copy(Machine& m, uint32_t dst_bp, int32_t n, uint32_t src_bp, int32_t len);

// After a compare (WCMP :76-109). NOT pure — the stop point depends on
// the bytes, so this IS the read loop (P30 ruling 2): string 2 is
// (dst_bp, n) = ac2/ac0, string 1 is (src_bp, len) = ac3/ac1, both read
// until both counts are exhausted with the shorter padded by blanks,
// stopping at the first unequal pair. ac0 = string-2 count at the stop,
// ac1 = result (−1 str1<str2, 0, +1), ac2/ac3 = pointers at the stop —
// ONE PAST the failing byte on a mismatch (B-4 reproduced). c and ovr
// untouched. Returns ac1.
int32_t residues_after_compare(Machine& m, uint32_t site, uint32_t dst_bp, int32_t n, uint32_t src_bp, int32_t len);
inline int32_t compare(Machine& m, uint32_t site, uint32_t dst_bp, int32_t n, uint32_t src_bp, int32_t len) {
  return residues_after_compare(m, site, dst_bp, n, src_bp, len);
}

// After a block move (WBLM :37-39). PURE: ac1 = 0; ac2 = src_word + k;
// ac3 = dst_word + k (k signed). ac0, c, ovr untouched.
void residues_after_blm(Machine& m, uint32_t dst_word, uint32_t src_word, int32_t k);

} // namespace strings
} // namespace hw
