package os;

import java.io.*;

public class FSConsole implements FSStreamIO, FSObject {
   public String path;

   public FSConsole() {
    path=null;
   }

   public String getPath() {
    return path;
   }

   public void setPath(String path) {
    if(this.path!=null)
     throw new RuntimeException("Attempt to change an FSObject path");
    this.path=path;
   }

   public int available() {
    // system console available doesn't work
    return 0;
   }

   public int read(byte[] bytes) {
    try {
     return System.in.read(bytes);
    }
    catch(IOException exception) {
     throw new RuntimeException("FSConsole read() failed: " + exception.getMessage());
    }
   }

   public void write(byte[] bytes) {
    try {
     System.out.write(bytes);
    }
    catch(IOException exception) {
     throw new RuntimeException("FSConsole write() failed: " + exception.getMessage());
    }
   }

   public void close() {
   }
}
