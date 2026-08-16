// src/types/Wide.hpp
#pragma once
#include <cstdint>

namespace types {

// Abstract 32-bit value passed by reference.
// PL/I passes all arguments by reference — when a game function
// receives an int32_t argument, the caller pushes the address
// and the callee dereferences it.

class Wide {
public:
  virtual ~Wide() = default;

  virtual int32_t get_value() const = 0;
  virtual void set_value(int32_t v) = 0;
};

} // namespace types
