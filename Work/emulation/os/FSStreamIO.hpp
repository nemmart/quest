#pragma once
#include <cstdint>
#include <vector>


namespace os {
class FSStreamIO {
public:
  virtual ~FSStreamIO() = default;

  virtual int32_t available() = 0;
  virtual int32_t read(std::vector<uint8_t>& bytes) = 0;
  virtual void write(const std::vector<uint8_t>& bytes) = 0;
  virtual void close() = 0;
};

} // namespace os
