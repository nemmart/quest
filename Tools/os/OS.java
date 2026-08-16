package os;

import java.net.*;
import java.util.*;
import java.io.*;

import hw.*;

public class OS {
   static public final int SUCCESS=0;

   static public OS global=new OS();

   public ServerSocket           acceptor;
   public int                    nextPID=100, nextTID=100;
   public Map<Integer,OSProcess> pids;
   public Map<Integer,OSQueue>   queues;
   public Map<String,Integer>    services;
   public Set<Integer>           connections;

/*
   static public Page readOnlyPage(byte[] content) {
    return new ArrayPage(content, false, false);
   }

   static public Page codePage(byte[] content) {
    return new ArrayPage(content, false, true);
   }

   static public Page privatePage() {
    return new ArrayPage(true, true);
   }

   static public Page privatePage(byte[] content) {
    return new ArrayPage(content, true, true);
   }

   static public Page sharedPage(byte[] content, FSFile file) {
    ArrayPage page=new ArrayPage(content, true, false);

    page.setFile(file);
    return page;
   }
*/

   static public int aosError(String name) {
    Integer value=AOSVSSymbols.symbols.get(name);

    if(value==null)
     throw new RuntimeException("Undefined AOS/VS symbol '" + name + "'");
    return value.intValue();
   }

   static public int aosSymbol(String name) {
    Integer value=AOSVSSymbols.symbols.get(name);

    if(value==null)
     throw new RuntimeException("Undefined AOS/VS symbol '" + name + "'");
    return value.intValue();
   }

   public FSChannel openFile(OSProcess process, String fullPath, int options) {
    if(fullPath.startsWith("@")) {
     if(fullPath.equalsIgnoreCase("@INPUT")) {
      return FSChannel.openForStreamedIO(process.console(), FSChannel.READ_PERMISSION);
     }
     else if(fullPath.equalsIgnoreCase("@OUTPUT")) {
      return FSChannel.openForStreamedIO(process.console(), FSChannel.WRITE_PERMISSION);
     }
     throw new OSError(OSError.FS_FILE_NOT_FOUND);
    }
    else
     return FSChannel.openForStreamedIO(fullPath, true);
   }

   public OS() {
    pids=new HashMap<Integer,OSProcess>();
    queues=new HashMap<Integer,OSQueue>();
    services=new HashMap<String,Integer>();
    connections=new HashSet<Integer>();
   }

   synchronized public int registerProcess(OSProcess process) {
    int pid;

    synchronized(pids) {
     pid=nextPID++;
     pids.put(pid, process);
    }
    synchronized(queues) {
     queues.put(pid, new OSQueue());
    }
    return pid;
   }

   synchronized public void unregisterProcess(OSProcess process) {
    int pid=process.pid;

    System.err.println("Unregistering process " + process.pid);
    synchronized(pids) {
     pids.remove(pid);
    }
    synchronized(queues) {
     queues.remove(pid);
    }
    synchronized(services) {
     List<String>     remove=new ArrayList<String>();
     Iterator<String> iterator=services.keySet().iterator();

     while(iterator.hasNext()) {
      String service=iterator.next();

      if(services.get(service)>>>16==pid)
       remove.add(service);
     }
     iterator=remove.iterator();
     while(iterator.hasNext())
      services.remove(iterator.next());
    }
    synchronized(connections) {
     List<Integer>     remove=new ArrayList<Integer>();
     Iterator<Integer> iterator=connections.iterator();
     int               connection;

     while(iterator.hasNext()) {
      connection=iterator.next();
      if((connection & 0xFFFF)==pid || (connection>>>16)==pid)
       remove.add(connection);
     }
     synchronized(queues) {
      iterator=remove.iterator();
      while(iterator.hasNext()) {
       connection=iterator.next();
       connections.remove(connection);
       if((connection>>>16)==pid) {
        System.err.println("message to " + (connection & 0xFFFF));
        sendMessage(OSMessage.terminateMessage(pid, connection & 0xFFFF));
       }
      }
     }
    }
   }

   synchronized public int nextTID() {
    return nextTID++;
   }

   public void registerService(OSProcess process, String service, int localPort) {
    synchronized(services) {
     if(services.get(service)!=null)
      throw new OSError(OSError.OS_SERVICE_ALREADY_EXISTS);
     services.put(service, (process.pid<<16)+localPort);
    }
   }

   public int retrieveService(String service) {
    synchronized(services) {
     if(services.get(service)==null)
      throw new OSError(OSError.OS_SERVICE_DOES_NOT_EXIST);
     return services.get(service);
    }
   }

   public void sendMessage(OSMessage message) {
    int     destinationPID=message.destinationPID();
    OSQueue queue;

    synchronized(queues) {
     queue=queues.get(destinationPID);
    }
    if(queue==null)
     throw new RuntimeException("Invalid PID specified");
    queue.enqueue(message);
   }

   public OSMessage receiveMessage(int pid) {
    OSQueue   queue;

    synchronized(queues) {
     queue=queues.get(pid);
    }
    if(queue==null)
     throw new RuntimeException("Invalid PID specified");
    return queue.dequeue();
   }

   public void connect(OSProcess process, int pid) {
System.err.println("Connecting " + process.pid + " with " + pid);
    synchronized(connections) {
     connections.add((process.pid<<16)+pid);
     connections.add((pid<<16)+process.pid);
    }
   }

/*

    pids=new HashMap<Integer,OSProcess>();
    try {
     acceptor=new ServerSocket(8781);
    }
    catch(IOException exception) {
     throw new RuntimeException("Unable to open server socket on port 8781 - " + exception.getMessage());
    }


   public Socket acceptClient() {
    Socket client=null;

    try {
     client=acceptor.accept();
    }
    catch(IOException ignore) {
    }
    return client;
   }

   public void run() {
    Socket client;

    System.out.println("OS started");
    while(true) {
     client=acceptClient();
     if(client==null) {
      try {
       Thread.sleep(500);  // sleep for half a second
      }
      catch(InterruptedException ignore) {
      }
     }
     else {
      FSChannel  channel;
      OSProcess  process;
      Thread     thread;

      process=new OSProcess(new FSTerminal(client));

      thread=new Thread(process);
      thread.start();
     }
    }
   }

   static public void main(String[] args) {
    OS.global.run();
   }
*/
}
