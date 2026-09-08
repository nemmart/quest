#include "OSError.hpp"
#include <cstdio>




namespace os {
const std::unordered_map<int32_t, std::string>& OSError::error_map() {
  static const std::unordered_map<int32_t, std::string> map = {
    {FS_INVALID_PATH,                    "FS - invalid path"},
    {FS_NOT_INITIALIZED,                 "FS - not initialized"},
    {FS_PARENT_DIRECTORY_DOES_NOT_EXIST, "FS - parent directory does not exist"},
    {FS_ALREADY_EXISTS,                  "FS - file/directory already exists"},
    {FS_DOES_NOT_EXIST,                  "FS - does not exist"},
    {FS_DIRECTORY_IS_NOT_EMPTY,          "FS - directory is not empty"},
    {FS_PROTECTED_DIRECTORY,             "FS - protected directory"},
    {FS_ERROR_READING_DATA,              "FS - error reading data"},
    {FS_ERROR_WRITING_DATA,              "FS - error writing data"},
    {FS_FILE_NOT_FOUND,                  "FS - file not found"},
    {FS_WRONG_FILE_TYPE,                 "FS - wrong file type"},
    {FS_PAGING_NOT_ALLOWED_ON_FILE,      "FS - paging is not allowed on this file"},
    {FS_STREAMING_NOT_ALLOWED_ON_FILE,   "FS - streaming is not allowed on this file"},
    {FS_SET_POSITION_NOT_ALLOWED_ON_FILE,"FS - set position is not allowed on this file"},
    {FS_INVALID_PAGE_NUMBER,             "FS - invalid page number"},
    {FS_INVALID_OPEN_ON_DIRECTORY,       "FS - invalid open on a directory"},
    {OS_NOT_IMPLEMENTED,                 "OS - not implemented yet"},
    {OS_UNHANDLED_SYSTEM_CALL,           "OS - unhandled system call"},
    {OS_INVALID_CALL_ARGUMENT,           "OS - invalid call argument"},
    {OS_MAXIMUM_NUMBER_OF_FILES_OPEN,    "OS - maximum number of files open"},
    {OS_INVALID_CHANNEL_NUMBER,          "OS - invalid channel number"},
    {OS_SERVICE_ALREADY_EXISTS,          "OS - service already exists"},
    {OS_SERVICE_DOES_NOT_EXIST,          "OS - service does not exist"},
  };
  return map;
}

std::string OSError::message_for_error(int32_t error) {
  auto& map = error_map();
  auto it = map.find(error);
  if(it == map.end()) {
    char buf[64];
    snprintf(buf, sizeof(buf), "Message not found for error number 0x%X", error);
    throw std::runtime_error(buf);
  }
  return it->second;
}

OSError::OSError(int32_t error)
  : std::runtime_error(message_for_error(error)), error_code(error) {}

} // namespace os
