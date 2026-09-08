#include "Disassembler.hpp"
#include "../hw/Memory.hpp"
#include <cstdio>




namespace debug {
using namespace hw;

const char* Disassembler::carry_actions[4] = {"", "Z", "O", "C"};
const char* Disassembler::shift_actions[4] = {"", "L", "R", "S"};
const char* Disassembler::skip_actions[8] = {"", "SKP", "SZC", "SNC", "SZR", "SNR", "SEZ", "SBN"};

static std::string hex4(uint32_t v) {
  char buf[16]; std::snprintf(buf, sizeof(buf), "0x%04X", v); return buf;
}
static std::string hex8(uint32_t v) {
  char buf[16]; std::snprintf(buf, sizeof(buf), "0x%08X", v); return buf;
}
static std::string hexX(uint32_t v) {
  char buf[16]; std::snprintf(buf, sizeof(buf), "0x%X", v); return buf;
}

std::string Disassembler::nova_suffix(uint32_t opcode) {
  uint32_t tmp = opcode>>3;
  uint32_t N = tmp&0x01; tmp>>=1;
  uint32_t CC = tmp&0x03; tmp>>=2;
  uint32_t SS = tmp&0x03;
  if (N==0 && CC==0 && SS==0) return "";
  std::string r = ".";
  r += carry_actions[CC];
  r += shift_actions[SS];
  if (N==1) r += "#";
  return r;
}

std::string Disassembler::nova_skip(uint32_t opcode) {
  uint32_t skip_code = opcode&0x07;
  if (skip_code == 0) return "";
  return std::string(",") + skip_actions[skip_code];
}

std::string Disassembler::word_byte_indexed(Memory& memory, uint32_t address, uint32_t opcode, int index_location) {
  uint32_t idx = (opcode>>index_location)&0x03;
  uint32_t offset = memory.read_word(address);
  static const char* bases[] = {"", "pc+", "ac2+", "ac3+"};
  return "[" + std::string(bases[idx]) + hexX(offset&0x7FFFFFFF) + "]";
}

std::string Disassembler::wide_byte_indexed(Memory& memory, uint32_t address, uint32_t opcode, int index_location) {
  uint32_t idx = (opcode>>index_location)&0x03;
  uint32_t offset = memory.read_wide(address);
  static const char* bases[] = {"", "pc+", "ac2+", "ac3+"};
  return "[" + std::string(bases[idx]) + hexX(offset&0x7FFFFFFF) + "]";
}

std::string Disassembler::word_indirect(Memory& memory, uint32_t address, uint32_t opcode, int index_location) {
  uint32_t idx = (opcode>>index_location)&0x03;
  uint32_t offset = memory.read_word(address);
  int32_t relative = static_cast<int32_t>((offset<<17))>>17;
  bool indirect = offset > 0x8000;
  std::string prefix = indirect ? "@" : "";
  static const char* bases[] = {"", "pc+", "ac2+", "ac3+"};
  std::string result = prefix + "[" + std::string(bases[idx]) + hexX(offset&0x7FFFFFFF) + "]";
  if (idx == 1) {
    char buf[32]; std::snprintf(buf, sizeof(buf), " (0x%X)", address + relative);
    result += buf;
  }
  return result;
}

std::string Disassembler::wide_indirect(Memory& memory, uint32_t address, uint32_t opcode, int index_location) {
  uint32_t idx = (opcode>>index_location)&0x03;
  uint32_t offset = memory.read_wide(address);
  int32_t relative = static_cast<int32_t>((offset<<1))>>1;
  bool indirect = (offset&0x80000000) != 0;
  std::string prefix = indirect ? "@" : "";
  static const char* bases[] = {"", "pc+", "ac2+", "ac3+"};
  std::string result = prefix + "[" + std::string(bases[idx]) + hexX(offset&0x7FFFFFFF) + "]";
  if (idx == 1) {
    char buf[32]; std::snprintf(buf, sizeof(buf), " (0x%X)", address + relative);
    result += buf;
  }
  return result;
}

std::string Disassembler::disassemble(Memory& memory, uint32_t address, const std::string& name, uint32_t opcode, const std::string& fmt) {
  if (fmt == "noArguments")
    return name;
  else if (fmt == "novaCompute")
    return name + nova_suffix(opcode) + " " + std::to_string((opcode>>13)&0x03) + "," + std::to_string((opcode>>11)&0x03) + nova_skip(opcode);
  else if (fmt == "register")
    return name + " " + std::to_string((opcode>>11)&0x03);
  else if (fmt == "registerRegister")
    return name + " " + std::to_string((opcode>>13)&0x03) + "," + std::to_string((opcode>>11)&0x03);
  else if (fmt == "shortDisplacement") {
    int32_t amount = static_cast<int32_t>(((opcode>>7)&0xF0) | ((opcode>>6)&0x0F));
    amount = (amount<<24)>>24;
    char buf[32]; std::snprintf(buf, sizeof(buf), " (0x%08X)", address + amount);
    return name + " " + std::to_string(amount) + buf;
  }
  else if (fmt == "bitPosition")
    return name + " " + std::to_string(((opcode>>10)&0x1C) | ((opcode>>4)&0x03));
  else if (fmt == "wordImmediate")
    return name + " " + hex4(memory.read_word(address+1));
  else if (fmt == "wideImmediate")
    return name + " " + hex8(memory.read_wide(address+1));
  else if (fmt == "tinyImmediateRegister")
    return name + " " + std::to_string(static_cast<int32_t>((opcode>>13))-4+1) + "," + std::to_string((opcode>>11)&0x03);
  else if (fmt == "registerWordImmediate") {
    uint32_t imm = memory.read_word(address+1);
    char buf[32]; std::snprintf(buf, sizeof(buf), " (0x%04X)", imm);
    return name + " " + std::to_string((opcode>>11)&0x03) + "," + std::to_string(imm) + buf;
  }
  else if (fmt == "registerWideImmediate") {
    uint32_t imm = memory.read_wide(address+1);
    return name + " " + std::to_string((opcode>>11)&0x03) + "," + hex8(imm);
  }
  else if (fmt == "wordImmediateRegister") {
    uint32_t imm = memory.read_word(address+1);
    char buf[32]; std::snprintf(buf, sizeof(buf), " (0x%04X)", imm);
    return name + " " + std::to_string(imm) + buf + "," + std::to_string((opcode>>11)&0x03);
  }
  else if (fmt == "bitOffset")
    return name + " " + std::to_string(((opcode>>10)&0x1C) | ((opcode>>4)&0x03));
  else if (fmt == "registerWordIndirect")
    return name + " " + std::to_string((opcode>>11)&0x03) + "," + word_indirect(memory, address+1, opcode, 13);
  else if (fmt == "registerWideIndirect")
    return name + " " + std::to_string((opcode>>11)&0x03) + "," + wide_indirect(memory, address+1, opcode, 13);
  else if (fmt == "registerWordByteIndexed")
    return name + " " + std::to_string((opcode>>11)&0x03) + "," + word_byte_indexed(memory, address+1, opcode, 13);
  else if (fmt == "registerWideByteIndexed")
    return name + " " + std::to_string((opcode>>11)&0x03) + "," + wide_byte_indexed(memory, address+1, opcode, 13);
  else if (fmt == "tinyImmediateWordIndirect")
    return name + " " + std::to_string(static_cast<int32_t>((opcode>>13))-4+1) + "," + word_indirect(memory, address+1, opcode, 11);
  else if (fmt == "tinyImmediateWideIndirect")   // L-forms: wide displacement, 3 words
    return name + " " + std::to_string(static_cast<int32_t>((opcode>>13))-4+1) + "," + wide_indirect(memory, address+1, opcode, 11);
  else if (fmt == "wordIndirect")
    return name + " " + word_indirect(memory, address+1, opcode, 11);
  else if (fmt == "wideIndirect")
    return name + " " + wide_indirect(memory, address+1, opcode, 11);
  else if (fmt == "wordIndirectArgument") {
    uint32_t argument = memory.read_word(address+2);
    return name + " " + word_indirect(memory, address+1, opcode, 11) + "," + std::to_string(argument);
  }
  else if (fmt == "wideIndirectArgument") {
    uint32_t argument = memory.read_word(address+3);
    return name + " " + wide_indirect(memory, address+1, opcode, 11) + "," + std::to_string(argument);
  }
  else
    return name;
}

int Disassembler::word_length(const std::string& fmt) {
  if (fmt == "noArguments") return 1;
  if (fmt == "novaCompute") return 1;
  if (fmt == "register") return 1;
  if (fmt == "registerRegister") return 1;
  if (fmt == "shortDisplacement") return 1;
  if (fmt == "bitPosition") return 1;
  if (fmt == "wordImmediate") return 2;
  if (fmt == "wideImmediate") return 3;
  if (fmt == "tinyImmediateRegister") return 1;
  if (fmt == "registerWordImmediate") return 2;
  if (fmt == "registerWideImmediate") return 3;
  if (fmt == "wordImmediateRegister") return 2;
  if (fmt == "bitOffset") return 1;
  if (fmt == "registerWordIndirect") return 2;
  if (fmt == "registerWideIndirect") return 3;
  if (fmt == "registerWordByteIndexed") return 2;
  if (fmt == "registerWideByteIndexed") return 3;
  if (fmt == "tinyImmediateWordIndirect") return 2;
  if (fmt == "tinyImmediateWideIndirect") return 3;  // was merged with the
                                          // word class at 2 — the LNADI/LNSBI
                                          // phantom-listing defect (P23 report)
  if (fmt == "wordIndirect") return 2;
  if (fmt == "wideIndirect") return 3;
  if (fmt == "wordIndirectArgument") return 3;
  if (fmt == "wideIndirectArgument") return 4;
  return 1;
}

} // namespace debug
