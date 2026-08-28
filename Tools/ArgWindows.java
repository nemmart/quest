import java.io.*;
import java.util.*;
import java.util.regex.*;

import hw.*;
import os.*;
import debug.*;

// Project 15 — ArgWindows: the call-site arg-push census.
//
// For every LCALL/XCALL whose target is a GAME routine (the addrbook entry
// set is the authority), walk BACKWARD from the call doing stack-depth
// accounting until 2*argc words of pushes are attributed. Straight-line
// windows are CLEAN and emitted mechanically into quest.argmap; anything
// with real control flow, stack-state-touching instructions, or accounting
// that doesn't close is PROBLEMATIC with the reason named. When in doubt,
// PROBLEMATIC — a wrong CLEAN is expensive, a spurious PROBLEMATIC is cheap.
//
// Slot numbering is by DEPTH, not push order: arg N lives at [wsp-2N] at
// the call (== [wfp-10-2N] in the callee, per hw/EagleIntegration.cpp), so
// arg1 is the LAST-pushed wide, arg argc the first-pushed.
public class ArgWindows {

   // ---- instruction stream ------------------------------------------------

   static class Instr {
     int    pc, opcode, length;
     String name;              // decoded name, '*' stripped; "SYSCALL" for the 0xC619/0x8006 pattern
     String format;
     boolean decodeFailed;
   }

   static TreeMap<Integer,Instr> instrs=new TreeMap<Integer,Instr>();

   // stack-family instruction names that are NOT tolerated inside a window
   // (user ruling: anything that touches stack state between the first push
   // and the call is not handled automatically). Pushes and calls are
   // handled explicitly in the walk; everything below is disqualifying.
   static final Set<String> STACK_TOUCHING=new HashSet<String>(Arrays.asList(
     "WSAVR", "WSAVS", "WSSVR", "WSSVS", "WRTN", "WPOPB", "WPOPJ",
     "LDASP", "STASP", "STAFP", "LDASB", "STASB", "LDASL", "STASL",
     "LDATS", "STATS", "ISZTS", "DSZTS",
     "XPSHJ", "LPSHJ", "WMSP", "WPOP", "WFPSH", "WFPOP", "DERR"
   ));

   // control-flow instruction names disqualifying inside a window
   // (SKIP_INSTRUCTIONS from Follow are checked separately)
   static final Set<String> FLOW=new HashSet<String>(Arrays.asList(
     "WBR", "XJMP", "LJMP", "XJSR", "LJSR", "XNDO", "XWDO", "LNDO", "LWDO",
     "LDSP", "QSEARCH", "SYSCALL", "JMP", "JSR"
   ));

   // known non-returning LCALL targets by name: an inner call to one of
   // these cannot close the accounting (it never comes back).
   static final Set<String> NORETURN=new HashSet<String>(Arrays.asList(
     "RETURN_MESSAGE"   // [[noreturn]] per c_src/quest/return_message.hpp
   ));

   static void buildInstructionIndex(SortedSet<Integer> codeLocations, Memory memory) {
     int nextPC=0;
     for(int pc : codeLocations) {
       if(pc<nextPC)
         continue;
       Instr in=new Instr();
       in.pc=pc;
       in.opcode=memory.readWord(pc);
       if(in.opcode==0xC619 && memory.readWord(pc+1)==0x8006) {   // system call (same special case as Follow)
         in.name="SYSCALL";
         in.length=3;
         in.format=null;
         instrs.put(pc, in);
         nextPC=pc+3;
         continue;
       }
       Instruction instruction=Instruction.decode(false, in.opcode);
       if(instruction==null || instruction.name==null) {
         in.name="?UNDECODED";
         in.decodeFailed=true;
         in.length=1;
         instrs.put(pc, in);
         nextPC=pc+1;
         continue;
       }
       String name=instruction.name;
       if(name.endsWith("*"))
         name=name.substring(0, name.length()-1);
       in.name=name;
       in.format=instruction.instructionFormat;
       in.length=(in.format!=null) ? Follow.wordLength(in.format) : 1;
       instrs.put(pc, in);
       nextPC=pc+in.length;
     }
   }

   // push width in WIDES for the push family; -1 if not a push
   static int pushWides(Instr in) {
     if(in.name.equals("XPEF") || in.name.equals("LPEF") ||
        in.name.equals("XPEFB") || in.name.equals("LPEFB"))
       return 1;
     if(in.name.equals("WPSH")) {
       int aa=(in.opcode>>11) & 0x03;      // last acc pushed (per EagleStack.setup)
       int xx=(in.opcode>>13) & 0x03;      // first acc pushed
       return ((aa-xx+4)%4)+1;
     }
     return -1;
   }

   static boolean isCall(Instr in) {
     return in.name.equals("LCALL") || in.name.equals("XCALL");
   }

   static int callArgc(Instr in, Memory memory) {
     int w=memory.readWord(in.pc + (in.name.equals("LCALL") ? 3 : 2));
     return w & 0x7FFF;
   }

   static int callTarget(Instr in, Memory memory) {
     if(in.name.equals("LCALL"))
       return Follow.resolveLTarget(memory, in.pc, in.opcode);
     return Follow.resolveXTarget(memory, in.pc, in.opcode);
   }

   // ---- addrbook ------------------------------------------------------------

   static TreeMap<Integer,String> book=new TreeMap<Integer,String>();  // entry -> name (commented entries included)

   static void loadBook(String path) throws IOException {
     BufferedReader r=new BufferedReader(new FileReader(path));
     String line;
     while((line=r.readLine())!=null) {
       String s=line.trim();
       if(s.isEmpty())
         continue;
       if(s.startsWith("#")) {
         s=s.substring(1).trim();
         if(s.isEmpty() || !isHex(s.split("\\s+")[0]))
           continue;                       // header comment, not a commented entry
       }
       String[] f=s.split("\\s+");
       if(f.length<2 || !isHex(f[0]))
         continue;
       book.put((int)Long.parseLong(f[0], 16), f[1]);
     }
     r.close();
   }

   static boolean isHex(String s) {
     if(s.length()<6)
       return false;
     for(char c : s.toCharArray())
       if(Character.digit(c, 16)<0)
         return false;
     return true;
   }

   // ---- census -------------------------------------------------------------

   static class Site {
     Instr        call;
     int          argc, target;
     String       targetName;
     String       cls;                     // CLEAN / CLEAN-WITH-INNER-CALLS / CLEAN-EMPTY / PROBLEMATIC
     String       reason="";               // for PROBLEMATIC: "reason [detail]"
     int          windowStart=-1;
     List<Instr>  innerCalls=new ArrayList<Instr>();
     int[]        slotPc;                  // slotPc[N-1] = push pc producing arg N (1-based slots)
   }

   static void problem(Site s, String reason) {
     if(s.cls==null) {                     // first problem wins; do not attempt recovery
       s.cls="PROBLEMATIC";
       s.reason=reason;
     }
   }

   // Backward walk. Word accounting:
   //   push of w wides            -> +2w words
   //   inner call, argc a         -> its WRTN consumed its args and the frame
   //                                 word the call pushed: net -2a across the
   //                                 call instruction; the walk continues
   //                                 backward to cover the inner pushes too.
   // debt  = words consumed by inner calls not yet covered by earlier pushes
   // attributed = outer arg words assigned so far, counted from the TOP of
   //              the arg block (nearest the call) downward — so the first
   //              push met walking backward supplies arg1's words.
   static void analyze(Site s, Memory memory, SortedSet<Integer> targets,
                       Map<Integer,Set<Integer>> sources, SymbolTable symbols) {
     int need=2*s.argc;
     int attributed=0, debt=0, steps=0;
     int cur=s.call.pc;
     s.slotPc=new int[s.argc];
     Arrays.fill(s.slotPc, -1);

     while(attributed<need || debt>0) {
       if(++steps>500) { problem(s, "depth-never-closed walk-exceeded-500-instructions"); return; }
       Map.Entry<Integer,Instr> e=instrs.lowerEntry(cur);
       if(e==null) { problem(s, "range-start-hit"); return; }
       Instr in=e.getValue();
       if(in.pc+in.length!=cur) { problem(s, "discontinuous-code " + String.format("%08X", in.pc)); return; }
       cur=in.pc;

       if(in.decodeFailed) { problem(s, "undecoded " + String.format("opcode-%04X-at-%08X", in.opcode, in.pc)); return; }

       int w=pushWides(in);
       if(w>0) {
         int words=2*w;
         int pay=Math.min(debt, words);
         debt-=pay;
         words-=pay;
         if(words>0) {
           if(attributed+words>need) { problem(s, "push-straddles-window-start " + String.format("%08X", in.pc)); return; }
           if(words%2!=0) { problem(s, "odd-word-attribution " + String.format("%08X", in.pc)); return; }
           // this push supplies from-top word offsets [attributed+1 .. attributed+words]
           for(int t=attributed+1;t<=attributed+words;t+=2) {
             int slot=(t+1)/2;             // wide N covers from-top words 2N-1,2N
             s.slotPc[slot-1]=in.pc;
           }
           attributed+=words;
         }
         continue;
       }

       if(isCall(in)) {
         int t=callTarget(in, memory);
         if(t<0) { problem(s, "unresolved-inner-call " + String.format("%08X", in.pc)); return; }
         String tn=symbols.addressToName.get(t);
         if(tn==null) tn=book.get(t);
         if(tn!=null && NORETURN.contains(tn)) { problem(s, "noreturn-inner-call " + String.format("%s@%08X", tn, in.pc)); return; }
         int a=callArgc(in, memory);
         debt+=2*a;
         s.innerCalls.add(in);
         continue;
       }

       // any other stack-state-touching instruction is disqualifying (user ruling)
       if(STACK_TOUCHING.contains(in.name)) { problem(s, "stack-op-in-window " + in.name + String.format("@%08X", in.pc)); return; }
       if(FLOW.contains(in.name)) { problem(s, "flow-in-window " + in.name + String.format("@%08X", in.pc)); return; }
       if(Follow.SKIP_INSTRUCTIONS.contains(in.name)) { problem(s, "skip-in-window " + in.name + String.format("@%08X", in.pc)); return; }
       if("novaCompute".equals(in.format) && (in.opcode & 0x07)!=0) { problem(s, "nova-skip-in-window " + in.name + String.format("@%08X", in.pc)); return; }
       // stack-neutral compute: fine, keep walking
     }

     s.windowStart=cur;

     // target-set check: no address STRICTLY inside (start, call) may be a
     // branch target — except each inner call's return word, and only when
     // its sole static source is that call itself (and it is not a symbol
     // entry). The window-start pc itself is exempt (user ruling): control
     // arriving there still executes every push.
     Map<Integer,Integer> innerReturn=new HashMap<Integer,Integer>();   // return word -> inner call pc
     for(Instr ic : s.innerCalls)
       innerReturn.put(ic.pc+ic.length, ic.pc);
     for(int a : targets.subSet(s.windowStart+1, s.call.pc)) {
       Integer icPc=innerReturn.get(a);
       if(icPc==null) { problem(s, "target-lands-in-window " + String.format("%08X", a)); return; }
       Set<Integer> src=sources.get(a);
       if(src==null || src.size()!=1 || !src.contains(icPc)) {
         problem(s, "inner-return-has-other-source " + String.format("%08X", a)); return;
       }
       if(symbols.addressToName.containsKey(a)) {
         problem(s, "inner-return-is-symbol-entry " + String.format("%08X", a)); return;
       }
     }

     for(int n=0;n<s.argc;n++)
       if(s.slotPc[n]<0) { problem(s, "slot-unattributed arg" + (n+1)); return; }

     s.cls=s.innerCalls.isEmpty() ? "CLEAN" : "CLEAN-WITH-INNER-CALLS";
   }

   // ---- Project 20: WPSH/WPOP frame-borrow brackets -------------------------
   //
   // Bracket shape (verified against quest.wpsh_wpop): WPSH r,r / LDAFP /
   // one frame-relative store / WPOP r,r. WPOP appears nowhere else in the
   // program, so detection is WPOP -> back-scan. Each bracket must be
   // PROVEN single-block with the SAME targets set the arg-window proof
   // uses: no branch target strictly inside the interior or on the WPOP
   // (the WPSH pc itself is exempt — control arriving there executes the
   // whole bracket), and nothing in the interior but stack-neutral,
   // flow-free compute (here: LDAFP + the store). Any failure FLAGS the
   // bracket (not emitted, reported) — the P16/P17/P18 stop-and-report
   // precedent.

   static class Borrow {
     Instr wpsh, wpop;
     int   reg;                            // borrowed AC (XX==AA on both ends)
     String fail;                          // null = proven
   }

   static int wpopXX(Instr in) { return (in.opcode>>13) & 0x03; }
   static int wpopAA(Instr in) { return (in.opcode>>11) & 0x03; }

   static List<Borrow> findBorrows(Memory memory, SortedSet<Integer> targets) {
     List<Borrow> borrows=new ArrayList<Borrow>();
     for(Instr in : instrs.values()) {
       if(!in.name.equals("WPOP"))
         continue;
       Borrow b=new Borrow();
       b.wpop=in;
       borrows.add(b);
       if(wpopXX(in)!=wpopAA(in)) { b.fail="wpop-multi-register"; continue; }
       b.reg=wpopAA(in);
       // back-chain exactly 3 contiguous instructions: store, LDAFP, WPSH
       Instr[] chain=new Instr[3];
       int cur=in.pc;
       boolean broken=false;
       for(int k=0;k<3;k++) {
         Map.Entry<Integer,Instr> e=instrs.lowerEntry(cur);
         if(e==null || e.getValue().pc+e.getValue().length!=cur) { b.fail="discontinuous-code before "+String.format("%08X", cur); broken=true; break; }
         chain[k]=e.getValue();
         cur=chain[k].pc;
       }
       if(broken)
         continue;
       Instr store=chain[0], ldafp=chain[1], wpsh=chain[2];
       if(!wpsh.name.equals("WPSH")) { b.fail="open-is-not-WPSH "+wpsh.name+String.format("@%08X", wpsh.pc); continue; }
       if(wpopXX(wpsh)!=b.reg || wpopAA(wpsh)!=b.reg) { b.fail="wpsh-register-mismatch"; continue; }
       if(!ldafp.name.equals("LDAFP")) { b.fail="no-LDAFP "+ldafp.name+String.format("@%08X", ldafp.pc); continue; }
       b.wpsh=wpsh;
       // interior discipline — the arg-window proof minus debt/attribution:
       // no flow, no stack ops, no skips, no calls, no undecoded words
       for(Instr i2 : new Instr[]{ldafp, store}) {
         if(i2.decodeFailed) { b.fail="undecoded-in-bracket "+String.format("%08X", i2.pc); break; }
         if(isCall(i2)) { b.fail="call-in-bracket "+String.format("%08X", i2.pc); break; }
         if(STACK_TOUCHING.contains(i2.name)) { b.fail="stack-op-in-bracket "+i2.name+String.format("@%08X", i2.pc); break; }
         if(FLOW.contains(i2.name)) { b.fail="flow-in-bracket "+i2.name+String.format("@%08X", i2.pc); break; }
         if(Follow.SKIP_INSTRUCTIONS.contains(i2.name)) { b.fail="skip-in-bracket "+i2.name+String.format("@%08X", i2.pc); break; }
         if("novaCompute".equals(i2.format) && (i2.opcode & 0x07)!=0) { b.fail="nova-skip-in-bracket "+i2.name+String.format("@%08X", i2.pc); break; }
       }
       if(b.fail!=null)
         continue;
       // single-block proof: no branch target strictly inside the bracket
       // interior OR on the WPOP itself (subSet upper bound wpop.pc+1)
       SortedSet<Integer> inside=targets.subSet(wpsh.pc+1, b.wpop.pc+1);
       if(!inside.isEmpty()) { b.fail="target-lands-in-bracket "+String.format("%08X", inside.first()); continue; }
     }
     return borrows;
   }

   // ---- sources map from Follow's tags --------------------------------------

   static Map<Integer,Set<Integer>> buildSources() {
     Map<Integer,Set<Integer>> sources=new HashMap<Integer,Set<Integer>>();
     for(Map.Entry<Integer,String> e : Follow.tags.entrySet()) {
       int pc=e.getKey();
       for(String tok : e.getValue().split(" ")) {
         if(tok.length()==8 && isHex(tok)) {
           int a=(int)Long.parseLong(tok, 16);
           sources.computeIfAbsent(a, k->new HashSet<Integer>()).add(pc);
         }
       }
     }
     return sources;
   }

   // ---- main -----------------------------------------------------------------

   public static void main(String[] args) throws Exception {
     if(args.length!=7) {
       System.err.println("Usage: java ArgWindows <dir> <PR file> <addrs file> <targets file> <addrbook file> <dis file> <out base>");
       System.err.println("       writes <out base>.argmap, <out base>.callsites, <out base>.wpsh_wpop");
       System.exit(1);
     }
     String dir=args[0], pr=args[1].toUpperCase();
     if(pr.endsWith(".PR"))
       pr=pr.substring(0, pr.length()-3);

     FS.initializeWithPath(dir);
     OSProcess process=new OSProcess(":", pr);
     SymbolTable symbols=process.symbols;
     Memory memory=process.memory;

     SortedSet<Integer> codeLocations=Follow.loadCodeLocations(args[2]);

     // compute the target set independently (covers skip fall-throughs by
     // construction) and cross-check against the quest.targets file
     Follow.process(symbols, codeLocations, memory);
     SortedSet<Integer> targets=new TreeSet<Integer>();
     for(int pc : Follow.targets)
       if(codeLocations.contains(pc))
         targets.add(pc);
     SortedSet<Integer> fileTargets=new TreeSet<Integer>();
     BufferedReader tr=new BufferedReader(new FileReader(args[3]));
     for(String line; (line=tr.readLine())!=null; )
       if(!line.trim().isEmpty())
         fileTargets.add((int)Long.parseLong(line.trim(), 16));
     tr.close();
     boolean targetsAgree=targets.equals(fileTargets);

     Map<Integer,Set<Integer>> sources=buildSources();
     loadBook(args[4]);
     buildInstructionIndex(codeLocations, memory);

     // census: every call, split game-target vs other
     List<Site> sites=new ArrayList<Site>();
     int totalCallEdges=0;
     for(Instr in : instrs.values()) {
       if(!isCall(in))
         continue;
       totalCallEdges++;
       int t=callTarget(in, memory);
       if(t<0 || !book.containsKey(t))
         continue;
       Site s=new Site();
       s.call=in;
       s.target=t;
       s.targetName=book.get(t);
       s.argc=callArgc(in, memory);
       sites.add(s);
     }

     // classify
     int xcallCount=0;
     for(Site s : sites) {
       if(s.call.name.equals("XCALL"))
         xcallCount++;
       if(s.argc==0) {
         s.cls="CLEAN-EMPTY";
         s.slotPc=new int[0];
         continue;
       }
       analyze(s, memory, targets, sources, symbols);
     }

     // duplicate-push-pc check: a push pc feeding two different call sites
     Map<Integer,List<Site>> pushOwners=new HashMap<Integer,List<Site>>();
     for(Site s : sites) {
       if(!s.cls.startsWith("CLEAN") || s.argc==0)
         continue;
       Set<Integer> pcs=new TreeSet<Integer>();
       for(int pc : s.slotPc)
         pcs.add(pc);
       for(int pc : pcs)
         pushOwners.computeIfAbsent(pc, k->new ArrayList<Site>()).add(s);
     }
     for(Map.Entry<Integer,List<Site>> e : pushOwners.entrySet()) {
       if(e.getValue().size()>1)
         for(Site s : e.getValue()) {
           s.cls=null;
           problem(s, "push-feeds-multiple-sites " + String.format("%08X", e.getKey()));
         }
     }

     // XCALL static-link check: WMOV/XWLDA immediately before each XCALL;
     // report if the link sits inside ANOTHER site's window
     List<String> linkNotes=new ArrayList<String>();
     for(Site s : sites) {
       if(!s.call.name.equals("XCALL"))
         continue;
       Map.Entry<Integer,Instr> e=instrs.lowerEntry(s.call.pc);
       if(e==null || e.getValue().pc+e.getValue().length!=s.call.pc) {
         linkNotes.add(String.format("XCALL %08X: no contiguous predecessor", s.call.pc));
         continue;
       }
       Instr link=e.getValue();
       if(!link.name.equals("WMOV") && !link.name.equals("XWLDA"))
         linkNotes.add(String.format("XCALL %08X: predecessor is %s, not WMOV/XWLDA", s.call.pc, link.name));
       for(Site o : sites) {
         if(o==s || o.windowStart<0)
           continue;
         if(link.pc>=o.windowStart && link.pc<o.call.pc)
           linkNotes.add(String.format("XCALL %08X: link %s@%08X sits inside window of %s@%08X",
                         s.call.pc, link.name, link.pc, o.targetName, o.call.pc));
       }
     }

     // dis-file sanity count: game-target calls per quest.dis
     int disGameCalls=0, disCalls=0;
     Pattern lp=Pattern.compile("LCALL \\[0x([0-9A-Fa-f]+)\\]");
     Pattern xp=Pattern.compile("XCALL .*\\(0x([0-9A-Fa-f]+)\\)");
     BufferedReader dr=new BufferedReader(new FileReader(args[5]));
     for(String line; (line=dr.readLine())!=null; ) {
       Matcher m=lp.matcher(line);
       Integer t=null;
       if(m.find()) t=(int)Long.parseLong(m.group(1), 16);
       else { m=xp.matcher(line); if(m.find()) t=(int)Long.parseLong(m.group(1), 16); }
       if(t!=null) {
         disCalls++;
         if(book.containsKey(t))
           disGameCalls++;
       }
     }
     dr.close();

     // ---- Project 20: borrow brackets -----------------------------------------
     List<Borrow> borrows=findBorrows(memory, targets);
     List<Borrow> proven=new ArrayList<Borrow>();
     List<Borrow> flagged=new ArrayList<Borrow>();
     for(Borrow b : borrows)
       (b.fail==null ? proven : flagged).add(b);
     proven.sort((x,y)->Integer.compare(x.wpsh.pc, y.wpsh.pc));

     // ---- write quest.argmap ------------------------------------------------
     // _PAIRS first (0-indexed, flat base+4N — a DIFFERENT word and a
     // DIFFERENT equation than the 1-indexed argN at wfp-10-2N, so a
     // reader never conflates the two). The opcode at the pc decides
     // store (WPSH) vs load (WPOP), exactly as XPEF-vs-LCALL does today.
     PrintWriter am=new PrintWriter(args[6] + ".argmap");
     am.printf("_PAIRS count %d%n", proven.size());
     for(int n=0;n<proven.size();n++) {
       Borrow b=proven.get(n);
       am.printf("_PAIRS slot%d at %08X%n", n, b.wpsh.pc);
       am.printf("_PAIRS slot%d at %08X%n", n, b.wpop.pc);
     }
     int argLines=0;
     for(Site s : sites) {
       if(!(s.cls.equals("CLEAN") || s.cls.equals("CLEAN-WITH-INNER-CALLS")))
         continue;
       for(int n=1;n<=s.argc;n++) {
         am.printf("%s arg%d at %08X%n", s.targetName, n, s.slotPc[n-1]);
         argLines++;
       }
     }
     am.close();

     // ---- write quest.callsites ----------------------------------------------
     PrintWriter cs=new PrintWriter(args[6] + ".callsites");
     TreeMap<String,Integer> classTotals=new TreeMap<String,Integer>();
     TreeMap<String,Integer> reasonTotals=new TreeMap<String,Integer>();
     for(Site s : sites) {
       StringBuilder b=new StringBuilder();
       b.append(String.format("call %s,%d at %08X %s", s.targetName, s.argc, s.call.pc, s.cls));
       if(s.cls.equals("CLEAN-WITH-INNER-CALLS")) {
         b.append(" inner=");
         List<String> parts=new ArrayList<String>();
         for(Instr ic : s.innerCalls) {
           int t=callTarget(ic, memory);
           String tn=symbols.addressToName.get(t);
           if(tn==null) tn=book.get(t);
           if(tn==null) tn="?";
           parts.add(String.format("%s@%08X", tn, ic.pc));
         }
         b.append(String.join(",", parts));
       }
       if(s.cls.equals("PROBLEMATIC")) {
         b.append(" ").append(s.reason);
         String key=s.reason.split(" ")[0];
         reasonTotals.merge(key, 1, Integer::sum);
       }
       cs.println(b);
       classTotals.merge(s.cls, 1, Integer::sum);
     }
     cs.println();
     cs.println("# ---- census summary ----");
     cs.printf("# game-target call sites: %d%n", sites.size());
     for(Map.Entry<String,Integer> e : classTotals.entrySet())
       cs.printf("#   %-24s %d%n", e.getKey(), e.getValue());
     if(!reasonTotals.isEmpty()) {
       cs.println("# PROBLEMATIC by reason:");
       for(Map.Entry<String,Integer> e : reasonTotals.entrySet())
         cs.printf("#   %-32s %d%n", e.getKey(), e.getValue());
     }
     cs.printf("# sanity: tool game-target sites %d vs quest.dis-derived %d (dis total call lines %d, tool total call edges %d)%n",
               sites.size(), disGameCalls, disCalls, totalCallEdges);
     cs.printf("# sanity: XCALL sites %d (expected 63)%n", xcallCount);
     cs.printf("# sanity: computed target set %s quest.targets file (%d vs %d entries)%n",
               targetsAgree ? "MATCHES" : "DIFFERS FROM", targets.size(), fileTargets.size());
     cs.printf("# borrow brackets (P20): %d WPOPs, %d proven single-block, %d FLAGGED%n",
               borrows.size(), proven.size(), flagged.size());
     for(int n=0;n<proven.size();n++) {
       Borrow b=proven.get(n);
       cs.printf("# borrow: slot%d WPSH %08X WPOP %08X AC%d%n", n, b.wpsh.pc, b.wpop.pc, b.reg);
     }
     for(Borrow b : flagged)
       cs.printf("# borrow: FLAGGED WPOP@%08X %s (stays on-stack)%n", b.wpop.pc, b.fail);
     for(String n : linkNotes)
       cs.println("# xcall-link: " + n);
     cs.close();

     // ---- verify the two files agree ------------------------------------------
     int expectedArgLines=0;
     for(Site s : sites)
       if(s.cls.equals("CLEAN") || s.cls.equals("CLEAN-WITH-INNER-CALLS"))
         expectedArgLines+=s.argc;
     if(expectedArgLines!=argLines)
       throw new RuntimeException("argmap/callsites disagree: " + argLines + " arg lines vs " + expectedArgLines + " expected");

     // ---- quest.wpsh_wpop: classify EVERY WPSH/WPOP (user ruling Aug 28 2026) --
     //
     // Regenerable artifact + gate: three mutually-exclusive, exhaustive
     // cases. Case 1 (paired) comes from the borrow pass above and inherits
     // its single-block PROOF — a FLAGGED bracket, a WPOP outside a proven
     // pair, an unattributable WPSH, a temp feeding anything but
     // RETURN_MESSAGE, or a count mismatch vs quest.dis means the artifact
     // is NOT written and the tool exits 2. CLEAN is only reported when
     // everything is good.
     List<String> wwFails=new ArrayList<String>();
     Map<Integer,Integer> pairOf=new TreeMap<Integer,Integer>();
     for(Borrow b : proven) {
       pairOf.put(b.wpsh.pc, b.wpop.pc);
       pairOf.put(b.wpop.pc, b.wpsh.pc);
     }
     for(Borrow b : flagged)
       wwFails.add(String.format("bracket not proven single-block: WPOP@%08X %s", b.wpop.pc, b.fail));
     Map<Integer,Site> slotOwner=new TreeMap<Integer,Site>();
     for(Site s : sites)
       if(s.cls!=null && s.cls.startsWith("CLEAN") && s.argc>0)
         for(int pc : s.slotPc)
           slotOwner.put(pc, s);
     TreeMap<Integer,String> annot=new TreeMap<Integer,String>();   // pc -> annotation
     TreeMap<Integer,Instr>  wws=new TreeMap<Integer,Instr>();
     for(Instr in : instrs.values())
       if(in.name.equals("WPSH") || in.name.equals("WPOP"))
         wws.put(in.pc, in);
     int nPaired=0, nArgGame=0, nArgRt=0, nTemp=0;
     for(Instr in : wws.values()) {
       if(pairOf.containsKey(in.pc)) {
         annot.put(in.pc, String.format("paired with %08X", pairOf.get(in.pc)));
         nPaired++;
         continue;
       }
       if(in.name.equals("WPOP")) {
         wwFails.add(String.format("WPOP@%08X outside any proven bracket", in.pc));
         continue;
       }
       // temp-create: WPSH r,r immediately followed by LDASP r (materialize
       // a stack temp and take its address). RETURN_MESSAGE-only by ruling.
       Map.Entry<Integer,Instr> ne=instrs.higherEntry(in.pc);
       boolean temp=false;
       if(wpopXX(in)==wpopAA(in) && ne!=null && ne.getKey()==in.pc+in.length
          && ne.getValue().name.equals("LDASP") && ((ne.getValue().opcode>>11)&0x03)==wpopAA(in))
         temp=true;
       // consuming call: forward scan over contiguous code, no flow allowed
       Instr consumer=null;
       String scanFail=null;
       int cur=in.pc, hops=0;
       while(hops++<24) {
         Map.Entry<Integer,Instr> e=instrs.higherEntry(cur);
         if(e==null || e.getKey()!=cur+instrs.get(cur).length) { scanFail="discontinuous"; break; }
         Instr i2=e.getValue();
         cur=i2.pc;
         if(isCall(i2)) { consumer=i2; break; }
         if(FLOW.contains(i2.name) || i2.name.equals("WRTN")) { scanFail="flow "+i2.name+String.format("@%08X", i2.pc); break; }
       }
       if(temp) {
         int t=consumer==null ? -1 : callTarget(consumer, memory);
         String tn=t<0 ? null : book.get(t);
         if(!"RETURN_MESSAGE".equals(tn)) {
           wwFails.add(String.format("temp-create WPSH@%08X feeds %s, not RETURN_MESSAGE",
                       in.pc, tn!=null ? tn : (scanFail!=null ? "("+scanFail+")" : "(no call found)")));
           continue;
         }
         annot.put(in.pc, "creates a temp on the stack (ref-arg for RETURN_MESSAGE)");
         nTemp++;
         continue;
       }
       Site owner=slotOwner.get(in.pc);
       if(owner!=null) {
         annot.put(in.pc, String.format("arg push for LCALL %s", owner.targetName));
         nArgGame++;
         continue;
       }
       if(consumer==null) {
         wwFails.add(String.format("WPSH@%08X unclassifiable: %s", in.pc, scanFail!=null ? scanFail : "no call in range"));
         continue;
       }
       int t=callTarget(consumer, memory);
       if(t>=0 && book.containsKey(t)) {
         wwFails.add(String.format("WPSH@%08X feeds game call %s@%08X but is in no CLEAN window",
                     in.pc, book.get(t), consumer.pc));
         continue;
       }
       String rn=t<0 ? null : symbols.addressToName.get(t);
       if(rn==null) {
         wwFails.add(String.format("WPSH@%08X feeds unresolvable call @%08X", in.pc, consumer.pc));
         continue;
       }
       annot.put(in.pc, String.format("arg push for LCALL %s [runtime]", rn));
       nArgRt++;
     }
     // sanity vs quest.dis: mnemonic occurrence count must equal ours
     int disWw=0;
     BufferedReader wr=new BufferedReader(new FileReader(args[5]));
     for(String line; (line=wr.readLine())!=null; )
       if(line.matches(".*\\bWPSH \\d,\\d.*") || line.matches(".*\\bWPOP \\d,\\d.*"))
         disWw++;
     wr.close();
     if(disWw!=wws.size())
       wwFails.add(String.format("count mismatch: %d WPSH/WPOP in tool stream vs %d in quest.dis", wws.size(), disWw));
     if(annot.size()!=wws.size() && wwFails.isEmpty())
       wwFails.add(String.format("classification not exhaustive: %d of %d annotated", annot.size(), wws.size()));
     if(!wwFails.isEmpty()) {
       for(String f : wwFails)
         System.err.println("wpsh_wpop: " + f);
       System.err.printf("wpsh_wpop: NOT CLEAN (%d problem%s) — %s.wpsh_wpop not written%n",
                         wwFails.size(), wwFails.size()==1?"":"s", args[6]);
       System.exit(2);
     }
     PrintWriter ww=new PrintWriter(args[6] + ".wpsh_wpop");
     ww.println("# Every WPSH/WPOP in QUEST, classified (ArgWindows). 3 mutually-exclusive cases:");
     ww.println("#   'paired with X'          save/restore bracket (frame-ptr borrow: WPSH r,r/LDAFP/store/WPOP r,r)");
     ww.println("#   'arg push for LCALL R'   argument marshalling (R game routine; [runtime] = game->RT, out of M4b scope)");
     ww.println("#   'creates a temp ...'     ref-arg temp construction (RETURN_MESSAGE only)");
     ww.printf("# %d total: %d paired (%d pairs) + %d arg->game + %d arg->RT + %d temp-create.%n",
               wws.size(), nPaired, nPaired/2, nArgGame, nArgRt, nTemp);
     ww.println();
     for(Instr in : wws.values())
       ww.printf("%08X %s %d,%d  // %s%n", in.pc, in.name, wpopXX(in), wpopAA(in), annot.get(in.pc));
     ww.close();
     System.out.printf("wpsh_wpop: CLEAN — %d classified (%d pairs proven single-block + %d arg->game + %d arg->RT + %d temp)%n",
                       wws.size(), nPaired/2, nArgGame, nArgRt, nTemp);

     System.out.printf("sites=%d argLines=%d edges=%d xcalls=%d targetsAgree=%b borrows=%d proven=%d flagged=%d%n",
                       sites.size(), argLines, totalCallEdges, xcallCount, targetsAgree,
                       borrows.size(), proven.size(), flagged.size());
     for(Map.Entry<String,Integer> e : classTotals.entrySet())
       System.out.printf("  %-24s %d%n", e.getKey(), e.getValue());
   }
}
