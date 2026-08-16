// src/types/Word.hpp
#pragma once
#include <cstdint>

namespace types {

// Abstract 16-bit value passed by reference.
// PL/I passes all arguments by reference — when a game function
// receives an int16_t argument, the caller pushes the address
// and the callee dereferences it.

class Word {
public:
  virtual ~Word() = default;

  virtual int16_t get_value() const = 0;
  virtual void set_value(int16_t v) = 0;
};

} // namespace types
