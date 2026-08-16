// src/emu_rt/syscall.cpp
#include "syscall.hpp"
#include "../hw/Machine.hpp"
#include "../emu_types/WordArray.hpp"
#include "../rt/ipc_send_receive.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"

namespace emu_rt {
using namespace hw;

// AOS/VS IPC packet field offsets (word offsets from packet base)
// ISEND/ISR send fields:
static constexpr int32_t IUFL=0x01;  // user flags
static constexpr int32_t IDPH=0x02;  // destination port (wide)
static constexpr int32_t IOPN=0x04;  // origin port number
static constexpr int32_t ILTH=0x05;  // send length (words)
static constexpr int32_t IPTR=0x06;  // send data pointer / value (wide)
// ISR receive fields:
static constexpr int32_t IRLT=0x09;  // receive length (words)
static constexpr int32_t IRPT=0x0A;  // receive data pointer (wide)
// IREC fields (different names, same packet):
static constexpr int32_t IOPH=0x02;  // origin port (wide) — overlaps IDPH
static constexpr int32_t IDPN=0x04;  // destination port number — overlaps IOPN

// SYSCALL 025 — ISEND
static int32_t dispatch_isend(Machine& machine) {
  types::Context& ctx=*machine.native_context;
  Memory& mem=*machine.memory;
  int32_t pkt=machine.ac[2];

  int32_t user_flags=static_cast<int32_t>(mem.read_word(pkt+IUFL));
  int32_t destination_port=static_cast<int32_t>(mem.read_wide(pkt+IDPH));
  int32_t origin_port=static_cast<int32_t>(mem.read_word(pkt+IOPN));
  int32_t send_length=static_cast<int32_t>(mem.read_word(pkt+ILTH));
  int32_t send_ptr=static_cast<int32_t>(mem.read_wide(pkt+IPTR));

  emu_types::WordArray send_data(mem, send_ptr, send_length);
  int32_t value=send_data.empty() ? send_ptr : 0;

  return rt::ipc_send(ctx, destination_port, origin_port, user_flags, send_data, value);
}

// SYSCALL 026 — IREC
static int32_t dispatch_irec(Machine& machine) {
  types::Context& ctx=*machine.native_context;
  Memory& mem=*machine.memory;
  int32_t pkt=machine.ac[2];

  int32_t receive_length=static_cast<int32_t>(mem.read_word(pkt+ILTH));
  int32_t receive_ptr=static_cast<int32_t>(mem.read_wide(pkt+IPTR));

  emu_types::WordArray receive_data(mem, receive_ptr, 0, receive_length);
  int32_t origin=0, destination_port=0, user_flags=0, value=0;

  int32_t err=rt::ipc_receive(ctx, origin, destination_port, user_flags, receive_data, value);
  if(err) return err;

  mem.write_word(pkt+IUFL, user_flags);
  mem.write_wide(pkt+IOPH, origin);
  mem.write_word(pkt+IDPN, destination_port);
  mem.write_word(pkt+ILTH, static_cast<int32_t>(receive_data.size()));
  if(receive_data.empty())
    mem.write_wide(pkt+IPTR, value);

  return 0;
}

// SYSCALL 0142 — ISR (send then receive)
static int32_t dispatch_isr(Machine& machine) {
  types::Context& ctx=*machine.native_context;
  Memory& mem=*machine.memory;
  int32_t pkt=machine.ac[2];

  int32_t user_flags=static_cast<int32_t>(mem.read_word(pkt+IUFL));
  int32_t destination_port=static_cast<int32_t>(mem.read_wide(pkt+IDPH));
  int32_t origin_port=static_cast<int32_t>(mem.read_word(pkt+IOPN));
  int32_t send_length=static_cast<int32_t>(mem.read_word(pkt+ILTH));
  int32_t send_ptr=static_cast<int32_t>(mem.read_wide(pkt+IPTR));
  int32_t receive_length=static_cast<int32_t>(mem.read_word(pkt+IRLT));
  int32_t receive_ptr=static_cast<int32_t>(mem.read_wide(pkt+IRPT));

  emu_types::WordArray send_data(mem, send_ptr, send_length);
  emu_types::WordArray receive_data(mem, receive_ptr, 0, receive_length);
  int32_t send_value=send_data.empty() ? send_ptr : 0;
  int32_t receive_value=0;

  int32_t err=rt::ipc_send_receive(ctx, destination_port, origin_port, user_flags,
                                   send_data, send_value, receive_data, receive_value);
  if(err) return err;

  mem.write_word(pkt+IUFL, user_flags);
  mem.write_word(pkt+IRLT, static_cast<int32_t>(receive_data.size()));
  if(receive_data.empty())
    mem.write_wide(pkt+IPTR, receive_value);

  return 0;
}

// SYSCALL 073 — GSHPT (get shared page table)
static int32_t dispatch_gshpt(Machine& machine) {
  types::Context& ctx=*machine.native_context;
  int32_t shared_start=0, page_count=0;
  int32_t err=ctx.os.gshpt(shared_start, page_count);
  if(err) return err;
  machine.ac[0]=shared_start;
  machine.ac[1]=page_count;
  return 0;
}

// SYSCALL 044 — SSHPT (set shared page table)
static int32_t dispatch_sshpt(Machine& machine) {
  types::Context& ctx=*machine.native_context;
  return ctx.os.sshpt(machine.ac[0], machine.ac[1]);
}

// SYSCALL 0167 — CON (connect to process)
static int32_t dispatch_con(Machine& machine) {
  types::Context& ctx=*machine.native_context;
  return ctx.os.connect(machine.ac[0]);
}

// SYSCALL 0170 — DCON (disconnect from process)
static int32_t dispatch_dcon(Machine& machine) {
  types::Context& ctx=*machine.native_context;
  return ctx.os.disconnect(machine.ac[0]);
}

// SYSCALL 0171 — SERVE (register server)
static int32_t dispatch_serve(Machine& machine) {
  types::Context& ctx=*machine.native_context;
  return ctx.os.serve(machine.ac[0]);
}

// SYSCALL 0310 — RETURN (terminate process)
// Never returns — throws std::runtime_error("EXIT!")
static int32_t dispatch_return(Machine& machine) {
  types::Context& ctx=*machine.native_context;
  std::string message;
  int32_t length=machine.ac[2]&0xFF;
  if(length>0) {
    message.resize(length);
    for(int32_t i=0; i<length; i++)
      message[i]=static_cast<char>(machine.memory->read_byte(
        static_cast<uint32_t>(machine.ac[1])+static_cast<uint32_t>(i)));
  }
  ctx.os.terminate_process(message);
  // never reached
}

// SYSCALL 0232 — UPDATE (flush file to disk — no-op)
static int32_t dispatch_update(Machine& machine) {
  return 0;
}

// Dispatch by syscall number. Returns 0 (success), >0 (error), -1 (not handled).
static int32_t dispatch(Machine& machine, int32_t call_number) {
  switch(call_number) {
  case 0025: return dispatch_isend(machine);
  case 0026: return dispatch_irec(machine);
  case 0044: return dispatch_sshpt(machine);
  case 0073: return dispatch_gshpt(machine);
  case 0142: return dispatch_isr(machine);
  case 0167: return dispatch_con(machine);
  case 0170: return dispatch_dcon(machine);
  case 0171: return dispatch_serve(machine);
  case 0232: return dispatch_update(machine);
  case 0310: return dispatch_return(machine);
  default:   return -1;
  }
}

// Native handler registered at 0x30000000.
// PSR|args already pushed, ac[3] = return address.
uint32_t syscall_handler(Machine& machine) {
  // Read syscall number from the pushed PSR|args word
  uint32_t slot=Machine::copy_segment(
    static_cast<uint32_t>(machine.wsp),
    static_cast<uint32_t>(machine.wsp)-1);
  int32_t call=machine.memory->read_word(slot)&0x7FFF;

  int32_t result=dispatch(machine, call);
  if(result==-1)
    return 0x30000000;  // not handled — fall through to emulated dispatch

  // Handled natively — pop PSR and return
  machine.set_psr(static_cast<int32_t>(static_cast<uint32_t>(machine.wide_pop())>>16));
  int32_t return_address=machine.ac[3];
  if(result==0)
    return static_cast<uint32_t>(return_address+1);
  machine.ac[0]=result;
  return static_cast<uint32_t>(return_address);
}

} // namespace emu_rt
