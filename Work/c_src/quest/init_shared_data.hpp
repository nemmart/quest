// src/quest/init_shared_data.hpp
#pragma once

namespace types { class Context; }

namespace quest {

// INIT_SHARED_DATA — open and map the three shared memory files.
// Sets ctx.shared to the resulting SharedData accessor.
// 0 LCALL args.  Void function.
void init_shared_data(types::Context& ctx);

} // namespace quest
