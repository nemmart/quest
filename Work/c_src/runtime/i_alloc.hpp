// src/runtime/i_alloc.hpp
//
// Native translations of the PL/1 runtime heap: I.ALLOC (0x7017E866)
// and the I.FREE family (I.FREEB 0x7017E945, I.FREEW 0x7017E949,
// I.FREE 0x7017E94C). Derivation: docs/I_ALLOC.md; empirical ground
// truth: docs/captures/.
//
// Native fast paths cover exactly the two capture-verified paths —
// extend-allocation and bottom-adjacent free with break retraction.
// Every other path (lock contention, waiters, deferred-free chain,
// MEMI mode [0x700001F2] != 0, non-empty free-list, non-adjacent or
// mergeable frees, ownership or validity failures, stack collision)
// falls back to full emulation via RTStubs::entry_address, keeping
// master and clone symmetric by construction — including the authentic
// QSEARCH trap.

#pragma once
#include <cstdint>

namespace hw { class Machine; class Memory; }

namespace rt {

// Heap globals (wide addresses; see I_ALLOC.md "Heap-state variables").
constexpr uint32_t HEAP_BREAK   = 0x700001F0;  // grows DOWNWARD; block = break+4
constexpr uint32_t HEAP_MODE    = 0x700001F2;  // 0 = stack-limit steal; !=0 = MEMI
constexpr uint32_t HEAP_FREEQ   = 0x700001F4;  // free-list descriptor (2 wides)
constexpr uint32_t HEAP_DEFER   = 0x700001FA;  // deferred-free chain head
constexpr uint32_t HEAP_LOWMARK = 0x700001FC;
constexpr uint32_t HEAP_HIMARK  = 0x700001FE;
constexpr uint32_t HEAP_LOCK    = 0x70000200;  // the I.LOCK/I.UNLOCK object
constexpr uint32_t HEAP_OWNER   = 0x70000204;  // owner comparand (stack base)

// Size-class computation (LDSP table 0x7017E879 + shared tail).
// Returns the block size in words INCLUDING the 4-word header, with
// the minimum of 8 applied. Out-of-range classes take the class-3
// default, as LDSP falls through to 0x7017E88A.
int32_t heap_class_size(int32_t request, int32_t heap_class);

} // namespace rt

namespace emu_rt {
uint32_t i_alloc(hw::Machine& machine);
uint32_t i_freeb(hw::Machine& machine);
uint32_t i_freew(hw::Machine& machine);
uint32_t i_free(hw::Machine& machine);
} // namespace emu_rt
