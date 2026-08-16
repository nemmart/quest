// src/rt/create_task.cpp
#include "create_task.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/PLIError.hpp"

namespace rt {

static constexpr int32_t STACK_OVERHEAD=0x0548;

void create_task_2(types::Context& ctx, int32_t entry_point,
                   int32_t ac2_value, int32_t stack_size) {
  int32_t total_stack=stack_size+STACK_OVERHEAD;
  int32_t err=ctx.os.create_task(entry_point, ac2_value, total_stack);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

} // namespace rt
