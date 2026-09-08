// hw/strings/Arena.hpp — the P33-B arena layout (quest.arena): one twin per
// WMSP claim, t@<block>.<k>, at a fixed word address in the otherwise
// unused emulated segment [0x75000000, 0x75800000), with a capacity.
//
// Three consumers: the IR loader (a `t@<block>.<k>` atom resolves to the
// twin's word address; `claim` carries its capacity), the Mapper (one
// arena row per twin, keyed (block, claim); P33-A binds it at the claim's
// WMSP hook), and the clone process (the pages are mapped RW at launch —
// the address-book precedent). QUEST_ARENA=<file>; required whenever
// QUEST_STRINGS_CHECK=1, and by any ir 6 file that names a twin.
#pragma once
#include "../Mapper.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace hw { class Memory; }

namespace hw {
namespace strings {

struct ArenaTemp {
  uint32_t id;          // 1-based (artifact order)
  uint32_t block;       // the claim-group block (identity of p@b)
  uint32_t claim;       // k, 1-based within the block
  uint32_t addr;        // word address of the twin (length word for a varying image)
  uint32_t capacity;    // bytes
  uint32_t wmsp_pc;     // the master's WMSP whose hook binds this row
  bool     bounded;     // bound=exact
  std::string routine, size_expr;
};

class Arena {
public:
  static bool loaded() { return !temps_.empty(); }
  static bool load_from_env();                                   // QUEST_ARENA; false = refuse to launch
  static bool load_file(const std::string& path, std::string* err);
  static const std::string& path() { return path_; }
  static const ArenaTemp* find(uint32_t block, uint32_t claim);
  static const ArenaTemp* by_wmsp(uint32_t pc);
  static const ArenaTemp* by_addr(uint32_t word);                // exact base only
  static const std::vector<ArenaTemp>& temps() { return temps_; }
  static std::vector<Mapper::ArenaLayout> layout();
  static void map_pages(Memory& memory);                         // clone process: RW, no exec
  static void reset_for_tests();
private:
  static std::vector<ArenaTemp> temps_;
  static std::map<uint64_t, size_t> by_key_;                     // (block<<32|claim) -> index
  static std::map<uint32_t, size_t> by_wmsp_;
  static std::string path_;
};

} // namespace strings
} // namespace hw
