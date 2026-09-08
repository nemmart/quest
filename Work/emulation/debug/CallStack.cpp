#include "CallStack.hpp"
#include "SymbolTable.hpp"
#include "../hw/Machine.hpp"
#include <cstdio>
#include <stdexcept>




namespace debug {
using namespace hw;

bool CallStack::debug_call_history = false;

CallStack::CallStack(SymbolTable* symbols, Machine* machine)
  : symbols(symbols), machine(machine) {}

std::string CallStack::location_description(int32_t instruction_address, int32_t symbol_address) {
  char buf[128];
  int32_t first_address = symbols->first_address(instruction_address);
  int32_t last_address = symbols->last_address(symbol_address);
  if(first_address==symbol_address && instruction_address<last_address) {
    snprintf(buf, sizeof(buf), "%s+0x%X   [%08X]",
      symbols->name_for_address(first_address).c_str(),
      instruction_address-first_address, instruction_address);
  } else {
    snprintf(buf, sizeof(buf), "%08X     guessing: %s+0x%X",
      instruction_address,
      symbols->name_for_address(first_address).c_str(),
      instruction_address-first_address);
  }
  return buf;
}

void CallStack::call(int32_t entry_address, int32_t return_address, int32_t call_instruction_address, int32_t arguments) {
  if(debug.count(symbols->name_for_address(entry_address)))
    machine->debug = true;

  Call c;
  c.entry_address = entry_address;
  c.return_address = return_address;
  c.call_instruction_address = call_instruction_address;
  c.frame_pointer = -1;
  c.local_variables = -1;
  call_stack.push_back(c);
}

void CallStack::augment(int32_t frame_pointer, int32_t local_variables) {
  if(call_stack.empty())
    throw std::runtime_error("Empty call stack");
  Call& c = call_stack.back();
  if(c.local_variables != -1)
    fprintf(stderr, "CALL HAS ALREADY BEEN AUGMENTED: %s\n", symbols->name_for_address(c.entry_address).c_str());
  c.frame_pointer = frame_pointer;
  c.local_variables = local_variables;
  if(debug_call_history)
    printf("Called %s   [%08X]\n", symbols->name_for_address(c.entry_address).c_str(), c.entry_address);
}

void CallStack::native_return(int32_t return_address) {
  // Return from a native translation: the frame was never augmented by
  // WSAVS (the native path doesn't build one), so skip that warning but
  // keep the return-address check.
  if(call_stack.empty())
    throw std::runtime_error("Empty call stack");
  Call& c = call_stack.back();
  if(return_address != c.return_address && return_address != c.return_address+1) {
    printf("native return address; %08X, stack return address: %08X\n", return_address, c.return_address);
    fprintf(stderr, "native return address; %08X, stack return address: %08X\n", return_address, c.return_address);
  }
  call_stack.pop_back();
}

void CallStack::call_return(int32_t return_address) {
  if(call_stack.empty())
    throw std::runtime_error("Empty call stack");
  Call& c = call_stack.back();
  if(c.local_variables == -1)
    fprintf(stderr, "CALL HAS NOT BEEN AUGMENTED\n");
  if(return_address != c.return_address && return_address != c.return_address+1) {
    printf("call return address; %08X, stack return address: %08X\n", return_address, c.return_address);
    fprintf(stderr, "call return address; %08X, stack return address: %08X\n", return_address, c.return_address);
  }
  if(debug.count(symbols->name_for_address(c.entry_address)))
    machine->debug = false;
  call_stack.pop_back();
  if(debug_call_history && !call_stack.empty())
    printf("Returning to %s(sp=%08X)\n",
      location_description(return_address, call_stack.back().entry_address).c_str(),
      machine->wsp);
}

void CallStack::backtrace(SymbolTable* symbols, int32_t pc) {
  for(int index = (int)call_stack.size()-1; index >= 0; index--) {
    Call& c = call_stack[index];
    std::string location;
    if(index == (int)call_stack.size()-1)
      location = location_description(pc, c.entry_address);
    else {
      int32_t cia = call_stack[index+1].call_instruction_address;
      if(cia != -1)
        location = location_description(cia, c.entry_address);
      else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%08X", call_stack[index+1].return_address);
        location = buf;
      }
    }
    printf("frame %2d -- %s\n", index, location.c_str());
  }
}

} // namespace debug
