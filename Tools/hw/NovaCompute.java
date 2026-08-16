package hw;

public class NovaCompute extends Instruction {
   static public final int COM=0;
   static public final int NEG=1;
   static public final int MOV=2;
   static public final int INC=3;
   static public final int ADC=4;
   static public final int SUB=5;
   static public final int ADD=6;
   static public final int AND=7;

   public int execute(Machine machine, int address, int opcode) {
    int XX, YY, OOO, SS, CC, N, KKK;
    int src, dst, c, segment;

    KKK=opcode & 0x07;
    opcode=opcode>>3;
    N=opcode & 0x01;
    opcode=opcode>>1;
    CC=opcode & 0x03;
    opcode=opcode>>2;
    SS=opcode & 0x03;
    opcode=opcode>>2;
    OOO=opcode & 0x07;
    opcode=opcode>>3;
    YY=opcode & 0x03;
    opcode=opcode>>2;
    XX=opcode & 0x03;

    c=machine.c<<16;
    src=machine.ac[XX] & 0xFFFF;
    dst=machine.ac[YY] & 0xFFFF;

    switch(CC) {
     case 0:
      // no action
      src=src | c;
      break;

     case 1:
      // clear carry
      break;

     case 2:
      // set carry
      src=src | 0x10000;
      break;

     case 3:
      // flip carry
      src=(src | c)^0x10000;
      break;
    }

    switch(operator) {
     case 0:
      // COM
      src=src^0xFFFF;
      break;

     case 1:
      // NEG
      src=(src^0xFFFF)+1;
      break;

     case 2:
      // MOV
      break;

     case 3:
      // INC
      src=src+1;
      break;

     case 4:
      // ADC
      src=(src^0xFFFF)+dst;
      break;

     case 5:
      // SUB
      src=(src^0xFFFF)+dst+1;
      break;

     case 6:
      // ADD
      src=src+dst;
      break;

     case 7:
      // AND
      src=src & (dst | 0x10000);
      break;
    }

    switch(SS) {
     case 0:
      c=(src>>16) & 0x01;
      src=src & 0xFFFF;
      break;

     case 1:
      c=(src>>15) & 0x01;
      src=(src & 0xFFFF)<<1 | ((src>>16) & 0x01);
      break;

     case 2:
      c=src & 0x01;
      src=(src>>1) & 0xFFFF;
      break;

     case 3:
      c=(src>>16) & 0x01;
      src=((src & 0xFF)<<8) | ((src & 0xFF00)>>8);
      break;
    }

    if(N==0) {
     machine.c=c;
     machine.ac[YY]=src;
    }

    switch(KKK) {
     case 0:
      // never skip
      break;

     case 1:
      return copySegment(address, address+2);

     case 2:
      // skip if CARRY is 0
      if(c==0)
       return copySegment(address, address+2);
      break;

     case 3:
      // skip if CARRY is 1
      if(c==1)
       return copySegment(address, address+2);
      break;

     case 4:
      // skip if result is 0
      if(src==0)
       return copySegment(address, address+2);
      break;

     case 5:
      // skip if result is not 0
      if(src!=0)
       return copySegment(address, address+2);
      break;

     case 6:
      // skip if carry=0 or result=0
      if(c==0 || src==0)
       return copySegment(address, address+2);
      break;

     case 7:
      // skip if carry!=0 and result!=0
      if(c==1 && src!=0)
       return copySegment(address, address+2);
      break;
    }

    return copySegment(address, address+1);
   }
}
