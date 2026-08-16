#pragma once
#include "FSStreamIO.hpp"
#include "FSObject.hpp"
#include <string>
#include <stdexcept>


namespace os {
class FSConsole : public FSStreamIO, public FSObject {
public:
  std::string path;

  FSConsole() {}

  // FSObject
  std::string get_path() override { return path; }
  void set_path(const std::string& p) override {
    if(!path.empty()) throw std::runtime_error("Attempt to change an FSObject path");
    path = p;
  }

  // FSStreamIO
  int32_t available() override { return 0; }
  int32_t read(std::vector<uint8_t>& bytes) override;
  void write(const std::vector<uint8_t>& bytes) override;
  void close() override {}
};

} // namespace os
