#pragma once
#include <cstdint>
#include <string>
#include <set>
#include <atomic>
#include "Memory.hpp"
#include "Mapper.hpp"
#include "Segment.hpp"


namespace debug { class CallStack; class SymbolTable; }
namespace os { class OSProcess; class OSTask; }

namespace hw {
using namespace debug;
using namespace os;

class MachineThread;

class Machine {
public:
  static constexpr int32_t OVK_FLAG = 0x01;
  static constexpr int32_t OVR_FLAG = 0x02;

  // Stack-claim zeroing (docs/Layering.md ruling 8, the fourth
  // deliberate infidelity; Project 10). Launch parses -zero= into
  // zero_mode; each machine's zero_claims gate is set from it (BOTH →
  // every machine; NONE → none; CLONE → only lockstep clones, wired at
  // task registration — the user's asymmetric garbage probe).
  static constexpr int32_t ZERO_BOTH  = 0;
  static constexpr int32_t ZERO_NONE  = 1;
  static constexpr int32_t ZERO_CLONE = 2;
  static int32_t zero_mode;

  static MachineThread* machine_thread;

  bool debug;

  OSProcess*  process;
  OSTask*     task;
  SymbolTable* symbols;
  Memory*     memory;      // not owned - shared across machines
  CallStack*  call_stack;
  Segment*    segments[8];
  int32_t     pc;
  int32_t     ac[4];
  double      fpac[4];
  int64_t     quads[4];
  double      fplr;
  int32_t     c;
  int32_t     ovk, ovr, ires, ixct, ffp, sr;
  int32_t     fpr;
  int32_t     wsb, wsl, wsp, wfp;
  uint64_t    instruction_count;
  std::atomic<bool>* halt_ptr;   // set by OSTask to &task->halt
  int32_t     lockstep_role;     // hw::Lockstep role (set at task registration)
  int32_t     lockstep_ordinal;  // per-process task creation ordinal
  uint8_t*    rtcov;             // RT-range coverage bitmap (RTStubs), or nullptr
  bool        native_break;      // set by RTBridge::native_return: end batch at post-call point
  bool        native_span;       // batch contained a native call (or master's matching emulated span)
  uint32_t    rt_pending_return; // master: run-to-return target after entering a translated routine (0 = none)
  uint32_t    (*pending_native)(Machine&); // clone: native L2 implementation whose dispatch was
                                 // DEFERRED so the batch could break AT the entry pc — the
                                 // L1→L2 crossing rendezvous of the crossings-only checker
                                 // (docs/CrossingsChecker.md). run_steps runs it first thing
                                 // on resume, in place of fetch+decode at the entry.
  bool        terminal_reached;  // batch ended at a terminal entry (RTStubs::terminal_bits
                                 // or QUEST_TERMINAL test pc): last verified pair, then detach
  bool        zero_claims;       // ruling 8: zero stack words exposed by wsp claims
                                 // (EagleStack claim sites + RTBridge::emulate_frame*)

  // ---- M4a mapping layer (docs/Mapper.md; Project 14) ----
  // The Machine OWNS the Mapper instance (configured by launch code at
  // OSTask construction) and forwards — os/-layer code calls these
  // forwarders, never the Mapper itself. The translation surface is the
  // three purpose-named calls; there is no direction-flagged public map.
  Mapper mapper;
  Mapper::Verdict equivalent(uint32_t master_v, uint32_t clone_v) const {
    return mapper.equivalent(master_v, clone_v);
  }
  uint32_t clone_location(uint32_t master_addr) const {
    return mapper.clone_location(master_addr);
  }
  bool frame_precedes(uint32_t a, uint32_t b) const {   // Ruling-A chain walks only
    return mapper.frame_precedes(a, b);
  }
  int32_t shadow_wsp() const { return mapper.shadow_wsp(wsp); }
  bool mapping_active() const { return mapper.has_records(); }
  size_t mapping_depth() const { return mapper.depth(); }
  // WRTN fixup: after the stock pop sequence ran against an AREA frame
  // (pre_wfp in the book range), set wsp to the real-stack value the
  // master's WRTN would have produced, drop the live record. No-op if
  // pre_wfp is not an area address. Every WRTN replica must call it.
  void area_wrtn_fixup(int32_t pre_wfp) { mapper.wrtn_fixup(*this, pre_wfp); }
  // Unwind (I.GOTO cut): drop every live record above the target frame
  // (target may be an area wfp or a real-stack wfp).
  void area_unwind_to(int32_t target_wfp) { mapper.unwind_to(*this, target_wfp); }

  // Segment helpers (static)
  static uint32_t get_segment(uint32_t address) { return (address>>28)&0x07; }
  static uint32_t set_segment(uint32_t segment, uint32_t address) { return (address&0x0FFFFFFF)|(segment<<28); }
  static uint32_t copy_segment(uint32_t sa, uint32_t a) { return (a&0x0FFFFFFF)|((sa>>28)<<28); }
  static uint32_t get_byte_segment(uint32_t address) { return (address>>29)&0x07; }
  static uint32_t set_byte_segment(uint32_t segment, uint32_t address) { return (address&0x1FFFFFFF)|(segment<<29); }
  static uint32_t copy_byte_segment(uint32_t sa, uint32_t a) { return (a&0x1FFFFFFF)|((sa>>29)<<29); }

  // Constructors
  Machine(OSProcess* process, OSTask* task, SymbolTable* symbols, Memory* memory);
  ~Machine();

  void copy_state(Machine& current);
  void add_debug(const std::string& name);

  // Execution
  uint32_t run_steps(uint32_t address, int32_t count);
  uint32_t run(uint32_t address);

  // PSR
  int32_t get_psr();
  void set_psr(int32_t psr);

  // Stack operations
  void wide_push(int32_t wide);
  int32_t wide_pop();
  void quad_push(int64_t quad);
  int64_t quad_pop();

  // Addressing helpers
  uint32_t eagle_x_byte_indexed(uint32_t pc, uint32_t ii);
  uint32_t eagle_l_byte_indexed(uint32_t pc, uint32_t ii);
  uint32_t eagle_x_resolve_indirect(uint32_t pc, uint32_t ii);
  uint32_t eagle_l_resolve_indirect(uint32_t pc, uint32_t ii);
  uint32_t eagle_resolve_indirect(uint32_t address);

  // Debug
  void dump_stack_area(uint32_t address, int32_t size);
  void backtrace();
};

} // namespace hw
