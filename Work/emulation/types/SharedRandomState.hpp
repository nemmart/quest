// src/types/SharedRandomState.hpp
#pragma once

namespace types {

class SharedRandomState {
public:
  virtual ~SharedRandomState() = default;
  virtual double get_state() const = 0;
  virtual void set_state(double state) = 0;
};

} // namespace types
