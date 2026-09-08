#include "EagleInstruction.hpp"
#include "Machine.hpp"
#include <cstring>
#include <cmath>
#include <stdexcept>




namespace hw {
static int64_t double_to_bits(double d) {
  int64_t bits;
  std::memcpy(&bits, &d, sizeof(bits));
  return bits;
}

static double bits_to_double(int64_t bits) {
  double d;
  std::memcpy(&d, &bits, sizeof(d));
  return d;
}

int32_t EagleInstruction::add(Machine& machine, int64_t src, int64_t dst) {
  int64_t result = dst + src;
  int64_t overflow = ((src ^ result) & ~(src ^ dst));
  // ALU carry-out of the 32-bit add = bit 32 of the unsigned sum.
  // Was >>31 (result sign bit) — corrected Aug 28 2026 against the DG
  // manual's WADD page: "CARRY set according to value of ALU carry".
  int64_t carry = ((dst & 0xFFFFFFFF) + (src & 0xFFFFFFFF)) >> 32;
  machine.ovr |= static_cast<int32_t>((static_cast<uint64_t>(overflow) >> 31) & 0x01);
  machine.c = static_cast<int32_t>(carry & 0x01);
  return static_cast<int32_t>(result & 0xFFFFFFFF);
}

int32_t EagleInstruction::sub(Machine& machine, int64_t src, int64_t dst) {
  int64_t result = dst - src;
  int64_t overflow = ((src ^ dst) & (result ^ dst));
  // ALU carry of 2's-complement subtraction dst + ~src + 1 = 1 iff no
  // borrow (dst >= src unsigned) — bit 32 of the complement-add, matching
  // narrow_sub's convention. Was >>31 of the raw difference — corrected
  // Aug 28 2026 against the DG manual's WSUB page ("ALU carry").
  int64_t carry = ((dst & 0xFFFFFFFF) + (~src & 0xFFFFFFFF) + 1) >> 32;
  machine.ovr |= static_cast<int32_t>((static_cast<uint64_t>(overflow) >> 31) & 0x01);
  machine.c = static_cast<int32_t>(carry & 0x01);
  return static_cast<int32_t>(result & 0xFFFFFFFF);
}

int32_t EagleInstruction::mul(Machine& machine, int64_t src, int64_t dst) {
  int64_t product = src * dst;
  int64_t overflow = product >> 31;
  if (overflow != 0 && overflow != -1)
    machine.ovr |= 1;
  return static_cast<int32_t>(product & 0xFFFFFFFF);
}

int32_t EagleInstruction::div(Machine& machine, int32_t src, int32_t dst) {
  // Hoisted verbatim from EagleCompute.cpp WDIV (P26).  The emulator
  // ASSIGNS ovr=1 (equivalent to |= for a 0/1 flag) and leaves the
  // destination untouched on the two fault shapes.
  if(src==0 || (src==-1 && static_cast<uint32_t>(dst)==0x80000000u)) {
    machine.ovr=1;
    return dst;
  }
  return dst/src;
}

int32_t EagleInstruction::cvwn(Machine& machine, int32_t src) {
  // Hoisted verbatim from EagleCompute.cpp CVWN (P26).
  int32_t result=static_cast<int32_t>(static_cast<uint32_t>(src)<<16)>>16;
  int32_t hi=src>>15;
  machine.ovr|=(hi!=0 && hi!=-1)?1:0;
  return result;
}

int32_t EagleInstruction::arithmetic_shift(Machine& machine, int32_t src, int32_t amount) {
  int32_t result = src;
  if (amount > 0) {
    if (amount < 32)
      result = src << amount;
    else
      result = 0;
  }
  else if (amount < 0) {
    if (amount > -32)
      result = src >> (-amount);
    else
      result = src >> 31;
  }
  machine.ovr |= static_cast<int32_t>(static_cast<uint32_t>(result ^ src) >> 31);
  return result;
}

int32_t EagleInstruction::logical_shift(Machine& machine, int32_t src, int32_t amount) {
  if (amount != 0) {
    if (amount > 0 && amount < 32)
      src = src << amount;
    else if (amount < 0 && amount > -32)
      src = static_cast<int32_t>(static_cast<uint32_t>(src) >> (-amount));
    else
      src = 0;
  }
  return src;
}

int32_t EagleInstruction::narrow_add(Machine& machine, int32_t src, int32_t dst) {
  // Sep 5 2026 (docs/HWFindings_Sep5.md), against the DG manual's NADD
  // page: "stores the 32-bit sign-extended result"; OVR = 16-bit ALU
  // overflow; CARRY = 16-bit ALU carry.  The operands are sign-extended
  // into 32 bits, so the 16-bit op's sign bit is BIT 15 — the overflow
  // test read bit 16 (missed e.g. 0x7FFF+1), and the result was the raw
  // 17-bit sum, not the sign-extended 16-bit result.
  src = (src << 16) >> 16;
  dst = (dst << 16) >> 16;
  int32_t result = dst + src;
  int32_t overflow = ((src ^ result) & ~(src ^ dst));
  int32_t carry = ((dst & 0xFFFF) + (src & 0xFFFF)) >> 16;
  machine.ovr |= static_cast<int32_t>(static_cast<uint32_t>(overflow) >> 15) & 0x01;
  machine.c = carry & 0x01;
  return (result << 16) >> 16;
}

int32_t EagleInstruction::narrow_sub(Machine& machine, int32_t src, int32_t dst) {
  // Sep 5 2026 (docs/HWFindings_Sep5.md): overflow bit 15 (see
  // narrow_add); CARRY is the ALU carry of dst + ~src + 1 = 1 iff no
  // borrow, matching the wide sub — the (-src)&0xFFFF shortcut gave 0
  // for src==0 where the hardware gives 1; result sign-extended.
  src = (src << 16) >> 16;
  dst = (dst << 16) >> 16;
  int32_t result = dst - src;
  int32_t overflow = ((src ^ dst) & (result ^ dst));
  int32_t carry = ((dst & 0xFFFF) + (~src & 0xFFFF) + 1) >> 16;
  machine.ovr |= static_cast<int32_t>(static_cast<uint32_t>(overflow) >> 15) & 0x01;
  machine.c = carry & 0x01;
  return (result << 16) >> 16;
}

int32_t EagleInstruction::narrow_mul(Machine& machine, int32_t src, int32_t dst) {
  // Sep 5 2026 (docs/HWFindings_Sep5.md), against the DG manual's NMUL
  // page: "sign extends the lower 16 bits of result to 32 bits"; OVR=1
  // iff the product leaves -32768..32767 (bit 15 test, as CVWN).  Was
  // zero-filled (dst & 0xFFFF), bit-16 test, and ovr ASSIGNED.
  src = (src << 16) >> 16;
  dst = (dst << 16) >> 16;
  dst = dst * src;
  int32_t overflow = dst >> 15;
  if (overflow != 0 && overflow != -1)
    machine.ovr |= 1;
  return (dst << 16) >> 16;
}

void EagleInstruction::validate_exponent(Machine& machine, double x) {
  if (x == 0.0)
    return;
  int32_t exponent = static_cast<int32_t>(double_to_bits(x) >> 52);
  exponent = ((exponent & 0x7FF) - 1019) >> 2;
  if (exponent < -64)
    throw std::runtime_error("Floating point underflow");
  if (exponent > 63)
    throw std::runtime_error("Floating point overflow");
}

int64_t EagleInstruction::double_to_eclipse_wide_float(Machine& machine, double x) {
  if (x == 0.0)
    return 0;
  int64_t bits = double_to_bits(x);
  int64_t sign = bits & (int64_t)0x8000000000000000LL;
  int64_t exponent = ((bits >> 52) & 0x7FF) - 1019;
  bits = (bits & 0x000FFFFFFFFFFFFFLL) | 0x0010000000000000LL;
  bits = bits << (exponent & 0x03);
  exponent = exponent >> 2;
  if (exponent < -64)
    throw std::runtime_error("Floating point underflow");
  if (exponent > 63)
    throw std::runtime_error("Floating point overflow");
  return sign | ((exponent + 64) << 56) | bits;
}

double EagleInstruction::eclipse_wide_float_to_double(Machine& machine, int64_t x) {
  int64_t mantissa = x & 0x00FFFFFFFFFFFFFFLL;
  if (mantissa == 0)
    return 0.0;
  int32_t left = 0;
  while ((mantissa & 0x00F0000000000000LL) == 0) {
    mantissa = mantissa << 4;
    left = left + 1;
  }
  int64_t exponent = (((x >> 56) & 0x7F) - 65 - left) * 4 + 1023;
  while ((mantissa & 0x00E0000000000000LL) != 0) {
    mantissa = static_cast<int64_t>(static_cast<uint64_t>(mantissa) >> 1);
    exponent = exponent + 1;
  }
  x = (x & (int64_t)0x8000000000000000LL) | (exponent << 52) | (mantissa & 0x000FFFFFFFFFFFFFLL);
  return bits_to_double(x);
}

double EagleInstruction::eclipse_wide_round(Machine& machine, double x) {
  int64_t eclipse = double_to_eclipse_wide_float(machine, x);
  int64_t mask = 0x00FFFFFF00000000LL;
  if (machine.fpr != 0) {
    if ((eclipse & mask) == mask) {
      int64_t rounded = eclipse & (int64_t)0xFF00000000000000LL;
      rounded = rounded + 0x0110000000000000LL;
      if ((static_cast<uint64_t>(rounded ^ eclipse) >> 63) != 0)
        throw std::runtime_error("Overflow during rounding");
      return eclipse_wide_float_to_double(machine, eclipse);
    }
    else
      eclipse = eclipse + 0x0000000080000000LL;
  }
  eclipse = eclipse & (int64_t)0xFFFFFFFF00000000LL;
  return eclipse_wide_float_to_double(machine, eclipse);
}

int32_t EagleInstruction::double_to_eclipse_float(Machine& machine, double x) {
  int64_t eclipse = double_to_eclipse_wide_float(machine, x);
  int64_t mask = 0x00FFFFFF00000000LL;
  if (machine.fpr != 0) {
    if ((eclipse & mask) == mask) {
      int64_t rounded = eclipse & (int64_t)0xFF00000000000000LL;
      rounded = rounded + 0x0110000000000000LL;
      if ((static_cast<uint64_t>(rounded ^ eclipse) >> 63) != 0)
        throw std::runtime_error("Overflow during rounding");
      return static_cast<int32_t>(static_cast<uint64_t>(rounded) >> 32);
    }
    else
      eclipse = eclipse + 0x0000000080000000LL;
  }
  return static_cast<int32_t>(static_cast<uint64_t>(eclipse) >> 32);
}

double EagleInstruction::eclipse_float_to_double(Machine& machine, int32_t x) {
  return eclipse_wide_float_to_double(machine, static_cast<int64_t>(x) << 32);
}

} // namespace hw
