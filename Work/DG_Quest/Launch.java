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

   static public void main(String[] args) {
    FSTerminal terminal;
    OSProcess  process;
    int        error, index, firstArg=1;

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
     process=null;
     error=0;
     args[index]=args[index].toUpperCase();
     if(args[index].endsWith(".PR"))
      args[index]=args[index].substring(0, args[index].length()-3);
     if(args[index].startsWith("@")) {
      System.err.println("Waiting for terminal client for " + args[index]);
      process=new OSProcess(waitForClient(), ":", args[index].substring(1));
     }
     else
      process=new OSProcess(new FSConsole(), ":", args[index]);
    }
   }
}
