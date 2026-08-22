#include "AddressBook.hpp"
#include "Memory.hpp"
#include "Permissions.hpp"
#include "../os/ArrayPage.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace hw {

AddressBook* AddressBook::instance = nullptr;

// Book line: "entry name alloc_base wfp_base argc frame variant flags"
// (hex fields without 0x except frame, e.g. "701766ED READ_IN 78006C50
// 78006C5C 0 0x06 WSAVS -"). Lines starting with # are ignored: a
// commented routine stays stacked (docs/M4aDesign.md §3).
bool AddressBook::load_from_env() {
  const char* path = getenv("QUEST_ADDRESS_BOOK");
  if(!path || !*path)
    return true;
  std::ifstream in(path);
  if(!in) {
    fprintf(stderr, "AddressBook: cannot open %s\n", path);
    return false;
  }
  AddressBook* book = new AddressBook();
  std::string line;
  uint32_t top = BASE;
  bool have_prev = false;
  int lineno = 0;
  while(std::getline(in, line)) {
    lineno++;
    if(line.empty() || line[0] == '#')
      continue;
    std::istringstream ss(line);
    std::string entry, name, alloc, wfp, argc, frame, variant, flags;
    if(!(ss >> entry >> name >> alloc >> wfp >> argc >> frame >> variant)) {
      fprintf(stderr, "AddressBook: %s:%d: bad line\n", path, lineno);
      delete book;
      return false;
    }
    ss >> flags;
    BookEntry e;
    e.entry_pc = static_cast<uint32_t>(strtoul(entry.c_str(), nullptr, 16));
    e.name = name;
    e.alloc_base = static_cast<uint32_t>(strtoul(alloc.c_str(), nullptr, 16));
    e.wfp_base = static_cast<uint32_t>(strtoul(wfp.c_str(), nullptr, 16));
    e.max_argc = static_cast<int32_t>(strtol(argc.c_str(), nullptr, 10));
    e.frame_wides = static_cast<int32_t>(strtol(frame.c_str(), nullptr, 0));
    e.variant = variant;
    e.live = false;
    uint32_t words = 2 * e.max_argc + 12 + 2 * e.frame_wides;
    e.size_words = (words + 15) & ~15u;
    // Consistency checks against the layout ruling: wfp_base = alloc_base
    // + 2*max_argc + 10 (R-C); blocks STRICTLY disjoint including the
    // closed right ends (Mapper.md I1: alloc_end(k) < alloc_base(k+1),
    // honored by the stride size+16); base ring 7.
    if(e.wfp_base != e.alloc_base + 2 * e.max_argc + 10 ||
       (have_prev ? e.alloc_base <= top : e.alloc_base < top) || (e.alloc_base & 15) != 0 ||
       (e.alloc_base >> 28) != (BASE >> 28) ||
       (variant != "WSAVS" && variant != "WSAVR")) {
      fprintf(stderr, "AddressBook: %s:%d: layout inconsistency for %s\n", path, lineno, name.c_str());
      delete book;
      return false;
    }
    top = e.alloc_base + e.size_words;
    have_prev = true;
    book->entries.push_back(e);
  }
  book->total_words = top - BASE;
  book->total_pages = (book->total_words + 1023) / 1024;
  for(BookEntry& e : book->entries)
    book->by_pc[e.entry_pc] = &e;
  instance = book;
  fprintf(stderr, "AddressBook: %s — %zu live routine(s), %u words, %u page(s) at %08X\n",
          path, book->entries.size(), book->total_words, book->total_pages, BASE);
  for(const BookEntry& e : book->entries)
    fprintf(stderr, "  %08X %-24s area %08X wfp %08X argc %d frame %d %s\n",
            e.entry_pc, e.name.c_str(), e.alloc_base, e.wfp_base,
            e.max_argc, e.frame_wides, e.variant.c_str());
  return true;
}

// Push-map line grammar (docs/Project16): "push <pc> <area_word_addr>"
// or "call <pc> <area_word_addr>", hex without 0x; # comments. Both go
// into ONE caller map (pc → area address); the keywords exist so the
// loader can validate each pc's role: a push slot must lie inside a book
// entry's arg region [alloc_base, alloc_base + 2*max_argc); a call slot
// must BE a book entry's marker slot (wfp_base − 10).
bool AddressBook::load_push_map_from_env() {
  const char* path = getenv("QUEST_PUSH_MAP");
  if(!path || !*path)
    return true;
  if(!instance) {
    fprintf(stderr, "AddressBook: QUEST_PUSH_MAP set without QUEST_ADDRESS_BOOK\n");
    return false;
  }
  std::ifstream in(path);
  if(!in) {
    fprintf(stderr, "AddressBook: cannot open %s\n", path);
    return false;
  }
  std::string line;
  int lineno = 0;
  size_t pushes = 0, calls = 0;
  while(std::getline(in, line)) {
    lineno++;
    size_t hash = line.find('#');
    if(hash != std::string::npos)
      line = line.substr(0, hash);
    std::istringstream ss(line);
    std::string kind, a, b;
    if(!(ss >> kind))
      continue;
    if(!(ss >> a >> b)) {
      fprintf(stderr, "AddressBook: %s:%d: bad push-map line\n", path, lineno);
      return false;
    }
    uint32_t pc = static_cast<uint32_t>(strtoul(a.c_str(), nullptr, 16));
    uint32_t slot = static_cast<uint32_t>(strtoul(b.c_str(), nullptr, 16));
    BookEntry* e = instance->by_area_address(slot);
    if(kind == "push") {
      if(!e || slot < e->alloc_base || slot >= e->alloc_base + 2 * static_cast<uint32_t>(e->max_argc)) {
        fprintf(stderr, "AddressBook: %s:%d: push slot %08X is not inside a book entry's arg region\n",
                path, lineno, slot);
        return false;
      }
      pushes++;
    }
    else if(kind == "call") {
      if(!e || slot != e->wfp_base - 10) {
        fprintf(stderr, "AddressBook: %s:%d: call slot %08X is not a book entry's marker slot (wfp-10)\n",
                path, lineno, slot);
        return false;
      }
      calls++;
      fprintf(stderr, "AddressBook: decorated call %08X → %s marker %08X\n",
              pc, e->name.c_str(), slot);
    }
    else {
      fprintf(stderr, "AddressBook: %s:%d: unknown push-map kind '%s'\n", path, lineno, kind.c_str());
      return false;
    }
    if(instance->caller_map.count(pc)) {
      fprintf(stderr, "AddressBook: %s:%d: duplicate caller pc %08X\n", path, lineno, pc);
      return false;
    }
    instance->caller_map[pc] = slot;
  }
  fprintf(stderr, "AddressBook: %s — caller map: %zu push redirect(s), %zu decorated call(s)\n",
          path, pushes, calls);
  return true;
}

BookEntry* AddressBook::lookup_pc(uint32_t pc) {
  auto it = by_pc.find(pc);
  return it == by_pc.end() ? nullptr : it->second;
}

BookEntry* AddressBook::by_area_address(uint32_t v) {
  if(!in_range(v))
    return nullptr;
  for(BookEntry& e : entries)
    if(v >= e.alloc_base && v < e.alloc_base + e.size_words)
      return &e;
  return nullptr;
}

void AddressBook::map_pages(Memory& memory) {
  uint32_t first = BASE >> 10;
  for(uint32_t p = 0; p < total_pages; p++)
    memory.map_page(new os::ArrayPage(), first + p,
                    Permissions::PERMISSION_READ | Permissions::PERMISSION_WRITE);
  fprintf(stderr, "AddressBook: mapped %u page(s) at %08X (RW, no exec) for %s\n",
          total_pages, BASE, memory.process_name.c_str());
}

} // namespace hw
