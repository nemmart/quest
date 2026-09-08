#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>


namespace os {
class OSError : public std::runtime_error {
public:
  static constexpr int32_t SUCCESS = 0;

  // FS errors
  static constexpr int32_t FS_INVALID_PATH = 0x1001;
  static constexpr int32_t FS_NOT_INITIALIZED = 0x1002;
  static constexpr int32_t FS_PARENT_DIRECTORY_DOES_NOT_EXIST = 0x1003;
  static constexpr int32_t FS_ALREADY_EXISTS = 0x1004;
  static constexpr int32_t FS_DOES_NOT_EXIST = 0x1005;
  static constexpr int32_t FS_DIRECTORY_IS_NOT_EMPTY = 0x1006;
  static constexpr int32_t FS_PROTECTED_DIRECTORY = 0x1007;
  static constexpr int32_t FS_ERROR_READING_DATA = 0x1008;
  static constexpr int32_t FS_ERROR_WRITING_DATA = 0x1009;
  static constexpr int32_t FS_FILE_NOT_FOUND = 0x100A;
  static constexpr int32_t FS_WRONG_FILE_TYPE = 0x100B;
  static constexpr int32_t FS_PAGING_NOT_ALLOWED_ON_FILE = 0x100C;
  static constexpr int32_t FS_STREAMING_NOT_ALLOWED_ON_FILE = 0x100D;
  static constexpr int32_t FS_SET_POSITION_NOT_ALLOWED_ON_FILE = 0x100E;
  static constexpr int32_t FS_INVALID_PAGE_NUMBER = 0x100F;
  static constexpr int32_t FS_INVALID_OPEN_ON_DIRECTORY = 0x1010;

  // OS errors
  static constexpr int32_t OS_NOT_IMPLEMENTED = 0x2000;
  static constexpr int32_t OS_UNHANDLED_SYSTEM_CALL = 0x2001;
  static constexpr int32_t OS_INVALID_CALL_ARGUMENT = 0x2002;
  static constexpr int32_t OS_MAXIMUM_NUMBER_OF_FILES_OPEN = 0x2003;
  static constexpr int32_t OS_INVALID_CHANNEL_NUMBER = 0x2004;
  static constexpr int32_t OS_SERVICE_ALREADY_EXISTS = 0x2005;
  static constexpr int32_t OS_SERVICE_DOES_NOT_EXIST = 0x2006;

  int32_t error_code;

  explicit OSError(int32_t error);
  int32_t error() const { return error_code; }

  static std::string message_for_error(int32_t error);

private:
  static const std::unordered_map<int32_t, std::string>& error_map();
};

} // namespace os
