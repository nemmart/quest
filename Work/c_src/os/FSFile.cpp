#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "FSFile.hpp"
#include "ArrayPage.hpp"
#include "MappedPage.hpp"
#include "Utility.hpp"
#include "OSError.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <functional>

// Platform detection: use POSIX mmap on Linux, Cygwin, macOS.
// Only skip mmap on native Windows (MSVC / MinGW without Cygwin).
#if defined(_WIN32) && !defined(__CYGWIN__)
#define USE_MMAP 0
#else
#define USE_MMAP 1
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace os {

bool FSFile::COMMIT = true;

FSFile::FSFile()
  : length(0), page_count(0), modified(false),
    mapped_data(nullptr), mapped_length(0), mapped_fd(-1), memory_mapped(false) {}

FSFile::~FSFile() {
  close_mapped();
}

void FSFile::set_path(const std::string& p) {
  if(!path.empty()) throw std::runtime_error("Attempt to change an FSObject path");
  path = p;
}

int32_t FSFile::load_pages(const std::string& fp) {
  std::vector<uint8_t> content;
  try {
    file_path = fp;
    content = Utility::read_file(fp);
  }
  catch(...) {
    return OSError::FS_ERROR_READING_DATA;
  }
  memory_mapped = false;
  modified = false;
  length = static_cast<int32_t>(content.size());
  page_count = (length + 2047) / 2048;
  pages.resize(page_count);
  page_set = PageSet();

  for(int32_t index = 0; index < length; index += 2048) {
    std::vector<uint8_t> page(2048, 0);
    for(int32_t offset = 0; offset < 2048 && index + offset < length; offset++)
      page[offset] = content[index + offset];
    int32_t page_num = index >> 11;
    pages[page_num] = page;
    auto* ap = new ArrayPage(page, [this]{ modified_notification(); });
    array_pages.push_back(ap);
    page_set.map_page(ap, page_num);
  }
  return 0; // SUCCESS
}

void FSFile::enable_shared_trace(const std::string& name) {
  for(size_t p = 0; p < array_pages.size(); p++) {
    if(array_pages[p]->trace_label.empty())
      array_pages[p]->trace_label = name + ":" + std::to_string(p);
  }
}

void FSFile::enable_memory_mapping() {
#if !USE_MMAP
  return;
#else
  std::lock_guard<std::mutex> lock(mmap_mutex);
  if(memory_mapped) return;
  if(file_path.empty()) return;

  mapped_length = page_count * 2048;

  mapped_fd = ::open(file_path.c_str(), O_RDWR);
  if(mapped_fd < 0) {
    fprintf(stderr, "Memory mapping failed for %s: open failed\n", file_path.c_str());
    return;
  }

  // Pad to page boundary
  struct stat st;
  if(::fstat(mapped_fd, &st) != 0) {
    ::close(mapped_fd); mapped_fd = -1;
    fprintf(stderr, "Memory mapping failed for %s: fstat failed\n", file_path.c_str());
    return;
  }
  if(static_cast<int32_t>(st.st_size) < mapped_length) {
    if(::ftruncate(mapped_fd, mapped_length) != 0) {
      ::close(mapped_fd); mapped_fd = -1;
      fprintf(stderr, "Memory mapping failed for %s: ftruncate failed\n", file_path.c_str());
      return;
    }
  }

  mapped_data = static_cast<uint8_t*>(
    ::mmap(nullptr, mapped_length, PROT_READ | PROT_WRITE, MAP_SHARED, mapped_fd, 0));
  if(mapped_data == MAP_FAILED) {
    mapped_data = nullptr;
    ::close(mapped_fd); mapped_fd = -1;
    fprintf(stderr, "Memory mapping failed for %s: mmap failed\n", file_path.c_str());
    return;
  }

  // Copy current array contents into mapped buffer, then replace pages
  for(int32_t i = 0; i < page_count; i++) {
    int32_t base = i * 2048;
    if(i < static_cast<int32_t>(pages.size()) && !pages[i].empty())
      memcpy(mapped_data + base, pages[i].data(), 2048);
    page_set.map_page(new MappedPage(mapped_data, base), i);
  }

  pages.clear();
  memory_mapped = true;
  fprintf(stderr, "Memory-mapped: %s\n", file_path.c_str());
#endif
}

std::vector<uint8_t> FSFile::full_contents() {
  std::vector<uint8_t> bytes(length, 0);
  if(memory_mapped && mapped_data) {
    std::lock_guard<std::mutex> lock(mmap_mutex);
    memcpy(bytes.data(), mapped_data, length);
  } else {
    // Read from the PageSet's Page objects, which have the live data
    // (FSFile::pages is a stale copy from initial load)
    for(int32_t pg = 0; pg < page_count; pg++) {
      Page* page = page_set.find_page(pg);
      if(!page) continue;
      for(int32_t offset = 0; offset < 2048; offset++) {
        int32_t pos = pg * 2048 + offset;
        if(pos < length)
          bytes[pos] = static_cast<uint8_t>(page->read(offset));
      }
    }
  }
  return bytes;
}

std::vector<uint8_t> FSFile::load_page(int32_t page_number) {
  if(memory_mapped && mapped_data) {
    std::vector<uint8_t> page(2048, 0);
    int32_t base = page_number * 2048;
    std::lock_guard<std::mutex> lock(mmap_mutex);
    int32_t copy_len = (base + 2048 <= mapped_length) ? 2048 : (mapped_length - base);
    if(copy_len > 0)
      memcpy(page.data(), mapped_data + base, copy_len);
    return page;
  }
  // Read from PageSet (live data), not from stale pages array
  Page* pg = page_set.find_page(page_number);
  if(pg) {
    std::vector<uint8_t> result(2048, 0);
    for(int32_t i = 0; i < 2048; i++)
      result[i] = static_cast<uint8_t>(pg->read(i));
    return result;
  }
  return std::vector<uint8_t>(2048, 0);
}

std::vector<std::vector<uint8_t>> FSFile::all_pages() {
  std::vector<std::vector<uint8_t>> snapshot(page_count);
  for(int32_t p = 0; p < page_count; p++)
    snapshot[p] = load_page(p);
  return snapshot;
}

int32_t FSFile::store_pages(const std::string* fp) {
  if(memory_mapped) {
    if(modified && mapped_data) {
#if USE_MMAP
      ::msync(mapped_data, mapped_length, MS_SYNC);
#endif
      modified = false;
    }
    return 0;
  }
  // Array-based write path
  auto content = full_contents();
  const std::string& target = (fp && !fp->empty()) ? *fp : file_path;
  try {
    if(modified && COMMIT)
      Utility::write_file(target, content);
    modified = false;
  }
  catch(...) {
    return OSError::FS_ERROR_WRITING_DATA;
  }
  return 0;
}

void FSFile::force() {
#if USE_MMAP
  if(memory_mapped && mapped_data)
    ::msync(mapped_data, mapped_length, MS_SYNC);
#endif
}

void FSFile::close_mapped() {
#if USE_MMAP
  if(memory_mapped) {
    if(mapped_data && mapped_data != MAP_FAILED) {
      ::munmap(mapped_data, mapped_length);
      mapped_data = nullptr;
    }
    if(mapped_fd >= 0) {
      ::close(mapped_fd);
      mapped_fd = -1;
    }
  }
#endif
}

void FSFile::extend(int32_t new_page_count) {
  if(memory_mapped)
    throw std::runtime_error("Cannot extend a memory-mapped file at runtime: " + file_path);
  if(new_page_count < static_cast<int32_t>(pages.size()) + 16)
    new_page_count = static_cast<int32_t>(pages.size()) + 16;
  pages.resize(new_page_count, std::vector<uint8_t>(2048, 0));
}

void FSFile::set_length(int32_t new_length) {
  if(memory_mapped)
    throw std::runtime_error("Cannot resize a memory-mapped file at runtime: " + file_path);
  int32_t new_page_count = (new_length + 2047) / 2048;
  if(new_page_count > static_cast<int32_t>(pages.size()))
    extend(new_page_count);
  page_count = new_page_count;
  length = new_length;
}

} // namespace os
