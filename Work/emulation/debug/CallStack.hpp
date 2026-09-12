#pragma once
#include <cstdint>
#include <vector>
#include <set>
#include <string>
#include "Call.hpp"


namespace hw { class Machine; }

namespace debug {
using namespace hw;

class SymbolTable;

class CallStack {
public:
  static bool debug_call_history;

  std::vector<Call> call_stack;
  SymbolTable* symbols;
  std::set<std::string> debug;
  Machine* machine;

  CallStack(SymbolTable* symbols, Machine* machine);

  std::string location_description(int32_t instruction_address, int32_t symbol_address);
  // P54 reopening (docs/Project54/a003, item 1): the symbol table is NULLABLE —
  // a scratch Machine (the self-test rigs, tests/lowerc_rig.cpp) has none — and
  // every name lookup goes through here so a shadow-stack push never segfaults.
  std::string sym(int32_t address) const;
  void call(int32_t entry_address, int32_t return_address, int32_t call_instruction_address, int32_t arguments);
  void augment(int32_t frame_pointer, int32_t local_variables);
  void call_return(int32_t return_address);
  void native_return(int32_t return_address);   // return from a native translation (no WSAVS frame)
  void backtrace(SymbolTable* symbols, int32_t pc);
};

} // namespace debug
