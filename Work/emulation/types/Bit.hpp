// src/types/Bit.hpp
#pragma once

namespace types {

// Abstract single-bit value passed by reference.
// Used when game functions receive or return boolean flags
// through PL/I by-reference arguments.

class Bit {
public:
  virtual ~Bit() = default;

  virtual bool get_value() const = 0;
  virtual void set_value(bool v) = 0;
};

} // namespace types
