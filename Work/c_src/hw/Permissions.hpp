#pragma once
#include <cstdint>


namespace hw {
struct Permissions {
  static constexpr uint32_t PERMISSION_MAPPED = 8;
  static constexpr uint32_t PERMISSION_READ = 4;
  static constexpr uint32_t PERMISSION_WRITE = 2;
  static constexpr uint32_t PERMISSION_EXECUTE = 1;
  static constexpr uint32_t PERMISSIONS_READ_EXECUTE = 5;
  static constexpr uint32_t PERMISSIONS_READ_WRITE = 6;
  static constexpr uint32_t PERMISSIONS_READ_WRITE_EXECUTE = 7;
};

} // namespace hw
