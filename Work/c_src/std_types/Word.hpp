// src/std_types/Word.hpp
#pragma once
#include "../types/Word.hpp"

namespace std_types {

// Plain value Word. Used for temporaries and post-emulator code.

class Word : public types::Word {
public:
  Word(int16_t v = 0) : value_(v) {}

  int16_t get_value() const override { return value_; }
  void set_value(int16_t v) override { value_ = v; }

private:
  int16_t value_;
};

} // namespace std_types
