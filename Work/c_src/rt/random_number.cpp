// src/rt/random_number.cpp
#include "random_number.hpp"
#include "../types/Context.hpp"
#include "../types/SharedRandomState.hpp"
#include <cmath>

namespace rt {

static constexpr double MULTIPLIER=31415933.0;
static constexpr double INCREMENT=14181771.0;
static constexpr double MODULUS=67108864.0;  // 2^26

int32_t random_number_3(types::Context& ctx, int32_t lower, int32_t upper) {
  double seed=ctx.random_state->get_state();
  seed=std::fmod(seed*MULTIPLIER+INCREMENT, MODULUS);
  ctx.random_state->set_state(seed);

  double range=static_cast<double>(upper-lower+1);
  return lower+static_cast<int32_t>(seed*range/MODULUS);
}

} // namespace rt
