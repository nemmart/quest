// src/types/Location.hpp
#pragma once
#include <cstdint>

namespace types {

// Abstract coordinate pair (x, y).
// Passed to spatial functions: DIST, FIND_OBJECT, UPDATE_SCREENS,
// TERRITORY, TERRAIN, CREATE_MAP, WRITE_OBJECT, etc.
//
// Backed by emulated memory (emu_types::Location) during the
// strangler fig transition, or plain values (std_types::Location)
// for temporaries and post-emulator code.

class Location {
public:
  virtual ~Location() = default;

  virtual int16_t get_x() const = 0;
  virtual int16_t get_y() const = 0;
  virtual void set_x(int16_t v) = 0;
  virtual void set_y(int16_t v) = 0;
};

} // namespace types
