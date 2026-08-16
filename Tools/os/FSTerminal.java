package os;

import java.net.*;
import java.io.*;

public class FSTerminal implements FSStreamIO, FSObject {
   public String       path;
   public Socket       socket;
   public InputStream  inputStream;
   public OutputStream outputStream;

   public FSTerminal(Socket socket) {
    byte[] clearScreen=new byte[]{014};

    path=null;
    this.socket=socket;
    try {
     this.inputStream=new BufferedInputStream(socket.getInputStream());
     this.outputStream=new BufferedOutputStream(socket.getOutputStream());
     outputStream.write(clearScreen);
    }
    catch(IOException exception) {
     throw new RuntimeException("FSTerminal getInput/OutputStream() failed: " + exception.getMessage());
    }
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
    try {
     return inputStream.available();
    }
    catch(IOException exception) {
     throw new RuntimeException("FSTerminal available() failed: " + exception.getMessage());
    }
   }

   public void process() {
   }

   public int read(byte[] bytes, boolean lineMode) {
    byte[] charBuffer=new byte[1], erase=new byte[]{031, ' ', 031}, nextline=new byte[]{'\r', '\n'};
    int    position=0, length;

    if(!lineMode)
     return read(bytes);

    try {
     while(true) {
      length=read(charBuffer);
      if(length<0)
       return (position==0)?-1:position;
      if(charBuffer[0]>=' ' && charBuffer[0]<127) {
       if(position==bytes.length)
        charBuffer[0]=007;          // bell
       else
        bytes[position++]=charBuffer[0];
       outputStream.write(charBuffer);
      }
      else if(charBuffer[0]=='\b' || charBuffer[0]==127) { // backspace or del
       if(position==0) {
        charBuffer[0]=007;
        outputStream.write(charBuffer);
       }
       else {
        position--;
        outputStream.write(erase);
       }
      }
      else if(charBuffer[0]=='\r' || charBuffer[0]=='\n') {  // enter
       outputStream.write(nextline);
       outputStream.flush();
       return position;
      }
      if(inputStream.available()==0)
       outputStream.flush();
     }
    }
    catch(IOException exception) {
     throw new RuntimeException("FSTerminal read line failed: " + exception.getMessage());
    }
   }

   public int read(byte[] bytes) {
    int length=0;

    try {
     length=inputStream.read(bytes);
     if(length<0)
      throw new RuntimeException("FSTerminal disconnect");
    }
    catch(IOException exception) {
     throw new RuntimeException("FSTerminal read() failed: " + exception.getMessage());
    }
    return length;
   }


   public void write(byte[] bytes) {
    int last, index;

    try {
     outputStream.write(bytes);
     outputStream.flush();
    }
    catch(IOException exception) {
     throw new RuntimeException("FSTerminal write() failed: " + exception.getMessage());
    }
   }

   public void close() {
   }
}
