// hw/strings/EagleString.cpp — see EagleString.hpp. Every loop below is a
// transcription of the EagleSpecial.cpp arm it names; keep them in step
// (tests/strings_selftest.cpp will say when they are not).
#include "EagleString.hpp"
#include "../Machine.hpp"
#include "../Memory.hpp"
#include "../Instruction.hpp"
#include "../EagleSpecial.hpp"
#include <stdexcept>

namespace hw {
namespace strings {

// ---- pieces -------------------------------------------------------------

EagleString EagleString::literal(uint32_t bp, int32_t len) {
  EagleString s; s.bp = bp; s.len = len; return s;
}

EagleString EagleString::fixed(uint32_t bp, int32_t n) {
  EagleString s; s.bp = bp; s.len = n; return s;
}

EagleString EagleString::varying(Memory& memory, uint32_t len_word_addr) {
  EagleString s;
  s.bp = (len_word_addr + 1) * 2;
  s.len = static_cast<int32_t>(memory.read_word(len_word_addr) & 0xFFFFu);
  return s;
}

EagleString EagleString::substr(int32_t i, int32_t n) const {
  EagleString s; s.bp = bp + static_cast<uint32_t>(i); s.len = n; return s;
}

EagleString EagleString::chr(uint8_t x) {
  EagleString s; s.inline_byte = true; s.byte = x; s.len = 1; return s;
}

uint32_t EagleString::byte_at(Memory& memory, int32_t k) const {
  if(inline_byte) return byte;
  int32_t step = len < 0 ? -1 : 1;
  return memory.read_byte(bp + static_cast<uint32_t>(step * k));
}

std::string EagleString::bytes(Memory& memory) const {
  std::string out;
  int32_t count = len < 0 ? -len : len;
  for(int32_t k = 0; k < count; k++)
    out.push_back(static_cast<char>(byte_at(memory, k) & 0xFF));
  return out;
}

// ---- WCMV (EagleSpecial.cpp:42-74) --------------------------------------

void copy(Machine& m, uint32_t site, uint32_t dst_bp, int32_t n, const EagleString& src_piece) {
  if(src_piece.inline_byte)
    throw std::runtime_error("strings::copy: a char() piece is a WSTB source, not a WCMV source");
  Memory& memory = *m.memory;
  int32_t segment = static_cast<int32_t>(Instruction::get_segment(site));
  int32_t dst_count = n;                       // ac0
  int32_t src_count = src_piece.len;           // ac1
  int32_t dst = static_cast<int32_t>(dst_bp);  // ac2
  int32_t src = static_cast<int32_t>(src_piece.bp); // ac3
  int32_t dst_direction = EagleSpecial::direction(dst_count);
  int32_t src_direction = EagleSpecial::direction(src_count);
  int32_t copy_byte;
  while(dst_count != 0) {
    if(src_count == 0)
      copy_byte = ' ';
    else {
      if(segment != static_cast<int32_t>(Instruction::get_byte_segment(static_cast<uint32_t>(src))))
        throw std::runtime_error("WCMV: crossing segments not allowed");
      copy_byte = static_cast<int32_t>(memory.read_byte(static_cast<uint32_t>(src)));
      src_count = src_count + src_direction;
      src = src - src_direction;
    }
    if(segment != static_cast<int32_t>(Instruction::get_byte_segment(static_cast<uint32_t>(dst))))
      throw std::runtime_error("WCMV: crossing segments not allowed");
    memory.write_byte(static_cast<uint32_t>(dst), static_cast<uint32_t>(copy_byte));
    dst_count = dst_count + dst_direction;
    dst = dst - dst_direction;
  }
  // The exit lines (:66-73) as the pure residue rule of §3.
  residues_after_copy(m, dst_bp, n, src_piece.bp, src_piece.len);
}

void assign_fixed(Machine& m, uint32_t site, uint32_t dst_bp, int32_t n, const EagleString& src) {
  copy(m, site, dst_bp, n, src);
}

int32_t assign_varying(Machine& m, uint32_t site, uint32_t len_word_addr, int32_t cap, const EagleString& src) {
  int32_t count = src.len < cap ? src.len : cap;   // the WSLE/WMOV min shape (§2.4)
  m.memory->write_word(len_word_addr, static_cast<uint32_t>(count) & 0xFFFFu);   // XNSTA before the copy
  copy(m, site, (len_word_addr + 1) * 2, count, src);
  return count;
}

// ---- WSTB (EagleGeneral.cpp:119) ----------------------------------------

void append_char(Machine& m, uint32_t dst_bp, uint8_t x) {
  m.memory->write_byte(dst_bp, static_cast<uint32_t>(x) & 0xFFu);
}

// ---- WBLM (EagleSpecial.cpp:20-40) --------------------------------------

void block_move(Machine& m, uint32_t site, uint32_t dst_word, uint32_t src_word, int32_t k) {
  Memory& memory = *m.memory;
  int32_t segment = static_cast<int32_t>(Instruction::get_segment(site));
  int32_t src_count = k;                       // ac1
  int32_t src_direction = EagleSpecial::direction(src_count);
  int32_t src = static_cast<int32_t>(src_word);  // ac2
  int32_t dst = static_cast<int32_t>(dst_word);  // ac3
  if((src & 0x80000000) != 0 || (dst & 0x80000000) != 0)
    throw std::runtime_error("WBLM instruction with indirection!");
  while(src_count != 0) {
    if(segment != static_cast<int32_t>(Instruction::get_segment(static_cast<uint32_t>(src))) ||
       segment != static_cast<int32_t>(Instruction::get_segment(static_cast<uint32_t>(dst))))
      throw std::runtime_error("WBLM: crossing segments not allowed");
    int32_t word = static_cast<int32_t>(memory.read_word(static_cast<uint32_t>(src)));
    memory.write_word(static_cast<uint32_t>(dst), static_cast<uint32_t>(word));
    src_count += src_direction;
    src = src - src_direction;
    dst = dst - src_direction;
  }
  residues_after_blm(m, dst_word, src_word, k);
}

// ---- reference equality ---------------------------------------------------

bool pad_equal(Memory& memory, const EagleString& a, const EagleString& b) {
  int32_t na = a.len < 0 ? -a.len : a.len;
  int32_t nb = b.len < 0 ? -b.len : b.len;
  int32_t n = na > nb ? na : nb;
  for(int32_t k = 0; k < n; k++) {
    uint32_t ca = k < na ? a.byte_at(memory, k) : ' ';
    uint32_t cb = k < nb ? b.byte_at(memory, k) : ' ';
    if(ca != cb) return false;
  }
  return true;
}

// ---- residues (StringsDesign §3) --------------------------------------

static int32_t sgn(int32_t v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }
static int32_t mag(int32_t v) { return v < 0 ? -v : v; }

void residues_after_copy(Machine& m, uint32_t dst_bp, int32_t n, uint32_t src_bp, int32_t len) {
  int32_t t = mag(n) < mag(len) ? mag(n) : mag(len);   // bytes transferred
  m.ac[0] = 0;                                          // dst_count runs to 0
  m.ac[1] = len - sgn(len) * t;                         // src_count toward 0
#ifdef P30_BROKEN_RESIDUE
  m.ac[3] = static_cast<int32_t>(src_bp) + n;          // deliberately wrong (teeth build)
#else
  m.ac[3] = static_cast<int32_t>(src_bp) + sgn(len) * t;  // advanced only per byte FETCHED
#endif
  m.ac[2] = static_cast<int32_t>(dst_bp) + n;          // one past the destination
  m.c = (m.ac[1] != 0) ? 1 : 0;                         // :70-73
}

// ---- WCMP (EagleSpecial.cpp:76-109) -------------------------------------

int32_t residues_after_compare(Machine& m, uint32_t site, uint32_t dst_bp, int32_t n, uint32_t src_bp, int32_t len) {
  Memory& memory = *m.memory;
  int32_t segment = static_cast<int32_t>(Instruction::get_segment(site));
  int32_t dst_count = n;                       // ac0 — string 2
  int32_t src_count = len;                     // ac1 — string 1
  int32_t dst = static_cast<int32_t>(dst_bp);  // ac2
  int32_t src = static_cast<int32_t>(src_bp);  // ac3
  int32_t dst_direction = EagleSpecial::direction(dst_count);
  int32_t src_direction = EagleSpecial::direction(src_count);
  int32_t result = 0, src_byte, dst_byte;
  while(dst_count != 0 || src_count != 0) {
    if(src_count == 0)
      src_byte = ' ';
    else {
      if(segment != static_cast<int32_t>(Instruction::get_byte_segment(static_cast<uint32_t>(src))))
        throw std::runtime_error("WCMP: crossing segments not allowed");
      src_byte = static_cast<int32_t>(memory.read_byte(static_cast<uint32_t>(src)));
      src_count = src_count + src_direction;
      src = src - src_direction;
    }
    if(dst_count == 0)
      dst_byte = ' ';
    else {
      if(segment != static_cast<int32_t>(Instruction::get_byte_segment(static_cast<uint32_t>(dst))))
        throw std::runtime_error("WCMP: crossing segments not allowed");
      dst_byte = static_cast<int32_t>(memory.read_byte(static_cast<uint32_t>(dst)));
      dst_count = dst_count + dst_direction;
      dst = dst - dst_direction;
    }
    if(src_byte < dst_byte) { result = -1; break; }
    if(src_byte > dst_byte) { result = 1; break; }
  }
  m.ac[0] = dst_count;
  m.ac[1] = result;
  m.ac[2] = dst;      // one past the failing byte on a mismatch (B-4)
  m.ac[3] = src;
  return result;
}

void residues_after_blm(Machine& m, uint32_t dst_word, uint32_t src_word, int32_t k) {
  m.ac[1] = 0;
  m.ac[2] = static_cast<int32_t>(src_word) + k;
  m.ac[3] = static_cast<int32_t>(dst_word) + k;
}

} // namespace strings
} // namespace hw
