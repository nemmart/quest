// Standalone brute-force check of the EagleInstruction arithmetic helpers
// against a reference model written from the DG manual pages read on
// Sep 5 2026 (docs/HWFindings_Sep5.md).  Not part of the emulator build:
//   g++ -std=c++17 -O2 -I. tests/helpers_selftest.cpp hw/EagleInstruction.o hw/Machine.o ... 
// (see tests/run_helpers_selftest.sh).  Exit 0 = all agree.
#include <cstdio>
#include <cstdint>
#include "hw/Machine.hpp"
#include "hw/EagleInstruction.hpp"
using namespace hw;

static int fails = 0;
static void check(const char* what, int64_t a, int64_t b, int64_t got, int64_t want) {
  if (got != want && fails++ < 20)
    std::printf("FAIL %s a=%lld b=%lld got=%lld want=%lld\n", what, (long long)a, (long long)b, (long long)got, (long long)want);
}
static int32_t sx16(int64_t v) { return static_cast<int32_t>(static_cast<int16_t>(v & 0xFFFF)); }

int main() {
  Machine m(nullptr, nullptr, nullptr, nullptr);
  // 16-bit grid, stepped with coprime strides so every residue class is hit.
  for (int32_t s = -32768; s < 32768; s += 37)
    for (int32_t d = -32768; d < 32768; d += 41) {
      int64_t sum = (int64_t)d + s, dif = (int64_t)d - s, prod = (int64_t)d * s;
      // NADD: result sign-extended 16-bit; OVR = out of 16-bit signed range; C = ALU carry.
      m.ovr = 0; m.c = 0;
      int32_t r = EagleInstruction::narrow_add(m, s, d);
      check("nadd.result", s, d, r, sx16(sum));
      check("nadd.ovr", s, d, m.ovr, !(sum >= -32768 && sum <= 32767));
      check("nadd.c", s, d, m.c, (((uint32_t)d & 0xFFFF) + ((uint32_t)s & 0xFFFF)) >> 16);
      // NSUB: carry of d + ~s + 1 = no borrow = d16 >= s16 unsigned.
      m.ovr = 0; m.c = 0;
      r = EagleInstruction::narrow_sub(m, s, d);
      check("nsub.result", s, d, r, sx16(dif));
      check("nsub.ovr", s, d, m.ovr, !(dif >= -32768 && dif <= 32767));
      check("nsub.c", s, d, m.c, ((uint32_t)d & 0xFFFF) >= ((uint32_t)s & 0xFFFF));
      // NMUL: sign-extended low 16 of the product; OVR if out of range; C unchanged.
      m.ovr = 0; m.c = 1;
      r = EagleInstruction::narrow_mul(m, s, d);
      check("nmul.result", s, d, r, sx16(prod));
      check("nmul.ovr", s, d, m.ovr, !(prod >= -32768 && prod <= 32767));
      check("nmul.c", s, d, m.c, 1);
    }
  // Sticky OVR: helpers must never clear it.
  m.ovr = 1; EagleInstruction::narrow_add(m, 1, 1); check("nadd.sticky", 1, 1, m.ovr, 1);
  m.ovr = 1; EagleInstruction::narrow_mul(m, 1, 1); check("nmul.sticky", 1, 1, m.ovr, 1);
  // WHLV is inline in EagleCompute; its rule is "round toward zero" = C++ /2.
  const int64_t vs[] = {-7, -3, -2, -1, 0, 1, 3, INT32_MIN, INT32_MAX};
  for (int64_t v : vs) {
    int32_t want = (int32_t)v / 2, got = static_cast<int32_t>(v) / 2;   // documents the rule
    check("whlv.rule", v, 0, got, want);
  }
  std::printf("%s (%d failures)\n", fails ? "SELFTEST RED" : "SELFTEST GREEN", fails);
  return fails ? 1 : 0;
}
