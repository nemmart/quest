import java.io.*;
import java.net.*;
import java.util.*;

import hw.*;
import os.*;
import debug.*;

public class Follow {
   static SortedSet<Integer>        targets=new TreeSet<Integer>();
   static SortedMap<Integer,String> tags=new TreeMap<Integer,String>();

   static final Set<String> SKIP_INSTRUCTIONS=new HashSet<String>(Arrays.asList(
             "WSEQ", "WSNE", "WSLT", "WSLE", "WSGT", "WSGE", "WUSGT", "WUSGE",
             "WSEQI", "WSNEI", "WSLEI", "WSGTI", "WUGTI", "WULEI", "NSANA",  "WSANA",
             "WSKBZ", "WSKBO", "WSZB", "WSZBO", "FSEQ", "FSNE", "FSGT", "FSGE", "FSLE", "FSLT",
             "FSNO", "FSND", "FSNM", "FSNU", "FSNUD", "FSNUO", "FSNOD", "FSNER",
             "ISZTS", "DSZTS", "XNISZ", "XNDSZ", "XWISZ", "XWDSZ", "LNISZ", "LNDSZ", "LWISZ", "LWDSZ",
             // special ones
             "WMESS", "WCLM"
   ));

   static public int wordLength(String instructionFormat) {
     int wordLength, oldWordLength;

     oldWordLength=OldDisassembler.wordLength(instructionFormat);
     //wordLength=Disassembler.wordLength(instructionFormat);
     //if(wordLength!=oldWordLength) throw new RuntimeException("word length mismatch");
     return oldWordLength;
   }

   static public String disassemble(Memory memory, int address, String name, int opcode, String instructionFormat) {
     String dis, oldDis;

     oldDis=OldDisassembler.disassemble(memory, address, name, opcode, instructionFormat);
     //dis=Disassembler.disassemble(memory, address, name, opcode, instructionFormat);
     //if(!dis.equals(oldDis)) throw new RuntimeException("disassemble text mismatch");
     return oldDis;
   }

   static int resolveXTarget(Memory memory, int addr, int opcode) {
    int     index=(opcode>>11) & 0x03;
    int     offset=memory.readWord(addr+1);
    boolean indirect=(offset & 0x8000)!=0;

    if(indirect) {
     System.err.println("CAUTION: INDIRECT INDEXING");
     return -1;
    }
    if(index==1) {
     int relative=(offset<<17)>>17;
     return (addr+1)+relative;
    }
    if(index==0) {
     return (addr & 0xFFFF0000) | (offset & 0x7FFF);
    }
    return -1;      // ac2/ac3 relative — can't resolve statically
   }

   static int resolveLTarget(Memory memory, int addr, int opcode) {
    int     index=(opcode>>11) & 0x03;
    int     offset;
    boolean indirect;

    offset=memory.readWide(addr+1);
    indirect=(offset<0);

    if(indirect) {
     System.err.println("CAUTION: INDIRECT INDEXING");
     return -1;
    }
    if(index==1) {
     offset=(offset<<1)>>1;
     return (addr+1)+offset;
    }
    if(index==0) {
     return offset & 0x7FFFFFFF;
    }
    return -1;
   }

   static int resolveLRegTarget(Memory memory, int addr, int opcode) {
    int     index=(opcode>>13) & 0x03;
    int     offset;
    boolean indirect;

    offset=memory.readWide(addr+1);
    indirect=(offset<0);

    if(indirect) {
     System.err.println("CAUTION: INDIRECT INDEXING");
     return -1;
    }
    if(index==1) {
     offset=(offset<<1)>>1;
     return (addr+1)+offset;
    }
    if(index==0) {
     return offset & 0x7FFFFFFF;
    }
    return -1;
   }

   static public void process(SymbolTable symbols, SortedSet<Integer> codeLocations, Memory memory) {
    TreeSet<Integer> results=new TreeSet<Integer>();
    Set<Integer>     process=new HashSet<Integer>();
    Set<Integer>     additional=new HashSet<Integer>();
    Set<Integer>     newCalls=new HashSet<Integer>();
    String           blockEndString;
    int              opcode;
    Instruction      instruction;
    String           name, format, tagString;
    int              length, oldLength, displacement, target;
    int              epilog=symbols.nameToAddress.get("I.EPILOG"), istop=symbols.nameToAddress.get("I.STOP"),
                     prolog=symbols.nameToAddress.get("I.PROLOG"), on_serror=symbols.nameToAddress.get("O.SERROR");
    int              fatal_282=symbols.nameToAddress.get("?FATAL")+0x282;
    int              oset_24=symbols.nameToAddress.get("O.SET")+0x24;
    int              nextPC=0;

    for(int addr : symbols.addressToName.keySet()) {
      String symbol=symbols.addressToName.get(addr);

      if(symbol.equals("C.COLLAT") || symbol.equals("I.NOTICE") || symbol.equals("?STACK_OVERHEAD"))
        continue;
      if(codeLocations.contains(addr))
        targets.add(addr);
      if(symbol.equals("SWAT.REX")) {
        // these get called through some complex mechanism...
        if(codeLocations.contains(addr + 0x23))
          targets.add(addr + 0x23);
        if(codeLocations.contains(addr + 0x3E))
          targets.add(addr + 0x3E);
      }
    }

    for(int pc : codeLocations) {
      if(pc<nextPC)
        continue;
      opcode=memory.readWord(pc);
      tagString="u";
      if(opcode==0xC619 && memory.readWord(pc+1)==0x8006) {
        if(memory.readWord(pc+2)==0310)   // 0310 terminates the process
          tagString="n";
        else {
          tagString=String.format("n %08X %08X", pc+3, pc+4);
          targets.add(pc+3);
          targets.add(pc+4);
        }
        tags.put(pc, tagString);
        nextPC=pc+3;
        continue;
      }
      instruction=Instruction.decode(false, opcode);
      if(instruction==null || instruction.name==null) {
        System.err.println("CAUTION: INSTRUCTION DECODE FAILED!");
        continue;
      }
      name=instruction.name;
      if(name.endsWith("*"))
        name=name.substring(0, name.length()-1);
      format=instruction.instructionFormat;
      length=(format!=null) ? wordLength(format) : 1;

      nextPC=pc+length;
      tagString="u";
      if(name.equals("WRTN") || name.equals("DERR") || name.equals("WPOPJ")) {
        if(name.equals("WPOPJ")) System.err.println("CAUTION: WPOPJ FOUND");
        tagString="n";
      }
      else if(name.equals("WBR")) {
        displacement=((opcode>>7) & 0xF0) + ((opcode>>6) & 0x0F);
        displacement=(displacement<<24)>>24;
        tagString=String.format("n %08X", pc+displacement);
        targets.add(pc+displacement);
      }
      else if(name.equals("XJMP") || name.equals("XPSHJ")) {
        if(name.equals("XPSHJ")) System.err.println("CAUTION: XPSHJ FOUND");
        target=resolveXTarget(memory, pc, opcode);
        if(target>=0) {
          tagString=String.format("n %08X %08X", target, pc+2);
          targets.add(target);
          targets.add(pc+2);
        }
        else
          throw new RuntimeException("Bad XJMP/XPSHJ");
      }
      else if(name.equals("LDSP")) {
        int rangeLow, rangeHigh, entryAddr, entry;

        target=resolveLRegTarget(memory, pc, opcode);
        rangeLow=memory.readWide(target-4);
        rangeHigh=memory.readWide(target-2);
        System.err.printf("LDSP: pc=0x%08X target=0x%08X from=%d to=%d\n", pc, target, rangeLow, rangeHigh);
        tagString="n";
        for(int i=rangeLow;i<=rangeHigh;i++) {
          entryAddr=target + (i-rangeLow)*2;
          entry=memory.readWide(entryAddr);
          if(entry!=-1) {
            tagString=tagString + String.format(" %08X", entryAddr+entry);
            targets.add(entryAddr+entry);
          }
        }
        tagString=tagString + String.format(" %08X", pc+3);
        targets.add(pc+3);
      }
      else if(name.equals("LJMP")) {
        throw new RuntimeException("LJMP not supported");
      }
      else if(name.equals("XJSR")) {
        throw new RuntimeException("XJSR not supported");
      }
      else if(name.equals("LJSR")) {
        target=resolveLTarget(memory, pc, opcode);
        if(target>=0) {
          tagString=String.format("j %08X n", target);
          if(target==prolog) {
            tagString=tagString + String.format(" %08X", pc+7);
            targets.add(pc+7);
          }
          else if(target!=epilog && target!=istop) {
            tagString=tagString + String.format(" %08X", pc+3);
            targets.add(pc+3);
          }
        }
        else
          tagString="u";
      }
      else if(name.equals("XCALL")) {
        target=resolveXTarget(memory, pc, opcode);
        if(target>=0) {
          tagString=String.format("c %08X n", target);
          targets.add(target);
        }
        else
          throw new RuntimeException("BAD XCALL");
        tagString=tagString + String.format(" %08X", pc+3);
        targets.add(pc+3);
      }
      else if(name.equals("LCALL")) {
        target=resolveLTarget(memory, pc, opcode);
        if(target<0 || target==on_serror)
          throw new RuntimeException("BAD LCALL");
        tagString=String.format("c %08X n %08X", target, pc+4);
        targets.add(pc+4);
      }
      else if(name.equals("XNDO") || name.equals("XWDO")) {
        displacement=memory.readWord(pc+2);
        tagString=String.format("n %08X %08X", pc+3, pc+1+displacement);
        targets.add(pc+3);
        targets.add(pc+1+displacement);
      }
      else if(name.equals("LNDO") || name.equals("LWDO")) {
        displacement=memory.readWord(pc+3);
        tagString=String.format("n %08X %08X", pc+4, pc+1+displacement);
        targets.add(pc+4);
        targets.add(pc+1+displacement);
      }
      else if(name.equals("QSEARCH")) {
        throw new RuntimeException("QSEARCH not supported");
      }
      else if(SKIP_INSTRUCTIONS.contains(name)) {
        tagString=String.format("n %08X %08X", pc+length, pc+length+1);
        targets.add(pc+length);
        targets.add(pc+length+1);
      }
      else if(format!=null && format.equals("novaCompute")) {
        if(name.equals("JMP") || name.equals("JSR"))
          System.err.printf("CAUTION: NOVA JUMP AT ADDRESS %x\n", pc);
        int skipCode=opcode & 0x07;
        if(skipCode==0)
          tagString=String.format("n %08X", pc+1);
        else if(skipCode==1) {
          tagString=String.format("n %08X", pc+2);
          targets.add(pc+2);
        }
        else {
          tagString=String.format("n %08X %08X", pc+1, pc+2);
          targets.add(pc+1);
          targets.add(pc+2);
        }
      }
      else
        tagString=String.format("n %08X", pc+length);

      tags.put(pc, tagString);
    }
   }

   static public SortedSet<Integer> loadCodeLocations(String path) {
     BufferedReader     reader;
     String             line, split[];
     int                start, stop;
     SortedSet<Integer> codeLocations=new TreeSet<Integer>();

     try {
       reader=new BufferedReader(new FileReader(path));
       while(true) {
         line=reader.readLine();
         if(line==null)
           break;
         split=line.split(" ");
         if(!split[0].equals("code"))
           continue;
         start=Integer.parseInt(split[1], 16);
         stop=Integer.parseInt(split[2], 16);
         for(int i=start;i<stop;i++)
           codeLocations.add(i);
       }
       return codeLocations;
     }
     catch(Exception exception) {
       throw new RuntimeException(exception);
     }
   }

   static public void main(String[] args) throws Exception {
    FSTerminal       terminal;
    String           file;
    FSChannel        channel;
    PageSet          filePages;
    OSProcess        process;
    SymbolTable      symbols;
    Memory           fileMemory, processMemory;
    SortedSet<Integer> codeLocations;

    if(args.length!=4) {
     System.err.println("Usage: java Forward <dir> <PR file> <name> <SS file>");
     System.exit(1);
    }

    FS.initializeWithPath(args[0]);

    file=args[1].toUpperCase();
    if(file.endsWith(".PR"))
     file=file.substring(0, file.length()-3);

    channel=FSChannel.openForPagedIO(":" + file + ".PR", true);
    filePages=channel.pageSet();

    process=new OSProcess(":", file);
    symbols=process.symbols;
    processMemory=process.memory;

    int sharedStartPage=filePages.readWord(0x10F);
    int sharedPageCount=filePages.readWord(0x113);
    int fileSharedOffset=filePages.readWord(0x11A);
    int SEGMENT_BASE=OSProcess.SEGMENT_BASE*1024;

    codeLocations=loadCodeLocations(args[3]);

    process(symbols, codeLocations, process.memory);

    PrintWriter pw;

    pw=new PrintWriter(args[2] + ".targets");
    for(int pc : targets) {
      if(codeLocations.contains(pc))
        pw.printf("%08X\n", pc);
    }
    pw.close();

    pw=new PrintWriter(args[2] + ".tags");
    for(int pc : tags.keySet()) {
      pw.printf("%08X %s\n", pc, tags.get(pc));
    }
    pw.close();
   }
}

