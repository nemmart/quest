// src/rt/delay.cpp
#include "delay.hpp"
#include <thread>
#include <chrono>

namespace rt {

void delay_1(types::Context& ctx, int32_t milliseconds) {
  if(milliseconds>0)
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

} // namespace rt
