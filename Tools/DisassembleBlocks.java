import java.io.*;
import java.util.*;

import hw.*;
import os.*;
import debug.*;

public class DisassembleBlocks {

   // ── Instruction decoding ──

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

   static public String findSymbol(String description, SymbolTable symbols) {
    int    bracket=description.indexOf("[0x"), paren=description.indexOf("(0x");
    int    start=(paren!=-1) ? paren : bracket, addr;
    String symbol;

    if(description.startsWith("XCALL") || description.startsWith("LCALL") ||
       description.startsWith("XJSR") || description.startsWith("LJSR") ||
       description.startsWith("XJMP") || description.startsWith("LJMP")) {
     if(start==-1)
      return " # addr not found";
     addr=Integer.parseInt(description.substring(start+3, start+11), 16);
     symbol=symbols.addressToName.get(addr);
     if(symbol==null && (description.startsWith("XJMP") || description.startsWith("LJMP")))
      return "";
     if(symbol==null)
      return " # symbol not found";
     else
      return " # " + symbol;
    }
    return "";
   }

   // Decode one instruction, return description string and word length
   static public String decodeText(Memory memory, int address, SymbolTable symbols) {
    int opcode=memory.readWord(address);

    if(opcode==0xc619 && memory.readWord(address+1)==0x8006)
     return String.format("SYSCALL 0%o", memory.readWord(address+2));

    Instruction instruction=Instruction.decode(false, opcode);
    if(instruction==null) return "undef";

    String name=instruction.name;
    if(name==null)
     throw new RuntimeException("Unnamed instruction for opcode " + String.format("%04X", opcode));
    if(name.endsWith("*"))
     name=name.substring(0, name.length()-1);
    if(instruction.instructionFormat==null)
     return name;

    String desc=disassemble(memory, address, name, opcode, instruction.instructionFormat);

    String sym=findSymbol(desc, symbols);
    return desc + ";" + sym;
   }

   static public int decodeLength(Memory memory, int address) {
    int opcode=memory.readWord(address);

    if(opcode==0xc619 && memory.readWord(address+1)==0x8006) return 3;

    Instruction instruction=Instruction.decode(false, opcode);
    if(instruction==null) return 1;

    String name=instruction.name;
    if(name==null)
     throw new RuntimeException("Unnamed instruction for opcode " + String.format("%04X", opcode));
    if(instruction.instructionFormat==null) return 1;

    return wordLength(instruction.instructionFormat);
   }

   // Extract the opcode mnemonic from a description
   static public String opName(String desc) {
    String clean=desc.replaceAll(";.*", "");
    int sp=clean.indexOf(' ');
    return sp==-1 ? clean : clean.substring(0, sp);
   }

   // Extract first register: "WSGTI 0,10 (0x000A);" → 0
   static public int reg1(String desc) {
    String clean=desc.replaceAll(";.*", "");
    String rest=clean.substring(opName(desc).length()).trim();
    if(rest.isEmpty()) return -1;
    try { return Integer.parseInt(rest.substring(0, 1)); }
    catch(NumberFormatException e) { return -1; }
   }

   // Extract second register: "WSGT 0,0;" → 0, "NLDAI 686 (0x02AE),1;" → 1
   static public int reg2(String desc) {
    String clean=desc.replaceAll(";.*", "");
    int comma=clean.lastIndexOf(',');
    if(comma==-1) return -1;
    String after=clean.substring(comma+1).trim();
    if(after.isEmpty()) return -1;
    try { return Integer.parseInt(after.substring(0, 1)); }
    catch(NumberFormatException e) { return -1; }
   }

   // ── ASSERT pattern detection ──
   //
   // Checks if the 3 instructions at addresses[idx], [idx+1], [idx+2]
   // form an ASSERT pattern. Returns true if absorbed.
   //
   // Patterns:
   //  1. WSGTI R,N; WSGT R,R; DERR
   //  2. WUGTI R,N; WSGT R,R; DERR
   //  3. NLDAI N,Rx; WSLE Rx,Ry; DERR
   //  4. WSLT R1,R2; WSLE R1,R3; DERR
   //  5. WSLT R,R; WUSGE R,R2; DERR
   //  6. WSUB R1,R2; WULEI R2,N; DERR

   static public boolean isAssertPattern(String desc1, String desc2, String desc3) {
    String op1=opName(desc1), op2=opName(desc2), op3=opName(desc3);
    if(!op3.equals("DERR")) return false;

    int r1_1=reg1(desc1), r1_2=reg2(desc1);
    int r2_1=reg1(desc2), r2_2=reg2(desc2);

    // Pattern 1: WSGTI R,N; WSGT R,R; DERR
    if(op1.equals("WSGTI") && op2.equals("WSGT") && r1_1==r2_1 && r2_1==r2_2) return true;

    // Pattern 2: WUGTI R,N; WSGT R,R; DERR
    if(op1.equals("WUGTI") && op2.equals("WSGT") && r1_1==r2_1 && r2_1==r2_2) return true;

    // Pattern 3: NLDAI N,Rx; WSLE Rx,Ry; DERR
    if(op1.equals("NLDAI") && op2.equals("WSLE") && r1_2==r2_1) return true;

    // Pattern 4: WSLT R1,R2; WSLE R1,R3; DERR
    if(op1.equals("WSLT") && op2.equals("WSLE") && r1_1==r2_1) return true;

    // Pattern 5: WSLT R,R; WUSGE R,R2; DERR
    if(op1.equals("WSLT") && op2.equals("WUSGE") && r1_1==r1_2 && r1_1==r2_1) return true;

    // Pattern 6: WSUB R1,R2; WULEI R2,N; DERR
    if(op1.equals("WSUB") && op2.equals("WULEI") && r1_2==r2_1) return true;

    return false;
   }

   // ── Main disassembly ──

   static public void disassembleBlocks(SymbolTable symbols, Memory memory,
                                         List<Integer> addresses,
                                         Set<Integer> targets,
                                         Map<Integer, String> tags) {
    boolean inBlock=false;
    int lastAddr=-1;
    int i=0;

    while(i<addresses.size()) {
     int addr=addresses.get(i);

     // Block boundary?
     if(targets.contains(addr)) {
      // End previous block
      if(inBlock && lastAddr>=0) {
       String tag=tags.get(lastAddr);
       System.out.println(tag!=null ? tag : "n");
       System.out.println();
      }
      else if(inBlock && lastAddr==-2) {
       // Assert was absorbed — fall through to this block
       System.out.printf("n %08X\n\n", addr);
      }
      // Start new block
      String sym=symbols.addressToName.get(addr);
      if(sym!=null)
       System.out.println("# " + sym);
      System.out.printf("%08X:\n", addr);
      inBlock=true;
     }

     // Check for ASSERT pattern: look ahead at next 2 instructions
     if(i+2<addresses.size()) {
      int a1=addr, a2=addresses.get(i+1), a3=addresses.get(i+2);
      String d1=decodeText(memory, a1, symbols);
      String d2=decodeText(memory, a2, symbols);
      String d3=decodeText(memory, a3, symbols);

      if(isAssertPattern(d1, d2, d3)) {
       // Emit all 3 as part of current block, skip target checks
       System.out.println(d1);
       System.out.println(d2);
       System.out.println(d3);
       // Override lastAddr to -1 so we use synthetic fall-through tag
       // instead of DERR's dead-end tag
       lastAddr=-2;  // sentinel: means "use fall-through to next address"
       i+=3;
       continue;
      }
     }

     // Normal instruction
     String desc=decodeText(memory, addr, symbols);
     System.out.println(desc);
     lastAddr=addr;
     i++;
    }

    // End final block
    if(inBlock && lastAddr>=0) {
     String tag=tags.get(lastAddr);
     System.out.println(tag!=null ? tag : "n");
    }
    else if(inBlock && lastAddr==-2) {
     System.out.println("n");  // assert at very end — no successor
    }
   }

   // ── File loading ──

   static public Set<Integer> loadTargets(String filename) throws IOException {
    Set<Integer> targets=new LinkedHashSet<>();
    BufferedReader reader=new BufferedReader(new FileReader(filename));
    String line;
    while((line=reader.readLine())!=null) {
     line=line.trim();
     if(!line.isEmpty())
      targets.add(Integer.parseUnsignedInt(line, 16));
    }
    reader.close();
    return targets;
   }

   static public void loadTags(String filename, List<Integer> addresses,
                                Map<Integer, String> tags) throws IOException {
    BufferedReader reader=new BufferedReader(new FileReader(filename));
    String line;
    while((line=reader.readLine())!=null) {
     line=line.trim();
     if(line.isEmpty()) continue;
     int space=line.indexOf(' ');
     if(space==-1) continue;
     int address=Integer.parseUnsignedInt(line.substring(0, space), 16);
     String tag=line.substring(space+1).trim();
     addresses.add(address);
     tags.put(address, tag);
    }
    reader.close();
   }

   // ── Entry point ──

   static public void main(String[] args) throws Exception {
    if(args.length!=4) {
     System.err.println("Usage: java DisassembleBlocks <dir> <PR file> <targets file> <tags file>");
     System.exit(1);
    }

    FS.initializeWithPath(args[0]);

    String file=args[1].toUpperCase();
    if(file.endsWith(".PR"))
     file=file.substring(0, file.length()-3);

    FSChannel channel=FSChannel.openForPagedIO(":" + file + ".PR", true);
    OSProcess process=new OSProcess(":", file);
    SymbolTable symbols=process.symbols;
    Memory memory=process.memory;

    Set<Integer> targets=loadTargets(args[2]);
    List<Integer> addresses=new ArrayList<>();
    Map<Integer, String> tags=new LinkedHashMap<>();
    loadTags(args[3], addresses, tags);

    disassembleBlocks(symbols, memory, addresses, targets, tags);
   }
}
