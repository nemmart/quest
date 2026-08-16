package os;

public class OSMessage {
   public int originPID;
   public int originPort;
   public int destinationPID;
   public int destinationPort;
   public int userFlags;
   public int pointer;
   public int[] content;

   static public OSMessage terminateMessage(int pid, int destinationPID) {
    return new OSMessage(OS.aosSymbol("?SPTM"), destinationPID<<16, OS.aosSymbol("?TEXT")+pid, 0, new int[0]);
   }

   public OSMessage(int origin, int destination, int userFlags, int pointer, int[] content) {
    originPID=origin>>>16;
    originPort=origin & 0xFFFF;
    destinationPID=destination>>>16;
    destinationPort=destination & 0xFFFF;
    this.userFlags=userFlags;
    this.pointer=pointer;
    this.content=content;
   }

   public int origin() {
    return (originPID<<16)|originPort;
   }

   public int destination() {
    return (destinationPID<<16)|destinationPort;
   }

   public int originPID() {
    return originPID;
   }

   public int originPort() {
    return originPort;
   }

   public int destinationPID() {
    return destinationPID;
   }

   public int destinationPort() {
    return destinationPort;
   }

   public int userFlags() {
    return userFlags;
   }

   public int pointer() {
    return pointer;
   }

   public int[] content() {
    return content;
   }
}
