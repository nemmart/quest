#pragma once
#include "FSObject.hpp"
#include "../hw/PageSet.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>


namespace os {
using namespace hw;

class FSFile : public FSObject {
public:
  static bool COMMIT;

  std::string path;
  std::string file_path;
  int32_t length;
  int32_t page_count;
  bool modified;
  std::vector<std::vector<uint8_t>> pages;
  PageSet page_set;
  std::vector<class ArrayPage*> array_pages;

  // Memory-mapped fields
  uint8_t* mapped_data;
  int32_t mapped_length;
  int mapped_fd;
  bool memory_mapped;
  std::mutex mmap_mutex;

  FSFile();
  ~FSFile();

  std::string get_path() override { return path; }
  void set_path(const std::string& p) override;

  void modified_notification() { modified = true; }

  int32_t load_pages(const std::string& file_path);
  void enable_shared_trace(const std::string& name);
  void enable_memory_mapping();
  bool is_memory_mapped() { return memory_mapped; }

  int32_t get_length() { return length; }
  std::vector<uint8_t> full_contents();
  std::vector<uint8_t> load_page(int32_t page_number);
  std::vector<std::vector<uint8_t>> all_pages();
  int32_t get_page_count() { return page_count; }
  PageSet* get_page_set() { return &page_set; }

  int32_t store_pages(const std::string* file_path);
  void force();
  void close_mapped();
  void extend(int32_t new_page_count);
  void set_length(int32_t new_length);
};

} // namespace os
