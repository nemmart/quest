// src/runtime/check_error_handler.cpp — Project 8 Stage B: the
// both-in-parallel comparison implementation (PROMPT.md: "check IS the
// shadow stage ... the comparison machinery lives permanently").
//
// Side-effect ownership is STRICT: mv owns every real-memory write,
// native owns the chain; this composite never lets native touch cells
// or mv touch the chain (they already can't — the subclasses only know
// their own state). Each decision method calls BOTH, compares the
// COMPARED fields of the two outcomes field-by-field BEFORE the
// wrapper stages anything, and throws loudly naming both sides on any
// mismatch. The returned outcome carries the verified-equal COMPARED
// fields plus mv's MV-RESIDUE fields (so the common residue images
// keep their bit-faithful values in check mode).
//
// Read methods (sig_record, resume_flag, has_handler) are compared
// too: mv reads the real cells, native reads TaskL2State — every read
// is a cross-check of the re-hosting.
//
// NOT check-thrown: ruling a/d ABORT-INTENDED sites inside the native
// implementation (I.EPILOG mismatch, recordless unwind target) fire
// abort_world in every mode — they are the ruling, not a comparison.
#include "error_handler.hpp"
#include "mv_error_handler.hpp"
#include "native_error_handler.hpp"
#include <stdexcept>
#include <string>
#include <cstdio>

namespace rt {

namespace {

[[noreturn]] void mismatch(const char* method, const char* field,
                           int64_t mv_value, int64_t native_value) {
  char buf[224];
  snprintf(buf, sizeof(buf),
           "[check_error_handler] %s outcome mismatch on %s: mv=%08llX native=%08llX",
           method, field,
           static_cast<unsigned long long>(static_cast<uint64_t>(mv_value) & 0xFFFFFFFFull),
           static_cast<unsigned long long>(static_cast<uint64_t>(native_value) & 0xFFFFFFFFull));
  throw std::runtime_error(buf);
}

void expect(const char* method, const char* field, int64_t mv_value, int64_t native_value) {
  if(mv_value != native_value)
    mismatch(method, field, mv_value, native_value);
}

} // namespace

class check_error_handler : public error_handler_api {
  mv_error_handler mv;
  native_error_handler nat;

public:
  void establish(hw::Machine& machine, const EstablishIn& in,
                 EstablishOutcome& out) override {
    EstablishOutcome a, b;
    mv.establish(machine, in, a);
    nat.establish(machine, in, b);
    expect("establish", "old_head", a.old_head, b.old_head);
    out = a;
  }

  void disestablish(hw::Machine& machine, int32_t caller_wfp) override {
    mv.disestablish(machine, caller_wfp);
    nat.disestablish(machine, caller_wfp);   // ruling a check lives inside
  }

  void cut(hw::Machine& machine, int32_t target_frame,
           int32_t entry_wfp, CutOutcome& out) override {
    CutOutcome a, b;
    mv.cut(machine, target_frame, entry_wfp, a);
    nat.cut(machine, target_frame, entry_wfp, b);
    expect("cut", "wsp_restore", a.wsp_restore, b.wsp_restore);
    out = a;
  }

  void register_node(hw::Machine& machine, const RegisterIn& in,
                     RegisterOutcome& out) override {
    RegisterOutcome a, b;
    mv.register_node(machine, in, a);
    nat.register_node(machine, in, b);
    expect("register_node", "allocated", a.allocated, b.allocated);
    expect("register_node", "found", a.found, b.found);
    out = a;   // node/scratch: mv residue for the wrapper's images
  }

  void revert_node(hw::Machine& machine, int32_t caller_wfp,
                   int32_t type, int32_t raw_key2,
                   RevertOutcome& out) override {
    RevertOutcome a, b;
    mv.revert_node(machine, caller_wfp, type, raw_key2, a);
    nat.revert_node(machine, caller_wfp, type, raw_key2, b);
    expect("revert_node", "gate_passed", a.gate_passed, b.gate_passed);
    expect("revert_node", "found", a.found, b.found);
    out = a;
  }

  void select(hw::Machine& machine, int32_t type, int32_t raw_key2,
              SelectOutcome& out) override {
    SelectOutcome a, b;
    mv.select(machine, type, raw_key2, a);
    nat.select(machine, type, raw_key2, b);
    expect("select", "found", a.found, b.found);
    expect("select", "frame (token)", a.frame, b.frame);
    expect("select", "handler", a.handler, b.handler);
    expect("select", "any_search", a.any_search, b.any_search);
    out = a;   // last_*: mv residue for the helper image
  }

  bool has_handler(hw::Machine& machine) override {
    bool a = mv.has_handler(machine);
    bool b = nat.has_handler(machine);
    expect("has_handler", "result", a, b);
    return a;
  }

  void record_raise(hw::Machine& machine, const SigRecord& rec) override {
    mv.record_raise(machine, rec);
    nat.record_raise(machine, rec);
  }

  SigRecord sig_record(hw::Machine& machine) override {
    SigRecord a = mv.sig_record(machine);
    SigRecord b = nat.sig_record(machine);
    expect("sig_record", "type", a.type, b.type);
    expect("sig_record", "key2", a.key2, b.key2);
    expect("sig_record", "code", a.code, b.code);
    return a;
  }

  void set_resume_flag(hw::Machine& machine, int32_t flag) override {
    mv.set_resume_flag(machine, flag);
    nat.set_resume_flag(machine, flag);
  }

  int32_t resume_flag(hw::Machine& machine) override {
    int32_t a = mv.resume_flag(machine);
    int32_t b = nat.resume_flag(machine);
    expect("resume_flag", "value", a, b);
    return a;
  }
};

// ---------------------------------------------------------------------
// Selection (moved here from mv_error_handler.cpp in Stage B: the
// factory needs all three types). Default CHECK during bring-up
// (PROMPT.md); Stage C flips to native once the matrix passes; mv is
// the attic run.
HandlerMode handler_mode = HandlerMode::CHECK;

error_handler_api& error_handler() {
  static mv_error_handler mv;
  static native_error_handler nat;
  static check_error_handler chk;
  switch(handler_mode) {
    case HandlerMode::MV:     return mv;
    case HandlerMode::NATIVE: return nat;
    case HandlerMode::CHECK:  break;
  }
  return chk;
}

} // namespace rt
