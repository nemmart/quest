// hw/strings/ClaimDelta.cpp — see ClaimDelta.hpp.
#include "ClaimDelta.hpp"

namespace hw {
namespace strings {

void ClaimDelta::claim(int32_t wfp, int32_t ac) {
#ifdef P33_BROKEN_DELTA_SIGN
  delta_[wfp] -= 2 * ac;                       // TEETH build for tests/run_strhooks_selftest.sh: must go RED
#else
  delta_[wfp] += 2 * ac;                       // WMSP: wsp += 2·ac (EagleStack.cpp:569)
#endif
}

void ClaimDelta::release(int32_t wfp, int32_t old_wsp, int32_t new_wsp) {
  delta_[wfp] -= (old_wsp - new_wsp);          // STASP: wsp := ac (EagleStack.cpp:518)
}

void ClaimDelta::frame_exit(int32_t wfp) {
  delta_.erase(wfp);                           // WRTN / ON-pop: nothing outstanding
}

size_t ClaimDelta::frame_exit_at_or_above(int32_t wfp, int32_t* bad_wfp, int32_t* bad_delta) {
  *bad_wfp = 0; *bad_delta = 0;
  size_t n = 0;
  for(auto it = delta_.lower_bound(wfp); it != delta_.end(); it = delta_.erase(it)) {
    if(it->second != 0 && *bad_delta == 0) { *bad_wfp = it->first; *bad_delta = it->second; }
    n++;
  }
  return n;
}

size_t ClaimDelta::frame_exit_where(const std::function<bool(int32_t)>& gone, int32_t* bad_wfp, int32_t* bad_delta) {
  *bad_wfp = 0; *bad_delta = 0;
  size_t n = 0;
  for(auto it = delta_.begin(); it != delta_.end();) {
    if(!gone(it->first)) { ++it; continue; }
    if(it->second != 0 && *bad_delta == 0) { *bad_wfp = it->first; *bad_delta = it->second; }
    it = delta_.erase(it); n++;
  }
  return n;
}

int32_t ClaimDelta::delta(int32_t wfp) const {
  auto it = delta_.find(wfp);
  return it == delta_.end() ? 0 : it->second;
}

bool ClaimDelta::check(int32_t wfp, int32_t master_wsp, int32_t clone_wsp) const {
  return master_wsp - clone_wsp == delta(wfp);
}

} // namespace strings
} // namespace hw
