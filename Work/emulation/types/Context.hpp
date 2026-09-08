// src/types/Context.hpp
#pragma once

namespace types {
class OperatingSystem;
class SharedRandomState;
}

namespace quest { class SharedData; }

// Context is a resource container threaded through all rt:: and quest::
// functions.  It holds references to the services that native code needs.
// Created once per task and cached on Machine.
//
// Some members are pointers that start null and are set later when the
// backing resource becomes available (e.g., random_state depends on
// shared memory mapped by INIT_SHARED_DATA).

namespace types {

struct Context {
  OperatingSystem& os;
  SharedRandomState* random_state;
  quest::SharedData* shared;

  explicit Context(OperatingSystem& os) : os(os), random_state(nullptr), shared(nullptr) {}
};

} // namespace types
