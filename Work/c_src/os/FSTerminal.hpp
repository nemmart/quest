#pragma once
#include "FSStreamIO.hpp"
#include "FSObject.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>


namespace os {
class FSTerminal : public FSStreamIO, public FSObject {
public:
  std::string path;
  int socket_fd;

  explicit FSTerminal(int socket_fd);
  ~FSTerminal();

  // FSObject
  std::string get_path() override { return path; }
  void set_path(const std::string& p) override {
    if(!path.empty()) throw std::runtime_error("Attempt to change an FSObject path");
    path = p;
  }

  // FSStreamIO
  int32_t available() override;
  int32_t read(std::vector<uint8_t>& bytes) override;
  void write(const std::vector<uint8_t>& bytes) override;
  void close() override {}

  // Line-mode read for terminal interaction
  int32_t read(std::vector<uint8_t>& bytes, bool line_mode);
};

} // namespace os
