package os;

import hw.*;

public class OSContextTask extends OSContext {
   public OSContextTask(OSProcess process, OSTask task, Memory memory, Machine machine) {
    super(process, task, memory, machine);
   }

   public int dispatchSystemCall(int call) {
    switch(call) {
     case TASK:
      return TASK();

     case REC:
      return REC();

     case KILAD:
      return KILAD();

     case UIDSTAT:
      return UIDSTAT();
    }
    throw new RuntimeException("Dispatch system call - missing case");
   }


   public int TASK() {
    OSTask task;
    int    packetType=readPacketWide("?DLNK"), priority=readPacketWord("?DPRI"), taskID=readPacketWord("?DID");
    int    startAddress=readPacketWide("?DPC"), taskAC2=readPacketWide("?DAC2"), stackBase=readPacketWide("?DSTB");
    int    faultHandler=readPacketWord("?DSFLT"), stackHigh=readPacketWord("?DSSZ"), stackLow=readPacketWord("?DSSL");
    int    flags=readPacketWord("?DFLGS"), taskCount=readPacketWord("?DNUM");

    System.out.println();
    System.out.println("TASK:");
    System.out.printf("   packet type = %08X\n", packetType);
    System.out.printf("   priority = %04X\n", priority);
    System.out.printf("   task ID = %04X\n", taskID);
    System.out.printf("   starting address = %08X\n", startAddress);
    System.out.printf("   AC2 = %08X\n", taskAC2);
    System.out.printf("   stack base = %08X\n", stackBase);
    System.out.printf("   stack size high = %04X\n", stackHigh);
    System.out.printf("   stack size low = %04X\n", stackLow);
    System.out.printf("   stack fault handler = %04X\n", faultHandler);
    System.out.printf("   flags = %04X\n", flags);
    System.out.printf("   task count = %04X\n", taskCount);
    System.out.println();

    task=new OSTask(process, startAddress, stackBase, (stackHigh<<16)+stackLow, faultHandler);
    if(process.registerTask(task)) {
     task.machine.ac[2]=taskAC2;
     task.launch();
     writePacketWord("?DID", task.tid);
    }
    return SUCCESS;
   }

   public int REC() {
    int value;

    System.out.println();
    System.out.println("REC:");
    System.out.printf("   mailbox = %08X\n", ac0);
    System.out.printf("   mailbox value = %08X\n", memory.readWide(ac0));
    System.out.println();

    synchronized(memory) {
     do {
      value=memory.readWide(ac0);
      if(value==0 && process.countTasks()>1) {
       try {
        memory.wait(3000);
       }
       catch(InterruptedException ignore) {
        throw new RuntimeException("System call 'REC' interrupted");
       }
      }
     } while(value==0 && process.countTasks()>1);
     memory.writeWide(ac0, 0);
    }
    System.out.printf("mailbox result: %08X\n", value);
    ac1=value;
    return SUCCESS;
   }

   public int KILAD() {
    task.killAddress=ac0;
    return SUCCESS;
   }

   public int UIDSTAT() {
    writePacketWord("?UUID", process.taskSlot(task));
    writePacketWord("?UTSTAT", 0);
    writePacketWord("?UTID", task.tid);
    writePacketWord("?UTPRI", 0);
    return SUCCESS;
   }
}
