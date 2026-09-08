// src/hw/RTBridge.hpp
//
// Calling-convention bridge for native runtime functions, re-derived from
// EagleStack (LCALL/XPEF/WRTN) and confirmed empirically (stack captures at
// ?FILL_WORDS entries — see SessionPlan.md task B).
//
// State at native dispatch (LCALL/XCALL executed, WSAVS has NOT run):
//   [wsp, wsp+1]    wide (psr<<16)|argcount   (LCALL's push)
//   [wsp-2N ...]    wide arg-N pointer         (args pushed in reverse; PL/1
//                                               passes by reference, so each
//                                               slot holds the argument's
//                                               address)
//   ac3             return address
//
// native_return() replicates WRTN's post-state for the no-frame path:
// psr restored from the frame word, args popped (callee-pops: wsp -=
// 2*argcount + the frame wide), ac0-2 and carry restored to entry values
// (WRTN restores the caller's, which on this path are the entry values),
// ac3 = wfp, and execution resumes at the entry return address. It also
// requests a batch break (Machine::native_break) so the lockstep pair
// rendezvouses at the post-call point.
#pragma once
#include <cstdint>

namespace hw {

class Machine;

class RTBridge {
public:
  RTBridge(Machine& machine);

  int32_t arg_count() const { return argc; }
  uint32_t arg_pointer(int n) const;   // slot content: the argument's address
  int32_t arg_word(int n) const;       // narrow value through the pointer (sign-extended)
  int32_t arg_wide(int n) const;       // wide value through the pointer
  void set_arg_word(int n, int32_t value);   // store through the pointer
  void set_arg_wide(int n, int32_t value);

  // Register arguments (the third "sick" pattern): some routines read
  // arguments from the CALLER'S registers via the WSAVS-saved slots
  // (e.g. ?UNSIGNED_TO_CHAR takes the destination string address in the
  // caller's ac2, read at [ac3+0x7FFC]). entry_ac exposes the captured
  // entry values; call before set_return_ac (which overwrites them).
  int32_t entry_ac(int n) const { return saved_ac[n]; }
  int32_t entry_carry() const { return saved_c; }

  // Register returns: some routines return values by patching the
  // WSAVS-saved slots so WRTN restores modified registers (e.g. ?UDIV32
  // stores the quotient into the saved-ac0 slot). set_return_ac replaces
  // the value native_return restores AND the value emulate_frame writes
  // into the slot — call it BEFORE emulate_frame.
  void set_return_ac(int n, int32_t value);

  // Dead-stack residue fidelity: the emulated body writes its WSAVS frame
  // (five saved-register wides) and locals into stack memory that WRTN
  // abandons but does not erase; some callers read such residue (e.g.
  // ?WRITE_SCREEN loads never-written locals [4..5] into ac0 for SYSCALL
  // 0303). Native translations must leave a bit-identical footprint:
  // emulate_frame() writes the exact WSAVS image and returns the frame
  // base (the would-be ac3) so the translation can replicate its local
  // writes on top.
  uint32_t emulate_frame();
  void write_frame_word(uint32_t frame_base, int32_t offset, int32_t value);   // narrow local write
  void write_frame_wide(uint32_t frame_base, int32_t offset, int32_t value);   // wide local write
  void write_frame_byte(uint32_t frame_base, int32_t byte_offset, int32_t value);   // byte local write (byte addr = frame_base*2 + byte_offset)

  // Unwind and resume at the caller. Returns the pc for the dispatch site
  // to return (the entry return address).
  uint32_t native_return();

  // Transfer pairing (docs/SharedProtocol.md): end this native routine by
  // transferring control to an arbitrary emulated address — handler
  // dispatch, unwind resume, or an LJSR continuation — instead of
  // returning to the caller. The wrapper is responsible for ALL register
  // and stack state at the target (there is no single convention to
  // replicate; each transfer site defines its own). This helper only ends
  // the batch so the pair rendezvouses at the target: the master's
  // run-to-return terminates when its emulated body's pc leaves the RT
  // range, which is the same architectural point.
  static uint32_t native_transfer(Machine& machine, uint32_t target_pc);

  // ---- SS convention (LJSR + WSSVS/WSSVR routines, e.g. O.ON) ----
  // LJSR sets ac3=pc+3 but pushes NO frame word and does NOT clear ovr;
  // the callee's WSSVS/WSSVR pushes a SIX-wide image (psr<<16 FIRST,
  // then ac0, ac1, ac2, wfp, ac3|c<<31) whose psr wide has argc=0, so
  // WRTN pops no arguments. Construct with RTBridge(machine, SS) at
  // dispatch; arg accessors are meaningless in this mode.
  enum Convention { LCALL_FRAME, SS };
  RTBridge(Machine& machine, Convention convention);

  // Writes the exact WSSVS/WSSVR image at [wsp+2 .. wsp+13] (residue;
  // wsp is not moved) and returns the would-be frame pointer
  // (entry wsp + 12). ovk_bit: 1 for WSSVS bodies, 0 for WSSVR — the
  // PUSHED psr is the entry psr; ovk_bit only documents which variant
  // ran (it does not alter the image).
  uint32_t emulate_frame_ss();

  // WRTN replica for SS frames: psr/ac0-2/carry restored to entry
  // values, ac3 = restored wfp, wsp set to final_wsp (normally the
  // entry wsp; O.ON's allocate path returns entry wsp + 8 because the
  // new chain node persists on the stack). Pops the shadow call-stack
  // frame and requests the batch break.
  uint32_t native_return_ss(int32_t final_wsp);

  int32_t entry_wsp() const { return saved_wsp; }
  uint32_t entry_return() const { return return_addr; }   // the LCALL/XCALL return address captured at dispatch
  int32_t entry_psr() const { return saved_psr; }

  // ---- Native syscall (HeapSignalPlan.md design, approved Aug 2026) ----
  // Mirrors OSTask::dispatch_system_call steps 2-3 ONLY: context
  // construction + AC copy-in, then mediator-routed dispatch (the mediator
  // is pc-agnostic — verified — and routing through it is mandatory: it is
  // the master/clone rendezvous and the clone-output comparison point).
  // The emulated-trap plumbing is deliberately absent: no [wsp-2]
  // call-number read, no psr wide pop, no ret/ret+1 skip-return — the
  // native caller consumes the returned error code instead.
  //
  // entry_pc: the calling native routine's entry address, substituted for
  // the trap address in the "System Call %o, called from %08X" log line
  // (logging parity with the emulated path; the dispatchSystemCall/<wsp>
  // line is dropped — meaningless without a trap frame).
  //
  // Error semantics — NOT structurally enforced: a returned error code
  // (vs a throw) is a PER-HANDLER property. OSContext handlers catch
  // OSError internally (OSContextFS.cpp:84, OSContextIPC.cpp:51, ...)
  // and convert to codes; OSContext::dispatch_system_call itself has no
  // catch and the base case is a pure throw ("Dispatch system call -
  // missing case", "Unimplemented system call <octal>"). MEMI/MEM and
  // the FS calls return codes; anything unimplemented throws.
  //
  // Blocking is likewise a per-handler, per-STATE property (REC blocks
  // only on an empty mailbox with count_tasks() > 1); only REC guards it
  // today. INTWT / WDELAY / RETURN block unguarded — see
  // UNIMPLEMENTED.md. Non-blocking calls only until that changes.
  //
  // Returns the OS error code (os::OSError::SUCCESS = 0); ac0-2 updated
  // in place on success, untouched on error (the emulated path's
  // "ac0 = error" belongs to the skip-return tail the caller owns).
  int32_t syscall(int32_t call, uint32_t entry_pc,
                  int32_t& ac0, int32_t& ac1, int32_t& ac2);

private:
  Machine& machine;
  uint32_t return_addr;
  uint32_t frame_word;   // (psr<<16)|argcount (LCALL_FRAME mode only)
  int32_t argc;
  int32_t saved_ac[3];
  int32_t saved_c;
  int32_t saved_wsp;     // SS mode: entry wsp
  int32_t saved_psr;     // SS mode: entry psr

  // Ruling 8 clone-side symmetry (docs/Project10): zero the frame
  // area the entry's WSAVx/WSSVx would have claimed. fp = the
  // would-be frame pointer emulate_frame*/ just computed.
  void zero_frame_claim(uint32_t fp);
};

} // namespace hw
