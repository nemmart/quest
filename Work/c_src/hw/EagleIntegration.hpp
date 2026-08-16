// src/hw/EagleIntegration.hpp
#pragma once
#include "Machine.hpp"
#include "../types/PLIError.hpp"
#include <cstdint>
#include <stdexcept>

namespace hw {

// Re-export PLIError in hw namespace for backward compatibility
using types::PLIError;

class EagleIntegration {
public:
  explicit EagleIntegration(Machine& m);

  int arg_count() const;
  uint32_t arg_addr(int n) const;
  uint32_t arg_wide(int n) const;
  uint32_t wrtn(uint32_t result);
  uint32_t wrtn_void();
  uint32_t throw_lib_error(uint32_t signal_code);

private:
  Machine&  machine;
  uint32_t  fp;
};

} // namespace hw
