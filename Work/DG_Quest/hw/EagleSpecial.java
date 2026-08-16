package hw;

public class EagleSpecial extends Instruction {
   static public final int WBLM=0;
   static public final int WCMV=1;
   static public final int WCMP=2;
   static public final int WCST=3;
   static public final int WMESS=10;
   static public final int ENQT=11;
   static public final int DEQUE=12;

   public int direction(int count) {
    if(count>0)
     return -1;
    else if(count<0)
     return 1;
    else
     return 0;
   }

   public int execute(Machine machine, int address, int opcode) {
    int dst, src, dstCount, srcCount, dstDirection, srcDirection, copy, dstByte, srcByte, segment, result;
    int prev, next, mask;

    switch(operator) {
     case WBLM:
//      System.out.printf("  ac1=%08X\n", machine.ac[1]);
//      System.out.printf("  ac2=%08X\n", machine.ac[2]);
//      System.out.printf("  ac3=%08X\n", machine.ac[3]);
      segment=getSegment(address);
      srcCount=machine.ac[1];
      srcDirection=direction(srcCount);
      src=machine.ac[2];
      dst=machine.ac[3];
      if((src & 0x80000000)!=0 || (dst & 0x80000000)!=0)
       throw new RuntimeException("WBLM instruction with indirection!");
      while(srcCount!=0) {
       if(segment!=getSegment(src) || segment!=getSegment(dst))
        throw new RuntimeException("WBLM: crossing segments not allowed");
       copy=machine.memory.readWord(src);
       machine.memory.writeWord(dst, copy);
       srcCount+=srcDirection;
       src=src-srcDirection;
       dst=dst-srcDirection;
      }
      machine.ac[1]=0;
      machine.ac[2]=src;
      machine.ac[3]=dst;
      return copySegment(address, address+1);

     case WCMV:
//      System.out.printf("  ac0=%08X\n", machine.ac[0]);
//      System.out.printf("  ac1=%08X\n", machine.ac[1]);
//      System.out.printf("  ac2=%08X\n", machine.ac[2]);
//      System.out.printf("  ac3=%08X   [", machine.ac[3]);
      for(int index=0;index<machine.ac[1] && index<32;index++) {
       int value=machine.memory.readByte(machine.ac[3]+index);
       if(value>=32 && value<127)
        System.out.print((char)value);
       else
        System.out.print(" ");
      }
      System.out.println("]");

      segment=getSegment(address);
      dstCount=machine.ac[0];
      srcCount=machine.ac[1];
      dst=machine.ac[2];
      src=machine.ac[3];
      dstDirection=direction(dstCount);
      srcDirection=direction(srcCount);
      while(dstCount!=0) {
       if(srcCount==0)
        copy=' ';
       else {
        if(segment!=getByteSegment(src))
         throw new RuntimeException("WCMV: crossing segments not allowed");
        copy=machine.memory.readByte(src);
        srcCount=srcCount+srcDirection;
        src=src-srcDirection;
       }
       if(segment!=getByteSegment(dst))
        throw new RuntimeException("WCMV: crossing segments not allowed");
       machine.memory.writeByte(dst, copy);
       dstCount=dstCount+dstDirection;
       dst=dst-dstDirection;
      }
      machine.ac[0]=dstCount;
      machine.ac[1]=srcCount;
      machine.ac[2]=dst;
      machine.ac[3]=src;
      if(srcCount!=0)
       machine.c=1;
      else
       machine.c=0;
      return copySegment(address, address+1);

     case WCMP:
//      System.out.printf("  ac0=%08X\n", machine.ac[0]);
//      System.out.printf("  ac1=%08X\n", machine.ac[1]);
//      System.out.printf("  ac2=%08X\n", machine.ac[2]);
//      System.out.printf("  ac3=%08X\n", machine.ac[3]);
      segment=getSegment(address);
      dstCount=machine.ac[0];
      srcCount=machine.ac[1];
      dst=machine.ac[2];
      src=machine.ac[3];
      dstDirection=direction(dstCount);
      srcDirection=direction(srcCount);
      result=0;
      while(dstCount!=0 || srcCount!=0) {
       if(srcCount==0)
        srcByte=' ';
       else {
        if(segment!=getByteSegment(src))
         throw new RuntimeException("WCMP: crossing segments not allowed");
        srcByte=machine.memory.readByte(src);
        srcCount=srcCount+srcDirection;
        src=src-srcDirection;
       }
       if(dstCount==0)
        dstByte=' ';
       else {
        if(segment!=getByteSegment(dst))
         throw new RuntimeException("WCMP: crossing segments not allowed");
        dstByte=machine.memory.readByte(dst);
        dstCount=dstCount+dstDirection;
        dst=dst-dstDirection;
       }
       if(srcByte<dstByte) {
        result=-1;
        break;
       }
       if(srcByte>dstByte) {
        result=1;
        break;
       }
      }
      machine.ac[0]=dstCount;
      machine.ac[1]=result;
      machine.ac[2]=dst;
      machine.ac[3]=src;
      return copySegment(address, address+1);

     case WCST:
      System.out.println("WCST");
      System.out.printf("  ac0=%08X   [", machine.ac[0]);
      for(int index=0;index<16;index++) {
       int value=machine.memory.readWord(machine.ac[0]+index);
       if(index<15)
        System.out.printf("%04X ", value);
       else
        System.out.printf("%04X]\n", value);
      }
      System.out.printf("  ac1=%08X\n", machine.ac[1]);
      System.out.printf("  ac3=%08X   [", machine.ac[3]);
      for(int index=0;index<machine.ac[1] && index<32;index++) {
       int value=machine.memory.readByte(machine.ac[3]+index);
       if(value>=32 && value<127)
        System.out.print((char)value);
       else
        System.out.print(" ");
      }
      System.out.println("]");
      if((machine.ac[0] & 0x80000000)!=0)
       throw new RuntimeException("WCST instruction with indirection");
      src=machine.ac[3];
      srcCount=machine.ac[1];
      srcDirection=direction(machine.ac[1]);
      while(srcCount!=0) {
       srcByte=machine.memory.readByte(src);
       mask=0x8000>>(srcByte & 0x0F);
       if((machine.memory.readWord(copySegment(machine.ac[0], machine.ac[0]+(srcByte>>4))) & mask)!=0)
        break;
       srcCount=srcCount+srcDirection;
       src=src-srcDirection;
      }
      machine.ac[1]=srcCount;
      machine.ac[3]=src;
//      System.out.printf("  ac0=%08X\n", machine.ac[0]);
//      System.out.printf("  ac1=%08X\n", machine.ac[1]);
//      System.out.printf("  ac3=%08X\n", machine.ac[3]);
      return copySegment(address, address+1);

     case WMESS:
      // FIX FIX FIX -- should be atomic
      if((machine.ac[2] & 0x80000000)!=0)
       throw new RuntimeException("WMESS indirection");
      src=machine.memory.readWide(machine.ac[2]);
      if(((src ^ machine.ac[0]) & machine.ac[3])==0) {
       machine.memory.writeWide(machine.ac[2], machine.ac[1]);
       machine.ac[1]=src;
       return copySegment(address, address+2);
      }
      else {
       machine.ac[1]=src;
       return copySegment(address, address+1);
      }

     case ENQT:
      if(machine.ac[1]==-1) {
       int tail=machine.memory.readWide(machine.ac[0]+2);

       if(tail==-1) {
        machine.memory.writeWide(machine.ac[0], machine.ac[2]);
        machine.memory.writeWide(machine.ac[0]+2, machine.ac[2]);
        machine.memory.writeWide(machine.ac[2], -1);
        machine.memory.writeWide(machine.ac[2]+2, -1);
        return copySegment(address, address+1);
       }
       else {
        machine.memory.writeWide(machine.ac[0]+2, machine.ac[2]);
        machine.memory.writeWide(tail+2, machine.ac[2]);
        machine.memory.writeWide(machine.ac[2], tail);
        machine.memory.writeWide(machine.ac[2]+2, -1);
       }
      }
      else {
       int tail=machine.memory.readWide(machine.ac[0]+2);

       if(tail==machine.ac[1]) {
        machine.memory.writeWide(machine.ac[1]+2, machine.ac[2]);
        machine.memory.writeWide(machine.ac[0]+2, machine.ac[2]);
        machine.memory.writeWide(machine.ac[2], tail);
        machine.memory.writeWide(machine.ac[2]+2, -1);
       }
       else {
        next=machine.memory.readWide(machine.ac[1]+2);

        machine.memory.writeWide(machine.ac[2]+2, next);
        machine.memory.writeWide(machine.ac[1]+2, machine.ac[2]);
        machine.memory.writeWide(machine.ac[2], machine.ac[1]);
        machine.memory.writeWide(next, machine.ac[2]);
       }
      }
      return copySegment(address, address+2);

     case DEQUE:
      if(machine.ac[1]==-1)
       machine.ac[1]=machine.memory.readWide(machine.ac[0]);
      if(machine.ac[1]==-1)
       return copySegment(address, address+1);

      prev=machine.memory.readWide(machine.ac[1]);
      next=machine.memory.readWide(machine.ac[1]+2);
      if(prev==-1)
       machine.memory.writeWide(machine.ac[0], next);
      else
       machine.memory.writeWide(prev+2, next);
      if(next==-1)
       machine.memory.writeWide(machine.ac[0]+2, prev);
      else
       machine.memory.writeWide(next, prev);
      if(next==-1 && prev==-1)
       return copySegment(address, address+1);
      return copySegment(address, address+2);
    }

    throw new RuntimeException("Internal error - some case is not returning");
   }
}
