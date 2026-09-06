// hw/strings/StrHooks.cpp — see StrHooks.hpp.
#include "StrHooks.hpp"
#include "../Machine.hpp"
#include "../Memory.hpp"
#include "../Decoder.hpp"
#include "../Instruction.hpp"
#include "../EagleStack.hpp"
#include "../Lockstep.hpp"
#include "../../os/OSProcess.hpp"
#include "../../os/Trace.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <set>

namespace hw {
namespace strings {

bool StrHooks::active = false;
std::vector<HookRow> StrHooks::rows_;
std::unordered_map<uint32_t, HookPc> StrHooks::pcs_;
uint32_t StrHooks::onpop_pc_ = 0;
std::string StrHooks::path_;
std::vector<ArenaEvent> StrHooks::events_[64];
std::vector<MachineHooks*> StrHooks::all_;

static bool decode_verified = false;

[[noreturn]] static void hook_abort(Machine* m, const std::string& msg) {
  fflush(stdout);
  fprintf(stderr, "STRHOOKS: %s\n", msg.c_str());
  if(Lockstep::enabled)
    Lockstep::abort_world(msg.c_str(), m, /*save=*/false);
  throw std::runtime_error(msg);
}

static std::string hex(uint32_t v) {
  char b[12]; snprintf(b, sizeof b, "%08X", v); return b;
}

// ---- the loader ----------------------------------------------------------

static bool kv(const std::string& tok, const char* key, std::string& val) {
  size_t k = strlen(key);
  if(tok.compare(0, k, key) != 0 || tok.size() <= k || tok[k] != '=') return false;
  val = tok.substr(k + 1);
  return true;
}

static bool parse_hex(const std::string& s, uint32_t& v) {
  char* e = nullptr;
  unsigned long x = strtoul(s.c_str(), &e, 16);
  if(s.empty() || *e) return false;
  v = static_cast<uint32_t>(x);
  return true;
}

static bool parse_dec(const std::string& s, uint32_t& v) {
  char* e = nullptr;
  unsigned long x = strtoul(s.c_str(), &e, 10);
  if(s.empty() || *e) return false;
  v = static_cast<uint32_t>(x);
  return true;
}

bool StrHooks::load_file(const std::string& path, std::string* err) {
  rows_.clear(); pcs_.clear(); onpop_pc_ = 0;
  std::ifstream in(path);
  if(!in) { *err = "cannot open " + path; return false; }
  std::string line;
  int lineno = 0;
  std::map<uint32_t, uint32_t> first_seen;        // row → count of n=1 claims
  std::map<uint32_t, std::set<uint32_t>> claims;  // row → set of claim ordinals
  std::map<uint32_t, uint32_t> stasps;            // row → stasp count
  std::set<uint32_t> blocks;
  auto fail = [&](const std::string& m) { *err = path + ":" + std::to_string(lineno) + ": " + m; return false; };
  while(std::getline(in, line)) {
    lineno++;
    if(line.empty() || line[0] == '#') continue;
    std::istringstream ss(line);
    std::string kind; ss >> kind;
    std::vector<std::string> t;
    for(std::string x; ss >> x;) t.push_back(x);
    if(kind == "row") {
      if(t.size() < 6) return fail("row: too few fields");
      HookRow r;
      std::string v;
      if(!parse_dec(t[0], r.id) || r.id == 0) return fail("row: bad id");
      if(!parse_hex(t[1], r.block)) return fail("row: bad block");
      r.routine = t[2];
      if(!kv(t[3], "first", v) || !parse_hex(v, r.first)) return fail("row: bad first=");
      if(!kv(t[4], "last", v) || !parse_hex(v, r.last)) return fail("row: bad last=");
      if(!kv(t[5], "nclaims", v) || !parse_dec(v, r.nclaims) || r.nclaims == 0) return fail("row: bad nclaims=");
      if(r.id != rows_.size() + 1) return fail("row: ids must be 1..n in order");
      if(!blocks.insert(r.block).second) return fail("row: duplicate block " + hex(r.block));
      rows_.push_back(r);
    }
    else if(kind == "wmsp" || kind == "stasp") {
      if(t.size() < 2) return fail(kind + ": too few fields");
      uint32_t pc, row, n = 0;
      std::string v;
      if(!parse_hex(t[0], pc)) return fail(kind + ": bad pc");
      if(!kv(t[1], "row", v) || !parse_dec(v, row) || row == 0 || row > rows_.size())
        return fail(kind + ": bad row= (rows must precede their pcs)");
      if(kind == "wmsp") {
        if(t.size() < 3 || !kv(t[2], "n", v) || !parse_dec(v, n) || n == 0) return fail("wmsp: bad n=");
        if(!claims[row].insert(n).second) return fail("wmsp: duplicate claim ordinal");
        if(n == 1) { first_seen[row]++; if(pc != rows_[row - 1].first) return fail("wmsp: n=1 pc is not the row's first="); }
        if(n == rows_[row - 1].nclaims && pc != rows_[row - 1].last) return fail("wmsp: last claim pc is not the row's last=");
      } else {
        stasps[row]++;
      }
      HookPc h{kind == "wmsp" ? HookKind::Wmsp : HookKind::Stasp, row, n};
      if(!pcs_.emplace(pc, h).second) return fail("duplicate pc " + hex(pc));
    }
    else if(kind == "unwind") {
      if(t.size() < 1) return fail("unwind: missing pc");
      uint32_t pc;
      if(!parse_hex(t[0], pc)) return fail("unwind: bad pc");
      if(!pcs_.emplace(pc, HookPc{HookKind::Unwind, 0, 0}).second) return fail("duplicate pc " + hex(pc));
    }
    else if(kind == "onpop") {
      if(t.size() < 1) return fail("onpop: missing pc");
      uint32_t pc;
      if(!parse_hex(t[0], pc)) return fail("onpop: bad pc");
      if(onpop_pc_ != 0) return fail("onpop: more than one");
      if(!pcs_.emplace(pc, HookPc{HookKind::OnPop, 0, 0}).second) return fail("duplicate pc " + hex(pc));
      onpop_pc_ = pc;
    }
    else return fail("unknown line kind " + kind);
  }
  if(rows_.empty()) { *err = path + ": no rows"; return false; }
  for(const HookRow& r : rows_) {
    if(first_seen[r.id] != 1) { *err = path + ": row " + std::to_string(r.id) + " has " + std::to_string(first_seen[r.id]) + " first claims (need exactly one)"; return false; }
    if(claims[r.id].size() != r.nclaims) { *err = path + ": row " + std::to_string(r.id) + " claims != nclaims"; return false; }
    for(uint32_t n = 1; n <= r.nclaims; n++)
      if(!claims[r.id].count(n)) { *err = path + ": row " + std::to_string(r.id) + " missing claim n=" + std::to_string(n); return false; }
    if(stasps[r.id] < 1) { *err = path + ": row " + std::to_string(r.id) + " has no STASP"; return false; }
  }
  if(onpop_pc_ == 0) { *err = path + ": no onpop line"; return false; }
  path_ = path;
  return true;
}

bool StrHooks::load_from_env() {
  const char* flag = getenv("QUEST_STRINGS_CHECK");
  bool want = flag && strcmp(flag, "0") != 0 && *flag;
  const char* path = getenv("QUEST_STRHOOKS");
  if(!want) {
    if(path)
      fprintf(stderr, "StrHooks: QUEST_STRHOOKS set without QUEST_STRINGS_CHECK=1 — ignored (checker dark)\n");
    return true;
  }
  if(!path) {
    fprintf(stderr, "StrHooks: QUEST_STRINGS_CHECK=1 but QUEST_STRHOOKS is not set — refusing to launch\n");
    return false;
  }
  std::string err;
  if(!load_file(path, &err)) {
    fprintf(stderr, "StrHooks: %s — refusing to launch\n", err.c_str());
    return false;
  }
  // P33-B: the arena layout is a second artifact (quest.arena); every WMSP
  // of the table must have its twin and every twin its WMSP.
  if(!Arena::loaded()) {
    fprintf(stderr, "StrHooks: QUEST_STRINGS_CHECK=1 needs QUEST_ARENA=<quest.arena> (one twin per claim) — refusing to launch\n");
    return false;
  }
  size_t nw = 0;
  for(const auto& kv : pcs_) {
    if(kv.second.kind != HookKind::Wmsp) continue;
    nw++;
    const ArenaTemp* a = Arena::by_wmsp(kv.first);
    if(!a || a->block != rows_[kv.second.row - 1].block || a->claim != kv.second.n) {
      fprintf(stderr, "StrHooks: wmsp %08X (block %08X claim %u) has no matching twin in %s — refusing to launch\n",
              kv.first, rows_[kv.second.row - 1].block, kv.second.n, Arena::path().c_str());
      return false;
    }
  }
  if(nw != Arena::temps().size()) {
    fprintf(stderr, "StrHooks: %zu wmsp hooks but %zu twins in %s — refusing to launch\n", nw, Arena::temps().size(), Arena::path().c_str());
    return false;
  }
  active = true;
  fprintf(stderr, "StrHooks: %s — %zu rows, %zu hooked pcs (onpop %08X), %zu twins; QUEST_STRINGS_CHECK armed\n",
          path, rows_.size(), pcs_.size(), onpop_pc_, Arena::temps().size());
  return true;
}


void StrHooks::attach(Machine& m) {
  if(!active) return;
  if(!decode_verified) {
    for(const auto& kv : pcs_) {
      uint32_t pc = kv.first;
      uint32_t opcode = m.memory->read_instruction_word(pc);
      Instruction* ins = Decoder::decode(m.segments[(pc >> 28) & 0x07]->lef, opcode);
      int32_t want = kv.second.kind == HookKind::Wmsp ? EagleStack::WMSP :
                     kv.second.kind == HookKind::Unwind ? EagleStack::WRTN : EagleStack::STASP;
      bool ok = ins && dynamic_cast<EagleStack*>(ins) != nullptr && ins->oper == want;
      if(!ok) {
        char buf[160];
        snprintf(buf, sizeof buf, "STRHOOKS: hooked pc %08X does not decode to %s (word %04X%s%s) — table/binary disagree",
                 pc, want == EagleStack::WMSP ? "WMSP" : want == EagleStack::WRTN ? "WRTN" : "STASP", opcode,
                 ins ? ", decodes to " : "", ins ? ins->name.c_str() : "");
        fprintf(stderr, "%s\n", buf);
        exit(2);
      }
    }
    decode_verified = true;
    fprintf(stderr, "StrHooks: %zu hooked pcs decode as their instruction — table verified against the program\n", pcs_.size());
  }
  if(m.strhooks == nullptr) {
    m.strhooks = new MachineHooks(m);
    all_.push_back(m.strhooks);
    if(m.mapper.arena_rows() == 0)
      m.mapper.configure_arena(Arena::layout());
  }
  if(m.lockstep_role == Lockstep::CLONE)
    drain(m);
}

void StrHooks::queue(int32_t ordinal, const ArenaEvent& e) {
  if(ordinal < 0 || ordinal >= 64) return;
  events_[ordinal].push_back(e);
}

size_t StrHooks::queued(int32_t ordinal) {
  if(ordinal < 0 || ordinal >= 64) return 0;
  return events_[ordinal].size();
}

void StrHooks::drain(Machine& clone) {
  int32_t o = clone.lockstep_ordinal;
  if(o < 0 || o >= 64 || events_[o].empty()) return;
  std::vector<ArenaEvent> ev;
  ev.swap(events_[o]);
  for(const ArenaEvent& e : ev) {
    switch(e.kind) {
      case ArenaEvent::Bind:     clone.mapper.arena_bind(e.arena_addr, e.wfp, e.master_addr); break;
      case ArenaEvent::Unmap:    clone.mapper.arena_unmap_frame(e.wfp); clone.mapper.claim_release(e.wfp); break;
      case ArenaEvent::ClaimIns: clone.mapper.claim_insert(e.wfp, static_cast<int32_t>(e.master_addr), static_cast<int32_t>(e.arena_addr)); break;
      case ArenaEvent::ClaimRel: clone.mapper.claim_release(e.wfp); break;
    }
  }
}

void StrHooks::report() {
  if(!active) return;
  for(MachineHooks* h : all_) {
    for(const auto& kv : h->max_claim_)
      fprintf(stderr, "StrHooks: %s max_claim t@%08X.%u = %u bytes (cap %u)\n", h->label().c_str(),
              Arena::temps()[kv.first - 1].block, Arena::temps()[kv.first - 1].claim, kv.second,
              Arena::temps()[kv.first - 1].capacity);
  }
  for(MachineHooks* h : all_)
    fprintf(stderr, "StrHooks: %s bind=%llu rebind=%llu unmap=%llu claim=%llu release=%llu frame_exit=%llu unwind=%llu onpop=%llu discarded_claims=%llu max_delta=%d\n",
            h->label().c_str(),
            (unsigned long long)h->n_bind, (unsigned long long)h->n_rebind, (unsigned long long)h->n_unmap,
            (unsigned long long)h->n_claim, (unsigned long long)h->n_release,
            (unsigned long long)h->n_frame_exit, (unsigned long long)h->n_unwind, (unsigned long long)h->n_onpop,
            (unsigned long long)h->n_discarded_claims, h->max_delta);
}

void StrHooks::reset_for_tests() {
  active = false; rows_.clear(); pcs_.clear(); onpop_pc_ = 0; path_.clear();
  for(auto& q : events_) q.clear();
  all_.clear();
  decode_verified = false;
}

// ---- the per-Machine hooks ------------------------------------------------

std::string MachineHooks::label() const {
  std::string l = m_.process ? m_.process->instance_label : std::string("?");
  l += m_.lockstep_role == Lockstep::MASTER ? "/master" :
       m_.lockstep_role == Lockstep::CLONE ? "/clone" : "/solo";
  return l;
}

bool MachineHooks::master() const {
  return m_.lockstep_role == Lockstep::MASTER;   // roles exist only under -lockstep
}

void MachineHooks::emit(const ArenaEvent& e) {
  if(master())
    StrHooks::queue(m_.lockstep_ordinal, e);
}

static void trace(const Machine& m, const char* text) {
  if(os::Trace::enabled("strings"))
    os::Trace::line("strings", m.process ? m.process->instance_label : std::string("?"), text);
}

void MachineHooks::wmsp(uint32_t pc, int32_t ac, int32_t wsp_before, int32_t wsp_after) {
  const HookPc* h = StrHooks::lookup(pc);
  if(h == nullptr || h->kind != HookKind::Wmsp)
    return;                                   // a WMSP outside the census (runtime, boot): not ours
  const HookRow& row = StrHooks::row(h->row);
  int32_t wfp = m_.wfp;
  if(wsp_after - wsp_before != 2 * ac) {      // the instruction's own effect, cross-checked
    char buf[160];
    snprintf(buf, sizeof buf, "WMSP at %08X: wsp moved %d, ac says %d wides (block %08X)",
             pc, wsp_after - wsp_before, ac, row.block);
    hook_abort(&m_, buf);
  }
  int32_t total_before = delta_.total();
  delta_.claim(wfp, ac);
  n_claim++;
  // P33-B: the claim as a stack insertion for the clone's stack leg (master
  // no-claim coordinates: wsp_before minus the claims outstanding below it)
  emit(ArenaEvent{ArenaEvent::ClaimIns, static_cast<uint32_t>(2 * ac), wfp, static_cast<uint32_t>(wsp_before - total_before)});
  int32_t d = delta_.delta(wfp);
  if(d > max_delta) max_delta = d;
  uint32_t master_addr = static_cast<uint32_t>(wsp_before + 2);   // LDASP r; WADI 2,r before every WMSP (57/57)
  auto it = live_.find(h->row);
  if(h->n == 1) {
    // Block entry: the group opens. A previous binding of this block in any
    // frame is superseded (a loop re-executing the block, §6.5, or a fresh
    // call of the routine after an exit that never reached WRTN).
    if(it != live_.end()) { n_rebind++; live_.erase(it); } else n_bind++;
    live_[h->row] = Live{wfp, wsp_before, 1};
  } else {
    if(it == live_.end() || it->second.wfp != wfp || it->second.claims_seen != h->n - 1) {
      char buf[200];
      snprintf(buf, sizeof buf, "WMSP %08X (claim %u of block %08X) without claim %u in this frame (wfp %08X, bound %s)",
               pc, h->n, row.block, h->n - 1, static_cast<uint32_t>(wfp),
               it == live_.end() ? "nowhere" : hex(static_cast<uint32_t>(it->second.wfp)).c_str());
      hook_abort(&m_, buf);
    }
    it->second.claims_seen = h->n;
  }
  // P33-B: claim k binds ITS twin t@b.k (one Mapper row per claim): the
  // master temp k's base <-> the twin's arena address. No rebind between
  // claims; every residue pointer into any temp of the group maps.
  const ArenaTemp* twin = Arena::by_wmsp(pc);
  if(twin == nullptr) hook_abort(&m_, "WMSP " + hex(pc) + " has no arena twin (loader should have refused)");
  // the twin's capacity vs the master's actual claim (the clone's `claim`
  // statement makes the same check on its side; here it is the master's)
  if(static_cast<uint32_t>(4 * ac) > twin->capacity) {
    char buf[200];
    snprintf(buf, sizeof buf, "WMSP %08X claims %d wides (%d bytes) but t@%08X.%u has capacity %u — quest.arena undersized",
             pc, ac, 4 * ac, twin->block, twin->claim, twin->capacity);
    hook_abort(&m_, buf);
  }
  if(static_cast<uint32_t>(4 * ac) > max_claim_[twin->id]) max_claim_[twin->id] = static_cast<uint32_t>(4 * ac);
  emit(ArenaEvent{ArenaEvent::Bind, twin->addr, wfp, master_addr});
  if(os::Trace::enabled("strings")) {
    char buf[200];
    snprintf(buf, sizeof buf, "claim pc=%08X block=%08X n=%u/%u ac=%d wfp=%08X master_addr=%08X twin=%08X delta=%d",
             pc, row.block, h->n, row.nclaims, ac, static_cast<uint32_t>(wfp), master_addr, twin->addr, d);
    trace(m_, buf);
  }
}

void MachineHooks::stasp(uint32_t pc, int32_t old_wsp, int32_t new_wsp) {
  const HookPc* h = StrHooks::lookup(pc);
  if(h == nullptr) return;
  if(h->kind == HookKind::OnPop) { onpop(m_.wfp); return; }
  if(h->kind != HookKind::Stasp) return;
  const HookRow& row = StrHooks::row(h->row);
  int32_t wfp = m_.wfp;
  auto it = live_.find(h->row);
  if(it == live_.end() || it->second.wfp != wfp) {
    char buf[200];
    snprintf(buf, sizeof buf, "STASP %08X releases block %08X, but the block is bound %s (wfp %08X)",
             pc, row.block, it == live_.end() ? "nowhere" : ("in frame " + hex(static_cast<uint32_t>(it->second.wfp))).c_str(),
             static_cast<uint32_t>(wfp));
    hook_abort(&m_, buf);
  }
  if(it->second.claims_seen != row.nclaims) {
    char buf[200];
    snprintf(buf, sizeof buf, "STASP %08X releases block %08X after %u of %u claims", pc, row.block, it->second.claims_seen, row.nclaims);
    hook_abort(&m_, buf);
  }
  if(new_wsp != it->second.base_wsp) {          // the STASP restores sp@block (census §1.5)
    char buf[200];
    snprintf(buf, sizeof buf, "STASP %08X restores wsp to %08X, but sp@block %08X (the first claim's wsp_before) is %08X",
             pc, static_cast<uint32_t>(new_wsp), row.block, static_cast<uint32_t>(it->second.base_wsp));
    hook_abort(&m_, buf);
  }
  delta_.release(wfp, old_wsp, new_wsp);
  n_release++;
  emit(ArenaEvent{ArenaEvent::ClaimRel, 0, wfp, 0});
  if(delta_.delta(wfp) != 0) {                  // every claim of the group was hooked and sized right
    char buf[200];
    snprintf(buf, sizeof buf, "STASP %08X (block %08X): delta after release is %d, not 0 — a claim of this group is unhooked or mis-sized",
             pc, row.block, delta_.delta(wfp));
    hook_abort(&m_, buf);
  }
  // The row stays bound (dead pointers in ac0/ac2/ac3 keep comparing);
  // only the claim accounting closes. The binding record persists until
  // frame exit so a stray later STASP/claim of this block is caught.
  it->second.claims_seen = 0;
  if(os::Trace::enabled("strings")) {
    char buf[160];
    snprintf(buf, sizeof buf, "release pc=%08X block=%08X wfp=%08X wsp %08X->%08X",
             pc, row.block, static_cast<uint32_t>(wfp), static_cast<uint32_t>(old_wsp), static_cast<uint32_t>(new_wsp));
    trace(m_, buf);
  }
}

bool MachineHooks::at_or_above(int32_t f, int32_t threshold) const {
  if(f == threshold) return true;
  return !m_.frame_precedes(static_cast<uint32_t>(f), static_cast<uint32_t>(threshold));
}

void MachineHooks::exit_frames_at_or_above(int32_t wfp, const char* what, bool strict) {
  // Rows: every binding whose frame is at or above wfp is gone (LIFO — for
  // an ordinary WRTN only the popped frame's own rows exist above it; for
  // an unwind cut the skipped frames come along).
  std::set<int32_t> frames;
  for(auto it = live_.begin(); it != live_.end();) {
    if(at_or_above(it->second.wfp, wfp)) { frames.insert(it->second.wfp); it = live_.erase(it); }
    else ++it;
  }
  for(int32_t f : frames) {
    emit(ArenaEvent{ArenaEvent::Unmap, 0, f, 0});
    n_unmap++;
  }
  // Δ: nothing may be outstanding in a frame that is being discarded.
  int32_t bad_wfp = 0, bad_delta = 0;
  delta_.frame_exit_where([&](int32_t f) { return at_or_above(f, wfp); }, &bad_wfp, &bad_delta);
  if(bad_delta != 0 && !strict) n_discarded_claims++;   // an unwind cut through a live group
  if(bad_delta != 0 && strict) {
    char buf[200];
    snprintf(buf, sizeof buf, "%s at pc %08X discards frame %08X with delta %d outstanding (wfp threshold %08X)",
             what, static_cast<uint32_t>(m_.pc), static_cast<uint32_t>(bad_wfp), bad_delta, static_cast<uint32_t>(wfp));
    hook_abort(&m_, buf);
  }
  if(!frames.empty() && os::Trace::enabled("strings")) {
    char buf[160];
    snprintf(buf, sizeof buf, "%s pc=%08X wfp>=%08X frames_unmapped=%zu", what, static_cast<uint32_t>(m_.pc),
             static_cast<uint32_t>(wfp), frames.size());
    trace(m_, buf);
  }
}

void MachineHooks::frame_exit(int32_t pre_wfp, bool unwind) {
  unwind = unwind || StrHooks::is_unwind_wrtn(static_cast<uint32_t>(m_.pc));
  if(unwind) n_unwind++; else n_frame_exit++;
  exit_frames_at_or_above(pre_wfp, unwind ? "unwind-WRTN" : "WRTN", /*strict=*/!unwind);
}

void MachineHooks::onpop(int32_t restored_wfp) {
  n_onpop++;
  exit_frames_at_or_above(restored_wfp, "ON-pop", /*strict=*/false);
}

} // namespace strings
} // namespace hw
