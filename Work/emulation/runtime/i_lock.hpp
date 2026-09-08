// src/runtime/i_lock.hpp
//
// I.LOCK (0x7017E7D0) / I.UNLOCK (0x7017E7ED) — the PL/1 heap lock,
// wrapped around every I.ALLOC and I.FREE call (via the XPSHJ helpers at
// 0x7017E9F9 / 0x7017EA01). Derivation: docs/I_ALLOC.md.
//
// Lock object layout (one argument, passed by reference; the arg slot
// holds the lock's address — I.LOCK/I.UNLOCK are always called with
// LPEF [0x70000200], so the slot content IS the object address):
//
//   [lock+0..1]   wide, zeroed by a successful acquirer
//   [lock+2] b15  the lock bit  (WSZBO 2,1 with ac1=32 resolves to
//                 lock + (32>>4) = lock+2, mask 0x8000>>(32&0xF) = 0x8000)
//   [lock+3]      contender count (XNISZ on entry, XNDSZ on success)
//
// I.UNLOCK's WMESS reads the WIDE at lock+2 against ac3 = 0x0000FFFF, so
// it tests the LOW half — the contender count — and atomically clears
// both words only when nobody is waiting.
//
// Both routines use WSAVR (not WSAVS): the pushed image is identical
// (ac0, ac1, ac2, wfp, ac3|carry — no psr), and the only difference is
// machine.ovk during the body. WRTN restores psr from the LCALL frame
// word, so after return the distinction is invisible and
// RTBridge::emulate_frame() needs no change.
//
// CONTENDED PATHS ARE NOT TRANSLATED. Both are detectable at entry with
// no side effects beforehand, so the wrapper falls back to emulation
// rather than aborting:
//   I.LOCK    contended  <=> [lock+2] bit 15 already set -> SYSCALL 0525
//   I.UNLOCK  has waiters <=> [lock+3] != 0              -> SYSCALL 0523
// Neither has ever executed (0523 is not even implemented in the OS
// layer, so the wake path would throw). Falling back keeps both engines
// emulating identically if contention ever does occur.
#pragma once
#include <cstdint>

namespace hw { class Machine; class Memory; }

namespace rt {

// True when the lock is already held (I.LOCK would block).
bool heap_lock_contended(hw::Memory& memory, uint32_t lock);
// True when contenders are registered (I.UNLOCK would signal them).
bool heap_lock_has_waiters(hw::Memory& memory, uint32_t lock);

// Uncontended acquire: bump and restore the contender count, set the
// lock bit, zero the owner wide.
void heap_lock_acquire(hw::Memory& memory, uint32_t lock);
// Uncontended release: clear the lock bit and the count in one wide.
// Returns the pre-clear wide (the body leaves it in ac1; the value is
// discarded by WRTN, but it is returned for exactness/testing).
int32_t heap_lock_release(hw::Memory& memory, uint32_t lock);

} // namespace rt

namespace emu_rt {
uint32_t i_lock(hw::Machine& machine);
uint32_t i_unlock(hw::Machine& machine);
}
