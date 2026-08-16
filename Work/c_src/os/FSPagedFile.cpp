#include "FSPagedFile.hpp"
#include "FSFile.hpp"
#include "OSError.hpp"
#include <cstdio>




namespace os {
std::vector<uint8_t> FSPagedFile::load_page(int32_t page_number) {
  return file->load_page(page_number);
}

std::vector<std::vector<uint8_t>> FSPagedFile::all_pages() {
  return file->all_pages();
}

PageSet* FSPagedFile::page_set() {
  return file->get_page_set();
}

int32_t FSPagedFile::page_count() {
  return file->get_page_count();
}

void FSPagedFile::close() {
  fprintf(stderr, "WRITING: %s\n", file->get_path().c_str());
  int32_t error = file->store_pages(nullptr);
  if(error != 0)
    throw OSError(error);
}

std::string FSPagedFile::get_path() {
  return file->get_path();
}

} // namespace os
