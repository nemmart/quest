#include "SymbolTable.hpp"
#include <stdexcept>




namespace debug {
uint32_t SymbolTable::four(const uint8_t* bytes, int offset) {
  return (static_cast<uint32_t>(bytes[offset])<<24) |
         (static_cast<uint32_t>(bytes[offset+1])<<16) |
         (static_cast<uint32_t>(bytes[offset+2])<<8) |
         static_cast<uint32_t>(bytes[offset+3]);
}

std::string SymbolTable::name(const uint8_t* bytes, int offset, int length) {
  return std::string(reinterpret_cast<const char*>(bytes+offset), length);
}

SymbolTable SymbolTable::parse_st_format(const std::vector<uint8_t>& bytes) {
  SymbolTable st;
  int index = 0, length = static_cast<int>(bytes.size());

  while (index < length) {
    if (bytes[index] == 0)
      index += 2;
    else {
      int size = bytes[index+1] & 0xFF;
      uint32_t address = four(bytes.data(), index+2);
      std::string symbol = name(bytes.data(), index+20, size);
      st.add_symbol(symbol, address);
      index = index + 20 + size + (size%2);
    }
  }
  return st;
}

SymbolTable SymbolTable::parse_sym_format(const std::vector<uint8_t>& bytes) {
  SymbolTable st;
  int index = 0, length = static_cast<int>(bytes.size());

  while (index < length) {
    if (bytes[index] <= ' ')
      index++;
    else {
      int start = index;
      while (index < length && bytes[index] > ' ' && bytes[index] < 127)
        index++;
      if (bytes[index] != ' ')
        throw std::runtime_error("SYM file format error");
      std::string symbol = name(bytes.data(), start, index-start);
      while (index < length && bytes[index] == ' ')
        index++;
      uint32_t address = 0;
      while (index < length) {
        uint8_t c = bytes[index];
        if (c >= '0' && c <= '9')
          address = address*16 + c - '0';
        else if (c >= 'a' && c <= 'f')
          address = address*16 + c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
          address = address*16 + c - 'A' + 10;
        else
          break;
        index++;
      }
      if (bytes[index] >= ' ')
        throw std::runtime_error("SYM file format error");
      st.add_symbol(symbol, address);
    }
  }
  return st;
}

void SymbolTable::add_symbol(const std::string& sym, uint32_t address) {
  name_to_address[sym] = address;
  auto it = address_to_name.find(address);
  if (it == address_to_name.end())
    address_to_name[address] = sym;
  else
    it->second = it->second + " / " + sym;
}

std::string SymbolTable::name_for_address(uint32_t address) const {
  auto it = address_to_name.find(address);
  if (it != address_to_name.end())
    return it->second;
  return "";
}

uint32_t SymbolTable::address_for_name(const std::string& sym) const {
  auto it = name_to_address.find(sym);
  if (it != name_to_address.end())
    return it->second;
  return 0xFFFFFFFF;
}

// Largest address <= given address (Java iterates all keys; we use sorted map)
uint32_t SymbolTable::first_address(uint32_t address) const {
  auto it = address_to_name.upper_bound(address);
  if (it == address_to_name.begin())
    return 0xFFFFFFFF;
  --it;
  return it->first;
}

// Smallest address > given address
uint32_t SymbolTable::last_address(uint32_t address) const {
  auto it = address_to_name.upper_bound(address);
  if (it == address_to_name.end())
    return 0xFFFFFFFF;
  return it->first;
}

} // namespace debug
