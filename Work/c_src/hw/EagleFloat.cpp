#include "EagleFloat.hpp"
#include "Machine.hpp"
#include <cmath>




namespace hw {

// Raw-bit shadow for float LOADS (Aug 2026).
//
// fpac[] is a C++ double, so the eclipse representation is reconstructed
// on store. That is lossy in two ways: an eclipse wide float carries a
// 56-bit mantissa against double's 53, and an arbitrary bit pattern that
// is not a valid float at all may reconstruct to an out-of-range
// exponent, which makes validate_exponent throw.
//
// The quads[] shadow already exists to carry exact bits past the double
// representation - stores prefer it (XFSTD/XFSTS check quads[YY] first)
// and FMOV copies it - but NO load ever set it, so a pure load/store
// round trip went through conversion and could not be bit-exact.
//
// That broke C?TRIM (0x7017FB81), which uses XFLDD/XFSTD as a four-word
// block move for a string descriptor - a pointer and a length, not a
// float. Reinterpreted as an eclipse double those bits underflow, and
// the emulator threw "Floating point underflow" inside ?FATAL, killing
// the process while it was trying to report an unrelated error.
//
// A pure load/store round trip must be lossless on real hardware, or
// that idiom could not work on the DG either. So loads now populate the
// shadow, and the invariant the rest of the file already implies holds:
// raw bits survive until arithmetic touches the accumulator, at which
// point every arithmetic case clears quads[YY].
//
// Single-precision loads store the value in the HIGH half, matching how
// XFSTS/LFSTS read it back (quads[YY] >> 32).
//
// quads[YY]==0 doubles as the "no shadow" sentinel; a genuinely zero
// value converts to zero by either path, so the collision is harmless.
static inline int64_t shadow_from_single(int32_t src) {
  return static_cast<int64_t>(static_cast<uint64_t>(
           static_cast<uint32_t>(src)) << 32);
}

void EagleFloat::setup(uint32_t opcode, const std::string& name, const std::string& fmt, int32_t op) {
  EagleInstruction::setup(opcode, name, fmt, op);
  opcode=opcode>>11;
  YY=opcode & 0x03;
  opcode=opcode>>2;
  XX=opcode & 0x03;
}

uint32_t EagleFloat::execute(Machine& machine, uint32_t address, uint32_t opcode) {
  int32_t src, resolved, exp;
  int64_t quad, mantissa;

  switch(oper) {
   case FTD: case FTE:
    return copy_segment(address, address+1);

   case FMOV:
    machine.fpac[YY]=machine.fpac[XX];
    machine.quads[YY]=machine.quads[XX];
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+1);

   case WFLAD:
    machine.fpac[YY]=static_cast<double>(machine.ac[XX]);
    machine.quads[YY]=0;
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+1);

   case WFFAD:
    quad=static_cast<int64_t>(machine.fpac[YY]);
    machine.ac[XX]=static_cast<int32_t>(quad);
    quad=quad>>31;
    if(quad!=0 && quad!=-1)
      // LAYERING NOTE (docs/Layering.md): FP faults THROW instead of
      // vectoring through I.FFALT — the real hardware would push a
      // fault frame and vector (0x700001CA). Because of this, I.FFALT
      // and its O.S* decode path are dead in our world and ruled
      // frozen/untranslated. If FP faults are ever changed to vector
      // faithfully, that ruling is void: I.FFALT becomes live L2 and
      // requires native translation FIRST.
      throw std::runtime_error("Floating point conversion overflow");
    return copy_segment(address, address+1);

   case FRDS:
    machine.fpac[YY]=eclipse_wide_round(machine, machine.fpac[XX]);
    machine.quads[YY]=0;
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+1);

   case FHLV:
    machine.fpac[YY]=machine.fpac[YY]*0.5;
    machine.quads[YY]=0;
    validate_exponent(machine, machine.fpac[YY]);
    return copy_segment(address, address+1);

   case FINT:
    if(machine.fpac[YY]>0)
      machine.fpac[YY]=std::floor(machine.fpac[YY]);
    else
      machine.fpac[YY]=std::ceil(machine.fpac[YY]);
    machine.quads[YY]=0;
    return copy_segment(address, address+1);

   case FRH:
    quad=double_to_eclipse_wide_float(machine, machine.fpac[YY]);
    machine.ac[0]=static_cast<int32_t>(static_cast<uint64_t>(quad)>>48);
    return copy_segment(address, address+1);

   case FEXP:
    quad=double_to_eclipse_wide_float(machine, machine.fpac[YY]);
    quad=quad & static_cast<int64_t>(0x80FFFFFFFFFFFFFFLL);
    quad=quad | (static_cast<int64_t>(machine.ac[0] & 0x00007F00)<<48);
    machine.fpac[YY]=eclipse_wide_float_to_double(machine, quad);
    machine.quads[YY]=0;
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+1);

   case FSCAL: {
    exp=(machine.ac[0]>>8) & 0x7F;
    quad=double_to_eclipse_wide_float(machine, machine.fpac[YY]);
    exp=exp-static_cast<int32_t>((quad>>56) & 0x7F);
    mantissa=quad & 0x00FFFFFFFFFFFFFFLL;
    if(exp>0)
      mantissa=static_cast<uint64_t>(mantissa)>>(exp*4);
    else if(exp<0)
      mantissa=mantissa<<(-exp*4);
    mantissa=mantissa & 0x00FFFFFFFFFFFFFFLL;
    if(mantissa==0) {
      machine.fpac[YY]=0.0;
      machine.fplr=0.0;
    }
    else {
      quad=((quad>>56) & 0x80) + ((machine.ac[0]>>8) & 0x7F);
      quad=(quad<<56) + mantissa;
      machine.fpac[YY]=eclipse_wide_float_to_double(machine, quad);
      machine.quads[YY]=quad;
    }
    return copy_segment(address, address+1);
   }

   case XFLDS:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_wide(resolved);
    machine.fpac[YY]=eclipse_float_to_double(machine, src);
    machine.quads[YY]=shadow_from_single(src);
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+2);

   case LFLDS:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_wide(resolved);
    machine.fpac[YY]=eclipse_float_to_double(machine, src);
    machine.quads[YY]=shadow_from_single(src);
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+3);

   case XFSTS:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), XX);
    if(machine.quads[YY]!=0)
      src=static_cast<int32_t>(static_cast<uint64_t>(machine.quads[YY])>>32);
    else
      src=double_to_eclipse_float(machine, machine.fpac[YY]);
    machine.memory->write_wide(resolved, src);
    return copy_segment(address, address+2);

   case LFSTS:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), XX);
    if(machine.quads[YY]!=0)
      src=static_cast<int32_t>(static_cast<uint64_t>(machine.quads[YY])>>32);
    else
      src=double_to_eclipse_float(machine, machine.fpac[YY]);
    machine.memory->write_wide(resolved, src);
    return copy_segment(address, address+3);

   case XFLDD:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), XX);
    quad=machine.memory->read_quad(resolved);
    machine.fpac[YY]=eclipse_wide_float_to_double(machine, quad);
    machine.quads[YY]=quad;
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+2);

   case LFLDD:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), XX);
    quad=machine.memory->read_quad(resolved);
    machine.fpac[YY]=eclipse_wide_float_to_double(machine, quad);
    machine.quads[YY]=quad;
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+3);

   case XFSTD:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), XX);
    if(machine.quads[YY]!=0)
      quad=machine.quads[YY];
    else
      quad=double_to_eclipse_wide_float(machine, machine.fpac[YY]);
    machine.memory->write_quad(resolved, quad);
    return copy_segment(address, address+2);

   case LFSTD:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), XX);
    if(machine.quads[YY]!=0)
      quad=machine.quads[YY];
    else
      quad=double_to_eclipse_wide_float(machine, machine.fpac[YY]);
    machine.memory->write_quad(resolved, quad);
    return copy_segment(address, address+3);

   case FAS: case FAD:
    machine.fpac[YY]=machine.fpac[XX]+machine.fpac[YY];
    machine.quads[YY]=0;
    validate_exponent(machine, machine.fpac[YY]);
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+1);

   case FSS: case FSD:
    machine.fpac[YY]=machine.fpac[YY]-machine.fpac[XX];
    machine.quads[YY]=0;
    validate_exponent(machine, machine.fpac[YY]);
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+1);

   case FMS: case FMD:
    machine.fpac[YY]=machine.fpac[XX]*machine.fpac[YY];
    machine.quads[YY]=0;
    validate_exponent(machine, machine.fpac[YY]);
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+1);

   case FDS: case FDD:
    if(machine.fpac[XX]==0.0)
      throw std::runtime_error("Division by zero");
    machine.fpac[YY]=machine.fpac[YY]/machine.fpac[XX];
    machine.quads[YY]=0;
    validate_exponent(machine, machine.fpac[YY]);
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+1);

   case FCMP:
    machine.fplr=machine.fpac[YY]-machine.fpac[XX];
    return copy_segment(address, address+1);

   case FSEQ:
    return copy_segment(address, address+((machine.fplr==0.0)?2:1));

   case FSNE:
    return copy_segment(address, address+((machine.fplr!=0.0)?2:1));

   case FSGE:
    return copy_segment(address, address+((machine.fplr>=0.0)?2:1));

   case FSGT:
    return copy_segment(address, address+((machine.fplr>0.0)?2:1));

   case FSLE:
    return copy_segment(address, address+((machine.fplr<=0.0)?2:1));

   case FSLT:
    return copy_segment(address, address+((machine.fplr<0.0)?2:1));

   case XFAMS:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_wide(resolved);
    machine.fpac[YY]=machine.fpac[YY]+eclipse_float_to_double(machine, src);
    machine.quads[YY]=0;
    validate_exponent(machine, machine.fpac[YY]);
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+2);

   case LFAMS:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), XX);
    quad=machine.memory->read_quad(resolved);
    machine.fpac[YY]=machine.fpac[YY]+eclipse_wide_float_to_double(machine, quad);
    machine.quads[YY]=0;
    validate_exponent(machine, machine.fpac[YY]);
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+3);

   case XFAMD:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), XX);
    quad=machine.memory->read_quad(resolved);
    machine.fpac[YY]=machine.fpac[YY]+eclipse_wide_float_to_double(machine, quad);
    machine.quads[YY]=0;
    validate_exponent(machine, machine.fpac[YY]);
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+2);

   case LFAMD:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), XX);
    quad=machine.memory->read_quad(resolved);
    machine.fpac[YY]=machine.fpac[YY]+eclipse_wide_float_to_double(machine, quad);
    machine.quads[YY]=0;
    validate_exponent(machine, machine.fpac[YY]);
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+3);

   case XFMMS:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_wide(resolved);
    machine.fpac[YY]=machine.fpac[YY]*eclipse_float_to_double(machine, src);
    machine.quads[YY]=0;
    validate_exponent(machine, machine.fpac[YY]);
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+2);

   case LFMMS:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), XX);
    quad=machine.memory->read_quad(resolved);
    machine.fpac[YY]=machine.fpac[YY]*eclipse_wide_float_to_double(machine, quad);
    machine.quads[YY]=0;
    validate_exponent(machine, machine.fpac[YY]);
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+3);

   case XFMMD:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), XX);
    quad=machine.memory->read_quad(resolved);
    machine.fpac[YY]=machine.fpac[YY]*eclipse_wide_float_to_double(machine, quad);
    machine.quads[YY]=0;
    validate_exponent(machine, machine.fpac[YY]);
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+2);

   case LFMMD:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), XX);
    quad=machine.memory->read_quad(resolved);
    machine.fpac[YY]=machine.fpac[YY]*eclipse_wide_float_to_double(machine, quad);
    machine.quads[YY]=0;
    validate_exponent(machine, machine.fpac[YY]);
    machine.fplr=machine.fpac[YY];
    return copy_segment(address, address+3);
  }
  throw std::runtime_error("Internal error - some case is not returning");
}

} // namespace hw
