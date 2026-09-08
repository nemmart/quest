// src/quest/get_object_index.hpp
#pragma once
#include <cstdint>

namespace types { class Context; }

namespace quest {

// GET_OBJECT_INDEX — find a free object slot or allocate a new one.
// 1 LCALL arg: &index (wide, output).
// Searches for an unreferenced empty slot (index >= 12), or
// grows the object table.  Void function.
void get_object_index(types::Context& ctx, int32_t& index);

} // namespace quest
