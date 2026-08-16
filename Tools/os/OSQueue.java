package os;

import java.util.*;

public class OSQueue {
   public List<OSMessage> messages;

   public OSQueue() {
    messages=new ArrayList<OSMessage>();
   }

   synchronized public void enqueue(OSMessage message) {
    messages.add(message);
    notifyAll();
   }

   synchronized public OSMessage dequeue() {
    OSMessage message;

    while(true) {
     if(messages.size()>0) {
      message=messages.get(0);
      messages.remove(0);
      return message;
     }

     try {
      wait(1000);
     }
     catch(InterruptedException exception) {
      return null;
     }
    }
   }
}