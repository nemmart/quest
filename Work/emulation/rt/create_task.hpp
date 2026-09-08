// src/rt/create_task.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace rt {

// ?CREATE_TASK — create and launch a new task
// entry_point: code address where the task starts
// ac2_value: initial AC2 register value for the task
// stack_size: stack size in words
void create_task_2(types::Context& ctx, int32_t entry_point,
                   int32_t ac2_value, int32_t stack_size);

} // namespace rt
