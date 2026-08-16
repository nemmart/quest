#pragma once
#include "Instruction.hpp"
#include <cstdint>
#include <cmath>
#include <cstring>


namespace hw {
class Machine;

class EagleInstruction : public Instruction {
public:
  int32_t add(Machine& machine, int64_t src, int64_t dst);
  int32_t sub(Machine& machine, int64_t src, int64_t dst);
  int32_t mul(Machine& machine, int64_t src, int64_t dst);
  int32_t arithmetic_shift(Machine& machine, int32_t src, int32_t amount);
  int32_t logical_shift(Machine& machine, int32_t src, int32_t amount);
  int32_t narrow_add(Machine& machine, int32_t src, int32_t dst);
  int32_t narrow_sub(Machine& machine, int32_t src, int32_t dst);
  int32_t narrow_mul(Machine& machine, int32_t src, int32_t dst);

  void validate_exponent(Machine& machine, double x);
  int64_t double_to_eclipse_wide_float(Machine& machine, double x);
  double eclipse_wide_float_to_double(Machine& machine, int64_t x);
  double eclipse_wide_round(Machine& machine, double x);
  int32_t double_to_eclipse_float(Machine& machine, double x);
  double eclipse_float_to_double(Machine& machine, int32_t x);
};

} // namespace hw
