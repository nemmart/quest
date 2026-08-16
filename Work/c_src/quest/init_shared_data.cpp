// src/quest/init_shared_data.cpp
#include "init_shared_data.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"

namespace quest {

void init_shared_data(types::Context& ctx) {
  ctx.shared=ctx.os.init_shared_data();
  ctx.random_state=ctx.os.get_random_state();
}

} // namespace quest
