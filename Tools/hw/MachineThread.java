package hw;

import java.util.*;

/***
 *
 *  This class is used so that all instructions are executed by the same thread, i.e., the machineThread.
 *  This is important to do because the MV series has complicated atomic semantics.  If it were multithreaded,
 *  it would require locking down individual pages of memory for each access and potential race conditions.
 *  By making it single threaded, we avoid all the funky locking issues.
 *
 ***/

public class MachineThread implements Runnable {
   private Thread           machineThread;
   private List<QueueEntry> queue;

   public MachineThread() {
    queue=new ArrayList<QueueEntry>();
    machineThread=new Thread(this);
    machineThread.start();
   }

   public void queueWait() {
    try {
     queue.wait();
    }
    catch(InterruptedException ignore) {
    }
   }

   public void run() {
    QueueEntry entry;

    while(true) {
     entry=null;
     synchronized(queue) {
      if(queue.size()!=0) {
       entry=queue.get(0);
       queue.remove(0);
      }
      else
       queueWait();
     }
     if(entry==null)
      continue;
     synchronized(entry) {
      entry.run();
      entry.notifyAll();
     }
    }
   }

   public int runSteps(Machine machine, int address, int steps) {
    QueueEntry entry=new QueueEntry(machine, address, steps);

    synchronized(entry) {
     synchronized(queue) {
      queue.add(entry);
      queue.notifyAll();
     }
     while(!entry.completed) {
      try {
       entry.wait();
      }
      catch(InterruptedException ignore) {
      }
     }
    }

    if(entry.exception!=null)
     throw entry.exception;
    return entry.address;
   }
}