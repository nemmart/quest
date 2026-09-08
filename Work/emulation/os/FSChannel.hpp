#pragma once
#include <cstdint>
#include <string>
#include <vector>


namespace hw { class PageSet; }

namespace os {
using namespace hw;

class FSFile;
class FSStreamIO;
class FSPagedFile;
class FSStreamedFile;
class FSTerminal;

class FSChannel {
public:
  static constexpr int32_t PAGED_IO = 0x00;
  static constexpr int32_t STREAM_IO = 0x01;
  static constexpr int32_t READ_PERMISSION = 0x10;
  static constexpr int32_t WRITE_PERMISSION = 0x20;
  static constexpr int32_t READ_WRITE_PERMISSION = 0x30;

  int32_t mode;

  // Exactly one of these is non-null
  FSPagedFile* paged_file;
  FSStreamedFile* streamed_file;
  FSStreamIO* stream_io;  // for raw stream (console/terminal) opens

  FSChannel(int32_t mode, FSPagedFile* pf);
  FSChannel(int32_t mode, FSStreamedFile* sf);
  FSChannel(int32_t mode, FSStreamIO* sio);
  ~FSChannel();

  static FSChannel* open_for_paged_io(const std::string& fs_path, bool read_only);
  static FSChannel* open_for_streamed_io(const std::string& fs_path, bool read_only);
  static FSChannel* open_for_streamed_io(FSStreamIO* stream, int32_t permission);

  bool paged_mode() { return (mode & 0x01) == PAGED_IO; }
  bool stream_mode() { return (mode & 0x01) == STREAM_IO; }
  bool read_only() { return (mode & WRITE_PERMISSION) == 0; }

  void close();
  std::vector<uint8_t> read_page(int32_t page_number);
  std::vector<std::vector<uint8_t>> read_all_pages();
  int32_t page_count();
  PageSet* page_set();

  void set_position(int32_t offset);
  int32_t read(std::vector<uint8_t>& bytes, bool line_mode);
  void write(const std::vector<uint8_t>& bytes);
  FSFile* get_file();
};

} // namespace os
