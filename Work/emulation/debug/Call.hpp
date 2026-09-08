#pragma once
#include <cstdint>


namespace debug {
struct Call {
  int32_t entry_address = 0;
  int32_t return_address = 0;
  int32_t call_instruction_address = 0;
  int32_t arguments = 0;
  int32_t frame_pointer = -1;
  int32_t local_variables = -1;
};

} // namespace debug
