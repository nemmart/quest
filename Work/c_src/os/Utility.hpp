#pragma once
#include <cstdint>
#include <vector>
#include <string>


namespace os {
class Utility {
public:
  static std::vector<uint8_t> read_file(const std::string& file_name);
  static void write_file(const std::string& file_name, const std::vector<uint8_t>& content);
};

} // namespace os
