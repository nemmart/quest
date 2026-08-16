#ifndef OS_PROBE_SUPPRESSIONS_HPP
#define OS_PROBE_SUPPRESSIONS_HPP

// Project 11 — the probe suppression list (docs/Project11/PROMPT.md,
// Layering.md ruling 8). Consulted ONLY in probe mode (-zero=clone,
// Lockstep::probe_relax_regs); shipping (-zero=both) and attic
// (-zero=none) semantics are untouched by construction — the sole
// caller is LockstepMediator::verify_arrival, and every path in it is
// behind the probe flag.
//
// Semantics (user amendment, collect-don't-halt): in probe mode a
// register-VALUE mismatch at the mediation compare never halts. The
// master stays authoritative — its garbage-authentic arguments
// execute; for MEDIATED calls the clone consumes the master's results
// (keeping the world 1988-true and both engines on the same downstream
// branch). Known specimens log one line; new specimens emit a FULL
// FORENSIC RECORD (stderr + probe_specimens.log in the host cwd) and
// continue. Halts remain for the control-flow-disagreement family
// only: call-number or site-pc mismatch here, pc forks and count skews
// in Lockstep::compare_pair (untouched).
//
// Key: (syscall number, site pc, register mask). Site pc = the
// game-side pc of the syscall issue site, address-2 from
// OSTask::dispatch_system_call — the same pc "System Call %o, called
// from %08X" logs. SITE-specific by design; never a global register
// exemption.

#include <cstdint>

namespace hw { class Machine; }

namespace os {

class OSTask;

struct ProbeSuppression {
  int32_t  call;      // syscall number (octal in comments)
  uint32_t site_pc;   // game-side issue site
  uint32_t reg_mask;  // bit0=ac0, bit1=ac1, bit2=ac2
  const char* note;   // dated, names its specimen
  bool     silent;    // SILENT tier (user, Aug 14): proven don't-care —
                      // no per-hit output at all; counted for the
                      // shutdown summary. false = NOTE tier (one-liner),
                      // for specimens still under observation.
                      // Promotion NOTE→SILENT is a dated table edit.
};

class ProbeSuppressions {
public:
  // Table lookup: an entry admits a mismatch iff call and site match
  // and the mismatching-register set is a SUBSET of the entry's mask.
  static const ProbeSuppression* find(int32_t call, uint32_t site_pc,
                                      uint32_t mismatch_mask);
  // Shutdown summary: one line per SILENT specimen with hits +
  // distinct ac0-value count (accounting survives the silence).
  static void summarize();

  // One-line log for a known specimen.
  static void log_known(const ProbeSuppression* entry, int32_t call,
                        uint32_t site_pc,
                        const int32_t* master_ac, const int32_t* clone_ac);

  // Full forensic record for a NEW specimen (stderr + specimens file),
  // sufficient to author the catalogue entry + suppression without
  // reproducing the run. local_call: LOCAL-class calls lack the
  // master-authority guarantee — flagged loudly for the reviewer.
  // recent_sites: ring of the last verified rendezvous site pcs.
  // PACKET-CONTENT tier (higher-signal than register cargo): a mediated
  // handler READ saw differing caller memory. In probe mode this is
  // flagged loudly (full block, stderr + specimens file) and the run
  // continues — NEVER silently admitted to the suppressible register
  // class; reviewer classifies. Outside probe mode verify_read still
  // aborts, untouched.
  static void packet_content_flag(uint32_t byte_lo, uint32_t byte_hi,
                                  uint32_t width, uint64_t master_value,
                                  uint64_t clone_value, hw::Machine* master);

  static void forensic_record(int32_t call, uint32_t master_site,
                              OSTask* master_task, OSTask* clone_task,
                              const int32_t* master_ac, const int32_t* clone_ac,
                              uint32_t mismatch_mask, bool local_call,
                              const uint32_t* recent_sites, int recent_count);
};

} // namespace os

#endif
