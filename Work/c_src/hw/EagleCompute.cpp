#include "EagleCompute.hpp"
#include "Machine.hpp"




namespace hw {
void EagleCompute::setup(uint32_t opcode, const std::string& name, const std::string& fmt, int32_t op) {
  EagleInstruction::setup(opcode, name, fmt, op);
  opcode=opcode>>11;
  YY=opcode & 0x03;
  opcode=opcode>>2;
  XX=opcode & 0x03;
}

uint32_t EagleCompute::execute(Machine& machine, uint32_t address, uint32_t opcode) {
  int32_t src, dst, value, resolved, mask;
  int64_t src_long, dst_long, remainder;

  switch(oper) {
   case WMOV:
    machine.ac[YY]=machine.ac[XX];
    return copy_segment(address, address+1);

   case WCOM:
    machine.ac[YY]=~machine.ac[XX];
    return copy_segment(address, address+1);

   case WNEG:
    machine.ac[YY]=sub(machine, machine.ac[XX], 0);
    return copy_segment(address, address+1);

   case WXCH:
    src=machine.ac[XX];
    machine.ac[XX]=machine.ac[YY];
    machine.ac[YY]=src;
    return copy_segment(address, address+1);

   case WADD:
    src=machine.ac[XX];
    dst=machine.ac[YY];
    machine.ac[YY]=add(machine, src, dst);
    return copy_segment(address, address+1);

   case WSUB:
    src=machine.ac[XX];
    dst=machine.ac[YY];
    machine.ac[YY]=sub(machine, src, dst);
    return copy_segment(address, address+1);

   case WADC:
    src=machine.ac[XX];
    dst=machine.ac[YY];
    machine.ac[YY]=add(machine, ~src, dst);
    return copy_segment(address, address+1);

   case WMUL:
    src=machine.ac[XX];
    dst=machine.ac[YY];
    machine.ac[YY]=mul(machine, src, dst);
    return copy_segment(address, address+1);

   case WDIV:
    // P26: body hoisted into EagleInstruction::div (shared with IRExec).
    machine.ac[YY]=div(machine, machine.ac[XX], machine.ac[YY]);
    return copy_segment(address, address+1);

   case ZEX:
    src=machine.ac[XX];
    machine.ac[YY]=src & 0xFFFF;
    return copy_segment(address, address+1);

   case SEX:
    src=machine.ac[XX];
    machine.ac[YY]=static_cast<int32_t>(src<<16)>>16;
    return copy_segment(address, address+1);

   case CVWN:
    // P26: body hoisted into EagleInstruction::cvwn (shared with IRExec).
    machine.ac[YY]=cvwn(machine, machine.ac[YY]);
    return copy_segment(address, address+1);

   case WINC:
    src=machine.ac[XX];
    machine.ac[YY]=add(machine, 1, src);
    return copy_segment(address, address+1);

   case WADI:
    src=machine.ac[YY];
    machine.ac[YY]=add(machine, XX+1, src);
    return copy_segment(address, address+1);

   case WSBI:
    src=machine.ac[YY];
    machine.ac[YY]=sub(machine, XX+1, src);
    return copy_segment(address, address+1);

   case WLSI:
    machine.ac[YY]=logical_shift(machine, machine.ac[YY], XX+1);
    return copy_segment(address, address+1);

   case WAND:
    src=machine.ac[XX]; dst=machine.ac[YY];
    machine.ac[YY]=src & dst;
    return copy_segment(address, address+1);

   case WIOR:
    src=machine.ac[XX]; dst=machine.ac[YY];
    machine.ac[YY]=src | dst;
    return copy_segment(address, address+1);

   case WXOR:
    src=machine.ac[XX]; dst=machine.ac[YY];
    machine.ac[YY]=src ^ dst;
    return copy_segment(address, address+1);

   case NADD:
    src=machine.ac[XX]; dst=machine.ac[YY];
    machine.ac[YY]=narrow_add(machine, src, dst);
    return copy_segment(address, address+1);

   case NSUB:
    src=machine.ac[XX]; dst=machine.ac[YY];
    machine.ac[YY]=narrow_sub(machine, src, dst);
    return copy_segment(address, address+1);

   case NNEG:
    src=machine.ac[XX];
    machine.ac[YY]=narrow_sub(machine, src, 0);
    return copy_segment(address, address+1);

   case NMUL:
    src=machine.ac[XX]; dst=machine.ac[YY];
    machine.ac[YY]=narrow_mul(machine, src, dst);
    return copy_segment(address, address+1);

   case NADI:
    src=machine.ac[YY];
    machine.ac[YY]=narrow_add(machine, XX+1, src);
    return copy_segment(address, address+1);

   case NSBI:
    src=machine.ac[YY];
    machine.ac[YY]=narrow_sub(machine, XX+1, src);
    return copy_segment(address, address+1);

   case NADDI:
    src=machine.memory->read_word(copy_segment(address, address+1));
    dst=machine.ac[YY];
    machine.ac[YY]=narrow_add(machine, src, dst);
    return copy_segment(address, address+2);

   case WHLV:
    // Sep 5 2026 (docs/HWFindings_Sep5.md): the DG manual's WHLV page —
    // "rounds toward 0".  >>1 floors (-3 -> -2; hardware -1).
    machine.ac[YY]=static_cast<int32_t>(machine.ac[YY])/2;
    return copy_segment(address, address+1);

   case WMOVR:
    machine.ac[YY]=static_cast<int32_t>(static_cast<uint32_t>(machine.ac[YY])>>1);
    return copy_segment(address, address+1);

   case ADDI:
    src=machine.memory->read_word(copy_segment(address, address+1));
    machine.ac[YY]=(machine.ac[YY]+src) & 0xFFFF;
    return copy_segment(address, address+2);

   case ANDI:
    src=machine.memory->read_word(copy_segment(address, address+1));
    machine.ac[YY]=machine.ac[YY] & src;
    return copy_segment(address, address+2);

   case WSEQ:
    src=machine.ac[XX];
    dst=(XX!=YY)?machine.ac[YY]:0;
    return copy_segment(address, address+((src==static_cast<uint32_t>(dst))?2:1));

   case WSNE:
    src=machine.ac[XX];
    dst=(XX!=YY)?machine.ac[YY]:0;
    return copy_segment(address, address+((src!=static_cast<uint32_t>(dst))?2:1));

   case WSLT:
    src=machine.ac[XX];
    dst=(XX!=YY)?machine.ac[YY]:0;
    return copy_segment(address, address+((static_cast<int32_t>(src)<static_cast<int32_t>(dst))?2:1));

   case WSLE:
    src=machine.ac[XX];
    dst=(XX!=YY)?machine.ac[YY]:0;
    return copy_segment(address, address+((static_cast<int32_t>(src)<=static_cast<int32_t>(dst))?2:1));

   case WSGT:
    src=machine.ac[XX];
    dst=(XX!=YY)?machine.ac[YY]:0;
    return copy_segment(address, address+((static_cast<int32_t>(src)>static_cast<int32_t>(dst))?2:1));

   case WSGE:
    src=machine.ac[XX];
    dst=(XX!=YY)?machine.ac[YY]:0;
    return copy_segment(address, address+((static_cast<int32_t>(src)>=static_cast<int32_t>(dst))?2:1));

   case WUSGT:
    src_long=machine.ac[XX] & 0xFFFFFFFFLL;
    dst_long=(XX!=YY)?(machine.ac[YY] & 0xFFFFFFFFLL):0;
    return copy_segment(address, address+((src_long>dst_long)?2:1));

   case WUSGE:
    src_long=machine.ac[XX] & 0xFFFFFFFFLL;
    dst_long=(XX!=YY)?(machine.ac[YY] & 0xFFFFFFFFLL):0;
    return copy_segment(address, address+((src_long>=dst_long)?2:1));

   case WSEQI:
    dst=machine.ac[YY];
    value=machine.memory->read_word(copy_segment(address, address+1));
    value=static_cast<int32_t>(value<<16)>>16;
    return copy_segment(address, address+((static_cast<int32_t>(dst)==value)?3:2));

   case WSNEI:
    dst=machine.ac[YY];
    value=machine.memory->read_word(copy_segment(address, address+1));
    value=static_cast<int32_t>(value<<16)>>16;
    return copy_segment(address, address+((static_cast<int32_t>(dst)!=value)?3:2));

   case WSLEI:
    dst=machine.ac[YY];
    value=machine.memory->read_word(copy_segment(address, address+1));
    value=static_cast<int32_t>(value<<16)>>16;
    return copy_segment(address, address+((static_cast<int32_t>(dst)<=value)?3:2));

   case WSGTI:
    dst=machine.ac[YY];
    value=machine.memory->read_word(copy_segment(address, address+1));
    value=static_cast<int32_t>(value<<16)>>16;
    return copy_segment(address, address+((static_cast<int32_t>(dst)>value)?3:2));

   case NSANA:
    src=machine.memory->read_word(copy_segment(address, address+1));
    return copy_segment(address, address+(((machine.ac[YY] & src)==0)?2:3));

   case WSANA:
    src=machine.memory->read_wide(copy_segment(address, address+1));
    return copy_segment(address, address+(((machine.ac[YY] & src)==0)?3:4));

   case WSKBZ:
    value=((opcode>>10) & 0x1C) | ((opcode>>4) & 0x03);
    src=(machine.ac[0]>>(31-value)) & 0x01;
    return copy_segment(address, address+((src==0)?2:1));

   case WSKBO:
    value=((opcode>>10) & 0x1C) | ((opcode>>4) & 0x03);
    src=(machine.ac[0]>>(31-value)) & 0x01;
    return copy_segment(address, address+((src==1)?2:1));

   case WBTZ:
    if(XX==YY)
     resolved=copy_segment(address, 0);
    else
     resolved=machine.eagle_resolve_indirect(machine.ac[XX]);
    resolved=copy_segment(address, resolved+static_cast<int32_t>(static_cast<uint32_t>(machine.ac[YY])>>4));
    src=machine.memory->read_word(resolved);
    src=src & ~(0x8000>>(machine.ac[YY]&0x0F));
    machine.memory->write_word(resolved, src);
    return copy_segment(address, address+1);

   case WBTO:
    if(XX==YY)
     resolved=copy_segment(address, 0);
    else
     resolved=machine.eagle_resolve_indirect(machine.ac[XX]);
    resolved=copy_segment(address, resolved+static_cast<int32_t>(static_cast<uint32_t>(machine.ac[YY])>>4));
    src=machine.memory->read_word(resolved);
    src=src | (0x8000>>(machine.ac[YY]&0x0F));
    machine.memory->write_word(resolved, src);
    return copy_segment(address, address+1);

   case WSZB:
    if(XX==YY)
     resolved=copy_segment(address, 0);
    else
     resolved=machine.eagle_resolve_indirect(machine.ac[XX]);
    resolved=copy_segment(address, resolved+static_cast<int32_t>(static_cast<uint32_t>(machine.ac[YY])>>4));
    src=(machine.memory->read_word(resolved)>>(15-(machine.ac[YY] & 0x0F))) & 0x01;
    return copy_segment(address, address+((src==0)?2:1));

   case WSZBO:
    if(XX==YY)
     resolved=copy_segment(address, 0);
    else
     resolved=machine.eagle_resolve_indirect(machine.ac[XX]);
    resolved=copy_segment(address, resolved+static_cast<int32_t>(static_cast<uint32_t>(machine.ac[YY])>>4));
    mask=0x8000>>(machine.ac[YY] & 0x0F);
    src=machine.memory->read_word(resolved);
    machine.memory->write_word(resolved, src|mask);
    src=src&mask;
    return copy_segment(address, address+((src==0)?2:1));

   case WLOB:
    src=machine.ac[XX];
    if(src==0)
     machine.ac[YY]+=32;
    else
     while((src & 0x80000000)==0) {
      machine.ac[YY]++;
      src=src<<1;
     }
    return copy_segment(address, address+1);

   case NLDAI:
    value=machine.memory->read_word(copy_segment(address, address+1));
    machine.ac[YY]=static_cast<int32_t>(value<<16)>>16;
    return copy_segment(address, address+2);

   case WNADI:
    src=machine.ac[YY];
    value=machine.memory->read_word(copy_segment(address, address+1));
    value=static_cast<int32_t>(value<<16)>>16;
    machine.ac[YY]=add(machine, value, src);
    return copy_segment(address, address+2);

   case WLSH:
    machine.ac[YY]=logical_shift(machine, machine.ac[YY], machine.ac[XX]);
    return copy_segment(address, address+1);

   case WLSHI:
    value=machine.memory->read_word(copy_segment(address, address+1));
    value=static_cast<int32_t>(value<<24)>>24;
    machine.ac[YY]=logical_shift(machine, machine.ac[YY], value);
    return copy_segment(address, address+2);

   case WLDAI:
    src=machine.memory->read_wide(copy_segment(address, address+1));
    machine.ac[YY]=src;
    return copy_segment(address, address+3);

   case WADDI:
    src=machine.memory->read_wide(copy_segment(address, address+1));
    dst=machine.ac[YY];
    machine.ac[YY]=add(machine, src, dst);
    return copy_segment(address, address+3);

   case WANDI:
    src=machine.memory->read_wide(copy_segment(address, address+1));
    dst=machine.ac[YY];
    machine.ac[YY]=src & dst;
    return copy_segment(address, address+3);

   case WIORI:
    src=machine.memory->read_wide(copy_segment(address, address+1));
    dst=machine.ac[YY];
    machine.ac[YY]=src | dst;
    return copy_segment(address, address+3);

   case WXORI:
    src=machine.memory->read_wide(copy_segment(address, address+1));
    dst=machine.ac[YY];
    machine.ac[YY]=src ^ dst;
    return copy_segment(address, address+3);

   case WUGTI:
    src_long=machine.memory->read_wide(copy_segment(address, address+1)) & 0xFFFFFFFFLL;
    dst_long=machine.ac[YY] & 0xFFFFFFFFLL;
    return copy_segment(address, address+((dst_long>src_long)?4:3));

   case WULEI:
    src_long=machine.memory->read_wide(copy_segment(address, address+1)) & 0xFFFFFFFFLL;
    dst_long=machine.ac[YY] & 0xFFFFFFFFLL;
    return copy_segment(address, address+((dst_long<=src_long)?4:3));

   case XNADD:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_word(resolved);
    dst=machine.ac[YY];
    machine.ac[YY]=narrow_add(machine, src, dst);
    return copy_segment(address, address+2);

   case LNADD:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_word(resolved);
    dst=machine.ac[YY];
    machine.ac[YY]=narrow_add(machine, src, dst);
    return copy_segment(address, address+3);

   case XNSUB:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_word(resolved);
    dst=machine.ac[YY];
    machine.ac[YY]=narrow_sub(machine, src, dst);
    return copy_segment(address, address+2);

   case LNSUB:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_word(resolved);
    dst=machine.ac[YY];
    machine.ac[YY]=narrow_sub(machine, src, dst);
    return copy_segment(address, address+3);

   case XNMUL:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_word(resolved);
    machine.ac[YY]=narrow_mul(machine, src, machine.ac[YY]);
    return copy_segment(address, address+2);

   case LNMUL:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_word(resolved);
    machine.ac[YY]=narrow_mul(machine, src, machine.ac[YY]);
    return copy_segment(address, address+3);

   case XNADI:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), YY);
    src=machine.memory->read_word(resolved);
    src=narrow_add(machine, XX+1, src);
    machine.memory->write_word(resolved, src);
    return copy_segment(address, address+2);

   case LNADI:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), YY);
    src=machine.memory->read_word(resolved);
    src=narrow_add(machine, XX+1, src);
    machine.memory->write_word(resolved, src);
    return copy_segment(address, address+3);

   case XNSBI:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), YY);
    src=machine.memory->read_word(resolved);
    src=narrow_sub(machine, XX+1, src);
    machine.memory->write_word(resolved, src);
    return copy_segment(address, address+2);

   case LNSBI:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), YY);
    src=machine.memory->read_word(resolved);
    src=narrow_sub(machine, XX+1, src);
    machine.memory->write_word(resolved, src);
    return copy_segment(address, address+3);

   case XNDSZ:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), YY);
    src=machine.memory->read_word(resolved);
    src=(src-1) & 0xFFFF;
    machine.memory->write_word(resolved, src);
    return copy_segment(address, address+((src==0)?3:2));

   case XNISZ:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), YY);
    src=machine.memory->read_word(resolved);
    src=(src+1) & 0xFFFF;
    machine.memory->write_word(resolved, src);
    return copy_segment(address, address+((src==0)?3:2));

   case XWISZ:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), YY);
    src=machine.memory->read_wide(resolved);
    src++;
    machine.memory->write_wide(resolved, src);
    return copy_segment(address, address+((src==0)?3:2));

   case XWADD:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_wide(resolved);
    machine.ac[YY]=add(machine, src, machine.ac[YY]);
    return copy_segment(address, address+2);

   case LWADD:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_wide(resolved);
    machine.ac[YY]=add(machine, src, machine.ac[YY]);
    return copy_segment(address, address+3);

   case XWSUB:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_wide(resolved);
    machine.ac[YY]=sub(machine, src, machine.ac[YY]);
    return copy_segment(address, address+2);

   case LWSUB:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_wide(resolved);
    machine.ac[YY]=sub(machine, src, machine.ac[YY]);
    return copy_segment(address, address+3);

   case XWADI:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), YY);
    src=machine.memory->read_wide(resolved);
    src=add(machine, XX+1, src);
    machine.memory->write_wide(resolved, src);
    return copy_segment(address, address+2);

   case XWSBI:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), YY);
    src=machine.memory->read_wide(resolved);
    src=sub(machine, XX+1, src);
    machine.memory->write_wide(resolved, src);
    return copy_segment(address, address+2);

   case XWMUL:
    resolved=machine.eagle_x_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_wide(resolved);
    machine.ac[YY]=mul(machine, src, machine.ac[YY]);
    return copy_segment(address, address+2);

   case LWMUL:
    resolved=machine.eagle_l_resolve_indirect(copy_segment(address, address+1), XX);
    src=machine.memory->read_wide(resolved);
    machine.ac[YY]=mul(machine, src, machine.ac[YY]);
    return copy_segment(address, address+3);

   case DIV:
    if((machine.ac[0] & 0xFFFF)>=(machine.ac[2] & 0xFFFF) || (machine.ac[2] & 0xFFFF)==0) {
     machine.c=1;
     return copy_segment(address, address+1);
    }
    src=(machine.ac[0]<<16) | (machine.ac[1] & 0xFFFF);
    machine.ac[1]=src/(machine.ac[2] & 0xFFFF);
    machine.ac[0]=src%(machine.ac[2] & 0xFFFF);
    return copy_segment(address, address+1);

   case DIVX: {
    src=static_cast<int32_t>(machine.ac[1]<<16)>>16;
    dst=machine.ac[2] & 0xFFFF;
    if(dst==0 || (dst==0xFFFF && src==static_cast<int32_t>(0xFFFF8000))) {
     machine.ac[0]=(src>>15) & 0xFFFF;
     machine.c=1;
    }
    else {
     machine.ac[1]=src/static_cast<int32_t>(dst);
     machine.ac[0]=src%static_cast<int32_t>(dst);
     machine.c=0;
    }
    return copy_segment(address, address+1);
   }

   case WDIVS: {
    if(machine.ac[2]==0) {
     machine.ovr=1;
     return copy_segment(address, address+1);
    }
    src_long=machine.ac[0] & 0xFFFFFFFFLL;
    dst_long=machine.ac[1] & 0xFFFFFFFFLL;
    dst_long=(src_long<<32)+dst_long;
    src_long=static_cast<int32_t>(machine.ac[2]);
    remainder=dst_long%src_long;
    dst_long=dst_long/src_long;
    if((dst_long>>31)!=0 && (dst_long>>31)!=-1) {
     machine.ovr=1;
     return copy_segment(address, address+1);
    }
    machine.ac[1]=static_cast<int32_t>(dst_long);
    machine.ac[0]=static_cast<int32_t>(remainder);
    return copy_segment(address, address+1);
   }
  }

  throw std::runtime_error("Internal error - some case is not returning");
}

} // namespace hw
