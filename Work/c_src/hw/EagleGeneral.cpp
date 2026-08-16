#include "EagleGeneral.hpp"
#include "Machine.hpp"
#include "NativeRegistry.hpp"
#include "RTStubs.hpp"
#include "Decoder.hpp"
#include <cstdio>
#include "../os/OSProcess.hpp"




namespace hw {
void EagleGeneral::setup(uint32_t opcode, const std::string& name, const std::string& fmt, int32_t op) {
  EagleInstruction::setup(opcode, name, fmt, op);
  opcode=opcode>>11;
  AA=opcode & 0x03;
  opcode=opcode>>2;
  II=opcode & 0x03;
}

uint32_t EagleGeneral::execute(Machine& machine, uint32_t address, uint32_t opcode) {
  int32_t resolved, src, L, H, value;
  NativeFunc native;

  switch(oper) {
   case XWLDA:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), II);
    src=machine.memory->read_wide(resolved);
    machine.ac[AA]=src;
    return copy_segment(address, address+2);

   case XWSTA:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), II);
    src=machine.ac[AA];
    machine.memory->write_wide(resolved, src);
    return copy_segment(address, address+2);

   case LWLDA:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), II);
    machine.ac[AA]=machine.memory->read_wide(resolved);
    return copy_segment(address, address+3);

   case LWSTA:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), II);
    src=machine.ac[AA];
    machine.memory->write_wide(resolved, src);
    return copy_segment(address, address+3);

   case XNLDA:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), II);
    src=machine.memory->read_word(resolved);
    src=static_cast<int32_t>(src<<16)>>16;
    machine.ac[AA]=src;
    return copy_segment(address, address+2);

   case XNSTA:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), II);
    src=machine.ac[AA];
    machine.memory->write_word(resolved, src&0xFFFF);
    return copy_segment(address, address+2);

   case LNLDA:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), II);
    src=machine.memory->read_word(resolved);
    src=static_cast<int32_t>(src<<16)>>16;
    machine.ac[AA]=src;
    return copy_segment(address, address+3);

   case LNSTA:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), II);
    src=machine.ac[AA];
    machine.memory->write_word(resolved, src & 0xFFFF);
    return copy_segment(address, address+3);

   case XLEF:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), II);
    machine.ac[AA]=resolved;
    return copy_segment(address, address+2);

   case LLEF:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), II);
    machine.ac[AA]=resolved;
    return copy_segment(address, address+3);

   case XLEFB:
    resolved=machine.eagle_x_byte_indexed(copy_segment(address, address+1), II);
    machine.ac[AA]=resolved;
    return copy_segment(address, address+2);

   case LLEFB:
    resolved=machine.eagle_l_byte_indexed(copy_segment(address, address+1), II);
    machine.ac[AA]=resolved;
    return copy_segment(address, address+3);

   case XLDB:
    resolved=machine.eagle_x_byte_indexed(copy_segment(address, address+1), II);
    machine.ac[AA]=machine.memory->read_byte(resolved);
    return copy_segment(address, address+2);

   case XSTB:
    resolved=machine.eagle_x_byte_indexed(copy_segment(address, address+1), II);
    machine.memory->write_byte(resolved, machine.ac[AA] & 0xFF);
    return copy_segment(address, address+2);

   case LLDB:
    resolved=machine.eagle_l_byte_indexed(copy_segment(address, address+1), II);
    machine.ac[AA]=machine.memory->read_byte(resolved);
    return copy_segment(address, address+3);

   case LSTB:
    resolved=machine.eagle_l_byte_indexed(copy_segment(address, address+1), II);
    machine.memory->write_byte(resolved, machine.ac[AA] & 0xFF);
    return copy_segment(address, address+3);

   case WLDB:
    machine.ac[AA]=machine.memory->read_byte(machine.ac[II]);
    return copy_segment(address, address+1);

   case WSTB:
    machine.memory->write_byte(machine.ac[II], machine.ac[AA] & 0xFF);
    return copy_segment(address, address+1);

   case LPSR:
    machine.ac[0]=machine.get_psr()<<16;
    return copy_segment(address, address+1);

   case CRYTO:
    machine.c=1;
    return copy_segment(address, address+1);

   case CRYTZ:
    machine.c=0;
    return copy_segment(address, address+1);

   case XJMP:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), AA);
    return resolved;

   case XJSR:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), AA);
    machine.ac[3]=copy_segment(address, address+2);
    if(machine.process) {
      native=machine.process->native_registry.lookup(static_cast<uint32_t>(resolved));
      if(native) {
        // Nested-in-fallback guard — same rule as the LCALL/XCALL sites
        // in EagleStack.cpp: inside an outer fallback span the clone
        // re-emulates everything (the master's registry is empty), so
        // running an inner native here would skew the span's compared
        // instruction counts. Found live: LJSR I.FREEW inside the
        // emulated ?LIB_ERROR fallback body. Do not re-arm.
        if(machine.rt_pending_return!=0)
          return resolved;
        if(RTStubs::defer_dispatch(static_cast<uint32_t>(resolved))) {
          // Crossings-only checker: an L1→L2 crossing into translated
          // L2 (I.PROLOG/O.ON/O.REVERT/I.EPILOG/I.GOTO arrive by LJSR).
          // Defer the native call so the batch breaks AT the entry pc —
          // the crossing rendezvous — and the implementation runs on
          // resume (Machine::pending_native).
          machine.pending_native=native;
          return resolved;
        }
        return native(machine);
      }
    }
    return resolved;

   case XNDO:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), AA);
    src=machine.memory->read_word(resolved);
    src=narrow_add(machine, 1, src);
    machine.memory->write_word(resolved, src);
    if(static_cast<int32_t>(src)>static_cast<int32_t>(machine.ac[II])) {
     machine.ac[II]=src;
     value=machine.memory->read_word(copy_segment(address, address+2));
     return copy_segment(address, address+1+value);
    }
    machine.ac[II]=src;
    return copy_segment(address, address+3);

   case XWDO:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), AA);
    src=machine.memory->read_wide(resolved);
    src=add(machine, 1, src);
    machine.memory->write_wide(resolved, src);
    if(static_cast<int32_t>(src)>static_cast<int32_t>(machine.ac[II])) {
     machine.ac[II]=src;
     value=machine.memory->read_word(copy_segment(address, address+2));
     return copy_segment(address, address+1+value);
    }
    machine.ac[II]=src;
    return copy_segment(address, address+3);

   case LJMP:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), AA);
    return resolved;

   case LJSR:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), AA);
    machine.ac[3]=copy_segment(address, address+3);
    if(machine.process) {
      native=machine.process->native_registry.lookup(static_cast<uint32_t>(resolved));
      if(native) {
        // Nested-in-fallback guard — same rule as the LCALL/XCALL sites
        // in EagleStack.cpp: inside an outer fallback span the clone
        // re-emulates everything (the master's registry is empty), so
        // running an inner native here would skew the span's compared
        // instruction counts. Found live: LJSR I.FREEW inside the
        // emulated ?LIB_ERROR fallback body. Do not re-arm.
        if(machine.rt_pending_return!=0)
          return resolved;
        if(RTStubs::defer_dispatch(static_cast<uint32_t>(resolved))) {
          // Crossings-only checker: an L1→L2 crossing into translated
          // L2 (I.PROLOG/O.ON/O.REVERT/I.EPILOG/I.GOTO arrive by LJSR).
          // Defer the native call so the batch breaks AT the entry pc —
          // the crossing rendezvous — and the implementation runs on
          // resume (Machine::pending_native).
          machine.pending_native=native;
          return resolved;
        }
        return native(machine);
      }
    }
    return resolved;

   case LNDO:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), AA);
    src=machine.memory->read_word(resolved);
    src=narrow_add(machine, 1, src);
    machine.memory->write_word(resolved, src);
    if(static_cast<int32_t>(src)>static_cast<int32_t>(machine.ac[II])) {
     machine.ac[II]=src;
     value=machine.memory->read_word(copy_segment(address, address+3));
     return copy_segment(address, address+1+value);
    }
    machine.ac[II]=src;
    return copy_segment(address, address+4);

   case LWDO:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), AA);
    src=machine.memory->read_wide(resolved);
    src=add(machine, 1, src);
    machine.memory->write_wide(resolved, src);
    if(static_cast<int32_t>(src)>static_cast<int32_t>(machine.ac[II])) {
     machine.ac[II]=src;
     value=machine.memory->read_word(copy_segment(address, address+3));
     return copy_segment(address, address+1+value);
    }
    machine.ac[II]=src;
    return copy_segment(address, address+4);

   case LDSP:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), II);
    L=machine.memory->read_wide(resolved-4);
    H=machine.memory->read_wide(resolved-2);
    if(L<=static_cast<int32_t>(machine.ac[AA]) && static_cast<int32_t>(machine.ac[AA])<=H) {
     value=machine.memory->read_wide(copy_segment(resolved, resolved+(static_cast<int32_t>(machine.ac[AA])-L)*2));
     if(value!=-1)
      return copy_segment(address, value+resolved+(static_cast<int32_t>(machine.ac[AA])-L)*2);
    }
    return copy_segment(address, address+3);

   case WCLM:
    if(II==AA) {
     resolved=copy_segment(address, address+1);
     address=copy_segment(address, address+4);
    }
    else
     resolved=machine.eagle_resolve_indirect(machine.ac[AA]);
    L=machine.memory->read_wide(resolved);
    H=machine.memory->read_wide(resolved+2);
    if(L<=static_cast<int32_t>(machine.ac[II]) && static_cast<int32_t>(machine.ac[II])<=H)
     return copy_segment(address, address+2);
    return copy_segment(address, address+1);

   case WBR:
    value=((opcode>>7) & 0xF0) + ((opcode>>6) & 0x0F);
    value=static_cast<int32_t>(value<<24)>>24;
    return copy_segment(address, address+value);

   case XCT: {
    // Execute Accumulator. Executes bits 16-31 of the accumulator (the
    // LOW half, in DG's MSB-0 numbering) as an instruction. Multiword
    // instructions fetch their extra words from the words immediately
    // FOLLOWING the XCT — which is exactly what passing the XCT's own
    // `address` down achieves, since every execute() resolves its extra
    // words as copy_segment(address, address+N).
    //
    // Continuation falls out of the callee's own return value:
    //   one-word instruction  -> address+1
    //   two-word instruction  -> address+2
    //   jump or skip          -> the effective address
    // matching the three cases in the DG description.
    //
    // Used by Quest at 0x7017E9F6 (free-list insert: executes a
    // constructed ENQH/ENQT opcode) and 0x7017ECF4 (I.GOTO: builds
    // FSS n,n from the FPU status word to zero the faulting FPAC).
    uint32_t sub=static_cast<uint32_t>(machine.ac[AA]) & 0xFFFF;
    bool lef=machine.segments[(address>>28) & 0x07]->lef;
    Instruction* instruction=Decoder::decode(lef, sub);
    if(instruction==nullptr) {
      char buf[64];
      snprintf(buf, sizeof(buf), "XCT of undecodable instruction %04X at %08X",
               sub, address);
      throw std::runtime_error(buf);
    }
    return instruction->execute(machine, address, sub);
   }
  }
  throw std::runtime_error("Internal error - some case is not returning");
}

} // namespace hw
