package hw;

public class QueueEntry {
   public Machine          machine;
   public int              address;
   public int              steps;
   public boolean          completed;
   public RuntimeException exception;

   public QueueEntry(Machine machine, int address, int steps) {
    this.machine=machine;
    this.address=address;
    this.steps=steps;
    completed=false;
    exception=null;
   }

   public void run() {
    try {
     address=machine.runSteps(address, steps);
     completed=true;
    }
    catch(RuntimeException exception) {
     this.exception=exception;
     completed=true;
    }
   }
}
