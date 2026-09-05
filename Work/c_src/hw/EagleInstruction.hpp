#pragma once
#include "Instruction.hpp"
#include <cstdint>
#include <cmath>
#include <cstring>


namespace hw {
class Machine;

class EagleInstruction : public Instruction {
public:
  // static: P23 IRExec calls these directly — the IR #-ops are DEFINED
  // as these helpers (docs/Project23/WideCarry.md), single source of truth.
  static int32_t add(Machine& machine, int64_t src, int64_t dst);
  static int32_t sub(Machine& machine, int64_t src, int64_t dst);
  // P26: the whole effectful family is static and shared with IRExec
  // (same-helpers principle, P23 ruling; docs/IR.md, Project26/Census.md).
  static int32_t mul(Machine& machine, int64_t src, int64_t dst);
  // P26: hoisted from EagleCompute's WDIV/CVWN inline bodies (byte-identical;
  // K=1 stock gate). div: divisor 0 or INT_MIN/-1 -> ovr=1, returns dst
  // UNCHANGED; else truncating signed quotient, no flag.
  static int32_t div(Machine& machine, int32_t src, int32_t dst);
  // cvwn: sign-extend the low 16 bits; ovr |= 1 if the wide did not fit.
  static int32_t cvwn(Machine& machine, int32_t src);
  static int32_t arithmetic_shift(Machine& machine, int32_t src, int32_t amount);
  static int32_t logical_shift(Machine& machine, int32_t src, int32_t amount);
  static int32_t narrow_add(Machine& machine, int32_t src, int32_t dst);
  static int32_t narrow_sub(Machine& machine, int32_t src, int32_t dst);
  static int32_t narrow_mul(Machine& machine, int32_t src, int32_t dst);

  void validate_exponent(Machine& machine, double x);
  int64_t double_to_eclipse_wide_float(Machine& machine, double x);
  double eclipse_wide_float_to_double(Machine& machine, int64_t x);
  double eclipse_wide_round(Machine& machine, double x);
  int32_t double_to_eclipse_float(Machine& machine, double x);
  double eclipse_float_to_double(Machine& machine, int32_t x);
};

} // namespace hw
