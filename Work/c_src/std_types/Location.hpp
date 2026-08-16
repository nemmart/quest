// src/std_types/Location.hpp
#pragma once
#include "../types/Location.hpp"

namespace std_types {

// Plain value Location. Used for temporaries and post-emulator code.
// Just holds x and y directly — no memory backing.

class Location : public types::Location {
public:
  Location(int16_t x, int16_t y) : x_(x), y_(y) {}

  int16_t get_x() const override { return x_; }
  int16_t get_y() const override { return y_; }
  void set_x(int16_t v) override { x_ = v; }
  void set_y(int16_t v) override { y_ = v; }

private:
  int16_t x_;
  int16_t y_;
};

} // namespace std_types
