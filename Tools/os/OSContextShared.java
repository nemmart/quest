package os;

import java.util.*;
import hw.*;

public class OSContextShared extends OSContext {
   public OSContextShared(OSProcess process, OSTask task, Memory memory, Machine machine) {
    super(process, task, memory, machine);
   }

   public int dispatchSystemCall(int call) {
    switch(call) {
     case GSHPT:
      return GSHPT();

     case SSHPT:
      return SSHPT();

     case SOPEN:
      return SOPEN();

     case SPAGE:
      return SPAGE();
    }
    throw new RuntimeException("Dispatch system call - missing case");
   }

   public int GSHPT() {
    ac0=process.sharedStart;
    ac1=process.sharedPages.length;
    return SUCCESS;
   }

   public int SSHPT() {
    OSSharedPageSource[] replacement;
    int                  index;

    if(ac0!=process.sharedStart)
     return OSError.OS_NOT_IMPLEMENTED;
    replacement=new OSSharedPageSource[ac1];
    for(index=0;index<ac1;index++)
     if(index<process.sharedPages.length)
      replacement[index]=process.sharedPages[index];
    process.sharedPages=replacement;
    return SUCCESS;
   }

   public int SOPEN() {
    String file=fullPath(readString(ac0));
    int    slot=-1;

    System.out.println();
    System.out.println("SOPEN:");
    System.out.println("  ac0 = " + file);
    System.out.println("  ac1 = " + ac1);
    System.out.println("  ac2 = " + ac2);

    if(ac1!=-1)
     throw new RuntimeException("Unsupported SOPEN option");

    try {
     synchronized(process.channels) {
      slot=process.assignChannel(FSChannel.openForPagedIO(file, ac2==0));
      ac1=slot;
     }
    }
    catch(OSError error) {
     return error.error();
    }

    System.out.println("Assigned channel " + slot);
    System.out.println();
    return SUCCESS;
   }

   public int SPAGE() {
    FSChannel channel;
    int       pageCount, diskPageNumber, mapAddress, memoryPageNumber;
    boolean   readOnly;
    byte[]    pageContents;

    readOnly=readPacketFlag("?PSTI", "?SPRO");
    pageCount=readPacketByte("?PSTI", OSContext.LOW)/4;
    diskPageNumber=readPacketWide("?PRNH")/2048;
    mapAddress=readPacketWide("?PCAD");

    if(mapAddress%1024!=0)
     throw new RuntimeException("Non aligned SPAGE load");

    memoryPageNumber=mapAddress>>10;

    System.out.println();
    System.out.println("SPAGE:");
    System.out.println("   read only = " + readOnly);
    System.out.printf("   memory destination = %08X\n", readPacketWide("?PCAD"));
    System.out.printf("   page count (ignored) = %d\n", pageCount);
    System.out.printf("   start page = %d\n", diskPageNumber);
    System.out.println();

    try {
     synchronized(process.channels) {
      // we map from the diskPageNumber through end of file
      channel=process.retrieveChannel(ac1);
      if(channel.readOnly()) {
       readOnly=true;
       System.out.println("   Channel is read only!");
      }

      // If this file is being mapped for shared writable access,
      // switch it from array-backed to memory-mapped.  This way
      // writes go directly to disk via the OS page cache.
      // Files opened for streamed I/O (like USER_DATA_FILE) never
      // reach this code path, so they stay array-backed and can
      // still be resized.
      if(!readOnly) {
       FSFile fsFile=channel.getFile();
       if(fsFile!=null)
        fsFile.enableMemoryMapping();
      }

      process.mapFile(channel, memoryPageNumber, channel.pageCount()-diskPageNumber, diskPageNumber, true, !readOnly, false);
     }
    }
    catch(OSError error) {
     return error.error();
    }

    return SUCCESS;
   }

}
