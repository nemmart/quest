package debug;

import java.util.*;

import hw.*;

public class CallStack {

   static public boolean debugCallHistory=false;

   public List<Call>  callStack;
   public SymbolTable symbols;
   public Set<String> debug;
   public Machine     machine;

   public CallStack(SymbolTable symbols, Machine machine) {
    callStack=new ArrayList<Call>();
    this.symbols=symbols;
    debug=new HashSet<String>();
    this.machine=machine;
   }

   public String locationDescription(int instructionAddress, int symbolAddress) {
    String symbol;
    int    firstAddress, lastAddress;

    firstAddress=symbols.firstAddress(instructionAddress);
    lastAddress=symbols.lastAddress(symbolAddress);
    if(firstAddress==symbolAddress && instructionAddress<lastAddress)
     return symbols.nameForAddress(firstAddress) + "+0x" + String.format("%X", instructionAddress-firstAddress) + "   " + String.format("[%08X]", instructionAddress);
    else
     return String.format("%08X", instructionAddress) + "     guessing: " + symbols.nameForAddress(firstAddress) + "+0x" + String.format("%X", instructionAddress-firstAddress);
   }

   public void call(int entryAddress, int returnAddress, int callInstructionAddress, int arguments) {
    Call call=new Call();

    if(debug.contains(symbols.nameForAddress(entryAddress))) {
     machine.debug=true;
    }

    call.entryAddress=entryAddress;
    call.returnAddress=returnAddress;
    call.callInstructionAddress=callInstructionAddress;
    call.framePointer=-1;
    call.localVariables=-1;
    callStack.add(call);
   }

   public void augment(int framePointer, int localVariables) {
    Call call;

    if(callStack.size()==0)
     throw new RuntimeException("Empty call stack");
    call=callStack.get(callStack.size()-1);
    if(call.localVariables!=-1) {
     System.err.println("CALL HAS ALREADY BEEN AUGMENTED: " + symbols.nameForAddress(call.entryAddress));
//     throw new RuntimeException("Call has already been augmented: " + symbols.nameForAddress(call.entryAddress));
    }
    call.framePointer=framePointer;
    call.localVariables=localVariables;
    if(debugCallHistory)
     System.out.println("Called " + symbols.nameForAddress(call.entryAddress) + "   " + String.format("[%08X]", call.entryAddress));
   }

   public void callReturn(int returnAddress) {
    Call call;

    if(callStack.size()==0)
     throw new RuntimeException("Empty call stack");
    call=callStack.get(callStack.size()-1);
    if(call.localVariables==-1) {
     System.err.println("CALL HAS NOT BEEN AUGMENTED");
//     throw new RuntimeException("Call has not been augmented");
    }
    if(returnAddress!=call.returnAddress && returnAddress!=call.returnAddress+1) {
     System.out.printf("call return address; %08X, stack return address: %08X\n", returnAddress, call.returnAddress);
     System.err.printf("call return address; %08X, stack return address: %08X\n", returnAddress, call.returnAddress);
//     throw new RuntimeException("Call return addresses do not match");
    }
    if(debug.contains(symbols.nameForAddress(call.entryAddress))) {
     machine.debug=false;
    }
    callStack.remove(callStack.size()-1);
    if(debugCallHistory)
     System.out.println("Returning to " + locationDescription(returnAddress, callStack.get(callStack.size()-1).entryAddress) + "(sp=" + String.format("%08X", machine.wsp) + ")");
   }

   public void backtrace(SymbolTable symbols, int pc) {
    Call   call;
    int    index, offset, callInstructionAddress;
    String location;

    for(index=callStack.size()-1;index>=0;index--) {
     call=callStack.get(index);
     if(index==callStack.size()-1)
      location=locationDescription(pc, call.entryAddress);
     else {
      callInstructionAddress=callStack.get(index+1).callInstructionAddress;

      if(callInstructionAddress!=-1)
       location=locationDescription(callStack.get(index+1).callInstructionAddress, call.entryAddress);
      else
       location=String.format("%08X\n", callStack.get(index+1).returnAddress);
     }
     System.out.printf("frame %2d -- %s\n", index, location);
    }
   }
}
