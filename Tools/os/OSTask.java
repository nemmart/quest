package os;

import hw.*;

public class OSTask implements Runnable {

   static public final int                  SUCCESS=0;
   static public final ThreadLocal<Machine> machineForBacktrace=new ThreadLocal<Machine>();

   public OSProcess process;
   public int       tid;
   public String    workingDirectory;
   public int       startAddress;
   public int       wfp, wsp, wsb, wsl;
   public int       stackFaultHandler;
   public int       killAddress;
   public Memory    memory;
   public Machine   machine;
   public Thread    thread;
   public boolean   halt;

   static public void backtrace() {
    machineForBacktrace.get().backtrace();
    machineForBacktrace.get().dumpHistory();
   }

   public OSTask(OSProcess process, int startAddress, int wfp, int wsp, int wsb, int wsl, int stackFaultHandler) {
    this.process=process;
    this.workingDirectory=process.workingDirectory;

    this.startAddress=startAddress;
    this.wfp=wfp;
    this.wsp=wsp;
    this.wsb=wsb;
    this.wsl=wsl;
    this.stackFaultHandler=stackFaultHandler;
    killAddress=-1;
    memory=process.memory;
    machine=new Machine(process, this, process.symbols, process.memory);
    machine.callStack.call(startAddress, -1, -1, 0);
    thread=null;
    halt=false;
   }

   public OSTask(OSProcess process, int startAddress, int stackBase, int stackSize, int stackFaultHandler) {
    this.process=process;
    this.workingDirectory=process.workingDirectory;

    this.startAddress=startAddress;
    this.wfp=0;
    this.wsp=stackBase;
    this.wsb=stackBase;
    this.wsl=stackBase+stackSize;
    this.stackFaultHandler=stackFaultHandler;
    killAddress=-1;
    memory=process.memory;
    machine=new Machine(process, this, process.symbols, process.memory);
//machine.addDebug("LOGON");
    machine.callStack.call(startAddress, -1, -1, 0);
    thread=null;
    halt=false;
   }

   public String fullPath(String filename) {
    if(filename.startsWith(":"))
     return filename;
    if(filename.startsWith("@"))
     return filename;
    if(workingDirectory.equals(":"))
     return ":"+filename;
    else
     return workingDirectory + ":" + filename;
   }

   public void launch() {
    thread=new Thread(this);

    thread.start();
   }

   public void halt() {
    halt=true;
    thread.interrupt();
   }

   public void run() {
    this.thread=Thread.currentThread();
    machineForBacktrace.set(machine);
    try {
     machine.run(startAddress);
     System.out.println("Thread halt");
    }
    catch(Exception exception) {
     machine.backtrace();
     exception.printStackTrace();
    }
    finally {
     System.out.println(machine.instructionCount + " instructions run");
     machine=null;
     process.unregisterTask(this);
     thread=null;
     machineForBacktrace.remove();
    }
   }

   public int dispatchSystemCall() {
    OSContext context;
System.out.printf("dispatchSystemCall: %08X\n", machine.wsp);
    int       address=memory.readWide((machine.wsp & 0x7FFFFFFF)-2);
    int       call=memory.readWord(address);
    int       returnAddress=machine.ac[3];
    int       error;

    context=OSContext.contextForCall(call, process, this, memory, machine);

    context.ac0=machine.ac[0];
    context.ac1=machine.ac[1];
    context.ac2=machine.ac[2];

    if(process.systemCallLogging) {
     System.out.printf("System Call %o, called from %08X\n", call, address-2);
     System.out.printf(" ac0=%08X\n", machine.ac[0]);
     System.out.printf(" ac1=%08X\n", machine.ac[1]);
     System.out.printf(" ac2=%08X\n", machine.ac[2]);
    }

    error=context.dispatchSystemCall(call);

    if(process.systemCallLogging)
     System.out.println("RETURNING AC1=" + context.ac1);

    machine.setPSR(machine.widePop()>>>16);
    if(error==SUCCESS) {
     machine.ac[0]=context.ac0;
     machine.ac[1]=context.ac1;
     machine.ac[2]=context.ac2;
     return returnAddress+1;
    }
    else {
     System.out.println("****** ERROR RETURN ******  CODE=" + String.format("%04X", error));
     System.out.println();
     machine.ac[0]=error;
     return returnAddress;
    }
   }
}
