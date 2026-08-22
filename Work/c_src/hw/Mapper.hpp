// src/hw/Mapper.hpp — the master↔clone address mapper.
// Implements docs/Mapper.md (DESIGN OF RECORD for the mapping layer,
// first-principles spec) as one module; Project 14 Phase A. Supersedes
// the accreted Machine::T/T_any/T_inv + OSContext::clone_word_address +
// the mediator's inline form decomposition, all deleted with this.
//
// THE OBJECT (Mapper.md §1): a piecewise-affine map A between word-address
// spaces, composed with a stateless encoding codec E. A is defined at any
// instant by the live-record list (one record per live redirected frame)
// plus one derived leg:
//   - AREA legs: each live record maps its block [alloc_base,
//     alloc_base+size] onto the master's stack frame at the same offset
//     from wfp. CLOSED on the right BY PRINCIPLE: a one-past-the-end
//     pointer (WCMV cursor residue, C-style end pointers) is a reference
//     to that object's extent and belongs to the frame it walked off.
//   - the stack COMPRESSION leg: real-stack addresses AT or above a
//     redirected frame's frame word shift by the words the master pushed
//     and the clone did not (10 + 2*frame per live record at or below),
//     DERIVED from the record list, bounded above by wsl (I2: values
//     above wsl are not stack addresses and take the identity). AT — the
//     Finding A ruling: s == W is the record's own anchor (the clone's
//     wsp at that routine's base level) and takes the record's OWN
//     shift, imaging to the master's wsp (master_wfp + 2*frame); same
//     >= threshold shadow_wsp always documented. One walk, two
//     directions — there is no separate inverse implementation.
//   - identity everywhere else (a non-live area address stays as-is: it
//     will diverge loudly, which is right — nothing legitimate holds one).
//
// Q2 RULING (Project 14): A is a bijection on interval INTERIORS; the band
// from a live frame's extent end to its block's closed right end is a
// deliberate forward-only, two-to-one overlay — an end pointer is a
// reference to extent, meaningful for comparison, never a data location.
// The inverse resolves those master addresses to the stack-leg preimage,
// because a dereference targets data and the data at that master cell
// lives at the clone's stack word. I4 asserts the strict round trip on
// interiors and the master-value fixpoint on the overlay; I6 likewise.
// FINDING A EXTENSION (ruled Aug 22 2026): one more two-to-one point per
// live record, at the OPPOSITE edge — master_wfp + 2*frame (the master's
// wsp while the routine runs at base level) is the image of BOTH the
// stack anchor W (a position value) and the area's last-local word (a
// data location). Same resolution by the same principle: the inverse
// goes to the DATA (the area); the stack-leg mapping of u == W asserts
// the fixpoint, not the strict trip.
//
// THE SURFACE (Mapper.md §1.3 + the Q1-a ruling): exactly THREE
// purpose-named calls, so a direction can never be picked by accident:
//   equivalent(master_v, clone_v)  — COMPARISON. Equality verdict;
//                                    clone→master inside; asymmetric by
//                                    definition (a master value that
//                                    coincidentally maps somewhere must
//                                    not rescue a mismatch).
//   frame_precedes(a, b)           — ORDER. Two clone-side frame
//                                    addresses compared in master
//                                    coordinates; the Ruling-A chain
//                                    walks' only legal instrument.
//   clone_location(master_addr)    — DEREFERENCE. master→clone; the ONLY
//                                    address a caller may act on
//                                    (mediation verify-reads and write
//                                    replay). Form-aware (codec applied).
// No direction-flagged public map; no comparison policy outside this
// module. The layout invariant behind end-inclusive attribution is the
// STRIDE: the book tool advances bases by size+16, so block_end <
// next_base and the closed right end is unambiguous (I1; the earlier
// "round exact multiples up another 16" drafting was vacuous and is dead).
#pragma once
#include <cstdint>
#include <vector>
#include "AddressBook.hpp"

namespace hw {

class Machine;

// One live redirected frame (per Machine, LIFO — nests with the stack, I5).
struct LiveRecord {
  BookEntry* entry;
  int32_t    area_wfp;      // == entry->wfp_base
  int32_t    W;             // clone real wsp at the redirected WSAVS (the LCALL frame word)
  int32_t    argc;          // actual argc from the frame word
  int32_t    frame_wides;
  int32_t    master_wfp;    // fwd(W) + 10: where the master's frame pointer sits
  int32_t    shift_after;   // cumulative shift for real-stack addresses above W (words)
};

class Mapper {
public:
  // The encoding codec E (Mapper.md §1.2) — total over the forms that
  // occur; separable by prefix in every form (I3, base 0x74000000):
  //   form    | real stack | area
  //   word    | 0x70       | 0x74
  //   byte    | 0xE0       | 0xE8   (word = v >> 1, low bit preserved)
  //   @-word  | 0xF0       | 0xF4   (bit 31 masked / re-encoded)
  // Anything else decodes to None (identity; probed — see probe()).
  enum class Form : uint8_t { Word, Byte, AtWord, None };
  enum class Kind : uint8_t { RAW, MAPPED, MISMATCH };
  struct Verdict {
    Kind kind;
    Form clone_form;             // decoded form of the clone value (None if unlisted/equal-raw)
    uint32_t clone_word;         // decodings, for divergence dumps
    uint32_t master_word;
    uint32_t mapped;             // encode(form, fwd(clone_word)) — what the clone value maps to
    const LiveRecord* record;    // the record the mapping hit (area leg, or
                                 // stack-leg attribution incl. u == W), or nullptr
  };

  // Launch-time configuration (Mapper.md §3): the Mapper is CONFIGURED,
  // never self-discovering — no file I/O, no env reads, no os/-layer
  // reach-in. book == nullptr on the master and on non-QUEST processes:
  // master-vs-clone is a property of the configuration, not of code paths
  // querying roles. main-task stack bounds back the redirect-time
  // main-task assert (§3 Tasks: the listener's mapper is configured too,
  // and stays empty by construction). The stack-leg bound is the LIVE
  // wsl; I2 latches wsl − heap_break at the first push (the boot-time
  // wsl-steal precedes any redirect) and asserts, while records live,
  // that the difference is unchanged and that wsl clears all stack-leg
  // activity (Finding B ruling — legitimate I?ALLOC/I?FREE motion moves
  // wsl and the break together; anything else is still an abort).
  void configure(Machine* owner, AddressBook* book, bool is_main_task);

  bool redirect_configured() const { return book_ != nullptr; }
  BookEntry* entry_for_pc(uint32_t pc) const { return book_ ? book_->lookup_pc(pc) : nullptr; }
  bool is_area_address(uint32_t v) const { return book_ && book_->in_range(v); }
  bool has_records() const { return !records_.empty(); }
  size_t depth() const { return records_.size(); }

  // ---- the translation surface (three calls; see header comment) ----
  Verdict equivalent(uint32_t master_v, uint32_t clone_v) const;
  uint32_t clone_location(uint32_t master_addr) const;
  bool frame_precedes(uint32_t a, uint32_t b) const;

  // ---- the standing drift value (I4 global check: shadow_wsp == master
  // wsp at every pair; the record whose frame word the clone's wsp still
  // points at counts as "above", hence the ≥ threshold) ----
  int32_t shadow_wsp(int32_t clone_wsp) const;

  // ---- the three mutation kinds (Mapper.md §3), all clone-role, all
  // traced (`redirect`): redirected WSAVS/WSAVR pushes; redirected WRTN
  // (incl. frames.cpp::wrtn, the I.EPILOG path) pops the innermost;
  // unwind cut (I.GOTO / area_unwind_to) pops a suffix. Process death
  // (?FATAL/?RETURN/ABORT) discards the world; no mapping survives it.
  const LiveRecord& push_record(Machine& m, BookEntry* e, uint32_t entry_pc,
                                int32_t W, int32_t argc, int32_t frame_wides);
  void wrtn_fixup(Machine& m, int32_t pre_wfp);
  void unwind_to(Machine& m, int32_t target_wfp);

private:
  enum class Dir { ToMaster, ToClone };
  static Form decode(uint32_t v, uint32_t& word, uint32_t& low_bit);
  static uint32_t encode(Form f, uint32_t word, uint32_t low_bit);
  // A itself: one walk, direction-flagged, PRIVATE (never public).
  uint32_t map_word(uint32_t u, Dir dir, const LiveRecord** rec = nullptr) const;
  // map_word + the I4 round-trip assert (strict on interiors, fixpoint on
  // the closed-end overlay band per the Q2 ruling).
  uint32_t map_checked(uint32_t u, Dir dir, const LiveRecord** rec = nullptr) const;
  // §1.2 unlisted-form probe: decode failed, but a shifted/masked reading
  // of the raw value lands in a mapped range — "unlisted form seen" is a
  // finding (boundary 2), never a silent identity. Returns true if fired.
  bool probe(uint32_t v) const;
  // I2, Finding B ruling (M4aDesign §12): wsl is dual-purpose (stack
  // limit AND stack/heap fence) and legitimately moves at I?ALLOC/I?FREE
  // commit points — always together with the heap break, by the same
  // size. So the assert is (a) wsl − heap_break unchanged since the
  // latch (a slip = wsl motion with no matching break motion), plus
  // (b) stack clearance: wsl strictly above all live stack-leg activity,
  // so the reclassified band [new_wsl, old_wsl) never overlaps the leg.
  void i2_assert(Machine& m) const;
  // The stack-leg domain bound: the LIVE wsl (owner's current fence).
  // Pre-Finding-B this was the latched wsl; under the diff-invariant the
  // live value is the machine's own record of the stack/heap boundary,
  // and addresses above it (e.g. the fo message buffer) are heap, not
  // stack — they must take the identity.
  int32_t stack_bound() const;

  Machine*     owner_ = nullptr;      // abort context + live wsl (stack_bound)
  AddressBook* book_ = nullptr;
  bool main_task_ = false;                    // set at configure; asserted at every push
  int32_t latched_diff_ = 0;                  // I2: wsl − heap_break, latched at first push
  std::vector<LiveRecord> records_;
};

} // namespace hw
