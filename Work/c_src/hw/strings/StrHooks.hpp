// hw/strings/StrHooks.hpp — the P33-A checker hooks for the string family
// (docs/Project29/StringsDesign.md §6, docs/Project33/REPORT.md).
//
// Three things live here, all dark unless QUEST_STRINGS_CHECK=1:
//
//  1. StrHooks — the HOOK TABLE (quest.strhooks, QUEST_STRHOOKS=<file>):
//     the 19 claim-group blocks (identity of p@b), their arena layout
//     (provisional in P33-A), the 57 WMSP pcs, the 19 STASP pcs and the
//     I.GOTO landing-stub STASP (the ON-pop event). Process-wide, immutable
//     after load. Loader refuses duplicates, a block without exactly one
//     first WMSP, a STASP whose claims are not its block's, and — at the
//     first attach — any hooked pc whose word does not decode to WMSP/STASP.
//
//  2. MachineHooks — the per-Machine state, BOTH roles (ruling S1, Sep 6:
//     symmetric Δ): a ClaimDelta keyed on this machine's own wfp, the
//     per-row binding this machine has seen (block, wfp, sp@block), and
//     counters for the verdict lines. The hooks are called from the
//     instruction arms (EagleStack.cpp WMSP/STASP/WRTN, frames.cpp's clone
//     twins) so that a WMSP embedded in an IR block fires exactly like one
//     fetched by the master. Loud checks (METHOD §8), all aborts:
//       - STASP restores wsp to sp@block (the first claim's wsp_before);
//       - Δ(wfp) == 0 after the STASP (every claim of the group hooked);
//       - Δ(wfp) == 0 at frame exit for every frame the exit erases.
//
//  3. The per-ordinal ARENA EVENT QUEUE: rows are keyed on the MASTER's
//     wfp but live in the CLONE's mapper (compare_pair calls the clone's
//     equivalent()). The master's hooks queue bind/unmap events; the clone
//     drains them into its own mapper at the top of its next run_steps —
//     the worker runs master half, clone half, compare, nothing between,
//     so every event precedes the clone's execution of the same block.
//
//  P33-B: the arena has one twin per claim (t@<block>.<k>, quest.arena —
//  hw/strings/Arena). Claim k of block b binds ITS row at its WMSP hook
//  (master_addr = wsp_before + 2); there is no rebind between claims any
//  more — every master temp has its own clone twin, so every residue
//  pointer maps. Rows are unmapped at frame exit as before.
//
//  Flag off: StrHooks::active is false, Machine::strhooks stays nullptr,
//  the three instruction arms take one null test each — no other path.
#pragma once
#include "ClaimDelta.hpp"
#include "Arena.hpp"
#include "../Mapper.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace hw {
class Machine;
namespace strings {

enum class HookKind : uint8_t { None, Wmsp, Stasp, OnPop, Unwind };

struct HookRow {
  uint32_t id;            // 1-based row id (the artifact's)
  uint32_t block;         // the claim-group block address — the identity of p@b
  uint32_t first, last;   // first / last WMSP pc of the group
  uint32_t nclaims;
  std::string routine;
};

struct HookPc {
  HookKind kind;
  uint32_t row;           // 1-based; 0 for OnPop
  uint32_t n;             // claim ordinal within the group (Wmsp)
};

struct ArenaEvent {
  enum Kind : uint8_t { Bind, Unmap } kind;
  uint32_t arena_addr;    // Bind
  int32_t  wfp;           // Bind: the master's wfp; Unmap: the frame
  uint32_t master_addr;   // Bind
};

class MachineHooks;

class StrHooks {
public:
  static bool active;                       // QUEST_STRINGS_CHECK=1 + table loaded
  // Parse QUEST_STRINGS_CHECK / QUEST_STRHOOKS. Returns false when the
  // configuration is unusable (flag set without a table, malformed table):
  // the caller refuses to launch (the INJECT/POKE discipline).
  static bool load_from_env();
  static bool load_file(const std::string& path, std::string* err);   // exposed for tests
  static const HookPc* lookup(uint32_t pc) {
    auto it = pcs_.find(pc);
    return it == pcs_.end() ? nullptr : &it->second;
  }
  static const std::vector<HookRow>& rows() { return rows_; }
  static const HookRow& row(uint32_t id) { return rows_[id - 1]; }
  static uint32_t onpop_pc() { return onpop_pc_; }
  static bool is_unwind_wrtn(uint32_t pc) { const HookPc* h = lookup(pc); return h && h->kind == HookKind::Unwind; }

  // First thing in Machine::run_steps when active: create the machine's
  // MachineHooks, configure its mapper's arena rows (once), verify the
  // table against the loaded program (once per process — decode the word
  // at every hooked pc), and — CLONE role — drain the master's queued
  // arena events into this machine's mapper.
  static void attach(Machine& m);
  static void queue(int32_t ordinal, const ArenaEvent& e);
  static void drain(Machine& clone);
  static size_t queued(int32_t ordinal);      // tests
  static void report();                        // shutdown summary (stderr)
  static void reset_for_tests();

private:
  static std::vector<HookRow> rows_;
  static std::unordered_map<uint32_t, HookPc> pcs_;
  static uint32_t onpop_pc_;
  static std::string path_;
  static std::vector<ArenaEvent> events_[64];
  static std::vector<MachineHooks*> all_;      // for report()
  friend class MachineHooks;
};

class MachineHooks {
public:
  explicit MachineHooks(Machine& m) : m_(m) {}
  // The three instruction hooks (EagleStack.cpp arms, frames.cpp twins).
  void wmsp(uint32_t pc, int32_t ac, int32_t wsp_before, int32_t wsp_after);
  void stasp(uint32_t pc, int32_t old_wsp, int32_t new_wsp);
  // WRTN: every frame with wfp >= pre_wfp. `unwind` = the WRTN is a cut
  // (I.GOTO / R?SIGNAL — `unwind` rows of the table, or the clone's
  // native i_goto): outstanding claims in the discarded frames are
  // legitimate (a signal from inside a group) and are erased silently;
  // an ORDINARY return with claims outstanding is the loud fault.
  void frame_exit(int32_t pre_wfp, bool unwind = false);
  void onpop(int32_t restored_wfp);            // I.GOTO landing: every frame with wfp >= restored
  int32_t delta(int32_t wfp) const { return delta_.delta(wfp); }
  const ClaimDelta& claims() const { return delta_; }

  // Counters (verdict lines).
  uint64_t n_bind = 0, n_rebind = 0, n_unmap = 0, n_claim = 0, n_release = 0,
           n_frame_exit = 0, n_onpop = 0, n_unwind = 0, n_discarded_claims = 0;
  int32_t  max_delta = 0;
  std::string label() const;

private:
  struct Live {                // this machine's current binding of a row
    int32_t  wfp;              // the frame that owns it (this machine's wfp)
    int32_t  base_wsp;         // sp@block: wsp_before of the FIRST claim
    uint32_t claims_seen;
  };
  void exit_frames_at_or_above(int32_t wfp, const char* what, bool strict);
  // "f is at or above threshold" in MASTER coordinates (Mapper::frame_precedes):
  // on the clone a frame may be an area address, whose numeric value says
  // nothing about its stack position. Must be asked while both frames'
  // records still live — hence the hooks fire BEFORE area_wrtn_fixup /
  // area_unwind_to.
  bool at_or_above(int32_t f, int32_t threshold) const;
  void emit(const ArenaEvent& e);
  bool master() const;
  Machine& m_;
  ClaimDelta delta_;
  std::map<uint32_t, Live> live_;              // row id → binding
  std::map<uint32_t, uint32_t> max_claim_;     // twin id → largest 4·ac seen (bytes), for the capacity column
  friend class StrHooks;
};

} // namespace strings
} // namespace hw
