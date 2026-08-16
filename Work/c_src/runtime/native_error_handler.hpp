// src/runtime/native_error_handler.hpp — Project 8 Stage B: the
// stack-free handler-state implementation.
//
// TaskL2State (docs/Project6/NativeDesign.md §1) is the SOLE authority
// for handler state: establisher records each carrying a node list,
// plus the re-hosted signal record and resume flag. This
// implementation writes NO contract-private cells (Registers E9/E10 —
// the clone writes nothing to the static band; the wsp reservations'
// contents stay unwritten) and never walks MV frames for handler
// state. Frames remain real (M3b): record.frame is the real frame
// address — the record identity, the dispatch token (H2: value-pinned;
// nothing may dereference it), and I.GOTO's cut level.
//
// Rulings carried (docs/Project8/PROMPT.md, binding):
//   a. I.EPILOG mismatched pop  -> abort_world(save=false) + throw.
//   b. Walker outputs           -> DROPPED entirely (Register E7).
//   c. Chain growth cap         -> NONE.
//   d. I.GOTO bad-chain shapes  -> ABORT-INTENDED (wired in frames.cpp,
//      common code; this class aborts on the adjacent condition of an
//      unwind target with no establishment record).
#pragma once
#include "error_handler.hpp"
#include <vector>
#include <cstdint>

namespace rt {

struct HandlerNode {          // one O.ON registration
  int32_t type;               // node[+2] semantics: 0 == inactive/reusable
  int32_t key2;               // node[+4]
  int32_t handler;            // node[+6], game pc
};

struct EstablisherRecord {    // one I.PROLOG bracket
  int32_t frame;              // establishing routine's MV frame pointer
  int32_t wsp_snapshot;       // entry wsp + 4 — I.GOTO's landing restore
  int32_t slot4, slot6;       // I.PROLOG inline words (0 in Quest)
  std::vector<HandlerNode> nodes;   // HEAD-FIRST (O.ON pushes at the
                              // head): search = front-to-back first key
                              // match; reuse backstop = LAST inactive
};

struct TaskL2State {          // per task, keyed by (process, wsb), lazy
  std::vector<EstablisherRecord> chain;   // back() = innermost
  int32_t sig_type = 0, sig_key2 = 0, sig_code = 0;   // re-hosted [C+2/4/6]
  int32_t resume_flag = 0;                            // re-hosted [wsb-0x2A]
  // walker outputs deliberately NOT hosted (ruling b, Register E7)
};

class native_error_handler : public error_handler_api {
public:
  void establish(hw::Machine& machine, const EstablishIn& in,
                 EstablishOutcome& out) override;
  void disestablish(hw::Machine& machine, int32_t caller_wfp) override;
  void cut(hw::Machine& machine, int32_t target_frame,
           int32_t entry_wfp, CutOutcome& out) override;
  void register_node(hw::Machine& machine, const RegisterIn& in,
                     RegisterOutcome& out) override;
  void revert_node(hw::Machine& machine, int32_t caller_wfp,
                   int32_t type, int32_t raw_key2,
                   RevertOutcome& out) override;
  void select(hw::Machine& machine, int32_t type, int32_t raw_key2,
              SelectOutcome& out) override;
  bool has_handler(hw::Machine& machine) override;
  void record_raise(hw::Machine& machine, const SigRecord& rec) override;
  SigRecord sig_record(hw::Machine& machine) override;
  void set_resume_flag(hw::Machine& machine, int32_t flag) override;
  int32_t resume_flag(hw::Machine& machine) override;

  // The per-(process, wsb) state, created lazily on first touch
  // (NativeDesign §8 — a fresh task's chain is empty, sig_*/flag are 0,
  // matching the zero-filled cells' semantics). Exposed for
  // check_error_handler's mismatch reports.
  static TaskL2State& state(hw::Machine& machine);
};

} // namespace rt
