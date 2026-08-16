package hw;

import os.*;
import debug.*;

public class Machine {
   static public final int     OVK=0x01;
   static public final int     OVR=0x02;

   static public MachineThread machineThread=new MachineThread();

   public boolean     debug=false;

   public OSProcess   process;
   public OSTask      task;
   public SymbolTable symbols;
   public Memory      memory;
   public CallStack   callStack;
   public Segment[]   segments;
   public int         pc;
   public int[]       ac;
   public double[]    fpac;
   public long[]      quads;
   public double      fplr;
   public int         c;
   public int         ovk, ovr, ires, ixct, ffp, sr;
   public int         fpr;  // round bit
   public int         wsb, wsl, wsp, wfp;
   public long        instructionCount=0;

   public Machine[]   history=new Machine[1000];
   public int         nextSlot=0;

   static public int getSegment(int address) {
    return (address>>>28) & 0x07;
   }

   static public int setSegment(int segment, int address) {
    return (address & 0xFFFFFFF) | (segment<<28);
   }

   static public int copySegment(int segmentAddress, int address) {
    int segment=(segmentAddress>>28) & 0x07;

    return (address & 0xFFFFFFF) | (segment<<28);
   }

   static public int getByteSegment(int address) {
    return (address>>>29) & 0x07;
   }

   static public int setByteSegment(int segment, int address) {
    return (address & 0x1FFFFFFF) | (segment<<29);
   }

   static public int copyByteSegment(int segmentAddress, int address) {
    int segment=(segmentAddress>>29) & 0x07;

    return (address & 0x1FFFFFFF) | (segment<<29);
   }

   public Machine(Machine current) {
    this.process=null;
    this.task=null;
    this.symbols=null;
    this.memory=current.memory;

    this.pc=current.pc;
    this.ac=new int[]{current.ac[0], current.ac[1], current.ac[2], current.ac[3]};
    this.fpac=new double[]{current.fpac[0], current.fpac[1], current.fpac[2], current.fpac[3]};
    this.quads=null;
    this.fplr=current.fplr;
    this.c=current.c;
    this.ovr=current.ovr;
    this.wfp=current.wfp;
    this.wsp=current.wsp;
    this.wsb=current.wsb;
    this.wsl=current.wsl;
   }

   public Machine(OSProcess process, OSTask task, SymbolTable symbols, Memory memory) {
    this.process=process;
    this.task=task;
    this.symbols=symbols;
    this.memory=memory;

    callStack=new CallStack(symbols, this);
    segments=new Segment[8];
    for(int index=0;index<8;index++)
     segments[index]=new Segment(index<4);
    ac=new int[]{0, 0, 0, 0};
    fpac=new double[]{0.0, 0.0, 0.0, 0.0};
    quads=new long[]{0, 0, 0, 0};

    fplr=0.0;
    c=0;
    ovk=0; ovr=0; ires=0; ixct=0; ffp=0; sr=0;
    fpr=0;

    wfp=task.wfp;
    wsp=task.wsp;
    wsb=task.wsb;
    wsl=task.wsl;
   }

   public void copyState(Machine current) {
    this.pc=current.pc;
    this.ac[0]=current.ac[0];
    this.ac[1]=current.ac[1];
    this.ac[2]=current.ac[2];
    this.ac[3]=current.ac[3];
    this.fpac[0]=current.fpac[0];
    this.fpac[1]=current.fpac[1];
    this.fpac[2]=current.fpac[2];
    this.fpac[3]=current.fpac[3];
    this.fplr=current.fplr;
    this.c=current.c;
    this.ovr=current.ovr;
    this.wfp=current.wfp;
    this.wsp=current.wsp;
    this.wsb=current.wsb;
    this.wsl=current.wsl;
   }

   public void addDebug(String name) {
    callStack.debug.add(name);
   }

   public void dumpStackArea(int address, int size) {
    System.out.printf("Stack area: %08X called from %08X\n", address, pc);
    for(int index=0;index<size;index++)
     System.out.printf("   %08X %02X = %04X\n", address+index, index, memory.readWord(address+index));
   }

   public void dumpHistory() {
    Instruction instruction;
    Machine     state;
    int         next=nextSlot;

    do {
     state=history[next];
     instruction=Instruction.decode(false, state.memory.readWord(state.pc));
     System.out.printf("%08X %04X %s\n", state.pc, state.memory.readWord(state.pc), instruction.disassemble(state.memory, state.pc, state.memory.readWord(state.pc)));
     System.out.printf("   AC0=%08X\n", state.ac[0]);
     System.out.printf("   AC1=%08X\n", state.ac[1]);
     System.out.printf("   AC2=%08X\n", state.ac[2]);
     System.out.printf("   AC3=%08X\n", state.ac[3]);
     System.out.printf("   FP0=%f\n", state.fpac[0]);
     System.out.printf("   FP1=%f\n", state.fpac[1]);
     System.out.printf("   FP2=%f\n", state.fpac[2]);
     System.out.printf("   FP3=%f\n", state.fpac[3]);
     next=(next+1)%history.length;
    } while(next!=nextSlot);
   }

   public int runSteps(int address, int count) {
    Instruction instruction;
    int         opcode, newPC;
    int         startLogging=0;

    pc=address;
    while(!task.halt && count>0) {
     count--;
     opcode=memory.readInstructionWord(pc);
     instruction=Instruction.decode(segments[address>>>28].lef, opcode);
     if(instruction==null)
      throw new RuntimeException("Opcode " + String.format("%4X", opcode) + " has not been defined");
//     if(pc>=0x7015CC78 && pc <0x7015D053)
//      instruction.debugBeforeExecution(this, pc, opcode);

     newPC=instruction.execute(this, pc, opcode);
     if(newPC==0x30000000)
       return newPC;

//     if(history[nextSlot]==null)
//      history[nextSlot]=new Machine(this);
//     else
//      history[nextSlot].copyState(this);
//     if(++nextSlot==history.length)
//      nextSlot=0;

     if(ovk>0 && ovr>0) {
//      dumpHistory();
      throw new RuntimeException("Overflow occurred at " + String.format("%08X", pc));
     }

//     if(pc>=0x7015CC78 && pc <0x7015D053)
//      instruction.debugAfterExecution(this, pc, opcode);
     pc=newPC;
     instructionCount++;
     count--;
    }
    return pc;
   }

   public int run(int address) {
    int newPC;

    // Note, this method is invoked by many threads (roughly one per AOS VS Task or Process).
    // To actually run instructions, execution is handed off to the MachineThread.  When the
    // MachineThread does a system call, control is handed back to the calling thread to actually
    // run the system call.   Thus instructions are run in a single threaded environment, but
    // system calls are run in a multithreaded environment.

    pc=address;
    while(!task.halt) {
      newPC=machineThread.runSteps(this, pc, 1000);
      if(newPC==0x30000000)
       newPC=task.dispatchSystemCall();
      pc=newPC;
    }
    return pc;
   }

   public int getPSR() {
    return (ovk<<15)+(ovr<<14)+(ires<<13)+(ixct<<12)+(ffp<<11)+sr;
   }

   public void setPSR(int psr) {
    sr=psr & 0x03;
    psr=psr>>11;
    ffp=psr & 0x01;
    psr=psr>>1;
    ixct=psr & 0x01;
    psr=psr>>1;
    ires=psr & 0x01;
    psr=psr>>1;
    ovr=psr & 0x01;
    psr=psr>>1;
    ovk=psr & 0x01;
   }

   public void widePush(int wide) {
    wsp=wsp+2;
    if(wsp>wsl && (wsl & 0x80000000)==0)
     throw new RuntimeException("Stack fault - upper limit - abort, " + String.format("pc=%08X", pc));
    memory.writeWide(copySegment(pc, wsp), wide);
   }

   public int widePop() {
    int value, stackPointer=copySegment(pc, wsp);

    value=memory.readWide(stackPointer);
    wsp=wsp-2;
    if(stackPointer<wsb)
     throw new RuntimeException("Stack fault - lower limit - abort, " + String.format("pc=%08X", pc));
    return value;
   }

   public void quadPush(long quad) {
    widePush((int)(quad>>>32));
    widePush((int)(quad & 0xFFFFFFFFl));
   }

   public long quadPop() {
    long high, low;

    low=widePop();
    high=widePop();
    return (high<<32) | (low & 0xFFFFFFFFl);
   }

   public int eagleXByteIndexed(int pc, int ii) {
    int address=memory.readWord(pc);

    switch(ii) {
     case 0:
      address=setByteSegment(getSegment(pc), address);
      break;
     case 1:
      address=pc*2+((address<<16)>>16);
      break;
     case 2:
      address=ac[2]*2+((address<<16)>>16);
      address=setByteSegment(getSegment(pc), address);
      break;
     case 3:
      address=ac[3]*2+((address<<16)>>16);
      address=setByteSegment(getSegment(pc), address);
      break;
    }
    return address;
   }

   public int eagleLByteIndexed(int pc, int ii) {
    int address=memory.readWide(pc);

    switch(ii) {
     case 1:
      address=pc*2+address;
      break;
     case 2:
      address=ac[2]*2+address;
      break;
     case 3:
      address=ac[3]*2+address;
      break;
    }
    return address;
   }

   public int eagleXResolveIndirect(int pc, int ii) {
    int count=0, address, indirect;

    address=memory.readWord(pc);
    indirect=(address & 0x8000)<<16;
    address=address & 0x7FFF;

    switch(ii) {
     case 0:
      address=copySegment(pc, address);
      break;
     case 1:
      address=pc+((address<<17)>>17);
      break;
     case 2:
      address=ac[2]+((address<<17)>>17);
      address=copySegment(pc, address);   // FIX FIX FIX - is this right?
      break;
     case 3:
      address=ac[3]+((address<<17)>>17);
      address=copySegment(pc, address);   // FIX FIX FIX - is this right?
      break;
    }
    address=(address & 0x7FFFFFFF)|indirect;  // Ensure ring 7 wraps to ring 0
    while(address<0) {
     if(count++==15)
      throw new RuntimeException("Indirection limit reached");
     address=memory.readWide(address & 0x7FFFFFFF);
    }
    return address;
   }

   public int eagleLResolveIndirect(int pc, int ii) {
    int count=0, address, indirect;

    address=memory.readWide(pc);
    indirect=(address & 0x80000000);
    address=address & 0x7FFFFFFF;

    switch(ii) {
     case 0:
      break;
     case 1:
      address=pc+address;
      break;
     case 2:
      address=ac[2]+address;
      break;
     case 3:
      address=ac[3]+address;
      break;
    }
    address=(address & 0x7FFFFFFF)|indirect;  // Ensure ring 7 wraps to ring 0
    while(address<0) {
     if(count++==15)
      throw new RuntimeException("Indirection limit reached");
     address=memory.readWide(address & 0x7FFFFFFF);
    }
    return address;
   }

   public int eagleResolveIndirect(int address) {
    int count=0;

    while(address<0) {
     if(count++==15)
      throw new RuntimeException("Indirection limit reached");
     address=memory.readWide(address & 0x7FFFFFFF);
    }
    return address;
   }

   public void backtrace() {
    callStack.backtrace(symbols, pc);
   }
}