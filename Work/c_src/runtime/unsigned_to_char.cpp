// src/runtime/unsigned_to_char.cpp
#include "unsigned_to_char.hpp"
#include "udiv32.hpp"
#include "../hw/Machine.hpp"
#include "../hw/RTBridge.hpp"
#include "../hw/RTStubs.hpp"
#include "../debug/Capture.hpp"

namespace rt {

// 0x7017DA6C is 00 00 "0123456789ABCDEF"; the body indexes it at
// remainder+1 past the XLEFB base (byte 1), i.e. TABLE[remainder].
static const char TABLE[17]="0123456789ABCDEF";

void unsigned_to_char_3(int32_t value, int32_t base, int32_t width,
                        bool padded, UnsignedToCharResult& out) {
  int32_t v, q, r, i;
  bool cont, error;

  out.digit_count=0;
  out.final_counter=1;       // local [7] initialized before the WSLE guard
  out.last_quotient=0;
  out.last_remainder=0;
  out.error=false;
  if(width<1)                // WSLE 0,1 with ac0=1: loop skipped entirely
    return;
  v=value;
  i=1;
  for(;;) {
    q=udiv32_3(v, base, r, error);
    if(error)
      out.error=true;
    out.digits[33-i]=TABLE[r&0xF];   // scratch byte at 0x1B+(33-i)
    out.digit_count=i;
    out.last_quotient=q;
    out.last_remainder=r;
    v=q;
    cont=(q!=0) || padded;   // WSEQ 0,0 then the ADD.O# flag test
    if(!cont)
      break;                 // quotient 0, no width flag: [7] stays i
    i=i+1;                   // XNDO: increment the counter...
    if(i>width)
      break;                 // ...and exit when it exceeds width
  }
  out.final_counter=i;
}

} // namespace rt

namespace emu_rt {

uint32_t unsigned_to_char(hw::Machine& machine) {
  hw::RTBridge bridge(machine);
  rt::UnsignedToCharResult result;
  int32_t argc, dest, value, base, flag_raw, width, k, carry, psr;
  uint32_t fb, dest_byte;
  bool padded;
  int j;

  argc=bridge.arg_count();
  if(argc<1 || argc>3) {
    hw::RTStubs::log_call(machine, "?UNSIGNED_TO_CHAR", "(native-fallback)");
    return hw::RTStubs::entry_address("?UNSIGNED_TO_CHAR");
  }

  hw::RTStubs::log_call(machine, "?UNSIGNED_TO_CHAR", "(native)");
  dest=bridge.entry_ac(2);                 // register argument: destination word address
  value=bridge.arg_wide(1);
  base=10;                                 // argc==1 default (0x7017DA9F)
  if(argc>=2) {
    base=bridge.arg_word(2);
    if(base>16) base=16;                   // clamp to [2,16], signed
    if(base<2) base=2;
  }
  flag_raw=0;
  if(argc>2)
    flag_raw=bridge.arg_word(3);           // sign-extended narrow
  padded=((flag_raw*2)&0xFFFF)!=0;         // the ADD.O# SEZ sense: 0 and 0x8000 are "no flag"
  width=32;
  if(padded && flag_raw<32)
    width=flag_raw;                        // min(flag,32), signed

  fb=bridge.emulate_frame();
  rt::unsigned_to_char_3(value, base, width, padded, result);
  if(result.error)
    machine.ovr=1;                         // WDIVS escape mirror (unreachable: base>=2)
  k=result.digit_count;

  // Locals — final values of every write the emulated body makes
  // (docs/UNSIGNED_TO_CHAR.md residue map, corrected this session).
  bridge.write_frame_word(fb, 0x2, argc);
  bridge.write_frame_word(fb, 0x3, flag_raw);
  bridge.write_frame_word(fb, 0x4, base);
  bridge.write_frame_word(fb, 0x5, width);
  bridge.write_frame_wide(fb, 0x8, k>0 ? result.last_quotient : value);
  bridge.write_frame_word(fb, 0x6, 32-k);
  bridge.write_frame_word(fb, 0x1E, width);
  bridge.write_frame_word(fb, 0x7, result.final_counter);
  if(k>0) {
    bridge.write_frame_wide(fb, 0xA, result.last_quotient);
    bridge.write_frame_wide(fb, 0xC, result.last_remainder);   // ?UDIV32's wide store through arg3
    for(j=33-k; j<=32; j++)
      bridge.write_frame_byte(fb, 0x1B+j, result.digits[j]);   // untouched scratch keeps prior residue
    // Inner-call residue at [fb+52 .. fb+69]: the three XPEF'd arg
    // pointers, the LCALL frame word, and ?UDIV32's WSAVS image with
    // the saved-ac0 slot patched to the quotient. Final iteration's
    // values persist. Inner-entry ac2 is 1 (NLDAI at 0x7017DAC1) on
    // the first iteration, thereafter the previous iteration's scratch
    // byte pointer (WADD at 0x7017DAE9). Inner-entry carry: see the
    // P24 re-derivation at the staging line below (the pre-fix note
    // here said "0 except argc>2 first iteration"; the k==1/argc<=2
    // leg is c=1 under the wide-carry fix).
    psr=(machine.get_psr()|0x8000)&0xFFFF;                     // ovk set by the outer WSAVS on the master
    bridge.write_frame_wide(fb, 52, static_cast<int32_t>(fb+0xC));
    bridge.write_frame_wide(fb, 54, static_cast<int32_t>(fb+0x20));
    bridge.write_frame_wide(fb, 56, static_cast<int32_t>(fb+0x8));
    bridge.write_frame_wide(fb, 58, static_cast<int32_t>((static_cast<uint32_t>(psr)<<16)|3));
    bridge.write_frame_wide(fb, 60, result.last_quotient);     // patched saved-ac0 slot
    bridge.write_frame_wide(fb, 62, base);
    bridge.write_frame_wide(fb, 64, k==1 ? 1 : static_cast<int32_t>(fb*2+0x1B+34-k));
    bridge.write_frame_wide(fb, 66, static_cast<int32_t>(fb));
    // P24 re-derivation: iteration 1's inner-entry carry is the entry
    // carry when argc>2 (the parse path skips daA3), else the daA3
    // WSUB 0,0 latch = c=1 under the fix (was 0). For k>1 the LAST
    // surviving iteration entered via the loop back-edge, whose final
    // c-writer is the XNDO at daCB (narrow, fix-invariant): index+1
    // never wraps 16 bits, c=0 — unchanged.
    carry=(k==1) ? ((argc>2) ? bridge.entry_carry() : 1) : 0;
    bridge.write_frame_wide(fb, 68, static_cast<int32_t>(0x7017DADCu|(static_cast<uint32_t>(carry)<<31)));
  }

  // Finish tail (0x7017DAF8..): first-digit byte pointer, clamped
  // length, staging copy (first WCMV), then the destination CHAR
  // VARYING — length word + digits (second WCMV, dest = entry ac2
  // shifted word->byte by WLSI).
  bridge.write_frame_wide(fb, 0x20, static_cast<int32_t>(fb*2+0x1B+33-k));
  bridge.write_frame_word(fb, 0x22, k);                        // min(k,32): k<=width<=32 always
  for(j=0; j<k; j++)
    bridge.write_frame_byte(fb, 0x46+j, result.digits[33-k+j]);
  dest_byte=static_cast<uint32_t>(dest)*2;
  machine.memory->write_byte(dest_byte, (k>>8)&0xFF);
  machine.memory->write_byte(dest_byte+1, k&0xFF);
  for(j=0; j<k; j++)
    machine.memory->write_byte(dest_byte+2+static_cast<uint32_t>(j), result.digits[33-k+j]&0xFF);

  debug::Capture::native_footprint(machine);   // env-gated footprint diffing (QUEST_CAPTURE)
  return bridge.native_return();
}

} // namespace emu_rt
