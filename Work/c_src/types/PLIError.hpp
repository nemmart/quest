// src/types/PLIError.hpp
#pragma once
#include <cstdint>
#include <stdexcept>

namespace types {

struct PLIError : public std::exception {
  uint32_t signal_code;
  explicit PLIError(uint32_t code) : signal_code(code) {}
  const char* what() const noexcept override { return "PLIError"; }
};

} // namespace types
