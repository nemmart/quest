package hw;

public class EagleCompute extends EagleInstruction {
   static public final int WMOV=0;
   static public final int WCOM=1;
   static public final int WNEG=2;
   static public final int WXCH=3;
   static public final int WADD=4;
   static public final int WSUB=5;
   static public final int WADC=6;
   static public final int WMUL=7;
   static public final int WDIV=8;
   static public final int ZEX=9;
   static public final int SEX=10;
   static public final int CVWN=11;
   static public final int WLSH=12;
   static public final int WASH=13;
   static public final int WINC=14;
   static public final int WADI=15;
   static public final int WSBI=16;
   static public final int WLSI=17;

   static public final int WAND=20;
   static public final int WIOR=21;
   static public final int WXOR=22;

   static public final int WHLV=30;
   static public final int WMOVR=31;

   static public final int ADDI=35;
   static public final int ANDI=36;

   static public final int NADD=40;
   static public final int NSUB=41;
   static public final int NNEG=42;
   static public final int NMUL=43;
   static public final int NADDI=44;
   static public final int NADI=45;
   static public final int NSBI=46;

   static public final int WSEQ=50;
   static public final int WSNE=51;
   static public final int WSLE=52;
   static public final int WSLT=53;
   static public final int WSGE=54;
   static public final int WSGT=55;
   static public final int WUSGE=56;
   static public final int WUSGT=57;

   static public final int WSKBZ=60;
   static public final int WSKBO=61;
   static public final int WBTZ=62;
   static public final int WBTO=63;
   static public final int WSZB=64;
   static public final int WSZBO=65;
   static public final int WLOB=66;

   static public final int WSEQI=80;
   static public final int WSNEI=81;
   static public final int WSLEI=82;
   static public final int WSGTI=83;
   static public final int NSANA=84;
   static public final int WSANA=85;

   static public final int NLDAI=100;
   static public final int WNADI=101;
   static public final int WLSHI=102;

   static public final int WLDAI=200;
   static public final int WADDI=201;
   static public final int WANDI=203;
   static public final int WIORI=204;
   static public final int WXORI=205;
   static public final int WUGTI=206;
   static public final int WULEI=207;

   static public final int XNADD=300;
   static public final int LNADD=301;
   static public final int XNSUB=302;
   static public final int LNSUB=303;
   static public final int XNMUL=304;
   static public final int LNMUL=305;
   static public final int XNADI=306;
   static public final int LNADI=307;
   static public final int XNSBI=308;
   static public final int LNSBI=309;

   static public final int XNDSZ=350;
   static public final int XNISZ=351;
   static public final int XWISZ=352;

   static public final int XWADD=400;
   static public final int LWADD=401;
   static public final int XWSUB=402;
   static public final int LWSUB=403;
   static public final int XWADI=404;
   static public final int XWSBI=405;
   static public final int XWMUL=406;
   static public final int LWMUL=407;

   static public final int DIV=500;
   static public final int DIVX=501;
   static public final int WDIVS=502;

   public int XX, YY;

   public void setup(int opcode, String name, String instructionFormat, int operation) {
    super.setup(opcode, name, instructionFormat, operation);

    opcode=opcode>>11;
    YY=opcode & 0x03;
    opcode=opcode>>2;
    XX=opcode & 0x03;
   }

   public int execute(Machine machine, int address, int opcode) {
    int  src, dst, value, resolved, mask;
    long srcLong, dstLong, remainder;

    switch(operator) {
     case WMOV:
      machine.ac[YY]=machine.ac[XX];
      return copySegment(address, address+1);

     case WCOM:
      machine.ac[YY]=~machine.ac[XX];
      return copySegment(address, address+1);

     case WNEG:
      machine.ac[YY]=sub(machine, machine.ac[XX], 0);
      return copySegment(address, address+1);

     case WXCH:
      src=machine.ac[XX];
      machine.ac[XX]=machine.ac[YY];
      machine.ac[YY]=src;
      return copySegment(address, address+1);

     case WADD:
      src=machine.ac[XX];
      dst=machine.ac[YY];
      machine.ac[YY]=add(machine, src, dst);
      return copySegment(address, address+1);

     case WSUB:
      src=machine.ac[XX];
      dst=machine.ac[YY];
      machine.ac[YY]=sub(machine, src, dst);
      return copySegment(address, address+1);

     case WADC:
      src=machine.ac[XX];
      dst=machine.ac[YY];
      machine.ac[YY]=add(machine, ~src, dst);
      return copySegment(address, address+1);

     case WMUL:
      src=machine.ac[XX];
      dst=machine.ac[YY];
      machine.ac[YY]=mul(machine, src, dst);
      return copySegment(address, address+1);

     case WDIV:
      if(machine.ac[XX]==0 || (machine.ac[XX]==-1 && machine.ac[YY]==0x80000000))
       machine.ovr=1;
      else
       machine.ac[YY]=machine.ac[YY]/machine.ac[XX];
      return copySegment(address, address+1);

     case ZEX:
      src=machine.ac[XX];
      machine.ac[YY]=src & 0xFFFF;
      return copySegment(address, address+1);

     case SEX:
      src=machine.ac[XX];
      machine.ac[YY]=(src<<16)>>16;
      return copySegment(address, address+1);

     case CVWN:
      src=machine.ac[YY];
      machine.ac[YY]=(src<<16)>>16;
      src=src>>15;
      machine.ovr|=(src!=0 && src!=-1)?1:0;
      return copySegment(address, address+1);

     case WINC:
      src=machine.ac[XX];
      machine.ac[YY]=add(machine, 1, src);
      return copySegment(address, address+1);

     case WADI:
      src=machine.ac[YY];
      machine.ac[YY]=add(machine, XX+1, src);
      return copySegment(address, address+1);

     case WSBI:
      src=machine.ac[YY];
      machine.ac[YY]=sub(machine, XX+1, src);
      return copySegment(address, address+1);

     case WLSI:
      machine.ac[YY]=logicalShift(machine, machine.ac[YY], XX+1);
      return copySegment(address, address+1);

     case WAND:
      src=machine.ac[XX];
      dst=machine.ac[YY];
      machine.ac[YY]=src & dst;
      return copySegment(address, address+1);

     case WIOR:
      src=machine.ac[XX];
      dst=machine.ac[YY];
      machine.ac[YY]=src | dst;
      return copySegment(address, address+1);

     case WXOR:
      src=machine.ac[XX];
      dst=machine.ac[YY];
      machine.ac[YY]=src ^ dst;
      return copySegment(address, address+1);

     case NADD:
      src=machine.ac[XX];
      dst=machine.ac[YY];
      machine.ac[YY]=narrowAdd(machine, src, dst);
      return copySegment(address, address+1);

     case NSUB:
      src=machine.ac[XX];
      dst=machine.ac[YY];
      machine.ac[YY]=narrowSub(machine, src, dst);
      return copySegment(address, address+1);

     case NNEG:
      src=machine.ac[XX];
      machine.ac[YY]=narrowSub(machine, 0, src);
      return copySegment(address, address+1);

     case NMUL:
      src=machine.ac[XX];
      dst=machine.ac[YY];
      machine.ac[YY]=narrowMul(machine, src, dst);
      return copySegment(address, address+1);

     case NADI:
      src=machine.ac[YY];
      machine.ac[YY]=narrowAdd(machine, XX+1, src);
      return copySegment(address, address+1);

     case NSBI:
      src=machine.ac[YY];
      machine.ac[YY]=narrowSub(machine, XX+1, src);
      return copySegment(address, address+1);

     case NADDI:
      src=machine.memory.readWord(copySegment(address, address+1));
      dst=machine.ac[YY];
      machine.ac[YY]=narrowAdd(machine, src, dst);
      return copySegment(address, address+2);

     case WHLV:
      machine.ac[YY]=machine.ac[YY]>>1;
      return copySegment(address, address+1);

     case WMOVR:
      machine.ac[YY]=machine.ac[YY]>>>1;
      return copySegment(address, address+1);

     case ADDI:
      src=machine.memory.readWord(copySegment(address, address+1));
      machine.ac[YY]=(machine.ac[YY]+src) & 0xFFFF;
      return copySegment(address, address+2);

     case ANDI:
      src=machine.memory.readWord(copySegment(address, address+1));
      machine.ac[YY]=machine.ac[YY] & src;
      return copySegment(address, address+2);

     case WSEQ:
      src=machine.ac[XX];
      if(XX!=YY)
       dst=machine.ac[YY];
      else
       dst=0;
      return copySegment(address, address+((src==dst)?2:1));

     case WSNE:
      src=machine.ac[XX];
      if(XX!=YY)
       dst=machine.ac[YY];
      else
       dst=0;
      return copySegment(address, address+((src!=dst)?2:1));

     case WSLT:
      src=machine.ac[XX];
      if(XX!=YY)
       dst=machine.ac[YY];
      else
       dst=0;
      return copySegment(address, address+((src<dst)?2:1));

     case WSLE:
      src=machine.ac[XX];
      if(XX!=YY)
       dst=machine.ac[YY];
      else
       dst=0;
      return copySegment(address, address+((src<=dst)?2:1));

     case WSGT:
      src=machine.ac[XX];
      if(XX!=YY)
       dst=machine.ac[YY];
      else
       dst=0;
      return copySegment(address, address+((src>dst)?2:1));

     case WSGE:
      src=machine.ac[XX];
      if(XX!=YY)
       dst=machine.ac[YY];
      else
       dst=0;
      return copySegment(address, address+((src>=dst)?2:1));

     case WUSGT:
      srcLong=machine.ac[XX];
      srcLong=srcLong & 0xFFFFFFFFl;
      if(XX!=YY) {
       dstLong=machine.ac[YY];
       dstLong=dstLong & 0xFFFFFFFFl;
      }
      else
       dstLong=0;
      return copySegment(address, address+((srcLong>dstLong)?2:1));

     case WUSGE:
      srcLong=machine.ac[XX];
      srcLong=srcLong & 0xFFFFFFFFl;
      if(XX!=YY) {
       dstLong=machine.ac[YY];
       dstLong=dstLong & 0xFFFFFFFFl;
      }
      else
       dstLong=0;
      return copySegment(address, address+((srcLong>=dstLong)?2:1));

     case WSEQI:
      dst=machine.ac[YY];
      value=machine.memory.readWord(copySegment(address, address+1));
      value=(value<<16)>>16;
      return copySegment(address, address+((dst==value)?3:2));

     case WSNEI:
      dst=machine.ac[YY];
      value=machine.memory.readWord(copySegment(address, address+1));
      value=(value<<16)>>16;
      return copySegment(address, address+((dst!=value)?3:2));

     case WSLEI:
      dst=machine.ac[YY];
      value=machine.memory.readWord(copySegment(address, address+1));
      value=(value<<16)>>16;
      return copySegment(address, address+((dst<=value)?3:2));

     case WSGTI:
      dst=machine.ac[YY];
      value=machine.memory.readWord(copySegment(address, address+1));
      value=(value<<16)>>16;
      return copySegment(address, address+((dst>value)?3:2));

     case NSANA:
      src=machine.memory.readWord(copySegment(address, address+1));
      return copySegment(address, address+(((machine.ac[YY] & src)==0)?2:3));

     case WSANA:
      src=machine.memory.readWide(copySegment(address, address+1));
      return copySegment(address, address+(((machine.ac[YY] & src)==0)?3:4));

     case WSKBZ:
      value=((opcode>>10) & 0x1C) | ((opcode>>4) & 0x03);
      src=(machine.ac[0]>>(31-value)) & 0x01;
      return copySegment(address, address+((src==0)?2:1));

     case WSKBO:
      value=((opcode>>10) & 0x1C) | ((opcode>>4) & 0x03);
      src=(machine.ac[0]>>(31-value)) & 0x01;
      return copySegment(address, address+((src==1)?2:1));

     case WBTZ:
      if(XX==YY)
       resolved=copySegment(address, 0);
      else
       resolved=machine.eagleResolveIndirect(machine.ac[XX]);
      resolved=copySegment(address, resolved+(machine.ac[YY]>>>4));
      src=machine.memory.readWord(resolved);
      src=src & ~(0x8000>>(machine.ac[YY]&0x0F));
      machine.memory.writeWord(resolved, src);
      return copySegment(address, address+1);

     case WBTO:
      if(XX==YY)
       resolved=copySegment(address, 0);
      else
       resolved=machine.eagleResolveIndirect(machine.ac[XX]);
      resolved=copySegment(address, resolved+(machine.ac[YY]>>>4));
      src=machine.memory.readWord(resolved);
      src=src | (0x8000>>(machine.ac[YY]&0x0F));
      machine.memory.writeWord(resolved, src);
      return copySegment(address, address+1);

     case WSZB:
      if(XX==YY)
       resolved=copySegment(address, 0);
      else
       resolved=machine.eagleResolveIndirect(machine.ac[XX]);
      resolved=copySegment(address, resolved+(machine.ac[YY]>>>4));
      src=(machine.memory.readWord(resolved)>>15-(machine.ac[YY] & 0x0F)) & 0x01;
      return copySegment(address, address+((src==0)?2:1));

     case WSZBO:
      // FIX FIX FIX -- should be atomic
      if(XX==YY)
       resolved=copySegment(address, 0);
      else
       resolved=machine.eagleResolveIndirect(machine.ac[XX]);
      resolved=copySegment(address, resolved+(machine.ac[YY]>>>4));
      mask=0x8000>>(machine.ac[YY] & 0x0F);
      src=machine.memory.readWord(resolved);
      machine.memory.writeWord(resolved, src|mask);
      src=src&mask;
      return copySegment(address, address+((src==0)?2:1));

     case WLOB:
      src=machine.ac[XX];
      if(src==0)
       machine.ac[YY]+=32;
      else
       while((src & 0x80000000)==0) {
        machine.ac[YY]++;
        src=src<<1;
       }
      return copySegment(address, address+1);

     case NLDAI:
      value=machine.memory.readWord(copySegment(address, address+1));
      machine.ac[YY]=(value<<16)>>16;
      return copySegment(address, address+2);

     case WNADI:
      src=machine.ac[YY];
      value=machine.memory.readWord(copySegment(address, address+1));
      value=(value<<16)>>16;
      machine.ac[YY]=add(machine, value, src);
      return copySegment(address, address+2);

     case WLSH:
      machine.ac[YY]=logicalShift(machine, machine.ac[YY], machine.ac[XX]);
      return copySegment(address, address+1);

     case WLSHI:
      value=machine.memory.readWord(copySegment(address, address+1));
      value=(value<<24)>>24;
      machine.ac[YY]=logicalShift(machine, machine.ac[YY], value);
      return copySegment(address, address+2);

     case WLDAI:
      src=machine.memory.readWide(copySegment(address, address+1));
      machine.ac[YY]=src;
      return copySegment(address, address+3);

     case WADDI:
      src=machine.memory.readWide(copySegment(address, address+1));
      dst=machine.ac[YY];
      machine.ac[YY]=add(machine, src, dst);
      return copySegment(address, address+3);

     case WANDI:
      src=machine.memory.readWide(copySegment(address, address+1));
      dst=machine.ac[YY];
      machine.ac[YY]=src & dst;
      return copySegment(address, address+3);

     case WIORI:
      src=machine.memory.readWide(copySegment(address, address+1));
      dst=machine.ac[YY];
      machine.ac[YY]=src | dst;
      return copySegment(address, address+3);

     case WXORI:
      src=machine.memory.readWide(copySegment(address, address+1));
      dst=machine.ac[YY];
      machine.ac[YY]=src ^ dst;
      return copySegment(address, address+3);

     case WUGTI:
      srcLong=machine.memory.readWide(copySegment(address, address+1));
      srcLong=srcLong & 0xFFFFFFFFl;
      dstLong=machine.ac[YY];
      dstLong=dstLong & 0xFFFFFFFFl;
      return copySegment(address, address+((dstLong>srcLong)?4:3));

     case WULEI:
      srcLong=machine.memory.readWide(copySegment(address, address+1));
      srcLong=srcLong & 0xFFFFFFFFl;
      dstLong=machine.ac[YY];
      dstLong=dstLong & 0xFFFFFFFFl;
      return copySegment(address, address+((dstLong<=srcLong)?4:3));

     case XNADD:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), XX);
      src=machine.memory.readWord(resolved);
      dst=machine.ac[YY];
      machine.ac[YY]=narrowAdd(machine, src, dst);
      return copySegment(address, address+2);

     case LNADD:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), XX);
      src=machine.memory.readWord(resolved);
      dst=machine.ac[YY];
      machine.ac[YY]=narrowAdd(machine, src, dst);
      return copySegment(address, address+3);

     case XNSUB:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), XX);
      src=machine.memory.readWord(resolved);
      dst=machine.ac[YY];
      machine.ac[YY]=narrowSub(machine, src, dst);
      return copySegment(address, address+2);

     case LNSUB:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), XX);
      src=machine.memory.readWord(resolved);
      dst=machine.ac[YY];
      machine.ac[YY]=narrowSub(machine, src, dst);
      return copySegment(address, address+3);

     case XNMUL:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      src=machine.memory.readWord(resolved);
      machine.ac[YY]=narrowMul(machine, src, machine.ac[YY]);
      return copySegment(address, address+2);

     case LNMUL:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      src=machine.memory.readWord(resolved);
      machine.ac[YY]=narrowMul(machine, src, machine.ac[YY]);
      return copySegment(address, address+3);

     case XNADI:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), YY);
      src=machine.memory.readWord(resolved);
      src=narrowAdd(machine, XX+1, src);
      machine.memory.writeWord(resolved, src);
      return copySegment(address, address+2);

     case LNADI:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), YY);
      src=machine.memory.readWord(resolved);
      src=narrowAdd(machine, XX+1, src);
      machine.memory.writeWord(resolved, src);
      return copySegment(address, address+3);

     case XNSBI:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), YY);
      src=machine.memory.readWord(resolved);
      src=narrowSub(machine, XX+1, src);
      machine.memory.writeWord(resolved, src);
      return copySegment(address, address+2);

     case LNSBI:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), YY);
      src=machine.memory.readWord(resolved);
      src=narrowSub(machine, XX+1, src);
      machine.memory.writeWord(resolved, src);
      return copySegment(address, address+3);

     case XNDSZ:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), YY);
      src=machine.memory.readWord(resolved);
      src=(src-1) & 0xFFFF;
      machine.memory.writeWord(resolved, src);
      return copySegment(address, address+((src==0)?3:2));

     case XNISZ:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), YY);
      src=machine.memory.readWord(resolved);
      src=(src+1) & 0xFFFF;
      machine.memory.writeWord(resolved, src);
      return copySegment(address, address+((src==0)?3:2));

     case XWISZ:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), YY);
      src=machine.memory.readWide(resolved);
      src++;
      machine.memory.writeWide(resolved, src);
      return copySegment(address, address+((src==0)?3:2));

     case XWADD:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      src=machine.memory.readWide(resolved);
      machine.ac[YY]=add(machine, src, machine.ac[YY]);
      return copySegment(address, address+2);

     case LWADD:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      src=machine.memory.readWide(resolved);
      machine.ac[YY]=add(machine, src, machine.ac[YY]);
      return copySegment(address, address+3);

     case XWSUB:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      src=machine.memory.readWide(resolved);
      machine.ac[YY]=sub(machine, src, machine.ac[YY]);
      return copySegment(address, address+2);

     case LWSUB:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      src=machine.memory.readWide(resolved);
      machine.ac[YY]=sub(machine, src, machine.ac[YY]);
      return copySegment(address, address+3);

     case XWADI:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), YY);
      src=machine.memory.readWide(resolved);
      src=add(machine, XX+1, src);
      machine.memory.writeWide(resolved, src);
      return copySegment(address, address+2);

     case XWSBI:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), YY);
      src=machine.memory.readWide(resolved);
      src=sub(machine, XX+1, src);
      machine.memory.writeWide(resolved, src);
      return copySegment(address, address+2);

     case XWMUL:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      src=machine.memory.readWide(resolved);
      machine.ac[YY]=mul(machine, src, machine.ac[YY]);
      return copySegment(address, address+2);

     case LWMUL:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      src=machine.memory.readWide(resolved);
      machine.ac[YY]=mul(machine, src, machine.ac[YY]);
      return copySegment(address, address+3);

     case DIV:
      if((machine.ac[0] & 0xFFFF)>=(machine.ac[2] & 0xFFFF) || (machine.ac[2] & 0xFFFF)==0) {
       machine.c=1;
       return copySegment(address, address+1);
      }
      src=(machine.ac[0]<<16) | (machine.ac[1] & 0xFFFF);
      machine.ac[1]=src/(machine.ac[2] & 0xFFFF);
      machine.ac[0]=src%(machine.ac[2] & 0xFFFF);
      return copySegment(address, address+1);

     case DIVX:
      src=(machine.ac[1]<<16)>>16;
      dst=machine.ac[2] & 0xFFFF;
      if(dst==0 || (dst==0xFFFF && src==0xFFFF8000)) {
       machine.ac[0]=(src>>15) & 0xFFFF;
       machine.c=1;
      }
      else {
       machine.ac[1]=src/dst;
       machine.ac[0]=src%dst;
       machine.c=0;
      }
      return copySegment(address, address+1);


     case WDIVS:
      if(machine.ac[2]==0) {
       machine.ovr=1;
       return copySegment(address, address+1);
      }
      srcLong=machine.ac[0];
      srcLong=srcLong & 0xFFFFFFFFl;
      dstLong=machine.ac[1];
      dstLong=dstLong & 0xFFFFFFFFl;
//      dstLong=(dstLong<<32)+srcLong;
      dstLong=(srcLong<<32)+dstLong;
      srcLong=machine.ac[2];
      remainder=dstLong%srcLong;
      dstLong=dstLong/srcLong;
      if((dstLong>>31)!=0 && (dstLong>>31)!=-1) {
       machine.ovr=1;
       return copySegment(address, address+1);
      }
      machine.ac[1]=(int)dstLong;
      machine.ac[0]=(int)remainder;
      return copySegment(address, address+1);
    }

    throw new RuntimeException("Internal error - some case is not returning");
   }
}
