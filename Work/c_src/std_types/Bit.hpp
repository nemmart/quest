// src/std_types/Bit.hpp
#pragma once
#include "../types/Bit.hpp"

namespace std_types {

// Plain value Bit. Used for temporaries and post-emulator code.

class Bit : public types::Bit {
public:
  Bit(bool v = false) : value_(v) {}

  bool get_value() const override { return value_; }
  void set_value(bool v) override { value_ = v; }

private:
  bool value_;
};

} // namespace std_types
