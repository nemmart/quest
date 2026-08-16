#include "Utility.hpp"
#include <fstream>
#include <stdexcept>




namespace os {
std::vector<uint8_t> Utility::read_file(const std::string& file_name) {
  std::ifstream file(file_name, std::ios::binary | std::ios::ate);
  if(!file)
    throw std::runtime_error("Unable to read file: " + file_name);
  auto length = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> bytes(length);
  file.read(reinterpret_cast<char*>(bytes.data()), length);
  if(!file)
    throw std::runtime_error("Error reading file: " + file_name);
  return bytes;
}

void Utility::write_file(const std::string& file_name, const std::vector<uint8_t>& content) {
  std::ofstream file(file_name, std::ios::binary | std::ios::trunc);
  if(!file)
    throw std::runtime_error("Unable to write file: " + file_name);
  file.write(reinterpret_cast<const char*>(content.data()), content.size());
  if(!file)
    throw std::runtime_error("Error writing file: " + file_name);
}

} // namespace os
