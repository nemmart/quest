package hw;

import debug.*;

public class Instruction {

   static public Instruction[] novaGeneral=Decoder.novaGeneralTable();
   static public Instruction[] novaIO=Decoder.novaIOTable();
   static public Instruction[] novaCompute=Decoder.novaComputeTable();
   static public Instruction[] eclipseMV=Decoder.eclipseMVTable();
   static public Instruction   lefInstruction=new LEFInstruction();

   public String name;
   public String instructionFormat;
   public int    operator;

   static public Instruction decode(boolean lefMode, int opcode) {
    int bottom;

    if(opcode<0x6000)
     return novaGeneral[opcode>>11];
    else if(opcode<0x8000) {
     if(lefMode)
      return lefInstruction;
     return novaIO[(opcode>>8) & 0x1F];
    }
    bottom=opcode & 0x0F;
    if(bottom!=8 && bottom!=9)
     return novaCompute[(opcode>>8) & 0x07];
    return eclipseMV[((opcode & 0x7FF0)>>3) + (opcode & 0x01)];
   }

   static public int getSegment(int address) {
    return Machine.getSegment(address);
   }

   static public int setSegment(int segment, int address) {
    return Machine.setSegment(segment, address);
   }

   static public int copySegment(int segmentAddress, int address) {
    return Machine.copySegment(segmentAddress, address);
   }

   static public int getByteSegment(int address) {
    return Machine.getByteSegment(address);
   }

   static public int setByteSegment(int segment, int address) {
    return Machine.setByteSegment(segment, address);
   }

   static public int copyByteSegment(int segmentAddress, int address) {
    return Machine.copyByteSegment(segmentAddress, address);
   }

   public Instruction() {
    name=null;
    instructionFormat=null;
    operator=-1;
   }

   public void setup(int opcode, String name, String instructionFormat, int operator) {
    this.name=name;
    this.instructionFormat=instructionFormat;
    this.operator=operator;
   }

   public void debugBeforeExecution(Machine machine, int address, int opcode) {
    System.out.printf("%08X %04X %s\n", address, opcode, disassemble(machine.memory, address, opcode));

//System.out.printf("    MEM=%04X\n", machine.memory.readWord(0x700038C7));

//System.out.printf("   AC0=%08X\n", machine.ac[0]);
//System.out.printf("   AC1=%08X\n", machine.ac[1]);
//System.out.printf("   AC2=%08X\n", machine.ac[2]);
//System.out.printf("   AC3=%08X\n", machine.ac[3]);

//System.out.printf("    WFP=%08X\n", machine.wfp);
//System.out.printf("    WSP=%08X\n", machine.wsp);
//System.out.printf("    WSL=%08X\n", machine.wsl);
//System.out.printf("    WSB=%08X\n", machine.wsb);
   }

   public void debugAfterExecution(Machine machine, int address, int opcode) {
//    System.out.printf("    WSP=%08X\n", machine.wsp);
System.out.printf("   AC0=%08X\n", machine.ac[0]);
System.out.printf("   AC1=%08X\n", machine.ac[1]);
System.out.printf("   AC2=%08X\n", machine.ac[2]);
System.out.printf("   AC3=%08X\n", machine.ac[3]);
//System.out.printf("   MEM=%08X\n", machine.memory.readWide(0x700008AA));
//System.out.printf("   WFP=%08X\n", machine.wfp);
//System.out.printf("   FP0=%f\n", machine.fpac[0]);
//System.out.printf("   FP1=%f\n", machine.fpac[1]);

//System.out.printf("   FP0=%f\n", machine.fpac[0]);
//System.out.printf("   FP1=%f\n", machine.fpac[1]);
//System.out.printf("   FP2=%f\n", machine.fpac[2]);
//System.out.printf("   FP3=%f\n", machine.fpac[3]);
//    System.out.printf("   PC=%08X\n", machine.pc);
   }

   public String disassemble(Memory memory, int address, int opcode) {
    if(name==null)
     throw new RuntimeException("Instruction name not set");
    if(instructionFormat==null)
     throw new RuntimeException("InstructionFormat for opcode " + String.format("[%08X]  %04X", address, opcode) + " (" + name + ") not set");

    return Disassembler.disassemble(memory, address, name, opcode, instructionFormat);
   }

   public int execute(Machine machine, int address, int opcode) {
    throw new RuntimeException("Instruction subclass must override execute " + name);
   }
}
