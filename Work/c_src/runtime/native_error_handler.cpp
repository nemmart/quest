// src/runtime/native_error_handler.cpp — Project 8 Stage B.
//
// Every method is specified by docs/Project6/L2Contract.md §3 (abstract
// operations) and docs/Project6/NativeDesign.md §1 (the entry-to-chain
// mapping table). No real-memory handler cell is read or written here;
// the abort sites implement rulings a/d's ABORT-INTENDED semantics
// (abort_world(save=false) then throw — Lockstep.hpp: the caller
// throws to unwind its own execution).
#include "native_error_handler.hpp"
#include "../hw/Machine.hpp"
#include "../hw/Lockstep.hpp"
#include "../os/OSProcess.hpp"
#include <map>
#include <mutex>
#include <stdexcept>
#include <cstdio>

namespace {

constexpr int32_t DEF_ON_ENTRY = 0x7017EF05;   // exhaustion handler value

[[noreturn]] void abort_intended(hw::Machine& machine, const char* what) {
  char buf[256];
  snprintf(buf, sizeof(buf),
           "L2 ABORT-INTENDED (native_error_handler): %s", what);
  hw::Lockstep::abort_world(buf, &machine, /*save=*/false);
  throw std::runtime_error(buf);
}

// The chain-search semantics over a record's node list, exactly the
// EE7A helper's rules re-expressed (o_on.cpp rt::chain_search is the
// reference): walk head-first; every inactive node (type==0) overwrites
// the backstop (LAST one wins); first key match returns found.
// key2 must already reflect the catch-all preamble (0 when key1<=0).
rt::HandlerNode* node_search(rt::EstablisherRecord& rec, int32_t key1,
                             int32_t key2, bool& found) {
  rt::HandlerNode* backstop = nullptr;
  found = false;
  for(auto& n : rec.nodes) {
    if(n.type == 0)
      backstop = &n;
    if(n.type == key1 && n.key2 == key2) {
      found = true;
      return &n;
    }
  }
  return backstop;   // not found: backstop-or-null
}

} // namespace

namespace rt {

TaskL2State& native_error_handler::state(hw::Machine& machine) {
  // Keyed by (emulated process, wsb): one host process runs the server
  // and both lockstep roles, and T.INIT provably creates tasks with
  // their own wsb (NativeDesign §8). Only the clone's tasks ever reach
  // here (masters never dispatch natives). Guarded because task
  // batches for different processes can run on different threads;
  // record references never escape a single api call's extent on the
  // dispatching thread.
  static std::map<std::pair<const void*, int32_t>, TaskL2State> states;
  static std::mutex states_mutex;
  std::lock_guard<std::mutex> lock(states_mutex);
  return states[{static_cast<const void*>(machine.process), machine.wsb}];
}

// I.PROLOG — push. old_head is the exit-ac0 residue: the pre-push
// innermost establisher's frame, 0 on an empty chain (the invariant
// [wsb-0x40] == innermost record's frame, computed deliberately —
// PROMPT boundary discipline 3).
void native_error_handler::establish(hw::Machine& machine, const EstablishIn& in,
                                     EstablishOutcome& out) {
  TaskL2State& s = state(machine);
  out.old_head = s.chain.empty() ? 0 : s.chain.back().frame;
  EstablisherRecord rec;
  rec.frame = in.frame;
  rec.wsp_snapshot = in.entry_wsp + 4;
  rec.slot4 = in.slot4;
  rec.slot6 = in.slot6;
  s.chain.push_back(std::move(rec));
  // Display copies: not performed (contract-private frame words,
  // Register E8/E9; the loop's register effects are the wrapper's).
}

// I.EPILOG — pop, ruling a: the mismatched-frame edge asserts and
// aborts ("the native version gets an extra check on PL/1
// correctness" — contract THIRD ADDENDUM item 1).
void native_error_handler::disestablish(hw::Machine& machine, int32_t caller_wfp) {
  TaskL2State& s = state(machine);
  if(s.chain.empty())
    abort_intended(machine, "I.EPILOG pop on an EMPTY chain (mis-bracketed PL/1 epilogue, or chain drift)");
  if(s.chain.back().frame != caller_wfp) {
    char buf[160];
    snprintf(buf, sizeof(buf),
             "I.EPILOG pop mismatch: wfp=%08X but innermost establisher frame=%08X (depth %zu)",
             static_cast<uint32_t>(caller_wfp),
             static_cast<uint32_t>(s.chain.back().frame), s.chain.size());
    abort_intended(machine, buf);
  }
  s.chain.pop_back();
}

// I.GOTO unwind — cut: pop every record with .frame strictly above the
// target; the record AT the target survives and supplies the landing
// snapshot. A target with no establishment record has no snapshot to
// land on — ABORT-INTENDED (the bad-token family, contract §3.3
// shape 3; the common pre-walk has already validated the frame chain
// itself).
void native_error_handler::cut(hw::Machine& machine, int32_t target_frame,
                               int32_t entry_wfp, CutOutcome& out) {
  (void)entry_wfp;
  TaskL2State& s = state(machine);
  // Ruling A (Project 12): frame ordering in MASTER coordinates (T maps
  // area frames back onto the master's stack); operands only.
  while(!s.chain.empty() && machine.frame_precedes(target_frame, s.chain.back().frame))
    s.chain.pop_back();
  if(s.chain.empty() || s.chain.back().frame != target_frame) {
    char buf[160];
    snprintf(buf, sizeof(buf),
             "I.GOTO unwind target %08X has no establishment record (innermost after cut: %08X, depth %zu)",
             static_cast<uint32_t>(target_frame),
             s.chain.empty() ? 0u : static_cast<uint32_t>(s.chain.back().frame),
             s.chain.size());
    abort_intended(machine, buf);
  }
  out.wsp_restore = s.chain.back().wsp_snapshot;
}

// O.ON — the caller's record; reuse (first key match, else LAST
// inactive) or allocate at the HEAD (the on-stack chain links new
// nodes at the head; head-first vector order preserves search order).
void native_error_handler::register_node(hw::Machine& machine, const RegisterIn& in,
                                         RegisterOutcome& out) {
  TaskL2State& s = state(machine);
  EstablisherRecord* rec = nullptr;
  for(auto it = s.chain.rbegin(); it != s.chain.rend(); ++it)
    if(it->frame == in.caller_frame) { rec = &*it; break; }
  if(rec == nullptr) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "O.ON from frame %08X which has no establishment record (depth %zu)",
             static_cast<uint32_t>(in.caller_frame), s.chain.size());
    abort_intended(machine, buf);
  }
  int32_t key2 = (in.type > 0) ? in.raw_key2 : 0;   // helper preamble
  bool found;
  HandlerNode* node = node_search(*rec, in.type, key2, found);
  out.found = found;
  out.node = 0;        // MV-RESIDUE (stack addresses; no native meaning)
  out.scratch = 0;
  if(node != nullptr) {
    node->type = in.type;
    node->key2 = (in.type <= 0) ? 0 : in.raw_key2;
    node->handler = in.handler;
    out.allocated = false;
  } else {
    HandlerNode fresh;
    fresh.type = in.type;
    fresh.key2 = (in.type <= 0) ? 0 : in.raw_key2;
    fresh.handler = in.handler;
    rec->nodes.insert(rec->nodes.begin(), fresh);
    out.allocated = true;   // drives the wrapper's +8 wsp reservation (H3)
    // The allocate path's caller_frame[+2] := frame-4 write (Contract
    // §3.4) is not mere bookkeeping: frame-4 = O.ON's entry wsp + 8 =
    // the post-allocate wsp — it UPDATES the establisher's landing
    // snapshot so a later unwind restores wsp ABOVE the node kept
    // alive on the stack. NativeDesign §1's cut row missed this
    // (found by check_error_handler on the first M-trigger run —
    // REPORT.md correction; the reuse path performs no update).
    rec->wsp_snapshot = in.entry_wsp + 8;
  }
}

// O.REVERT — gate on innermost establisher; deactivate in place (node
// retained for reuse).
void native_error_handler::revert_node(hw::Machine& machine, int32_t caller_wfp,
                                       int32_t type, int32_t raw_key2,
                                       RevertOutcome& out) {
  TaskL2State& s = state(machine);
  out.gate_passed = false;
  out.found = false;
  out.node = 0;
  out.scratch = 0;
  if(s.chain.empty() || s.chain.back().frame != caller_wfp)
    return;
  out.gate_passed = true;
  int32_t key2 = (type > 0) ? raw_key2 : 0;
  bool found;
  HandlerNode* node = node_search(s.chain.back(), type, key2, found);
  out.found = found;
  if(found)
    node->type = 0;   // deactivate in place
}

// The raise's select loop — records innermost-out, per record the node
// search with the catch-all preamble.
void native_error_handler::select(hw::Machine& machine, int32_t type,
                                  int32_t raw_key2, SelectOutcome& out) {
  TaskL2State& s = state(machine);
  int32_t key2 = (type > 0) ? raw_key2 : 0;
  out.found = false;
  out.frame = 0;
  out.handler = DEF_ON_ENTRY;
  out.any_search = !s.chain.empty();
  out.last_frame = 0;    // MV-RESIDUE fields: no native meaning
  out.last_node = 0;
  out.last_scratch = 0;
  out.last_found = false;
  for(auto it = s.chain.rbegin(); it != s.chain.rend(); ++it) {
    bool found;
    HandlerNode* node = node_search(*it, type, key2, found);
    if(found) {
      out.found = true;
      out.frame = it->frame;       // the token (H2: real frame address)
      out.handler = node->handler;
      return;
    }
  }
}

// Contract §2.4 over the chain (NativeDesign §5): a raise of (-1, 0)
// now would find a handler. Inactive nodes (type 0) never match -1.
bool native_error_handler::has_handler(hw::Machine& machine) {
  TaskL2State& s = state(machine);
  for(auto it = s.chain.rbegin(); it != s.chain.rend(); ++it)
    for(const auto& n : it->nodes)
      if(n.type == -1 && n.key2 == 0)
        return true;
  return false;
}

// O.SET semantics — the triple only; the walker is DROPPED (ruling b).
void native_error_handler::record_raise(hw::Machine& machine, const SigRecord& rec) {
  TaskL2State& s = state(machine);
  s.sig_type = rec.type;
  s.sig_key2 = rec.key2;
  s.sig_code = rec.code;
}

SigRecord native_error_handler::sig_record(hw::Machine& machine) {
  TaskL2State& s = state(machine);
  return SigRecord{s.sig_type, s.sig_key2, s.sig_code};
}

void native_error_handler::set_resume_flag(hw::Machine& machine, int32_t flag) {
  state(machine).resume_flag = flag;
}

int32_t native_error_handler::resume_flag(hw::Machine& machine) {
  return state(machine).resume_flag;
}

} // namespace rt
