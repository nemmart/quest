#pragma once
#include "FSPagedIO.hpp"
#include <cstdint>
#include <string>
#include <vector>


namespace hw { class PageSet; }

namespace os {
using namespace hw;

class FSFile;

class FSPagedFile : public FSPagedIO {
public:
  FSFile* file;

  explicit FSPagedFile(FSFile* file) : file(file) {}

  std::vector<uint8_t> load_page(int32_t page_number) override;
  std::vector<std::vector<uint8_t>> all_pages();
  PageSet* page_set();
  int32_t page_count();
  void close();
  std::string get_path();
};

} // namespace os
