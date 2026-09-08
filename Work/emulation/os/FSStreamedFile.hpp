#pragma once
#include "FSStreamIO.hpp"
#include <cstdint>
#include <vector>


namespace os {
class FSFile;

class FSStreamedFile : public FSStreamIO {
public:
  FSFile* file;
  int32_t position;
  bool updated;

  explicit FSStreamedFile(FSFile* file);

  int32_t available() override;
  int32_t read(std::vector<uint8_t>& bytes) override;
  void write(const std::vector<uint8_t>& bytes) override;
  void close() override;
  void set_position(int32_t position);
};

} // namespace os
