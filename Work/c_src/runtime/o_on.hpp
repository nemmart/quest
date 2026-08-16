// src/runtime/o_on.hpp
//
// O.ON (0x7017ED9B) + O.REVERT (0x7017EDCB): the PL/1 ON-condition
// registrars, translated natively while I.PROLOG and the entire signal
// path (O?SIGNAL, O.SET, the select loop, I.GOTO) REMAIN EMULATED —
// the Task D transition architecture. The native code maintains the
// handler chain in emulated stack frames exactly as the emulated
// writers would, so the emulated walkers keep working. Full derivation
// and residue analysis: docs/O_ON.md (incl. RESOLUTIONS).
//
// Conventions (both routines): called via LJSR (ac3 = pc+3; no frame
// word, ovr NOT cleared), WSSVS/WSSVR six-wide frames. Register
// arguments: O.ON takes ac0 = condition type (<=0 means catch-all;
// Quest always passes -1), ac1 = key2 (forced to 0 for catch-all),
// ac2 = handler address; O.REVERT takes ac0/ac1 as search keys.
// Implicit argument for both: the CALLER'S frame pointer (machine.wfp
// at dispatch), which is where the chain hangs (via @[frame+0xA], a
// pointer to the head slot installed by emulated I.PROLOG).
//
// Chain node (8 words): [+0] link, [+2] condition type (0 = inactive/
// reusable), [+4] key2, [+6] handler address.
//
// O.ON: search the caller frame's chain (shared helper, exact port of
// the unnamed routine at 0x7017EE7A including its skip-return and
// last-zero-node backstop); found -> overwrite the node's descriptor
// (reuse); not found -> reuse the backstop if any, else ALLOCATE by
// the frame-shift trick: the emulated body block-moves its own saved-
// register image up 8 words and the abandoned image becomes the node
// (type/key2/handler already in place), leaving wsp 8 words taller at
// return. The native replicates the relocated image, the node, the
// caller_frame[+2] bookkeeping pointer, the head link, and the +8 wsp.
//
// O.REVERT: no-op unless [wsb-0x40] (innermost condition frame,
// maintained by emulated I.PROLOG) equals the caller's frame; then
// search and zero node[+2] (deactivate-in-place; O.ON reuses such
// nodes).
#pragma once
#include <cstdint>

namespace hw { class Machine; class Memory; }

namespace rt {

// Exact port of the chain-search helper at 0x7017EE7A. Walks the chain
// at @[frame+0xA]; every node with node[+2]==0 overwrites the backstop
// (LAST zero node wins); a node matching key1/key2 returns found.
// key2 must already reflect the preamble (0 when key1<=0). Outputs the
// residue bookkeeping the emulated helper leaves behind: the final
// scratch value (last backstop or 0) and the result node.
struct ChainSearchResult {
  bool found;
  int32_t node;       // found node, or backstop-or-0 when !found
  int32_t scratch;    // final value of the helper's TOS scratch wide
};
void chain_search(hw::Memory& memory, int32_t frame, int32_t key1,
                  int32_t key2, ChainSearchResult& out);

} // namespace rt

namespace emu_rt {
uint32_t o_on(hw::Machine& machine);
uint32_t o_revert(hw::Machine& machine);
}
