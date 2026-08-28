#include "BlockSync.hpp"
#include "RTStubs.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <set>

namespace hw {

bool BlockSync::active = false;
uint32_t BlockSync::game_start = 0;
uint32_t BlockSync::game_stop = 0;
uint32_t BlockSync::sync_k = 50;
uint8_t* BlockSync::listed_bits = nullptr;

static std::vector<uint8_t> listed_store;

// Parse one "XXXXXXXX:" block-start line; returns address or 0.
static uint32_t start_line(const std::string& line) {
  if(line.size() != 9 || line[8] != ':')
    return 0;
  for(int i = 0; i < 8; i++)
    if(!isxdigit(static_cast<unsigned char>(line[i])))
      return 0;
  return static_cast<uint32_t>(strtoul(line.substr(0, 8).c_str(), nullptr, 16));
}

// A terminator line: lowercase tag, then successor addresses and further
// tag/address alternations ("n A B", "c T n R", "j T n", "n", ...). The
// GATE successors are the addresses following an 'n' on a 'c'/'j'
// terminator (post-call resumption points) — plus, for SYSCALL-terminated
// blocks, every successor (the caller decides via last_was_syscall).
static bool terminator_line(const std::string& line) {
  return !line.empty() && line[0] >= 'a' && line[0] <= 'z' &&
         (line.size() == 1 || line[1] == ' ');
}

bool BlockSync::load_from_env() {
  if(active)
    return true;
  const char* blocks_path = getenv("QUEST_BLOCKS");
  const char* list_path = getenv("QUEST_SYNC_LIST");
  if(!blocks_path || !*blocks_path || !list_path || !*list_path) {
    fprintf(stderr, "BlockSync: -lockstep requires QUEST_BLOCKS and "
                    "QUEST_SYNC_LIST (docs/Project22/BlockSyncDesign.md)\n");
    return false;
  }

  // ---- Pass 1: quest.blocks — block starts and the gate set ----
  std::ifstream bf(blocks_path);
  if(!bf) {
    fprintf(stderr, "BlockSync: cannot open QUEST_BLOCKS=%s\n", blocks_path);
    return false;
  }
  std::set<uint32_t> starts;
  std::set<uint32_t> gates;
  std::string line, last_insn;
  while(std::getline(bf, line)) {
    uint32_t s = start_line(line);
    if(s) {
      starts.insert(s);
      last_insn.clear();
      continue;
    }
    if(terminator_line(line)) {
      // Tokenize: tags are single lowercase letters, successors are hex.
      bool call_block = line[0] == 'c' || line[0] == 'j';
      bool syscall_block = last_insn.compare(0, 7, "SYSCALL") == 0;
      if(call_block || syscall_block) {
        std::string tok;
        std::ifstream dummy;   // (silence unused warnings on some g++)
        (void)dummy;
        size_t pos = 0;
        bool after_n = line[0] == 'n';
        while(pos < line.size()) {
          size_t sp = line.find(' ', pos);
          tok = line.substr(pos, sp == std::string::npos ? std::string::npos : sp - pos);
          pos = sp == std::string::npos ? line.size() : sp + 1;
          if(tok.size() == 1 && tok[0] >= 'a' && tok[0] <= 'z') {
            after_n = tok[0] == 'n';
            continue;
          }
          if(tok.size() == 8) {
            uint32_t a = static_cast<uint32_t>(strtoul(tok.c_str(), nullptr, 16));
            // c/j blocks: gate only the 'n' (resumption) successors — the
            // call target itself is RT- or game-entry machinery, not a
            // resumption point. SYSCALL blocks: every successor (both
            // skip arms resume from the trap).
            if(a && (syscall_block || (call_block && after_n)))
              gates.insert(a);
          }
        }
      }
      last_insn.clear();
      continue;
    }
    if(!line.empty() && line[0] != '#')
      last_insn = line;
  }
  if(starts.empty()) {
    fprintf(stderr, "BlockSync: no block starts parsed from %s\n", blocks_path);
    return false;
  }
  // Game-range terminal sites that are block starts are gates too.
  for(uint32_t s : starts)
    if(RTStubs::is_terminal_pc(s))
      gates.insert(s);

  // ---- Pass 2: the sync list ----
  std::ifstream lf(list_path);
  if(!lf) {
    fprintf(stderr, "BlockSync: cannot open QUEST_SYNC_LIST=%s\n", list_path);
    return false;
  }
  std::set<uint32_t> list;
  int lineno = 0;
  while(std::getline(lf, line)) {
    lineno++;
    size_t h = line.find('#');
    if(h != std::string::npos)
      line = line.substr(0, h);
    while(!line.empty() && isspace(static_cast<unsigned char>(line.back())))
      line.pop_back();
    size_t b = 0;
    while(b < line.size() && isspace(static_cast<unsigned char>(line[b])))
      b++;
    line = line.substr(b);
    if(line.empty())
      continue;
    char* end = nullptr;
    uint32_t a = static_cast<uint32_t>(strtoul(line.c_str(), &end, 16));
    if(!a || (end && *end)) {
      fprintf(stderr, "BlockSync: %s:%d: bad sync-list line '%s'\n",
              list_path, lineno, line.c_str());
      return false;
    }
    // Validation 1: refuse novelty.
    if(!starts.count(a)) {
      fprintf(stderr, "BlockSync: %s:%d: %08X is not a quest.blocks start "
                      "— the loader refuses novelty\n", list_path, lineno, a);
      return false;
    }
    list.insert(a);
  }
  if(list.empty()) {
    fprintf(stderr, "BlockSync: empty sync list %s\n", list_path);
    return false;
  }
  // Validation 2: gate addresses are PERMANENTLY listed.
  for(uint32_t g : gates)
    if(starts.count(g) && !list.count(g)) {
      fprintf(stderr, "BlockSync: gate address %08X (call/syscall resumption "
                      "or terminal) is delisted — refused; gates are "
                      "permanently listed\n", g);
      return false;
    }

  // ---- K ----
  const char* k = getenv("QUEST_SYNC_K");
  if(k && *k) {
    long v = strtol(k, nullptr, 10);
    if(v < 1 || v > 1000000) {
      fprintf(stderr, "BlockSync: QUEST_SYNC_K=%s out of range [1,1000000]\n", k);
      return false;
    }
    sync_k = static_cast<uint32_t>(v);
  }

  // ---- Bitmap ----
  game_start = *starts.begin();
  game_stop = *starts.rbegin() + 1;
  listed_store.assign(game_stop - game_start, 0);
  for(uint32_t a : list)
    listed_store[a - game_start] = 1;
  listed_bits = listed_store.data();
  active = true;
  fprintf(stderr, "BlockSync: %zu/%zu block entries listed, %zu gates, "
                  "range [%08X,%08X), K=%u\n",
          list.size(), starts.size(), gates.size(), game_start, game_stop, sync_k);
  return true;
}

} // namespace hw
