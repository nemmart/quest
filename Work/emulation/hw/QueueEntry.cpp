#include "QueueEntry.hpp"
#include "Machine.hpp"




namespace hw {
// Note: `completed` is set by the MachineThread worker (under its mutex),
// not here — in lockstep mode a master batch is only released after its
// clone counterpart has run and the pair has been compared.
void QueueEntry::run() {
  insn_before = machine->instruction_count;
  try {
    address = machine->run_steps(address, steps);
  }
  catch(std::runtime_error& e) {
    exception = new std::runtime_error(e);
  }
  insn_after = machine->instruction_count;
  block_after = machine->block_ordinal;
  native_span = machine->native_span;
  machine->native_span = false;
  terminal = machine->terminal_reached;
  machine->terminal_reached = false;
}

} // namespace hw
