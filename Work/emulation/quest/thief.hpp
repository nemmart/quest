// src/quest/thief.hpp
#pragma once

namespace types { class Context; }

namespace quest {

// THIEF — thief encounter (currently a no-op stub in the original binary).
// 1 LCALL arg (unused).  Void function.
void thief(types::Context& ctx);

} // namespace quest
