// src/hw/AddressBook.hpp — M4a address book and area translation
// (docs/M4aDesign.md §3–§5, docs/Project12/REPORT.md).
//
// The BOOK (process-wide, loaded once from QUEST_ADDRESS_BOOK) says which
// game routines run their WSAVS frame in a fixed memory AREA at
// 0x74000000+ instead of on the MV/8000 stack. The live-record
// table and every translation live in hw/Mapper (docs/Mapper.md is the
// design of record for the mapping layer; Project 14).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace hw {

class Memory;

struct BookEntry {
  uint32_t    entry_pc;      // the WSAVS/WSAVR pc
  std::string name;
  uint32_t    alloc_base;    // 16-word aligned
  uint32_t    wfp_base;      // = alloc_base + 2*max_argc + 10 (R-C ruling, P14:
                             // args at alloc_base; ac3|carry wide AT [wfp, wfp+2))
  int32_t     max_argc;
  int32_t     frame_wides;   // WSAVS operand (wides)
  std::string variant;       // WSAVS | WSAVR
  uint32_t    size_words;    // 2*max_argc + 12 + 2*frame_wides, rounded to 16; bases are spaced size+16 apart (gap)
  bool        live;          // dynamic re-entrancy tripwire (routines are not re-entrant)
};

class AddressBook {
public:
  static constexpr uint32_t BASE = 0x74000000u;
  static AddressBook* instance;             // nullptr = no book (stock behavior everywhere)

  static bool load_from_env();              // QUEST_ADDRESS_BOOK=<file>; false on parse error
  static bool active() { return instance != nullptr; }

  std::vector<BookEntry> entries;           // sorted by alloc_base
  std::unordered_map<uint32_t, BookEntry*> by_pc;
  uint32_t total_words = 0;
  uint32_t total_pages = 0;

  BookEntry* lookup_pc(uint32_t pc);
  bool in_range(uint32_t v) const { return v >= BASE && v < BASE + total_words; }
  BookEntry* by_area_address(uint32_t v);   // nullptr if not in any entry's block

  // ---- M4b (Project 16): the caller-side map ----
  // QUEST_PUSH_MAP=<file>; ONE map pc → area word address covering every
  // caller-side instruction of a converted site (ruling: prescribed
  // mechanism). Push pcs (XPEF/LPEF) map to the callee's arg slots — the
  // push STORES there instead of pushing. The LCALL pc maps to the
  // callee's marker slot (wfp_base − 10) — the call writes the marker
  // there, STILL pushes it to the stack (call-marker ruling), and sets
  // the per-machine written-not-pushed flag. Callee-entry pcs stay in
  // the book (the WSAVS path). The data is process-wide like the book;
  // EFFECT is clone-only because every query goes through the Mapper's
  // book-gated accessor (Mapper.md §3: gate = configuration).
  std::unordered_map<uint32_t, uint32_t> caller_map;   // caller pc → area word addr
  static bool load_push_map_from_env();     // call after load_from_env; requires a book

  // Clone launch: map exactly total_pages pages at BASE, RW, no exec.
  void map_pages(Memory& memory);
};

} // namespace hw
