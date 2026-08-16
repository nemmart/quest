import java.io.*;
import java.net.*;

import hw.*;
import os.*;

public class Launch {
   static public ServerSocket acceptor=null;

   static public FSTerminal waitForClient() {
    Socket client;

    try {
     if(acceptor==null)
      acceptor=new ServerSocket(8781);
     client=acceptor.accept();
     return new FSTerminal(client);
    }
    catch(Exception exception) {
     throw new RuntimeException("Wait for terminal connection failed: " + exception.getMessage());
    }
   }

/*
   static public Launchable construct(String name) {
    try {
     Object launchable=Class.forName(name).getDeclaredConstructor().newInstance();

     if(launchable instanceof Launchable)
      return (Launchable)launchable;
     return null;
    }
    catch(Exception exception) {
     return null;
    }
   }
*/

   static public void main(String[] args) {
    FSStreamIO stream;
    Launchable process;
    int        error, index, firstArg=1;
    int        type=0;

    if(args.length==0) {
     System.err.println("Usage: java Launch <PR file>");
     System.err.println("-or-   java Launch <dir> <PR file 1> <PR file 2> ... <PR file n>");
     System.err.println();
     System.err.println("If a PR file name starts with an @, then the launcher waits for a");
     System.err.println("terminal to connect to port 8781.");
     System.exit(1);
    }

    if(args.length==1) {
     FS.initializeWithPath(args[0]);
     firstArg=0;
    }
    else {
     FS.initializeWithPath(args[0]);
     firstArg=1;
    }

    for(index=firstArg;index<args.length;index++) {
     if(args[index].startsWith("@")) {
      System.err.println("Waiting for terminal client for " + args[index]);
      stream=waitForClient();
      args[index]=args[index].substring(1);
     }
     else
      stream=new FSConsole();

     type=0;
     if(args[index].toUpperCase().endsWith(".PR")) {
      type=1;
      args[index]=args[index].substring(0, args[index].length()-3);
     }
//     else if(args[index].toUpperCase().endsWith(".CLASS")) {
//      type=2;
//      args[index]=args[index].substring(0, args[index].length()-6);
//     }
     else if(FS.retrieve(":" + args[index].toUpperCase() + ".PR")!=null)
      type=1;
//     else if(construct(args[index])!=null)
//      type=2;

     if(type==0)
      System.out.println("Launch target " + args[index] + " not found");
     else {
      process=null;
      if(type==1)
       process=new OSProcess(":", args[index].toUpperCase());
      process.launch(stream);
     }
    }
   }
}
