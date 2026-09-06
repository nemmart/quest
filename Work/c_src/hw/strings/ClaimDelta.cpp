// hw/strings/ClaimDelta.cpp — see ClaimDelta.hpp.
#include "ClaimDelta.hpp"

namespace hw {
namespace strings {

void ClaimDelta::claim(int32_t wfp, int32_t ac) {
  delta_[wfp] += 2 * ac;                       // WMSP: wsp += 2·ac (EagleStack.cpp:569)
}

void ClaimDelta::release(int32_t wfp, int32_t old_wsp, int32_t new_wsp) {
  delta_[wfp] -= (old_wsp - new_wsp);          // STASP: wsp := ac (EagleStack.cpp:518)
}

void ClaimDelta::frame_exit(int32_t wfp) {
  delta_.erase(wfp);                           // WRTN / ON-pop: nothing outstanding
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
