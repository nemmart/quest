// hw/strings/ClaimDelta.hpp — the per-frame Δ accumulator (Project 30;
// StringsDesign.md §6.3 "wsp equality").
//
// On the master a claim group's WMSPs raise wsp by 2·ac each and its STASP
// lowers it back; on the clone (P33) the temporaries live in the arena and
// wsp never moves. So master_wsp − clone_wsp is exactly the master's
// OUTSTANDING claims in the current frame: Δ(frame). This class is that
// bookkeeping and nothing else — keyed on wfp, not on pcs:
//   claim(wfp, ac)               +2·ac      at each WMSP hook (57)
//   release(wfp, old, new)       −(old−new) at each STASP hook (19)
//   frame_exit(wfp)              Δ := 0     at the frame's WRTN / ON-pop
//   check(wfp, master_wsp, clone_wsp)  master_wsp − clone_wsp == Δ(wfp)
// Any other cause of wsp divergence breaks the equality, which is the
// point. P30 ships it dark: not wired into compare_pair.
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>

namespace hw {
namespace strings {

class ClaimDelta {
public:
  void claim(int32_t wfp, int32_t ac);
  void release(int32_t wfp, int32_t old_wsp, int32_t new_wsp);
  void frame_exit(int32_t wfp);
  int32_t delta(int32_t wfp) const;
  bool check(int32_t wfp, int32_t master_wsp, int32_t clone_wsp) const;
  size_t frames() const { return delta_.size(); }
  void clear() { delta_.clear(); }
private:
  std::map<int32_t, int32_t> delta_;
};

} // namespace strings
} // namespace hw
