// src/runtime/mv_error_handler.hpp — Project 8: the bit-faithful
// handler-state implementation (THE LIVING ATTIC).
//
// Owns every real-memory chain/state cell read and write, with exactly
// the pre-Project-8 wrapper semantics: the establisher frame slots
// ([wfp+2..+0xA], display words), the pushed-wide contents inside the
// I.PROLOG reservation, the chain head [wsb-0x40], the on-stack handler
// nodes (search/overwrite/allocate-by-relocation/deactivate), the
// C-record cells [wsb-0x3E/-0x3C/-0x3A] plus the walker outputs
// [wsb-0x36/-0x38], and the resume flag [wsb-0x2A].
//
// This subclass is the forensic reference if any ABORT-INTENDED site
// ever fires (contract THIRD ADDENDUM; PROMPT.md ruling d): it is the
// original translation kept alive as a buildable choice (-handler=mv).
//
// Memory-walk building blocks (rt::chain_search, rt::select_frames,
// rt::signal_walker) remain exported from o_on.cpp / o_signal.cpp and
// are mv-internal since Project 8 — no common wrapper code calls them.
#pragma once
#include "error_handler.hpp"

namespace rt {

class mv_error_handler : public error_handler_api {
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
};

} // namespace rt
