// src/hw/NativeRegistry.cpp
//
// The native-function hook mechanism: translated (native C++) functions can
// be registered against symbol addresses, and EagleStack's LCALL/XCALL will
// divert to them instead of executing the emulated code.
//
// The registry is currently EMPTY: no translations are registered, so the
// emulator runs everything emulated. Translations will be re-introduced
// one function at a time under lockstep validation (see PORTING_PLAN.md).
// The earlier generation of translations lives outside the build in
// emu_rt/, emu_quest/, rt/, quest/, types/, emu_types/ for reference.

#include "NativeRegistry.hpp"
#include "RTStubs.hpp"
#include "../debug/SymbolTable.hpp"
#include <cstdio>
#include <cctype>

namespace hw {
using namespace debug;

void NativeRegistry::register_by_name(SymbolTable& symbols, const std::string& name, NativeFunc fn) {
  uint32_t addr=symbols.address_for_name(name);
  if(addr!=0xFFFFFFFF) {
    registry[addr]=fn;
    fprintf(stderr, "NativeRegistry: %s -> 0x%08X\n", name.c_str(), addr);
  }
}

void NativeRegistry::register_relative(SymbolTable& symbols, const std::string& base, int32_t offset, NativeFunc fn) {
  uint32_t base_addr=symbols.address_for_name(base);
  if(base_addr!=0xFFFFFFFF) {
    uint32_t addr=base_addr+offset;
    registry[addr]=fn;
    fprintf(stderr, "NativeRegistry: %s+0x%X -> 0x%08X\n", base.c_str(), offset, addr);
  }
}

void NativeRegistry::register_by_address(uint32_t address, NativeFunc fn) {
  registry[address]=fn;
  fprintf(stderr, "NativeRegistry: 0x%08X (direct)\n", address);
}

NativeFunc NativeRegistry::lookup(uint32_t address) const {
  std::unordered_map<uint32_t, NativeFunc>::const_iterator it=registry.find(address);
  if(it!=registry.end())
    return it->second;
  return nullptr;
}

void NativeRegistry::register_all(SymbolTable& symbols, const std::string& program) {
  // Log-and-continue stubs for every RT entry (see RTStubs.hpp). The caller
  // (OSProcess::launch) gates this to the lockstep CLONE: the master and
  // non-lockstep runs stay pure emulation. Real translations will replace
  // stubs one at a time (SessionPlan.md).
  std::string upper=program;
  for(char& ch : upper) ch=toupper(ch);
  if(upper=="QUEST")
    RTStubs::register_stubs(*this, symbols);
}

} // namespace hw
