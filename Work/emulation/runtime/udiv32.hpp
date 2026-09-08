// src/runtime/udiv32.hpp
//
// ?UDIV32 (0x7017DB3E): quotient = *arg1 / *arg2, *arg3 = remainder;
// quotient returned in ac0 by patching the WSAVS-saved slot. The division
// is the WDIVS instruction with ac0=0: 64-bit {0:dividend} over SIGNED
// divisor, C++ truncation. On divisor==0 or quotient overflow, WDIVS sets
// ovr and leaves registers unchanged, so the body's escape values are
// quotient=dividend, remainder=0, with machine.ovr=1 surviving the return.
#pragma once
#include <cstdint>

namespace hw { class Machine; }

namespace rt {
// Returns the quotient; on error returns the escape quotient (=dividend),
// sets remainder to the escape value (0), and sets error.
int32_t udiv32_3(int32_t dividend, int32_t divisor, int32_t& remainder, bool& error);
}

namespace emu_rt {
uint32_t udiv32(hw::Machine& machine);
}
