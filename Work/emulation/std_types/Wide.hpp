// src/std_types/Wide.hpp
#pragma once
#include "../types/Wide.hpp"

namespace std_types {

// Plain value Wide. Used for temporaries and post-emulator code.

class Wide : public types::Wide {
public:
  Wide(int32_t v = 0) : value_(v) {}

  int32_t get_value() const override { return value_; }
  void set_value(int32_t v) override { value_ = v; }

private:
  int32_t value_;
};

} // namespace std_types
