#include "OSContextIPC.hpp"
#include "OSProcess.hpp"
#include "OSTask.hpp"
#include "OSError.hpp"
#include "OS.hpp"
#include "OSMessage.hpp"
#include "../hw/Memory.hpp"
#include "../hw/Machine.hpp"
#include <cstdio>
#include <stdexcept>
#include <vector>




namespace os {
using namespace hw;

OSContextIPC::OSContextIPC(OSProcess* p, OSTask* t, Memory* m, Machine* mc)
  : OSContext(p, t, m, mc) {}

int32_t OSContextIPC::dispatch_system_call(int32_t call) {
  switch(call) {
    case CREATE: return CREATE_call();
    case SERVE:  return SERVE_call();
    case CON:    return CON_call();
    case DCON:   return DCON_call();
    case ILKUP:  return ILKUP_call();
    case ISEND:  return ISEND_call();
    case IREC:   return IREC_call();
    case ISR:    return ISR_call();
  }
  throw std::runtime_error("Dispatch system call - missing case");
}

int32_t OSContextIPC::CREATE_call() {
  std::string name = read_string(ac0);
  int32_t type = read_packet_word("?CFTYP") & 0xFF;
  int32_t time_packet = read_packet_wide("?CTIM");
  int32_t acl_packet = read_packet_wide("?CACP");

  printf("\nCREATE:\n   name = %s\n   type = %04X\n", name.c_str(), type);
  printf("   time packet address = %08X\n   acl = %08X\n", time_packet, acl_packet);

  if(static_cast<uint32_t>(type) == aos_symbol("?FIPC")) {
    int32_t local_port = read_packet_word("?CPOR");
    printf("   local port = %04X\n\n", local_port);
    try {
      OS::global.register_service(process, name, local_port);
    }
    catch(OSError& error) {
      return error.error();
    }
    return SUCCESS;
  }
  throw std::runtime_error("BOMB");
}

int32_t OSContextIPC::SERVE_call() {
  printf("\nSERVE:\n   servmsg packet = %08X\n\nServer call ignored\n\n", ac2);
  return SUCCESS;
}

int32_t OSContextIPC::ILKUP_call() {
  std::string name = read_string(ac0);
  printf("\nILKUP:\n   service = %s\n\n", name.c_str());
  try {
    ac1 = OS::global.retrieve_service(name);
    ac2 = static_cast<int32_t>(aos_symbol("?FIPC"));
    printf("   service port = %08X\n", ac1);
    return SUCCESS;
  }
  catch(OSError& error) {
    return error.error();
  }
}

int32_t OSContextIPC::ISEND_call() {
  int32_t system_flags = read_packet_word("?ISFL");
  int32_t user_flags = read_packet_word("?IUFL");
  int32_t destination_port = read_packet_wide("?IDPH");
  int32_t origin_port = read_packet_word("?IOPN");
  int32_t length = read_packet_word("?ILTH");
  int32_t data = read_packet_wide("?IPTR");

  printf("\nISEND:\n   system flags = %04X\n   user flags = %04X\n", system_flags, user_flags);
  printf("   destination = %08X\n   origin port = %04X\n", destination_port, origin_port);
  printf("   length = %04X\n   data pointer = %08X\n\n", length, data);

  try {
    fprintf(stderr, "SENDING MESSAGE\n");
    std::vector<int32_t> content(length);
    for(int32_t i = 0; i < length; i++)
      content[i] = static_cast<int32_t>(mem_read_word(data + i));
    OSMessage message((process->pid << 16) | origin_port, destination_port, user_flags, data, content);
    OS::global.send_message(message);
  }
  catch(OSError& error) {
    return error.error();
  }
  return SUCCESS;
}

void OSContextIPC::irec_return() {
  int32_t system_flags = read_packet_word("?ISFL");
  int32_t user_flags = read_packet_word("?IUFL");
  int32_t origin_port = read_packet_wide("?IOPH");
  int32_t destination_port = read_packet_word("?IDPN");
  int32_t length = read_packet_word("?ILTH");
  int32_t data = read_packet_wide("?IPTR");

  fprintf(stderr, "\nIREC RETURN:\n");
  fprintf(stderr, "   system flags = %04X\n   user flags = %04X\n", system_flags, user_flags);
  fprintf(stderr, "   origin port = %08X\n   destination port = %04X\n", origin_port, destination_port);
  fprintf(stderr, "   length = %04X\n   data pointer = %08X\n\n", length, data);
}

int32_t OSContextIPC::IREC_call() {
  int32_t system_flags = read_packet_word("?ISFL");
  int32_t user_flags = read_packet_word("?IUFL");
  int32_t origin_port = read_packet_wide("?IOPH");
  int32_t destination_port = read_packet_word("?IDPN");
  int32_t length = read_packet_word("?ILTH");
  int32_t data = read_packet_wide("?IPTR");

  printf("\nIREC:\n   system flags = %04X\n   user flags = %04X\n", system_flags, user_flags);
  printf("   origin port = %08X\n   destination port = %04X\n", origin_port, destination_port);
  printf("   length = %04X\n   data pointer = %08X\n\n", length, data);

  try {
    OSMessage* message = OS::global.receive_message(process->pid);
    if(!message)
      throw std::runtime_error("System call 'IREC' interrupted");
    auto& content = message->content;
    if(length < static_cast<int32_t>(content.size()))
      throw std::runtime_error("Insufficient space to receive message");

    fprintf(stderr, "RECEIVED MESSAGE\n");
    write_packet_word("?IUFL", message->user_flags);
    write_packet_wide("?IOPH", message->origin());
    write_packet_word("?IDPN", message->destination_port);
    for(int32_t i = 0; i < static_cast<int32_t>(content.size()); i++)
      mem_write_word(data + i, content[i]);
    write_packet_word("?ILTH", static_cast<int32_t>(content.size()));
    if(content.empty())
      write_packet_wide("?IPTR", message->pointer);
    irec_return();
    delete message;
    return SUCCESS;
  }
  catch(OSError& error) {
    return error.error();
  }
}

int32_t OSContextIPC::ISR_call() {
  int32_t system_flags = read_packet_word("?ISFL");
  int32_t user_flags = read_packet_word("?IUFL");
  int32_t destination_port = read_packet_wide("?IDPH");
  int32_t origin_port = read_packet_word("?IOPN");
  int32_t send_length = read_packet_word("?ILTH");
  int32_t send_data = read_packet_wide("?IPTR");
  int32_t receive_length = read_packet_word("?IRLT");
  int32_t receive_data = read_packet_wide("?IRPT");

  printf("\nISR:\n   system flags = %04X\n   user flags = %04X\n", system_flags, user_flags);
  printf("   destination = %08X\n   origin port = %04X\n", destination_port, origin_port);
  printf("   send length = %04X\n   send pointer = %08X\n", send_length, send_data);
  printf("   receive length = %04X\n   receive pointer = %08X\n", receive_length, receive_data);

  try {
    fprintf(stderr, "ISR SENDING MESSAGE\n");
    std::vector<int32_t> content(send_length);
    for(int32_t i = 0; i < send_length; i++)
      content[i] = static_cast<int32_t>(mem_read_word(send_data + i));
    OSMessage send_msg((process->pid << 16) | origin_port, destination_port, user_flags, send_data, content);
    OS::global.send_message(send_msg);

    OSMessage* message = OS::global.receive_message(process->pid);
    if(!message)
      throw std::runtime_error("System call 'ISR' interrupted");
    auto& rcv_content = message->content;
    if(receive_length < static_cast<int32_t>(rcv_content.size()))
      throw std::runtime_error("Insufficient space to receive message");

    fprintf(stderr, "ISR RECEIVED MESSAGE\n");
    write_packet_word("?IUFL", message->user_flags);
    for(int32_t i = 0; i < static_cast<int32_t>(rcv_content.size()); i++)
      mem_write_word(receive_data + i, rcv_content[i]);
    write_packet_word("?IRLT", static_cast<int32_t>(rcv_content.size()));
    if(rcv_content.empty())
      write_packet_wide("?IPTR", message->pointer);
    delete message;
  }
  catch(OSError& error) {
    return error.error();
  }
  return SUCCESS;
}

int32_t OSContextIPC::CON_call() {
  printf("\nCON:\n   pid = %04X\n\n", ac0);
  OS::global.connect(process, ac0);
  return SUCCESS;
}

int32_t OSContextIPC::DCON_call() {
  printf("\nDCON call ignored\n\n");
  return SUCCESS;
}

} // namespace os
