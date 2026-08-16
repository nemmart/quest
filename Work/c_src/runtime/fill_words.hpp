// src/runtime/fill_words.hpp
//
// ?FILL_WORDS (0x7017E31C): fill `count` words at `dest` with `value`
// (2-arg form: value = 0). The original stores dest[0]=value then runs
// WBLM with count-1 in self-overlapping propagation; rt:: ports that loop
// exactly so all count edge cases (0, negative) match by construction.
#pragma once
#include <cstdint>

namespace hw { class Machine; class Memory; }

namespace rt {
void fill_words_2(hw::Memory& memory, uint32_t dest, int32_t count);
void fill_words_3(hw::Memory& memory, uint32_t dest, int32_t count, int32_t value);
}

namespace emu_rt {
// NativeRegistry entry: reads args via RTBridge, dispatches on arg count.
uint32_t fill_words(hw::Machine& machine);
}
