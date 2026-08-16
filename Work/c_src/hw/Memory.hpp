#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cstdio>
#include "PageSet.hpp"
#include "Segment.hpp"
#include "Permissions.hpp"


namespace hw {
class Memory : public PageSet {
public:
  std::vector<Segment*> segments;
  uint8_t permissions[4096*512];

  // Diagnostic: track accesses to reserved player record regions
  uint32_t sd_ptr_cache = 0;
  std::string process_name;
  void check_reserved_access(uint32_t address, bool is_write);

  Memory();
  ~Memory() override = default;

  void map_page(Page* page, uint32_t page_number, uint32_t permission);
  void unmap_page(uint32_t page_number) override;

  // M4a §1 census ride-along (docs/Project12): every page ever mapped in
  // ANY Memory, union across processes, plus a `pagemap` trace line per
  // map/unmap. dump_ever_mapped() writes the ranges at shutdown.
  static uint8_t ever_mapped[4096*512];
  static void dump_ever_mapped(FILE* out);
  Page* find_page(uint32_t page_number) override;

  uint32_t read_instruction_word(uint32_t address);
  uint32_t read_instruction_wide(uint32_t address);

  uint32_t read_byte(uint32_t address) override;
  uint32_t read_word(uint32_t address) override;
  uint32_t read_wide(uint32_t address) override;
  uint64_t read_quad(uint32_t address);
  void write_byte(uint32_t address, uint32_t value) override;
  void write_word(uint32_t address, uint32_t value) override;
  void write_wide(uint32_t address, uint32_t value) override;
  void write_quad(uint32_t address, uint64_t value);
};

} // namespace hw
