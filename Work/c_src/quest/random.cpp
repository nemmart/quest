// src/quest/random.cpp
#include "random.hpp"
#include "../rt/random_number.hpp"
#include "../types/Context.hpp"

namespace quest {

int32_t random(types::Context& ctx, int32_t threshold) {
  int32_t roll=rt::random_number_3(ctx, 1, 100);
  if(threshold>roll)
    return 0x8000;
  return 0x0000;
}

} // namespace quest
