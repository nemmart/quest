package hw;

public class EagleGeneral extends EagleInstruction {
   static public final int XWLDA=10;
   static public final int XWSTA=11;
   static public final int LWLDA=12;
   static public final int LWSTA=13;

   static public final int XNLDA=20;
   static public final int XNSTA=21;
   static public final int LNLDA=22;
   static public final int LNSTA=23;

   static public final int XLEF=40;
   static public final int LLEF=41;

   static public final int XLEFB=42;
   static public final int LLEFB=43;
   static public final int XLDB=44;
   static public final int XSTB=45;
   static public final int LLDB=46;
   static public final int LSTB=47;

   static public final int WLDB=50;
   static public final int WSTB=51;

   static public final int LPSR=90;
   static public final int CRYTO=91;
   static public final int CRYTZ=92;

   static public final int XJMP=100;
   static public final int XJSR=101;
   static public final int XNDO=102;
   static public final int XWDO=103;

   static public final int LJMP=110;
   static public final int LJSR=111;
   static public final int LNDO=112;
   static public final int LWDO=113;
   static public final int LDSP=114;
   static public final int WCLM=115;

   static public final int WBR=200;

   public int II;
   public int AA;

   public void setup(int opcode, String name, String instructionFormat, int operation) {
    super.setup(opcode, name, instructionFormat, operation);

    opcode=opcode>>11;
    AA=opcode & 0x03;
    opcode=opcode>>2;
    II=opcode & 0x03;
   }

   public int execute(Machine machine, int address, int opcode) {
    int resolved, src, L, H, value;

    switch(operator) {
     case XWLDA:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), II);
//System.out.printf("resolved=%08X\n", resolved);
      src=machine.memory.readWide(resolved);
//System.out.printf("read=%08X\n", src);
      machine.ac[AA]=src;
      return copySegment(address, address+2);

     case XWSTA:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), II);
      src=machine.ac[AA];
      machine.memory.writeWide(resolved, src);
      return copySegment(address, address+2);

     case LWLDA:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), II);
      machine.ac[AA]=machine.memory.readWide(resolved);
      return copySegment(address, address+3);

     case LWSTA:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), II);
      src=machine.ac[AA];
      machine.memory.writeWide(resolved, src);
      return copySegment(address, address+3);

     case XNLDA:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), II);
//System.out.printf("resolved=%08X\n", resolved);
      src=machine.memory.readWord(resolved);
//System.out.printf("read=%04X\n", src);
      src=(src<<16)>>16;
      machine.ac[AA]=src;
      return copySegment(address, address+2);

     case XNSTA:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), II);
      src=machine.ac[AA];
      machine.memory.writeWord(resolved, src&0xFFFF);
      return copySegment(address, address+2);

     case LNLDA:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), II);
//System.out.printf("resolved=%08X\n", resolved);
      src=machine.memory.readWord(resolved);
      src=(src<<16)>>16;
      machine.ac[AA]=src;
      return copySegment(address, address+3);

     case LNSTA:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), II);
      src=machine.ac[AA];
      machine.memory.writeWord(resolved, src & 0xFFFF);
      return copySegment(address, address+3);

     case XLEF:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), II);
//System.out.printf("resolved=%08X\n", resolved);
      machine.ac[AA]=resolved;
      return copySegment(address, address+2);

     case LLEF:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), II);
      machine.ac[AA]=resolved;
      return copySegment(address, address+3);

     case XLEFB:
      resolved=machine.eagleXByteIndexed(copySegment(address, address+1), II);
//System.out.printf("Byte address: %08X\n", resolved);
      machine.ac[AA]=resolved;
      return copySegment(address, address+2);

     case LLEFB:
      resolved=machine.eagleLByteIndexed(copySegment(address, address+1), II);
//System.out.printf("Byte address: %08X\n", resolved);
      machine.ac[AA]=resolved;
      return copySegment(address, address+3);

     case XLDB:
      resolved=machine.eagleXByteIndexed(copySegment(address, address+1), II);
      machine.ac[AA]=machine.memory.readByte(resolved);
      return copySegment(address, address+2);

     case XSTB:
      resolved=machine.eagleXByteIndexed(copySegment(address, address+1), II);
      machine.memory.writeByte(resolved, machine.ac[AA] & 0xFF);
      return copySegment(address, address+2);

     case LLDB:
      resolved=machine.eagleLByteIndexed(copySegment(address, address+1), II);
      machine.ac[AA]=machine.memory.readByte(resolved);
      return copySegment(address, address+3);

     case LSTB:
      resolved=machine.eagleLByteIndexed(copySegment(address, address+1), II);
      machine.memory.writeByte(resolved, machine.ac[AA] & 0xFF);
      return copySegment(address, address+3);

     case WLDB:
      machine.ac[AA]=machine.memory.readByte(machine.ac[II]);
      return copySegment(address, address+1);

     case WSTB:
      machine.memory.writeByte(machine.ac[II], machine.ac[AA] & 0xFF);
      return copySegment(address, address+1);

     case LPSR:
      machine.ac[0]=machine.getPSR()<<16;
      return copySegment(address, address+1);

     case CRYTO:
      machine.c=1;
      return copySegment(address, address+1);

     case CRYTZ:
      machine.c=0;
      return copySegment(address, address+1);

     case XJMP:
      // indexing is in AA
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), AA);
      return resolved;

     case XJSR:
      // indexing is in AA
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), AA);
      machine.ac[3]=copySegment(address, address+2);
      return resolved;

     case XNDO:
      // amazingly, indexing is in AA, register is in II
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), AA);
//System.out.printf("resolved=%08X\n", resolved);
      src=machine.memory.readWord(resolved);
      src=narrowAdd(machine, 1, src);
      machine.memory.writeWord(resolved, src);
      if(src>machine.ac[II]) {
       machine.ac[II]=src;
       value=machine.memory.readWord(copySegment(address, address+2));
       return copySegment(address, address+1+value);
      }
      machine.ac[II]=src;
      return copySegment(address, address+3);

     case XWDO:
      // amazingly, indexing is in AA, register is in II
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), AA);
      src=machine.memory.readWide(resolved);
      src=add(machine, 1, src);
      machine.memory.writeWide(resolved, src);
      if(src>machine.ac[II]) {
       machine.ac[II]=src;
       value=machine.memory.readWord(copySegment(address, address+2));
       return copySegment(address, address+1+value);
      }
      machine.ac[II]=src;
      return copySegment(address, address+3);

     case LJMP:
      // indeding is in AA
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), AA);
      return resolved;

     case LJSR:
      // indeding is in AA
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), AA);
      machine.ac[3]=copySegment(address, address+3);
      return resolved;

     case LNDO:
      // amazingly, indexing is in AA, register is in II
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), AA);
      src=machine.memory.readWord(resolved);
      src=narrowAdd(machine, 1, src);
      machine.memory.writeWord(resolved, src);
      if(src>machine.ac[II]) {
       machine.ac[II]=src;
       value=machine.memory.readWord(copySegment(address, address+3));
       return copySegment(address, address+1+value);
      }
      machine.ac[II]=src;
      return copySegment(address, address+4);

     case LWDO:
      // amazingly, indexing is in AA, register is in II
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), AA);
      src=machine.memory.readWide(resolved);
      src=add(machine, 1, src);
      machine.memory.writeWide(resolved, src);
      if(src>machine.ac[II]) {
       machine.ac[II]=src;
       value=machine.memory.readWord(copySegment(address, address+3));
       return copySegment(address, address+1+value);
      }
      machine.ac[II]=src;
      return copySegment(address, address+4);

     case LDSP:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), II);
      L=machine.memory.readWide(resolved-4);
      H=machine.memory.readWide(resolved-2);
System.out.printf("L=%08X H=%08X ac=%08X\n", L, H, machine.ac[AA]);
      if(L<=machine.ac[AA] && machine.ac[AA]<=H) {
       value=machine.memory.readWide(copySegment(resolved, resolved+(machine.ac[AA]-L)*2));
System.out.printf("value=%08X\n", value);
       if(value!=-1)
        return copySegment(address, value+resolved+(machine.ac[AA]-L)*2);
      }
      return copySegment(address, address+3);

     case WCLM:
      if(II==AA) {
       resolved=copySegment(address, address+1);
       address=copySegment(address, address+4);
      }
      else
       resolved=machine.eagleResolveIndirect(machine.ac[AA]);
      L=machine.memory.readWide(resolved);
      H=machine.memory.readWide(resolved+2);
      if(L<=machine.ac[II] && machine.ac[II]<=H)
       return copySegment(address, address+2);
      return copySegment(address, address+1);

     case WBR:
      value=((opcode>>7) & 0xF0) + ((opcode>>6) & 0x0F);
      value=(value<<24)>>24;
      return copySegment(address, address+value);

    }
    throw new RuntimeException("Internal error - some case is not returning");
   }
}
