import java.io.*;
import java.net.*;
import java.util.*;

import hw.*;
import os.*;
import debug.*;

public class StartStop {
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
     System.err.printf("CAUTION: INDIRECT INDEXING %08X\n", addr);
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
     System.err.printf("CAUTION: INDIRECT INDEXING %08X\n", addr);
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
     System.err.printf("CAUTION: INDIRECT INDEXING %08X\n", addr);
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

   // O.ON, I.GOTO, and similar runtime routines receive their control-transfer
   // target in AC2, loaded by the immediately preceding "XLEF 2,[pc+d]" (two words
   // back).  That target is reachable only at runtime, so seed it as a code entry.
   // The pattern MUST hold; if it ever doesn't (e.g. a different .PR file), abort
   // loudly rather than silently drop a reachable entry point.
   static void seedAc2XlefTarget(Memory memory, int pc, String routine, Set<Integer> additional) {
    int         xaddr=pc-2;
    int         xopcode=memory.readWord(xaddr);
    Instruction xinst=Instruction.decode(false, xopcode);
    int         aa=(xopcode>>11) & 0x03;   // XLEF destination register
    int         ii=(xopcode>>13) & 0x03;   // XLEF index mode

    if(xinst==null || !"XLEF".equals(xinst.name) || aa!=2 || ii!=1)
     throw new RuntimeException(String.format(
       "%s at %08X not preceded by pc-relative XLEF into AC2 (name=%s aa=%d ii=%d)",
       routine, pc, (xinst!=null ? xinst.name : "null"), aa, ii));
    int woffset=memory.readWord(xaddr+1);
    if((woffset & 0x8000)!=0)
     throw new RuntimeException(String.format(
       "%s at %08X has indirect XLEF target (offset=%04X) - cannot resolve statically",
       routine, pc, woffset));
    additional.add((xaddr+1) + ((woffset<<17)>>17));   // pc-relative, sign-extended 15 bits
   }

   static public TreeSet<Integer> reachable(SymbolTable symbols, Set<Integer> dispatchTables, Memory memory, int start, int stop) {
    TreeSet<Integer> results=new TreeSet<Integer>();
    Set<Integer>     process=new HashSet<Integer>();
    Set<Integer>     additional=new HashSet<Integer>();
    Set<Integer>     newCalls=new HashSet<Integer>();
    int              opcode;
    Instruction      instruction;
    String           name, format;
    int              length, oldLength, displacement, target;
    int              epilog=symbols.nameToAddress.get("I.EPILOG"), istop=symbols.nameToAddress.get("I.STOP"),
                     prolog=symbols.nameToAddress.get("I.PROLOG"), on_serror=symbols.nameToAddress.get("O.SERROR");
    int              fatal_282=symbols.nameToAddress.get("?FATAL")+0x282;
    int              oset_24=symbols.nameToAddress.get("O.SET")+0x24;
    int              on_on=symbols.nameToAddress.get("O.ON");
    int              i_goto=symbols.nameToAddress.get("I.GOTO");

    for(int addr : symbols.addressToName.keySet()) {
      String symbol=symbols.addressToName.get(addr);

      if(symbol.equals("C.COLLAT") || symbol.equals("I.NOTICE") || symbol.equals("?STACK_OVERHEAD"))
        continue;
      if(addr>=start && addr<stop)
        process.add(addr);
      if(symbol.equals("SWAT.REX")) {
        // these get called through some complex mechanism...
        process.add(addr + 0x23);
        process.add(addr + 0x3E);
      }
    }

    while(process.size()>0) {
      for(int pc : process) {
        if(pc>=stop) continue;
        opcode=memory.readWord(pc);
        if(opcode==0xC619 && memory.readWord(pc+1)==0x8006) {
          results.add(pc);
          results.add(pc+1);
          results.add(pc+2);
          if(memory.readWord(pc+2)!=0310) {  // 0310 terminates the process
            additional.add(pc+3);
            additional.add(pc+4);
          }
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

        for(int i=0;i<length;i++)
          results.add(pc+i);

        if(name.equals("WRTN") || name.equals("DERR") || name.equals("WPOPJ")) {
          if(name.equals("WPOPJ")) System.err.println("FOUND WPOPJ");
        }
        else if(name.equals("WBR")) {
         displacement=((opcode>>7) & 0xF0) + ((opcode>>6) & 0x0F);
         displacement=(displacement<<24)>>24;
         additional.add(pc+displacement);
        }
        else if(name.equals("XJMP") || name.equals("XPSHJ")) {
         target=resolveXTarget(memory, pc, opcode);
         if(target>=0)
           additional.add(target);
         else
           System.err.println("CAUTION BAD TARGET");
         // PUSHJ will return!
         if(name.equals("XPSHJ"))
           additional.add(pc+2);
        }
        else if(name.equals("LDSP")) {
          int rangeLow, rangeHigh, entryAddr, entry;

          target=resolveLRegTarget(memory, pc, opcode);
          dispatchTables.add(target-4);
          rangeLow=memory.readWide(target-4);
          rangeHigh=memory.readWide(target-2);
          System.err.printf("LDSP: pc=0x%08X target=0x%08X from=%d to=%d\n", pc, target, rangeLow, rangeHigh);
          for(int i=rangeLow;i<=rangeHigh;i++) {
            entryAddr=target + (i-rangeLow)*2;
            entry=memory.readWide(entryAddr);
            if(entry!=-1)
              additional.add(entryAddr+entry);
          }
          additional.add(pc+3);
        }
        else if(name.equals("LJMP")) {
         target=resolveLTarget(memory, pc, opcode);
         if(target>=0)
          additional.add(target);
         else
           System.err.println("CAUTION BAD TARGET");
        }
        else if(name.equals("XJSR")) {
         target=resolveXTarget(memory, pc, opcode);
         if(target>=0)
           additional.add(target);
         else
           System.err.println("CAUTION BAD TARGET");
         if(target!=epilog)
           additional.add(pc+2);
         if(target==oset_24)
           additional.add(pc+3);
        }
        else if(name.equals("LJSR")) {
         target=resolveLTarget(memory, pc, opcode);
         if(target==prolog)
           additional.add(pc+7);
         else if(target!=epilog && target!=istop && target!=i_goto)
           additional.add(pc+3);
         // O.ON and I.GOTO both take their transfer target in AC2, loaded by the
         // immediately preceding "XLEF 2,[pc+d]".  Seed that target as a code entry.
         // I.GOTO is a non-local transfer that does not return, so it is excluded
         // from the pc+3 fall-through above (like I.EPILOG / I.STOP).
         if(target==on_on)
           seedAc2XlefTarget(memory, pc, "O.ON", additional);
         else if(target==i_goto)
           seedAc2XlefTarget(memory, pc, "I.GOTO", additional);
        }
        else if(name.equals("XCALL")) {
         target=resolveXTarget(memory, pc, opcode);
         if(target>=0) {
           additional.add(target);
           if(symbols.addressToName.get(target)==null) {
             if(!newCalls.contains(target)) {
               symbols.addSymbol("internal" + newCalls.size(), target);
             }
             newCalls.add(target);
           }
         }
         additional.add(pc+3);
         if(target==fatal_282)
           additional.add(pc+4);
        }
        else if(name.equals("LCALL")) {
         target=resolveLTarget(memory, pc, opcode);
         if(target<0)
           System.err.println("CAUTION BAD TARGET");
         if(target!=on_serror)
           additional.add(pc+4);
         else
           System.err.println("Found LCALL O.SERROR");
        }
        else if(name.equals("XNDO") || name.equals("XWDO")) {
         displacement=memory.readWord(pc+2);
         additional.add(pc+3);
         additional.add(pc+1+displacement);
        }
        else if(name.equals("LNDO") || name.equals("LWDO")) {
         displacement=memory.readWord(pc+3);
         additional.add(pc+4);
         additional.add(pc+1+displacement);
        }
        else if(name.equals("QSEARCH")) {
          additional.add(pc+2);
          additional.add(pc+3);
          additional.add(pc+4);
        }
        else if(SKIP_INSTRUCTIONS.contains(name)) {
         additional.add(pc+length);
         additional.add(pc+length+1);
        }
        else if(format!=null && format.equals("novaCompute")) {
          if(name.equals("JMP") || name.equals("JSR"))
            System.err.printf("CAUTION: NOVA JUMP AT ADDRESS %x\n", pc);
          int skipCode=opcode & 0x07;
          if(skipCode==0)
           additional.add(pc+1);
          else if(skipCode==1)
           additional.add(pc+2);
          else {
           additional.add(pc+1);
           additional.add(pc+2);
          }
        }
        else
          additional.add(pc+length);
      }
      process.clear();
      for(Integer pc : additional) {
        if(!results.contains(pc))
          process.add(pc);
      }
      additional.clear();
     }
     return results;
   }

   static public void main(String[] args) {
    FSTerminal   terminal;
    String       file;
    FSChannel    channel;
    PageSet      filePages;
    OSProcess    process;
    SymbolTable  symbols;
    Memory       fileMemory, processMemory;
    Integer      start, stop, memoryStop;
    Set<Integer> dispatchTables=new HashSet<Integer>();

    if(args.length!=5) {
     System.err.println("Usage: java StartStop <dir> <PR file> <Start Symbol> <Stop Symbol> <Const Last Symbol>");
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

    start=symbols.nameToAddress.get(args[2]);
    stop=symbols.nameToAddress.get(args[3]);
    if(start==null) throw new RuntimeException("Symbol '" + args[2] + "' not found");
    if(stop==null) throw new RuntimeException("Symbol '" + args[3] + "' not found");

    TreeSet<Integer> reachable=reachable(symbols, dispatchTables, processMemory, start.intValue(), stop.intValue());
    List<Integer>    addresses=new ArrayList<Integer>();
    int              decompile=0;

    addresses.addAll(symbols.addressToName.keySet());
    addresses.addAll(dispatchTables);
    Collections.sort(addresses);

    if(args.length==5) {
      memoryStop=symbols.nameToAddress.get(args[4]);
      if(memoryStop==null) throw new RuntimeException("Symbol '" + args[4] + "' not found");
      System.out.printf("mem %08x %08x\n", SEGMENT_BASE, SEGMENT_BASE+(fileSharedOffset-8)*1024);
      System.out.printf("mem %08x %08x\n", SEGMENT_BASE+sharedStartPage*1024, memoryStop.intValue());
    }

    int addr=start;
    while(addr<stop) {
      if(dispatchTables.contains(addr)) {
        int count=processMemory.readWide(addr+2)-processMemory.readWide(addr)+1;

        System.out.printf("disp %08x %08x\n", addr, addr+count*2+4);
        addr=addr+count*2+4;
      }
      else if(reachable.contains(addr)) {
        System.out.printf("code %08x ", addr);
        while(addr<stop && reachable.contains(addr) && !dispatchTables.contains(addr))
          addr++;
        System.out.printf("%08x\n", addr);
      }
      else {
        System.out.printf("mem %08x ", addr);
        while(addr<stop && !dispatchTables.contains(addr) && !reachable.contains(addr))
          addr++;
        System.out.printf("%08x\n", addr);
      }
    }

    System.err.println();
    System.err.println("Summary:");
    System.err.println(reachable.size() + " reachable instruction words found!");
    System.err.println((stop-start) + " instruction words found!");
   }
}

