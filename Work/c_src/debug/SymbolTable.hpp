#pragma once
#include <cstdint>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>


namespace debug {
class SymbolTable {
public:
  std::unordered_map<std::string, uint32_t> name_to_address;
  std::map<uint32_t, std::string> address_to_name;

  SymbolTable() = default;

  void add_symbol(const std::string& name, uint32_t address);
  std::string name_for_address(uint32_t address) const;
  uint32_t address_for_name(const std::string& name) const;
  uint32_t first_address(uint32_t address) const;
  uint32_t last_address(uint32_t address) const;

  static uint32_t four(const uint8_t* bytes, int offset);
  static std::string name(const uint8_t* bytes, int offset, int length);
  static SymbolTable parse_st_format(const std::vector<uint8_t>& bytes);
  static SymbolTable parse_sym_format(const std::vector<uint8_t>& bytes);
};

} // namespace debug
