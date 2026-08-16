package os;

import java.util.*;
import hw.*;

public class OSContextSystem extends OSContext {
   public OSContextSystem(OSProcess process, OSTask task, Memory memory, Machine machine) {
    super(process, task, memory, machine);
   }

   public int dispatchSystemCall(int call) {
    switch(call) {
     case RECREATE:
      return RECREATE();

     case MEM:
      return MEM();

     case MEMI:
      return MEMI();

     case GTOD:
      return GTOD();

     case PNAME:
      return PNAME();

     case DADID:
      return DADID();

     case RETURN:
      return RETURN();

     case IXIT:
      return IXIT();

     case INTWT:
      return INTWT();

     case WDELAY:
      return WDELAY();
    }
    throw new RuntimeException("Dispatch system call - missing case");
   }

   public int RECREATE() {
    System.out.println();
    System.out.println("RECREATE:");
    System.out.printf("   name = %s\n", readString(ac0));
    System.out.println();
    return SUCCESS;
   }

   public int MEM() {
    ac0=process.sharedStart-process.unsharedStop;
    ac1=process.unsharedStop;
    ac2=(process.unsharedStop+process.SEGMENT_BASE<<10)-1;

    System.out.println();
    System.out.println("MEM (return values)");
    System.out.printf("  ac0 = %d\n", ac0);
    System.out.printf("  ac1 = %d\n", ac1);
    System.out.printf("  ac2 = %08X\n", ac2);
    System.out.println();
    return SUCCESS;
   }

   public int MEMI() {
    int index=process.unsharedStop, count;

    if(ac0>0) {
     count=ac0;
     while(count>0) {
      memory.mapPage(new ArrayPage(), process.unsharedStop+process.SEGMENT_BASE, Permissions.PERMISSIONS_READ_WRITE_EXECUTE);  // Should this have execute??
      process.unsharedStop++;
      count--;
     }
     ac1=(process.unsharedStop+process.SEGMENT_BASE<<10)-1;
     System.out.println();
     System.out.println("MEMI (return values)");
     System.out.printf("  ac1 = %08X\n", ac1);
     System.out.println();
     return SUCCESS;
    }
    return OSError.OS_NOT_IMPLEMENTED;
   }

   public int GTOD() {
    Calendar now=Calendar.getInstance();

    ac0=now.get(Calendar.SECOND);
    ac1=now.get(Calendar.MINUTE);
    ac2=now.get(Calendar.HOUR_OF_DAY);
    return SUCCESS;
   }

   public int PNAME() {
    System.out.println();
    System.out.println("PNAME:");
    System.out.printf("   ac0 = %08X\n", ac0);
    System.out.printf("   ac1 = %08X\n", ac0);
    System.out.println();
    if(ac1==-1 && ac0==0) {
     ac1=process.pid;
     return SUCCESS;
    }
    return OSError.OS_NOT_IMPLEMENTED;
   }

   public int DADID() {
    ac1=1;
    return SUCCESS;
   }

   public int RETURN() {
    System.out.println("TERMINATING PROCESS");
    if((ac2 & 0xFF)!=0)
     System.out.println("   message = " + readString(ac1, ac2 & 0xFF));
    else
     System.out.println("   no termination message");

    if((ac2 & 0xFF)!=0)
     System.err.println("termination message = " + readString(ac1, ac2 & 0xFF));
    else
     System.err.println("no termination message");

    process.terminating=true;
    synchronized(process.tasks) {
     for(int index=0;index<process.tasks.length;index++) {
      if(process.tasks[index]!=null && process.tasks[index]!=task)
       process.tasks[index].halt();
     }
    }
    while(true) {
     if(process.countTasks()>1) {
      try {
       System.err.println("Waiting for tasks to terminate");
       Thread.sleep(100);
      }
      catch(InterruptedException ignore) {
       // ignore this one
      }
     }
     else
      break;
    }
    throw new RuntimeException("EXIT!");
   }

   public int IXIT() {
    System.out.println();
    System.out.println("IXIT -- ignored");
    System.out.println();
    return SUCCESS;
   }

   public int INTWT() {
    System.out.println();
    System.out.println("INTWT");
    System.out.println();
    while(true) {
     try {
      Thread.sleep(1000);
     }
     catch(InterruptedException ignore) {
      throw new RuntimeException("System call 'INTWT' interrupted");
     }
    }
//    return SUCCESS;
   }

   public int WDELAY() {
    System.out.println();
    System.out.printf("WDELAY FOR %d MS\n", ac0);
    System.out.println();
    try {
     Thread.sleep(ac0);
    }
    catch(InterruptedException exception) {
     // let this one be
    }
    return SUCCESS;
   }
}
