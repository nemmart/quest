package hw;

public class EagleStack extends Instruction {
   static public final int XCALL=0;
   static public final int LCALL=1;
   static public final int WSAVR=2;
   static public final int WSAVS=3;
   static public final int WSSVR=4;
   static public final int WSSVS=5;
   static public final int WRTN=6;
   static public final int WPOPB=7;

   static public final int LDASP=20;
   static public final int STASP=21;
   static public final int LDAFP=22;
   static public final int STAFP=23;
   static public final int LDASB=24;
   static public final int STASB=25;
   static public final int LDASL=26;
   static public final int STASL=27;

   static public final int LDATS=80;
   static public final int STATS=81;
   static public final int ISZTS=82;
   static public final int DSZTS=83;

   static public final int XPEF=90;
   static public final int LPEF=91;
   static public final int XPEFB=92;
   static public final int LPEFB=93;
   static public final int XPSHJ=94;
   static public final int LPSHJ=95;

   static public final int WMSP=100;
   static public final int WPSH=101;
   static public final int WPOP=102;
   static public final int WFPSH=103;
   static public final int WFPOP=104;
   static public final int WPOPJ=110;

   static public final int DERR=120;

   public int AA, XX;

   public void setup(int opcode, String name, String instructionFormat, int operation) {
    super.setup(opcode, name, instructionFormat, operation);

    opcode=opcode>>11;
    AA=opcode & 0x03;
    opcode=opcode>>2;
    XX=opcode & 0x03;
   }

   public int handleOverflow(Machine machine, int address, int nextInstruction) {
    int wsl=machine.wsl;

    if(machine.task.stackFaultHandler==0)
     throw new RuntimeException("Stack handler set to 0x0000");
    if(machine.task.stackFaultHandler==0xFFFF)
     throw new RuntimeException("Stack handler set to 0xFFFF");

    machine.wsl=machine.wsl+100;
    machine.widePush(machine.getPSR()<<16);
    machine.widePush(machine.ac[0]);
    machine.widePush(machine.ac[1]);
    machine.widePush(machine.ac[2]);
    machine.widePush(machine.ac[3]);
    machine.widePush(address | (machine.c<<31));
    machine.ovk=0;
    machine.ovr=0;
    machine.ires=0;
    machine.wsp=machine.wsp | 0x80000000;
    machine.wsl=wsl | 0x80000000;
    machine.ac[0]=address;
    machine.ac[1]=1;
    return copySegment(address, machine.task.stackFaultHandler);
   }

   public int execute(Machine machine, int address, int opcode) {
    int frameSize, resolved, arguments, value;

    switch(operator) {
     case XCALL:
      // II in in AA
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), AA);
      arguments=machine.memory.readWord(copySegment(address, address+2));
      if((arguments&0x8000)==0)
       machine.widePush((machine.getPSR()<<16)|arguments);
      else
       machine.widePush(arguments & 0x7FFF);
      machine.ac[3]=copySegment(address, address+3);
      machine.ovr=0;
      if(resolved==0x30000000)
       return resolved;
//       return machine.task.dispatchSystemCall();
      else if(getSegment(resolved)!=7)
       throw new RuntimeException("ILLEGAL CALL: " + String.format("%08X", resolved));
      machine.callStack.call(resolved, machine.ac[3], address, arguments);
      return resolved;

     case LCALL:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), AA);      // II is in the AA location
      arguments=machine.memory.readWord(copySegment(address, address+3));
      if((arguments&0x8000)==0)
       machine.widePush((machine.getPSR()<<16)|arguments);
      else
       machine.widePush(arguments & 0x7FFF);
      machine.ac[3]=copySegment(address, address+4);
      machine.ovr=0;
      if(resolved==0x30000000)
       return resolved;
//       return machine.task.dispatchSystemCall();
      else if(getSegment(resolved)!=7)
       throw new RuntimeException("ILLEGAL CALL: " + String.format("%08X", resolved));
      machine.callStack.call(resolved, machine.ac[3], address, arguments);
      return resolved;

     case WSAVR: case WSAVS:
      frameSize=machine.memory.readWord(copySegment(address, address+1));
      if(machine.wsl>0 && machine.wsp+10+frameSize*2>machine.wsl)
       return handleOverflow(machine, address, copySegment(address, address+2));
      machine.widePush(machine.ac[0]);
      machine.widePush(machine.ac[1]);
      machine.widePush(machine.ac[2]);
      machine.widePush(machine.wfp);
      machine.widePush(machine.ac[3] | (machine.c<<31));
      machine.ac[3]=machine.wsp;
      machine.wfp=machine.wsp;
      machine.wsp=machine.wsp+frameSize*2;
      machine.ovk=(operator==WSAVR)?0:1;
      machine.callStack.augment(machine.wfp, frameSize);
      return copySegment(address, address+2);

     case WSSVR: case WSSVS:
      frameSize=machine.memory.readWord(address+1);
      if(machine.wsl>0 && machine.wsp+12+frameSize*2>machine.wsl)
       return handleOverflow(machine, address, copySegment(address, address+2));

      // determine calling address
      if((machine.memory.readWord(machine.ac[3]-3) & 0xE7FF)==0xA6E9)          // LJSR instruction
       machine.callStack.call(address, machine.ac[3], machine.ac[3]-3, 0);
      else if((machine.memory.readWord(machine.ac[3]-2) & 0xE7FF)==0xC619)     // XJSR instruction
       machine.callStack.call(address, machine.ac[3], machine.ac[3]-2, 0);
      else
       machine.callStack.call(address, machine.ac[3], -1, 0);

      machine.widePush(machine.getPSR()<<16);
      machine.widePush(machine.ac[0]);
      machine.widePush(machine.ac[1]);
      machine.widePush(machine.ac[2]);
      machine.widePush(machine.wfp);
      machine.widePush(machine.ac[3] | (machine.c<<31));
      machine.ac[3]=machine.wsp;
      machine.wfp=machine.wsp;
      machine.wsp=machine.wsp+frameSize*2;
      machine.ovr=0;
      machine.ovk=(operator==WSSVR)?0:1;
      machine.callStack.augment(machine.wfp, frameSize);
      return copySegment(address, address+2);

     case WRTN:
      machine.wsp=machine.wfp;
      value=machine.widePop();
      machine.wfp=machine.widePop();
      machine.ac[2]=machine.widePop();
      machine.ac[1]=machine.widePop();
      machine.ac[0]=machine.widePop();
      frameSize=machine.widePop();

      machine.ac[3]=machine.wfp;
      machine.setPSR(frameSize>>>16);
      frameSize=frameSize&0x7FFF;
      machine.wsp=machine.wsp-2*frameSize;
      machine.c=value>>>31;
      machine.callStack.callReturn(value & 0x7FFFFFFF);
      return value & 0x7FFFFFFF;

     case WPOPB:
      value=machine.widePop();
      machine.ac[3]=machine.widePop();
      machine.ac[2]=machine.widePop();
      machine.ac[1]=machine.widePop();
      machine.ac[0]=machine.widePop();
      frameSize=machine.widePop();
      machine.setPSR(frameSize>>>16);
      machine.wsp=machine.wsp-(frameSize & 0x7FFF)*2;
      machine.c=value>>>31;
      return value & 0x7FFFFFFF;

     case LDASP:
      machine.ac[AA]=machine.wsp;
      return copySegment(address, address+1);

     case STASP:
      machine.wsp=machine.ac[AA];
      return copySegment(address, address+1);

     case LDAFP:
      machine.ac[AA]=machine.wfp;
      return copySegment(address, address+1);

     case STAFP:
      machine.wfp=machine.ac[AA];
      return copySegment(address, address+1);

     case LDASB:
      machine.ac[AA]=machine.wsb;
      return copySegment(address, address+1);

     case STASB:
      machine.wsb=machine.ac[AA];
      return copySegment(address, address+1);

     case LDASL:
      machine.ac[AA]=machine.wsl;
      return copySegment(address, address+1);

     case STASL:
      machine.wsl=machine.ac[AA];
      return copySegment(address, address+1);

     case LDATS:
      machine.ac[AA]=machine.memory.readWide(machine.wsp);
      return copySegment(address, address+1);

     case STATS:
      machine.memory.writeWide(machine.wsp, machine.ac[AA]);
      return copySegment(address, address+1);

     case ISZTS: case DSZTS:
      value=machine.memory.readWide(copySegment(address, machine.wsp));
      if(operator==ISZTS)
       value++;
      if(operator==DSZTS)
       value--;
      machine.memory.writeWide(copySegment(address, machine.wsp), value);
      if(value==0)
       return copySegment(address, address+2);
      return copySegment(address, address+1);

     case WMSP:
      // FIX FIX FIX - should call the stack fault handler
      machine.wsp=machine.wsp+2*machine.ac[AA];
      if(machine.wsp>machine.wsl)
       throw new RuntimeException("Stack fault - upper limit - abort");
      if(machine.wsp<machine.wsb)
       throw new RuntimeException("Stack fault - lower limit - abort");
      return copySegment(address, address+1);

     case WPSH:
      value=XX;
      while(true) {
       machine.widePush(machine.ac[value]);
       if(value==AA)
        break;
       value=(value+1)%4;
      }
      return copySegment(address, address+1);

     case WPOP:
      value=XX;
      while(true) {
       machine.ac[value]=machine.widePop();
       if(value==AA)
        break;
       value=(value+3)%4;
      }
      return copySegment(address, address+1);

     case WFPSH:
      machine.quadPush(Double.doubleToLongBits(machine.fplr));
      machine.quadPush(Double.doubleToLongBits(machine.fpac[0]));
      machine.quadPush(Double.doubleToLongBits(machine.fpac[1]));
      machine.quadPush(Double.doubleToLongBits(machine.fpac[2]));
      machine.quadPush(Double.doubleToLongBits(machine.fpac[3]));
      return copySegment(address, address+1);

     case WFPOP:
      machine.fpac[3]=Double.longBitsToDouble(machine.quadPop());
      machine.fpac[2]=Double.longBitsToDouble(machine.quadPop());
      machine.fpac[1]=Double.longBitsToDouble(machine.quadPop());
      machine.fpac[0]=Double.longBitsToDouble(machine.quadPop());
      machine.fplr=Double.longBitsToDouble(machine.quadPop());
      return copySegment(address, address+1);

     case XPEF:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), AA);  // II is in the AA location
      machine.widePush(resolved);
      return copySegment(address, address+2);

     case LPEF:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), AA);  // II is in the AA location
      machine.widePush(resolved);
      return copySegment(address, address+3);

     case XPEFB:
      resolved=machine.eagleXByteIndexed(copySegment(address, address+1), AA);      // II is in the AA location
      machine.widePush(resolved);
      return copySegment(address, address+2);

     case LPEFB:
      resolved=machine.eagleLByteIndexed(copySegment(address, address+1), AA);      // II is in the AA location
      machine.widePush(resolved);
      return copySegment(address, address+3);

     case XPSHJ:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), AA);  // II is in the AA location
      machine.widePush(copySegment(address, address+2));
      return resolved;

     case LPSHJ:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), AA);  // II is in the AA location
      machine.widePush(copySegment(address, address+3));
      return resolved;

     case WPOPJ:
      value=machine.widePop();
      return copySegment(address, value);

     case DERR:
System.out.println("Running DERR " + (((opcode>>10) & 0x1C) + ((opcode>>4) & 0x03)));
      machine.widePush(address);
      machine.widePush(((opcode>>10) & 0x1C) + ((opcode>>4) & 0x03));   // extract out nnn:nn
      return copySegment(address, machine.memory.readWord(copySegment(address, 39)));

    }
    throw new RuntimeException("Internal error - some case is not returning");
   }
}
