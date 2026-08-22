// src/hw/Mapper.cpp — see Mapper.hpp / docs/Mapper.md.
#include "Mapper.hpp"
#include "Machine.hpp"
#include "Lockstep.hpp"
#include "Memory.hpp"
#include "../os/OSProcess.hpp"
#include "../os/Trace.hpp"
#include "../runtime/i_alloc.hpp"   // rt::HEAP_BREAK (I2 diff latch; hw→runtime precedent: RTStubs)
#include <atomic>
#include <cstdio>
#include <stdexcept>

namespace hw {

// I3 — prefix separability, the real content of the base ruling: the
// encodings of area addresses must be separable from every real-stack
// form BY PREFIX in every form. A constraint on the base (0x74000000
// satisfies it; 0x78000000 did not — area byte = 0xF0 = real @-word).
// Any future base change re-proves this table at compile time.
static constexpr uint32_t P_RW = 0x70000000u >> 24;                      // real word
static constexpr uint32_t P_AW = AddressBook::BASE >> 24;                // area word
static constexpr uint32_t P_RB = (0x70000000u << 1) >> 24;               // real byte
static constexpr uint32_t P_AB = (AddressBook::BASE << 1) >> 24;         // area byte
static constexpr uint32_t P_RA = (0x70000000u | 0x80000000u) >> 24;      // real @-word
static constexpr uint32_t P_AA = (AddressBook::BASE | 0x80000000u) >> 24;// area @-word
static_assert(P_AW != P_RW && P_AW != P_RB && P_AW != P_RA &&
              P_AB != P_RW && P_AB != P_RB && P_AB != P_RA &&
              P_AA != P_RW && P_AA != P_RB && P_AA != P_RA,
              "I3: area prefixes not separable from real-stack prefixes");
static_assert(P_RW == 0x70 && P_AW == 0x74 && P_RB == 0xE0 &&
              P_AB == 0xE8 && P_RA == 0xF0 && P_AA == 0xF4,
              "codec table in decode() no longer matches the base");

static std::atomic<uint64_t> probe_fires{0};

[[noreturn]] static void mapper_abort(Machine* m, const char* buf) {
  if(Lockstep::enabled)
    Lockstep::abort_world(buf, m, /*save=*/false);
  throw std::runtime_error(buf);
}

void Mapper::configure(Machine* owner, AddressBook* book, bool is_main_task) {
  owner_ = owner;
  book_ = book;
  main_task_ = is_main_task;
  if(!book_)
    return;
  // I1 — disjointness INCLUDING the closed right ends: for consecutive
  // areas alloc_end(k) < alloc_base(k+1). A layout obligation on the book
  // tool (the stride rule, size+16, is its implementation); re-asserted
  // here so a hand-edited book cannot silently violate it.
  // Extent-fits-block (R-C ruling, Project 14): the occupied span
  // [alloc_base, wfp_base + 2 + 2*frame) must sit inside the block — the
  // finding this assert exists for was a tool sizing drift that let the
  // (2*argc+12+2*frame) % 16 == 0 class write two words past its block.
  const BookEntry* prev = nullptr;
  for(const BookEntry& e : book_->entries) {
    if(prev && !(prev->alloc_base + prev->size_words < e.alloc_base)) {
      char buf[160];
      snprintf(buf, sizeof(buf), "MAPPER I1: blocks not strictly disjoint at closed ends: %s end %08X vs %s base %08X",
               prev->name.c_str(), prev->alloc_base + prev->size_words, e.name.c_str(), e.alloc_base);
      mapper_abort(owner_, buf);
    }
    if((e.wfp_base - e.alloc_base) + 2 + 2 * static_cast<uint32_t>(e.frame_wides) > e.size_words) {
      char buf[160];
      snprintf(buf, sizeof(buf), "MAPPER: frame extent exceeds book block for %s (wfp off %u + 2 + %d > size %u)",
               e.name.c_str(), e.wfp_base - e.alloc_base, 2 * e.frame_wides, e.size_words);
      mapper_abort(owner_, buf);
    }
    prev = &e;
  }
}

// ---- the codec E (stateless) ----

Mapper::Form Mapper::decode(uint32_t v, uint32_t& word, uint32_t& low_bit) {
  word = 0; low_bit = 0;
  switch(v >> 24) {
    case 0x70: case 0x74:            // word address (real / area)
      word = v; return Form::Word;
    case 0xE0: case 0xE8:            // byte address (real / area)
      word = v >> 1; low_bit = v & 1u; return Form::Byte;
    case 0xF0: case 0xF4:            // @-flagged word address (real / area)
      word = v & 0x7FFFFFFFu; return Form::AtWord;
    default:
      return Form::None;
  }
}

uint32_t Mapper::encode(Form f, uint32_t word, uint32_t low_bit) {
  switch(f) {
    case Form::Word:   return word;
    case Form::Byte:   return (word << 1) | low_bit;
    case Form::AtWord: return word | 0x80000000u;
    default:           return word;
  }
}

// ---- A: the piecewise-affine map, one walk, direction-flagged ----

uint32_t Mapper::map_word(uint32_t u, Dir dir, const LiveRecord** rec) const {
  if(rec) *rec = nullptr;
  if(records_.empty())
    return u;
  int32_t s = static_cast<int32_t>(u);
  if(dir == Dir::ToMaster) {
    if(book_ && book_->in_range(u)) {
      // Area legs, innermost-first; end-INCLUSIVE containment (ruling):
      // a one-past-the-end pointer belongs to the frame it walked off.
      // Unambiguous because I1 guarantees block_end < next_base (stride).
      for(auto it = records_.rbegin(); it != records_.rend(); ++it) {
        const BookEntry* e = it->entry;
        if(u >= e->alloc_base && u <= e->alloc_base + e->size_words) {
          if(rec) *rec = &*it;
          return static_cast<uint32_t>(it->master_wfp + (s - it->area_wfp));
        }
      }
      return u;   // non-live area address: loud identity
    }
    // The stack compression leg (signed, like every consumer of these
    // addresses), bounded above by the LIVE wsl (I2, Finding B ruling:
    // wsl is the stack/heap fence and legitimately moves at heap commit
    // points; i2_assert guarantees its motion is only ever that, and
    // its clearance check keeps the leg strictly below it. Addresses
    // above wsl — e.g. the fail-open message buffer — are heap and take
    // the identity).
    //
    // >= — the Finding A ruling (docs/Project14/FINDING_A_MAPPER_FIX.md,
    // corrected geometry in REPORT_FINDING_A_FIX.md): s == W is the
    // record's OWN anchor — the clone's wsp while that routine runs at
    // its base level (LDASP puts it in registers; dyn routines' MSP
    // cursors hold it). Its image is the MASTER's wsp there,
    // master_wfp + 2*frame = s + this record's own shift_after. The old
    // strict > attributed it one frame out, applying the outer shift
    // and landing INSIDE this frame's master band (the B2 I4 abort).
    // This is the same threshold shadow_wsp always used ("the record
    // whose frame word the clone's wsp still points at counts as
    // above") — the two legs now agree. It also subsumes Mapper.md
    // §3b's record-order-for-ties: with W ties (zero-arg protocol,
    // future), the innermost-first walk hits the innermost tied record,
    // whose cumulative shift_after is the sum over the tie.
    // The image s == W -> master_wfp + 2*frame is a two-to-one MERGE
    // POINT (its other preimage is the area's last-local word) —
    // resolved Q2-style in map_checked/I6, fixpoint not strict trip.
    if(s > stack_bound())
      return u;
    for(auto it = records_.rbegin(); it != records_.rend(); ++it)
      if(s >= it->W) {
        if(rec) *rec = &*it;
        return static_cast<uint32_t>(s + it->shift_after);
      }
    return u;
  }
  // Dir::ToClone — the same structure walked the other way.
  if(s > stack_bound())   // I2 bound (live), master side (master wsl == clone wsl)
    return u;
  for(auto it = records_.rbegin(); it != records_.rend(); ++it) {
    int32_t lo = it->master_wfp - 10 - 2 * it->argc;                // args start
    int32_t hi = it->master_wfp + 2 + 2 * it->frame_wides;          // frame extent end
    // hi EXCLUSIVE (Q2 ruling): the master address at the extent end is
    // the clone's first real-stack word above the redirect — a
    // dereference targets data, and the data lives on the stack leg.
    if(s >= lo && s < hi) {
      if(rec) *rec = &*it;
      return static_cast<uint32_t>(it->area_wfp + (s - it->master_wfp));
    }
    if(s >= hi)
      return static_cast<uint32_t>(s - it->shift_after);
  }
  return u;
}

uint32_t Mapper::map_checked(uint32_t u, Dir dir, const LiveRecord** rec) const {
  const LiveRecord* r = nullptr;
  uint32_t v = map_word(u, dir, &r);
  if(rec) *rec = r;
  if(v != u) {
    // I4 — round trip on every non-identity mapping. Q2 ruling: strict on
    // interval interiors; on the forward-only overlay band (from the
    // frame's extent end to the block's closed right end) the inverse
    // resolves to the stack-leg preimage, so the assert is the
    // master-value fixpoint instead.
    Dir back_dir = (dir == Dir::ToMaster) ? Dir::ToClone : Dir::ToMaster;
    // Two-to-one bands, each ruled forward-only with the fixpoint form of
    // I4 (Q2 ruling + its Finding A extension). Scoped PRECISELY so every
    // ordinary mapping keeps the strict round trip:
    //   area overlay  — an AREA-leg image at/past the extent end (the end
    //                   pointer band; inverse resolves to the stack leg);
    //   stack merge   — the STACK-leg image of the record's own anchor
    //                   u == W (the wsp position value; its master image
    //                   master_wfp + 2*frame is also the area's last-local
    //                   word, and the inverse resolves to the AREA, where
    //                   a dereference finds the live data).
    bool from_area = r && book_ && book_->in_range(u);
    bool area_overlay = (dir == Dir::ToMaster) && from_area &&
                        static_cast<int32_t>(v) >= r->master_wfp + 2 + 2 * r->frame_wides;
    bool stack_merge = (dir == Dir::ToMaster) && r && !from_area &&
                       static_cast<int32_t>(u) == r->W;
    bool overlay = area_overlay || stack_merge;
    uint32_t back = map_word(v, back_dir);
    if(overlay) {
      // Post-R-C, no observed producer lands ON the closed right end
      // (the known WCMV residue sits interior); the x%16==0 class is the
      // only one whose residue coincides with it. If a specimen ever
      // arrives, that is the first real exercise of end-inclusive
      // attribution — say so once, for the session that finds it.
      if(u == r->entry->alloc_base + r->entry->size_words) {
        static std::atomic<bool> noted{false};
        if(!noted.exchange(true))
          fprintf(stderr, "MAPPER: first closed-end residue mapping exercised (%s, %08X)\n",
                  r->entry->name.c_str(), u);
      }
      uint32_t again = map_word(back, Dir::ToMaster);
      if(again != v) {
        char buf[160];
        snprintf(buf, sizeof(buf), "MAPPER I4: overlay fixpoint failed for %08X: fwd=%08X inv=%08X fwd=%08X",
                 u, v, back, again);
        mapper_abort(owner_, buf);
      }
    } else if(back != u) {
      char buf[160];
      snprintf(buf, sizeof(buf), "MAPPER I4: round trip failed for %08X (%s): mapped=%08X back=%08X",
               u, dir == Dir::ToMaster ? "fwd" : "inv", v, back);
      mapper_abort(owner_, buf);
    }
  }
  return v;
}

// ---- the probe (Mapper.md §1.2) ----

bool Mapper::probe(uint32_t v) const {
  if(records_.empty())
    return false;
  const uint32_t readings[3] = { v >> 1, v & 0x7FFFFFFFu, (v & 0x7FFFFFFFu) >> 1 };
  for(uint32_t w : readings) {
    bool hit = false;
    for(auto it = records_.rbegin(); it != records_.rend() && !hit; ++it) {
      const BookEntry* e = it->entry;
      hit = (w >= e->alloc_base && w <= e->alloc_base + e->size_words);
    }
    if(!hit && !records_.empty()) {
      int32_t s = static_cast<int32_t>(w);
      hit = s > records_.front().W && s <= stack_bound();
    }
    if(hit) {
      probe_fires.fetch_add(1);
      fprintf(stderr, "MAPPER PROBE: unlisted pointer form %08X (reading %08X lands in a mapped range)\n", v, w);
      return true;
    }
  }
  return false;
}

// ---- the surface ----

Mapper::Verdict Mapper::equivalent(uint32_t master_v, uint32_t clone_v) const {
  Verdict vd;
  vd.clone_form = Form::None;
  vd.clone_word = 0; vd.master_word = 0;
  vd.mapped = clone_v;
  vd.record = nullptr;
  if(clone_v == master_v) {
    vd.kind = Kind::RAW;
    return vd;
  }
  uint32_t cw = 0, cl = 0, mw = 0, ml = 0;
  Form f = decode(clone_v, cw, cl);
  (void)decode(master_v, mw, ml);
  vd.clone_form = f; vd.clone_word = cw; vd.master_word = mw;
  if(f == Form::None) {
    vd.kind = Kind::MISMATCH;
    probe(clone_v);
    return vd;
  }
  const LiveRecord* r = nullptr;
  uint32_t mapped_w = map_checked(cw, Dir::ToMaster, &r);
  vd.mapped = encode(f, mapped_w, cl);
  vd.record = r;
  if(vd.mapped != master_v) {
    vd.kind = Kind::MISMATCH;
    return vd;
  }
  vd.kind = Kind::MAPPED;
  // I6 — the inverse agrees with the clone's own data: clone_location of
  // a verified pointer field equals the clone's field value (bijectivity
  // made a tested invariant). On the overlay band the agreement is on the
  // master value (Q2 ruling), matching I4's fixpoint form.
  uint32_t back = map_word(mapped_w, Dir::ToClone);
  // Overlay scoping mirrors map_checked exactly (area closed-end band /
  // Finding A stack merge point); everything else keeps strict agreement.
  bool from_area = r && book_ && book_->in_range(cw);
  bool overlay = (from_area &&
                  static_cast<int32_t>(mapped_w) >= r->master_wfp + 2 + 2 * r->frame_wides) ||
                 (r && !from_area && static_cast<int32_t>(cw) == r->W);
  if(overlay ? (map_word(back, Dir::ToMaster) != mapped_w) : (back != cw)) {
    char buf[160];
    snprintf(buf, sizeof(buf), "MAPPER I6: inverse disagrees with the clone's value: master_w=%08X inv=%08X clone_w=%08X",
             mapped_w, back, cw);
    mapper_abort(owner_, buf);
  }
  return vd;
}

uint32_t Mapper::clone_location(uint32_t master_addr) const {
  if(records_.empty())
    return master_addr;
  uint32_t w = 0, l = 0;
  Form f = decode(master_addr, w, l);
  if(f == Form::None) {
    probe(master_addr);
    return master_addr;
  }
  return encode(f, map_checked(w, Dir::ToClone), l);
}

bool Mapper::frame_precedes(uint32_t a, uint32_t b) const {
  // Ruling A (Project 12): every comparison the master makes in stack
  // coordinates the clone makes in master coordinates — operands only,
  // logic untouched. Signed, like the walks this serves.
  return static_cast<int32_t>(map_checked(a, Dir::ToMaster)) <
         static_cast<int32_t>(map_checked(b, Dir::ToMaster));
}

int32_t Mapper::shadow_wsp(int32_t clone_wsp) const {
  for(auto it = records_.rbegin(); it != records_.rend(); ++it)
    if(clone_wsp >= it->W)
      return clone_wsp + it->shift_after;
  return clone_wsp;
}

// ---- the three mutations ----

int32_t Mapper::stack_bound() const {
  return owner_->wsl & 0x7FFFFFFF;
}

void Mapper::i2_assert(Machine& m) const {
  if(records_.empty())
    return;
  // (a) The fence invariant. Legitimate heap motion (I?ALLOC's STASL
  // 0x7017E903 / I?FREE's 0x7017E9D2-D5, native i_alloc.cpp) moves wsl
  // and the heap break by the same size, same direction, at the same
  // commit — the difference is constant across it. A wsl write with no
  // matching break motion (e.g. a botched handler re-latch — everything
  // the old wsl-constancy form was written for) still changes the
  // difference and still aborts here.
  int32_t wsl_now  = m.wsl & 0x7FFFFFFF;
  int32_t brk_now  = static_cast<int32_t>(m.memory->read_wide(rt::HEAP_BREAK)) & 0x7FFFFFFF;
  int32_t diff_now = wsl_now - brk_now;
  if(diff_now != latched_diff_) {
    char buf[200];
    snprintf(buf, sizeof(buf),
             "MAPPER I2: wsl moved without the heap break while records live "
             "(latched diff %08X, now %08X; wsl %08X, break %08X)",
             static_cast<uint32_t>(latched_diff_), static_cast<uint32_t>(diff_now),
             static_cast<uint32_t>(wsl_now), static_cast<uint32_t>(brk_now));
    mapper_abort(owner_, buf);
  }
  // (b) Stack-clearance check REMOVED (task 015): as originally
  // specified it compared across address spaces (shadow_wsp fed a wsp
  // that was not always a clean real-stack value produced a 0x74 area
  // address, so wsl <= clear always fired). Isolation proved clause (a)
  // alone fixes fo with no regression, and clause (c) — the live wsl
  // domain bound in map_word/probe — is what actually prevents the
  // reclassified-buffer misclassification. So (b) is unnecessary.

}

const LiveRecord& Mapper::push_record(Machine& m, BookEntry* e, uint32_t entry_pc,
                                      int32_t W, int32_t argc, int32_t frame_wides,
                                      bool args_written) {
  if(records_.empty())
    latched_diff_ = (m.wsl & 0x7FFFFFFF) -
                    (static_cast<int32_t>(m.memory->read_wide(rt::HEAP_BREAK)) & 0x7FFFFFFF);
                    // I2 latch, diff form (Finding B ruling; post-steal, first push)
  else
    i2_assert(m);
  // Main-task assert (Mapper.md §3 Tasks): the listener never LCALLs game
  // routines today; if that ever changes this converts a silent
  // mistranslation into a finding.
  if(!main_task_) {
    char buf[160];
    snprintf(buf, sizeof(buf), "MAPPER: redirect of %s on a non-main task (wsp=%08X)",
             e->name.c_str(), static_cast<uint32_t>(W));
    mapper_abort(&m, buf);
  }
  // I5 — LIFO nesting with the stack.
  if(!records_.empty() && W <= records_.back().W) {
    char buf[160];
    snprintf(buf, sizeof(buf), "MAPPER I5: record for %s at W=%08X does not nest above %s at W=%08X",
             e->name.c_str(), static_cast<uint32_t>(W),
             records_.back().entry->name.c_str(), static_cast<uint32_t>(records_.back().W));
    mapper_abort(&m, buf);
  }
  // Re-entry backstop (the primary tripwire aborts at the WSAVS site
  // before any side effect) and the extent-fits-block runtime re-check
  // with the instruction's own frame size (catches book-vs-code drift).
  if(e->live) {
    char buf[160];
    snprintf(buf, sizeof(buf), "MAPPER I5: push of %s while its record is live", e->name.c_str());
    mapper_abort(&m, buf);
  }
  if((e->wfp_base - e->alloc_base) + 2 + 2 * static_cast<uint32_t>(frame_wides) > e->size_words) {
    char buf[160];
    snprintf(buf, sizeof(buf), "MAPPER: frame extent exceeds book block for %s at redirect (frame=%d size=%u)",
             e->name.c_str(), frame_wides, e->size_words);
    mapper_abort(&m, buf);
  }
  LiveRecord rec;
  rec.entry = e;
  rec.area_wfp = static_cast<int32_t>(e->wfp_base);
  rec.W = W;
  rec.argc = argc;
  rec.frame_wides = frame_wides;
  rec.args_written = args_written;
  // M4b (Project 16, mode-aware accounting — ratified Aug 22 2026): a
  // write-mode record's clone stack elides the ARGS as well as the WSAVS
  // image; only the marker was pushed, sitting 2*argc lower than the
  // master's. So the marker's master image is fwd(W) + 2*argc, and this
  // record's elision is 2*argc + 10 + 2*frame. Copy mode: unchanged.
  int32_t elided_args = args_written ? 2 * argc : 0;
  rec.master_wfp = static_cast<int32_t>(map_checked(static_cast<uint32_t>(W), Dir::ToMaster)) + elided_args + 10;
  rec.shift_after = (records_.empty() ? 0 : records_.back().shift_after) + elided_args + 10 + 2 * frame_wides;
  records_.push_back(rec);
  e->live = true;
  if(os::Trace::enabled("redirect")) {
    char buf[220];
    snprintf(buf, sizeof(buf), "WSAVS %-20s mode=%c pc=%08X area_wfp=%08X argc=%d frame=%d real_wsp=%08X shadow_wsp=%08X master_wfp=%08X ret=%08X depth=%zu",
             e->name.c_str(), args_written ? 'W' : 'C', entry_pc, static_cast<uint32_t>(rec.area_wfp), argc, frame_wides,
             static_cast<uint32_t>(m.wsp), static_cast<uint32_t>(shadow_wsp(m.wsp)),
             static_cast<uint32_t>(rec.master_wfp),
             static_cast<uint32_t>(m.memory->read_wide(static_cast<uint32_t>(rec.area_wfp))) & 0x7FFFFFFF,
             records_.size());
    // B1 evidence instrument (Project 14 Phase B ruling): nested entries
    // (name form PARENT.n@pc) append the static link — the saved-ac1 wide
    // of this frame's area image (wfp-6). Additive at line end; named
    // routines' lines are unchanged.
    if(e->name.find('@') != std::string::npos) {
      size_t len = strlen(buf);
      snprintf(buf + len, sizeof(buf) - len, " link=%08X",
               static_cast<uint32_t>(m.memory->read_wide(static_cast<uint32_t>(rec.area_wfp) - 6)));
    }
    os::Trace::line("redirect", m.process ? m.process->instance_label : std::string("?"), buf);
  }
  return records_.back();
}

void Mapper::wrtn_fixup(Machine& m, int32_t pre_wfp) {
  if(records_.empty() || !is_area_address(static_cast<uint32_t>(pre_wfp)))
    return;
  i2_assert(m);
  LiveRecord& top = records_.back();
  if(top.area_wfp != pre_wfp) {   // I5 — pop must take the innermost
    char buf[160];
    snprintf(buf, sizeof(buf),
             "AREA: WRTN through area frame %08X but the innermost live area frame is %08X (%s) — out-of-order return",
             static_cast<uint32_t>(pre_wfp), static_cast<uint32_t>(top.area_wfp), top.entry->name.c_str());
    mapper_abort(&m, buf);
  }
  // Copy mode: discard the caller's frame word + args (all on the real
  // stack). M4b write mode (ratified Aug 22 2026): only the marker was
  // pushed — wsp = W − 2 restores the caller's pre-window wsp; the args
  // were never on the clone stack. (The M4bNotes "wsp = W" wording
  // predates the call-marker ruling; W as the code defines it INCLUDES
  // the pushed marker.)
  int32_t new_wsp = top.args_written ? top.W - 2 : top.W - 2 - 2 * top.argc;
  if(os::Trace::enabled("redirect")) {
    char buf[200];
    snprintf(buf, sizeof(buf), "WRTN  %-20s mode=%c area_wfp=%08X ret_pc=%08X real_wsp=%08X (was %08X) shadow_wsp=%08X master_wfp=%08X depth=%zu",
             top.entry->name.c_str(), top.args_written ? 'W' : 'C',
             static_cast<uint32_t>(top.area_wfp), static_cast<uint32_t>(m.pc),
             static_cast<uint32_t>(new_wsp), static_cast<uint32_t>(m.wsp),
             static_cast<uint32_t>(new_wsp + (records_.size() > 1 ? records_[records_.size()-2].shift_after : 0)),
             static_cast<uint32_t>(top.master_wfp), records_.size() - 1);
    os::Trace::line("redirect", m.process ? m.process->instance_label : std::string("?"), buf);
  }
  m.wsp = new_wsp;
  top.entry->live = false;
  records_.pop_back();
}

void Mapper::unwind_to(Machine& m, int32_t target_wfp) {
  if(records_.empty())
    return;
  i2_assert(m);
  int32_t cut_W = target_wfp;
  if(is_area_address(static_cast<uint32_t>(target_wfp))) {
    // an area target: its own record stays; everything above it goes
    for(auto it = records_.rbegin(); it != records_.rend(); ++it)
      if(it->area_wfp == target_wfp) { cut_W = it->W; break; }
  }
  while(!records_.empty() && records_.back().W > cut_W) {   // I5: a suffix, by construction
    LiveRecord& top = records_.back();
    if(os::Trace::enabled("redirect")) {
      char buf[160];
      snprintf(buf, sizeof(buf), "UNWIND-DROP %-20s area_wfp=%08X (target %08X)",
               top.entry->name.c_str(), static_cast<uint32_t>(top.area_wfp), static_cast<uint32_t>(target_wfp));
      os::Trace::line("redirect", m.process ? m.process->instance_label : std::string("?"), buf);
    }
    top.entry->live = false;
    records_.pop_back();
  }
}

} // namespace hw
