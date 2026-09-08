#include "FSConsole.hpp"
#include <cstdio>
#include <stdexcept>




namespace os {
int32_t FSConsole::read(std::vector<uint8_t>& bytes) {
  size_t count = fread(bytes.data(), 1, bytes.size(), stdin);
  if(count == 0 && ferror(stdin))
    throw std::runtime_error("FSConsole read() failed");
  return static_cast<int32_t>(count);
}

void FSConsole::write(const std::vector<uint8_t>& bytes) {
  size_t count = fwrite(bytes.data(), 1, bytes.size(), stdout);
  if(count != bytes.size())
    throw std::runtime_error("FSConsole write() failed");
  fflush(stdout);
}

} // namespace os
