// hw/strings/Arena.cpp — see Arena.hpp.
#include "Arena.hpp"
#include "../Memory.hpp"
#include "../../os/ArrayPage.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace hw {
namespace strings {

std::vector<ArenaTemp> Arena::temps_;
std::map<uint64_t, size_t> Arena::by_key_;
std::map<uint32_t, size_t> Arena::by_wmsp_;
std::string Arena::path_;

static bool kv(const std::string& tok, const char* key, std::string& val) {
  size_t k = strlen(key);
  if(tok.compare(0, k, key) != 0 || tok.size() <= k || tok[k] != '=') return false;
  val = tok.substr(k + 1);
  return true;
}

bool Arena::load_file(const std::string& path, std::string* err) {
  temps_.clear(); by_key_.clear(); by_wmsp_.clear();
  std::ifstream in(path);
  if(!in) { *err = "cannot open " + path; return false; }
  std::string line;
  int lineno = 0;
  auto fail = [&](const std::string& m) { *err = path + ":" + std::to_string(lineno) + ": " + m; return false; };
  while(std::getline(in, line)) {
    lineno++;
    if(line.empty() || line[0] == '#') continue;
    std::istringstream ss(line);
    std::string kind; ss >> kind;
    if(kind != "temp") return fail("unknown line kind " + kind);
    std::vector<std::string> t;
    for(std::string x; ss >> x;) t.push_back(x);
    if(t.size() < 7) return fail("temp: too few fields");
    ArenaTemp a;
    char* e = nullptr;
    a.id = static_cast<uint32_t>(strtoul(t[0].c_str(), &e, 10));
    if(*e || a.id != temps_.size() + 1) return fail("temp: ids must be 1..n in order");
    // t@<block>.<k>
    if(t[1].compare(0, 2, "t@") != 0) return fail("temp: name must be t@<block>.<k>");
    size_t dot = t[1].find('.');
    if(dot == std::string::npos) return fail("temp: name must be t@<block>.<k>");
    a.block = static_cast<uint32_t>(strtoul(t[1].substr(2, dot - 2).c_str(), &e, 16));
    if(*e) return fail("temp: bad block in name");
    a.claim = static_cast<uint32_t>(strtoul(t[1].substr(dot + 1).c_str(), &e, 10));
    if(*e || a.claim == 0) return fail("temp: bad claim ordinal in name");
    std::string v;
    if(!kv(t[2], "arena", v)) return fail("temp: expected arena=");
    a.addr = static_cast<uint32_t>(strtoul(v.c_str(), &e, 16)); if(*e) return fail("temp: bad arena=");
    if(!kv(t[3], "cap", v)) return fail("temp: expected cap=");
    a.capacity = static_cast<uint32_t>(strtoul(v.c_str(), &e, 10)); if(*e || a.capacity == 0) return fail("temp: bad cap=");
    if(!kv(t[4], "bound", v)) return fail("temp: expected bound=");
    if(v != "exact" && v != "unbounded") return fail("temp: bound must be exact|unbounded");
    a.bounded = (v == "exact");
    if(!kv(t[5], "wmsp", v)) return fail("temp: expected wmsp=");
    a.wmsp_pc = static_cast<uint32_t>(strtoul(v.c_str(), &e, 16)); if(*e) return fail("temp: bad wmsp=");
    if(!kv(t[6], "routine", v)) return fail("temp: expected routine=");
    a.routine = v;
    a.size_expr = t.size() > 7 && t[7].compare(0, 5, "size=") == 0 ? t[7].substr(5) : "";
    if(!Mapper::is_arena(a.addr)) return fail("temp: arena address outside the segment");
    uint32_t end = a.addr + 1 + (a.capacity + 1) / 2;
    if(!Mapper::is_arena(end)) return fail("temp: capacity runs past the segment");
    if(!temps_.empty()) {
      const ArenaTemp& p = temps_.back();
      uint32_t pend = p.addr + 1 + (p.capacity + 1) / 2;
      if(!(pend < a.addr)) return fail("temp: overlaps the previous twin (closed ends, Mapper I1)");
    }
    uint64_t key = (static_cast<uint64_t>(a.block) << 32) | a.claim;
    if(by_key_.count(key)) return fail("temp: duplicate " + t[1]);
    if(by_wmsp_.count(a.wmsp_pc)) return fail("temp: two twins for one wmsp pc");
    by_key_[key] = temps_.size();
    by_wmsp_[a.wmsp_pc] = temps_.size();
    temps_.push_back(a);
  }
  if(temps_.empty()) { *err = path + ": no temps"; return false; }
  // every block's claims 1..n present
  std::map<uint32_t, uint32_t> maxk;
  for(const ArenaTemp& a : temps_) if(a.claim > maxk[a.block]) maxk[a.block] = a.claim;
  for(auto& b : maxk)
    for(uint32_t k = 1; k <= b.second; k++)
      if(!find(b.first, k)) { char buf[64]; snprintf(buf, sizeof buf, "block %08X lacks claim %u", b.first, k); *err = path + ": " + buf; return false; }
  path_ = path;
  return true;
}

bool Arena::load_from_env() {
  const char* path = getenv("QUEST_ARENA");
  if(!path) return true;                     // absent: no twins (an ir 6 file naming one refuses at load)
  if(loaded() && path_ == path) return true; // idempotent: the IR loader (Launch) and RTStubs both ask
  std::string err;
  if(!load_file(path, &err)) {
    fprintf(stderr, "Arena: %s — refusing to launch\n", err.c_str());
    return false;
  }
  fprintf(stderr, "Arena: %s — %zu twins\n", path, temps_.size());
  return true;
}

const ArenaTemp* Arena::find(uint32_t block, uint32_t claim) {
  auto it = by_key_.find((static_cast<uint64_t>(block) << 32) | claim);
  return it == by_key_.end() ? nullptr : &temps_[it->second];
}

const ArenaTemp* Arena::by_wmsp(uint32_t pc) {
  auto it = by_wmsp_.find(pc);
  return it == by_wmsp_.end() ? nullptr : &temps_[it->second];
}

const ArenaTemp* Arena::by_addr(uint32_t word) {
  for(const ArenaTemp& a : temps_) if(a.addr == word) return &a;
  return nullptr;
}

std::vector<Mapper::ArenaLayout> Arena::layout() {
  std::vector<Mapper::ArenaLayout> l;
  for(const ArenaTemp& a : temps_)
    l.push_back(Mapper::ArenaLayout{a.block, a.addr, a.capacity, a.claim});
  return l;
}

void Arena::map_pages(Memory& memory) {
  if(temps_.empty()) return;
  uint32_t lo = temps_.front().addr >> 10;
  const ArenaTemp& last = temps_.back();
  uint32_t hi = (last.addr + 1 + (last.capacity + 1) / 2) >> 10;
  for(uint32_t p = lo; p <= hi; p++)
    memory.map_page(new os::ArrayPage(), p, Permissions::PERMISSION_READ | Permissions::PERMISSION_WRITE);
  fprintf(stderr, "Arena: mapped %u page(s) at %08X (RW, no exec) for %s\n",
          hi - lo + 1, lo << 10, memory.process_name.c_str());
}

void Arena::reset_for_tests() { temps_.clear(); by_key_.clear(); by_wmsp_.clear(); path_.clear(); }

} // namespace strings
} // namespace hw
