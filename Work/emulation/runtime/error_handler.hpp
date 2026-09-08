// src/runtime/error_handler.hpp — Project 8: the L2 handler-state API
//
// The seam between the L2 wrapper bodies (runtime/*.cpp) and the
// representation of HANDLER STATE (the establisher chain, the recorded
// signal, the resume flag). Architecture: docs/Project8/PROMPT.md
// ("The architecture", user-specified Aug 13 2026); the state being
// abstracted: docs/Project6/L2Contract.md §2; the native representation
// this enables: docs/Project6/NativeDesign.md §1.
//
// Boundary discipline (PROMPT.md, binding):
//  1. Only handler STATE goes behind this interface. Exit-register
//     staging, wsp reservations (H3), and transfer pairing stay as
//     COMMON code in the wrappers — normative contract outputs staged
//     in exactly ONE place, whatever the implementation.
//  2. Decision methods return OUTCOME STRUCTS, not staged registers.
//     The wrapper maps the outcome onto ac0-3/wsp once, with staging
//     expressions lifted from the capture-validated wrappers.
//  3. Residue values that are byproducts of the bit-faithful (mv)
//     internals are NAMED FIELDS in the outcome structs. Fields are
//     annotated COMPARED (semantically meaningful in every
//     implementation; check_error_handler compares them field by
//     field before staging) or MV-RESIDUE (feed contract-private
//     memory images only — Register E8; the native implementation
//     leaves them 0, and check compares them only when both sides
//     define them).
//
// Implementations (PROMPT.md):
//   mv_error_handler     — bit-faithful; owns every real-memory chain/
//                          state cell write, exactly the pre-Project-8
//                          wrapper semantics. The LIVING ATTIC.
//   native_error_handler — TaskL2State sole authority (Stage B).
//   check_error_handler  — both in parallel, outcomes compared (Stage B).
//
// Stage A re-expressed the wrappers against this interface with zero
// behavior change (mv only); Stage B added native_error_handler +
// check_error_handler.
#pragma once
#include <cstdint>

namespace hw { class Machine; class RTBridge; }

namespace rt {

// ---------------------------------------------------------------------
// Selection (-handler= flag, Launch.cpp). Default CHECK during Stage B
// bring-up (PROMPT.md); Stage C flips the default to native once the
// matrix passes; mv remains the attic run.
enum class HandlerMode { MV, NATIVE, CHECK };
extern HandlerMode handler_mode;

// ---------------------------------------------------------------------
// Outcome structs (discipline point 2/3 above).

// I.PROLOG — push one establisher record (Contract §3.1).
struct EstablishIn {
  int32_t frame;        // the establishing routine's frame pointer (wfp)
  int32_t entry_wsp;    // wsp at entry (before the two pushes)
  int32_t entry_ac1;    // pushed as residue at [entry_wsp+4]
  int32_t entry_ac3;    // LJSR return; [entry_wsp+6] residue = ac3+4
  int32_t slot4;        // inline wide  -> abstract record slot4 ([wfp+4])
  int32_t slot6;        // inline narrow -> abstract record slot6 ([wfp+6])
  int32_t count;        // display count (drives mv's display-copy loop)
};
struct EstablishOutcome {
  int32_t old_head;     // COMPARED — exit ac0: the pre-push chain head
                        // value. In mv this is the real [wsb-0x40] read;
                        // a conforming implementation computes the same
                        // value deliberately (Contract §7 exit-register
                        // fidelity: the register file is never private).
};

// I.EPILOG — pop the innermost record (Contract §3.2). No outcome
// fields: the exit image is the CALLER's frame via WRTN (common code).
// The mismatched-frame edge is ruling (a): mv keeps the bit-faithful
// tolerant head-write; native asserts + abort_world(save=false)
// (contract THIRD ADDENDUM item 1) — Stage B.

// I.GOTO unwind — cut the chain to the target (Contract §3.3 shape 2).
struct CutOutcome {
  int32_t wsp_restore;  // COMPARED — the landing wsp: the target
                        // record's establishment-time snapshot. mv reads
                        // [target+2]; native reads record.wsp_snapshot.
};

// O.ON — register a handler node (Contract §3.4).
struct RegisterIn {
  int32_t caller_frame; // the establisher (machine.wfp at entry)
  int32_t type;         // raw entry ac0
  int32_t raw_key2;     // raw entry ac1 (helper preamble zeroes for type<=0)
  int32_t handler;      // entry ac2
  int32_t entry_wsp;    // E; the would-be frame is E+12
  int32_t entry_psr;    // for the allocate path's relocated psr wide
  int32_t entry_carry;  // for the relocated ret|c wide
  int32_t entry_ac3;    // LJSR return, re-pushed by the relocation
};
struct RegisterOutcome {
  bool    allocated;    // COMPARED — drives the normative wsp effect:
                        // entry wsp (reuse) / entry wsp + 8 (allocate)
  // MV-RESIDUE (feed the helper-residue image the wrapper lays):
  bool    found;        // helper search hit (ISZTS skip-return select)
  int32_t node;         // result node address (found node / backstop / 0)
  int32_t scratch;      // the helper's TOS scratch (last zero-key node)
};

// O.REVERT — deactivate in place (Contract §3.5).
struct RevertOutcome {
  bool    gate_passed;  // COMPARED — caller was the innermost establisher
                        // (mv: [wsb-0x40] == caller); helper residue is
                        // laid only when true.
  // MV-RESIDUE:
  bool    found;
  int32_t node;
  int32_t scratch;
};

// The raise's handler selection (Contract §3.9; the EE62 select loop).
struct SelectOutcome {
  bool    found;        // COMPARED
  int32_t frame;        // COMPARED — the establisher token (dispatch ac1;
                        // H2: the real frame address, value-pinned in M3b)
  int32_t handler;      // COMPARED — dispatch pc / ac2 (DEF?ON on
                        // exhaustion)
  // MV-RESIDUE (the LAST chain-search invocation, whose image alone
  // survives in the helper-residue slots):
  bool    any_search;
  int32_t last_frame;
  int32_t last_node;
  int32_t last_scratch;
  bool    last_found;
};

// The recorded signal (Contract §2.2 [C+2/4/6]).
struct SigRecord {
  int32_t type;
  int32_t key2;
  int32_t code;
};

// ---------------------------------------------------------------------
// The interface: handler-state decisions and state-cell access ONLY.
// Every method may read machine state; only the mv implementation
// writes real-memory chain/state cells (and its private residue that
// is inseparable from them: frame slots, display words, node cells).
class error_handler_api {
public:
  virtual ~error_handler_api() = default;

  // I.PROLOG: push {frame, snapshot=entry_wsp+4, slot4, slot6, nodes={}}.
  // The +4 wsp reservation itself is the WRAPPER's (normative, H3).
  virtual void establish(hw::Machine& machine, const EstablishIn& in,
                         EstablishOutcome& out) = 0;

  // I.EPILOG: pop the innermost record. caller_wfp is the frame being
  // disestablished (Contract §3.2's cold edge lives here).
  virtual void disestablish(hw::Machine& machine, int32_t caller_wfp) = 0;

  // I.GOTO unwind: pop every record strictly above target_frame; a
  // record AT the target survives with its nodes. entry_wfp is the
  // frame the walk starts from. The MV-stack cut itself (patches,
  // WRTN, snapshot restore) is the wrapper's — M3b-unchanged stack
  // surgery; only the chain bookkeeping happens here.
  virtual void cut(hw::Machine& machine, int32_t target_frame,
                   int32_t entry_wfp, CutOutcome& out) = 0;

  // O.ON: search the caller's record (first key match; LAST inactive
  // node is the reuse backstop); reuse/overwrite or allocate.
  virtual void register_node(hw::Machine& machine, const RegisterIn& in,
                             RegisterOutcome& out) = 0;

  // O.REVERT: no-op unless the caller is the innermost establisher;
  // then deactivate the matching node in place (type := 0).
  virtual void revert_node(hw::Machine& machine, int32_t caller_wfp,
                           int32_t type, int32_t raw_key2,
                           RevertOutcome& out) = 0;

  // The raise's select loop: records innermost-out, per record the
  // node search with the catch-all preamble (key2=0 when type<=0).
  virtual void select(hw::Machine& machine, int32_t type,
                      int32_t raw_key2, SelectOutcome& out) = 0;

  // Contract §2.4 — "true iff a raise of (-1, 0) now would find a
  // handler". Callers must check the walker gate FIRST (the gate is
  // code-space state, common to every implementation).
  virtual bool has_handler(hw::Machine& machine) = 0;

  // O.SET semantics (Contract §3.8): record the signal. In mv this
  // also runs the live walker and lays the walker-output cells; the
  // conforming implementation stores the triple only (ruling b: the
  // walker outputs are DROPPED — Register E7).
  virtual void record_raise(hw::Machine& machine, const SigRecord& rec) = 0;

  // DEF?ON / tail reads of the recorded signal.
  virtual SigRecord sig_record(hw::Machine& machine) = 0;

  // The resume flag (Contract §2.2). WRITTEN ONLY by the O?SIGNAL
  // entry path (H4 — shorthands never write it; staleness is
  // normative). Read as the full wide; the resumable test is its
  // sign bit (bit15 of the narrow at [C+0x16]).
  virtual void set_resume_flag(hw::Machine& machine, int32_t flag) = 0;
  virtual int32_t resume_flag(hw::Machine& machine) = 0;
};

// The selected implementation (per handler_mode). Stage A: always mv.
error_handler_api& error_handler();

} // namespace rt
