// src/hw/NativeRegistry.hpp
#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>

namespace debug { class SymbolTable; }

namespace hw {

class Machine;

using NativeFunc = uint32_t (*)(Machine&);

class NativeRegistry {
public:
  // Name-based: looks up address via symbols, silently skips if not found
  void register_by_name(debug::SymbolTable& symbols, const std::string& name, NativeFunc fn);

  // Relative to named symbol: for unnamed routines at known offsets
  void register_relative(debug::SymbolTable& symbols, const std::string& base, int32_t offset, NativeFunc fn);

  // Address-based: direct registration (fallback)
  void register_by_address(uint32_t address, NativeFunc fn);

  // Fast lookup at LCALL/XCALL time — returns nullptr if not found
  NativeFunc lookup(uint32_t address) const;

  // Populate all known native function mappings for this binary
  void register_all(debug::SymbolTable& symbols, const std::string& program);

private:
  std::unordered_map<uint32_t, NativeFunc> registry;
};

} // namespace hw
