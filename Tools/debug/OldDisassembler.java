package debug;

import hw.*;

public class OldDisassembler {
   static final public String[] carryActions=new String[]{"", "Z", "O", "C"};
   static final public String[] shiftActions=new String[]{"", "L", "R", "S"};
   static final public String[] skipActions=new String[]{"", "SKP", "SZC", "SNC", "SZR", "SNR", "SEZ", "SBN"};

   static public String novaSuffix(int opcode) {
    int SS, CC, N;

    opcode=opcode>>3;
    N=opcode & 0x01;
    opcode=opcode>>1;
    CC=opcode & 0x03;
    opcode=opcode>>2;
    SS=opcode & 0x03;

    if(N==0 && CC==0 && SS==0)
     return "";
    else
     return "." + carryActions[CC] + shiftActions[SS] + ((N==1)?"#":"");
   }

   static public String novaSkip(int opcode) {
    int skipCode=opcode & 0x07;

    if(skipCode==0)
     return "";
    else
     return "," + skipActions[skipCode];
   }

   static public String wordByteIndexed(Memory memory, int address, int opcode, int indexLocation) {
    int     index=(opcode>>indexLocation) & 0x03;
    int     offset=memory.readWord(address);
    int     add=(offset<<16)>>16;

    if(index==0)
     return "[" + String.format("0x%X", offset&0x7FFFFFFF) + "]";
    else if(index==1)
     return "[pc+" + String.format("0x%X", offset&0x7FFFFFFF) + "]" + String.format(" (0x%08X:%d)", address+(add>>1), add & 0x01);
    else if(index==2)
     return "[ac2+" + String.format("0x%X", offset&0x7FFFFFFF) + "]";
    else
     return "[ac3+" + String.format("0x%X", offset&0x7FFFFFFF) + "]";
   }

   static public String wideByteIndexed(Memory memory, int address, int opcode, int indexLocation) {
    int     index=(opcode>>indexLocation) & 0x03;
    int     offset=memory.readWide(address);
    boolean indirect=offset<0;

    if(index==0)
     return "[" + String.format("0x%X", offset&0x7FFFFFFF) + "]";
    else if(index==1)
     return "[pc+" + String.format("0x%X", offset&0x7FFFFFFF) + "]" + String.format(" (0x%08X:%d)", address+(offset>>1), offset & 0x01);
    else if(index==2)
     return "[ac2+" + String.format("0x%X", offset&0x7FFFFFFF) + "]";
    else
     return "[ac3+" + String.format("0x%X", offset&0x7FFFFFFF) + "]";
   }

   static public String wordIndirect(Memory memory, int address, int opcode, int indexLocation) {
    int     index=(opcode>>indexLocation) & 0x03;
    int     offset=memory.readWord(address), relative=(offset<<17)>>17;
    boolean indirect=offset>0x8000;

    if(index==0)
     return (indirect?"@":"") + "[" + String.format("0x%X", offset&0x7FFFFFFF) + "]";
    else if(index==1)
     return (indirect?"@":"") + "[pc+" + String.format("0x%X", offset&0x7FFFFFFF) + "]" + String.format(" (0x%X)", address+relative);
    else if(index==2)
     return (indirect?"@":"") + "[ac2+" + String.format("0x%X", offset&0x7FFFFFFF) + "]";
    else
     return (indirect?"@":"") + "[ac3+" + String.format("0x%X", offset&0x7FFFFFFF) + "]";
   }

   static public String wideIndirect(Memory memory, int address, int opcode, int indexLocation) {
    int     index=(opcode>>indexLocation) & 0x03;
    int     offset=memory.readWide(address), relative=(offset<<1)>>1;
    boolean indirect=offset<0;

    if(index==0)
     return (indirect?"@":"") + "[" + String.format("0x%X", offset&0x7FFFFFFF) + "]";
    else if(index==1)
     return (indirect?"@":"") + "[pc+" + String.format("0x%X", offset&0x7FFFFFFF) + "]" + String.format(" (0x%X)", address+relative);
    else if(index==2)
     return (indirect?"@":"") + "[ac2+" + String.format("0x%X", offset&0x7FFFFFFF) + "]";
    else
     return (indirect?"@":"") + "[ac3+" + String.format("0x%X", offset&0x7FFFFFFF) + "]";
   }

   static public String disassemble(Memory memory, int address, String name, int opcode, String instructionFormat) {
    if(instructionFormat.equals("noArguments"))
     return name;
    else if(instructionFormat.equals("novaCompute")) {
     return name + novaSuffix(opcode) + " " + ((opcode>>13) & 0x03) + "," + ((opcode>>11) & 0x03) + novaSkip(opcode);
    }
    else if(instructionFormat.equals("register"))
     return name + " " + ((opcode>>11) & 0x03);
    else if(instructionFormat.equals("registerRegister"))
     return name + " " + ((opcode>>13) & 0x03) + "," + ((opcode>>11) & 0x03);
    else if(instructionFormat.equals("shortDisplacement")) {   // only used for WBR
     int amount=((opcode>>7) & 0xF0) + ((opcode>>6) & 0x0F);

     amount=(amount<<24)>>24;
     return name + " " + amount + String.format(" (0x%08X)", address+amount);
    }
    else if(instructionFormat.equals("bitPosition"))
     return name + " " + (((opcode>>10) & 0x1C) | ((opcode>>4) & 0x03));
    else if(instructionFormat.equals("wordImmediate")) {
     int immediate=memory.readWord(address+1);

     return name + " " + String.format("0x%04X", immediate);
    }
    else if(instructionFormat.equals("wideImmediate")) {
     int immediate=memory.readWide(address+1);

     return name + " " + ((opcode>>11) & 0x03) + "," + String.format("0x%08X", immediate);
    }
    else if(instructionFormat.equals("tinyImmediateRegister")) {
     return name + " " + ((opcode>>13)-4+1) + "," + ((opcode>>11) & 0x03);
    }
    else if(instructionFormat.equals("registerWordImmediate")) {
     int immediate=memory.readWord(address+1);

     return name + " " + ((opcode>>11) & 0x03) + "," + immediate + String.format(" (0x%04X)", immediate);
    }
    else if(instructionFormat.equals("registerWideImmediate")) {
     int immediate=memory.readWide(address+1);

     return name + " " + ((opcode>>11) & 0x03) + "," + immediate + String.format(" (0x%08X)", immediate);
    }
    else if(instructionFormat.equals("wordImmediateRegister")) {
     int immediate=memory.readWord(address+1);

     return name + " " + immediate + String.format(" (0x%04X)", immediate) + "," + ((opcode>>11) & 0x03);
    }
    else if(instructionFormat.equals("bitOffset"))
     return name + " " + (((opcode>>10) & 0x1C) + ((opcode>>4) & 0x03));
    else if(instructionFormat.equals("registerWordIndirect")) {
     return name + " " + ((opcode>>11) & 0x03) + "," + wordIndirect(memory, address+1, opcode, 13);
    }
    else if(instructionFormat.equals("registerWideIndirect")) {
     return name + " " + ((opcode>>11) & 0x03) + "," + wideIndirect(memory, address+1, opcode, 13);
    }
    else if(instructionFormat.equals("registerWordByteIndexed")) {
     return name + " " + ((opcode>>11) & 0x03) + "," + wordByteIndexed(memory, address+1, opcode, 13);
    }
    else if(instructionFormat.equals("registerWideByteIndexed")) {
     return name + " " + ((opcode>>11) & 0x03) + "," + wideByteIndexed(memory, address+1, opcode, 13);
    }
    else if(instructionFormat.equals("tinyImmediateWordIndirect")) {
     return name + " " + ((opcode>>13)-4+1) + "," + wordIndirect(memory, address+1, opcode, 11);
    }
    else if(instructionFormat.equals("wordIndirect")) {
     return name + " " + wordIndirect(memory, address+1, opcode, 11);
    }
    else if(instructionFormat.equals("wideIndirect")) {
     return name + " " + wideIndirect(memory, address+1, opcode, 11);
    }
    else if(instructionFormat.equals("wordIndirectArgument")) {
     int argument=memory.readWord(address+2);

     // used by XNDO, XWDO
     return name + " " + ((opcode>>13)&0x03) + "," + argument + "," + wordIndirect(memory, address+1, opcode, 11);
    }
    else if(instructionFormat.equals("wideIndirectArgument")) {
     int argument=memory.readWord(address+3);

     // used by LCALL, LWDO, LNDO
     return name + " " + wideIndirect(memory, address+1, opcode, 11) + "," + argument;
    }
    else
     return name;
   }

   static public int wordLength(String instructionFormat) {
    if(instructionFormat.equals("noArguments"))
     return 1;
    else if(instructionFormat.equals("novaCompute"))
     return 1;
    else if(instructionFormat.equals("register"))
     return 1;
    else if(instructionFormat.equals("registerRegister"))
     return 1;
    else if(instructionFormat.equals("shortDisplacement"))
     return 1;
    else if(instructionFormat.equals("bitPosition"))
     return 1;
    else if(instructionFormat.equals("wordImmediate"))
     return 2;
    else if(instructionFormat.equals("wideImmediate"))
     return 3;
    else if(instructionFormat.equals("tinyImmediateRegister"))
     return 1;
    else if(instructionFormat.equals("registerWordImmediate"))
     return 2;
    else if(instructionFormat.equals("registerWideImmediate"))
     return 3;
    else if(instructionFormat.equals("wordImmediateRegister"))
     return 2;
    else if(instructionFormat.equals("bitOffset"))
     return 1;
    else if(instructionFormat.equals("registerWordIndirect"))
     return 2;
    else if(instructionFormat.equals("registerWideIndirect"))
     return 3;
    else if(instructionFormat.equals("registerWordByteIndexed"))
     return 2;
    else if(instructionFormat.equals("registerWideByteIndexed"))
     return 3;
    else if(instructionFormat.equals("tinyImmediateWordIndirect"))
     return 2;
    else if(instructionFormat.equals("wordIndirect"))
     return 2;
    else if(instructionFormat.equals("wideIndirect"))
     return 3;
    else if(instructionFormat.equals("wordIndirectArgument"))
     return 3;
    else if(instructionFormat.equals("wideIndirectArgument"))
     return 4;
    else
     return 1;
   }

}
