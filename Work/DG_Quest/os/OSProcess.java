package os;

import java.io.*;
import java.util.*;

import hw.*;
import debug.*;

public class OSProcess {
   static public final int SUCCESS=0;

   static public final int SEGMENT_BASE=0x70000000>>10;

   public int                  pid;
   public String               program;
   public String               workingDirectory;
   public SymbolTable          symbols;    // shared symbols across all machines
   public Memory               memory;     // shared memory across all machines
   public OSTask[]             tasks;      // each task has a machine
   public FSChannel[]          channels;
   public int                  unsharedStop;
   public int                  sharedStart;
   public OSSharedPageSource[] sharedPages;
   public String               startDirectory;
   public boolean              terminating;
   public boolean              systemCallLogging;

   static public byte[] privateCopy(byte[] source) {
    int    index, length=source.length;
    byte[] copy=new byte[length];

    for(index=0;index<length;index++)
     copy[index]=source[index];
    return copy;
   }

   public OSProcess(FSStreamIO terminal, String workingDirectory, String program) {
    String      fullPath;
    FSObject    symbolFile;
    FSChannel   channel=null;
    Page        page0, page8;
    int         sharedStartPage, sharedPageCount, fileSharedOffset, threads;
    OSTask      task;

    this.program=program;
    this.workingDirectory=workingDirectory;
    fullPath=fullPath(workingDirectory, program);

    tasks=new OSTask[32];
    channels=new FSChannel[128];
    channels[0]=FSChannel.openForStreamedIO(terminal, FSChannel.READ_WRITE_PERMISSION);

    symbolFile=FS.retrieve(fullPath+".ST");
    if(symbolFile!=null && (symbolFile instanceof FSFile)) {
     symbols=SymbolTable.parseSTFormat(((FSFile)symbolFile).fullContents());
    }
    if(symbols==null) {
     symbolFile=FS.retrieve(fullPath+".SYM");
     if(symbolFile!=null && (symbolFile instanceof FSFile))
      symbols=SymbolTable.parseSYMFormat(((FSFile)symbolFile).fullContents());
    }
    if(symbols==null)
     symbols=new SymbolTable();

    try {
     channel=FSChannel.openForPagedIO(fullPath+".PR", true);
     channels[1]=channel;

     page0=OS.readOnlyPage(channel.readPage(0));
     page8=OS.readOnlyPage(channel.readPage(8));
    }
    catch(OSError error) {
     throw new RuntimeException("Unable to load " + fullPath + ".PR");
    }

    sharedStartPage=page0.readWord(0x10F);
    sharedPageCount=page0.readWord(0x113);
    fileSharedOffset=page0.readWord(0x11A);
    threads=page0.readWord(0x10A);

    // instance variables that track shared page source
    sharedPages=new OSSharedPageSource[sharedPageCount];
    unsharedStop=fileSharedOffset-8;
    sharedStart=sharedStartPage;

    memory=new Memory();
    mapFile(channel, SEGMENT_BASE, fileSharedOffset-8, 8, false, true, true);
    mapFile(channel, sharedStartPage+SEGMENT_BASE, sharedPageCount, fileSharedOffset, true, false, false);

    terminating=false;
    pid=OS.global.registerProcess(this);
    systemCallLogging=true;

    //                    start address          wfp                 wsp                 wsb                 wsl                 fault handler
    task=new OSTask(this, page0.readWide(0x17C), page8.readWide(16), page8.readWide(18), page8.readWide(20), page8.readWide(22), page8.readWord(12));
    registerTask(task);
    task.launch();
   }

   public String fullPath(String workingDirectory, String filename) {
    if(filename.startsWith(":"))
     return filename;
    if(filename.startsWith("@"))
     return filename;
    if(workingDirectory.equals(":"))
     return ":"+filename;
    else
     return workingDirectory + ":" + filename;
   }

   public FSStreamIO console() {
    // Channel 0 is always Streaming device
    return (FSStreamIO)channels[0].file;
   }

   public void mapFile(FSChannel channel, int memoryPage, int pageCount, int filePage, boolean shared, boolean writePermission, boolean executePermission) {
    int         index;
    byte[]      content;
    Page        page;
    FSPagedFile pagedFile=(FSPagedFile)channel.file;
    FSFile      file=pagedFile.file;

    for(index=0;index<pageCount;index++) {
     content=channel.readPage(filePage+index);
     if(!shared)
      page=OS.privatePage(content);
     else if(shared && !writePermission && executePermission)
      page=OS.codePage(content);
     else if(shared && !writePermission && !executePermission)
      page=OS.readOnlyPage(content);
     else {
      page=OS.sharedPage(content, file);
     }
     memory.mapPage(page, memoryPage+index);
     if(shared && !executePermission)
      sharedPages[memoryPage+index-sharedStart-SEGMENT_BASE]=new OSSharedPageSource(channel, filePage+index);
    }
   }

   public int countTasks() {
    int count=0, index;

    synchronized(tasks) {
     for(index=0;index<tasks.length;index++)
      if(tasks[index]!=null)
       count++;
    }
    return count;
   }

   public int taskSlot(OSTask task) {
    int index;

    synchronized(tasks) {
     for(index=0;index<tasks.length;index++)
      if(tasks[index]==task)
       return index;
    }
    return -1;
   }

   public boolean registerTask(OSTask task) {
    int index, tid=OS.global.nextTID();

    synchronized(tasks) {
     if(terminating) {
      task.tid=-1;
      return false;
     }
     for(index=0;index<tasks.length;index++)
      if(tasks[index]==null) {
       task.tid=tid;
       tasks[index]=task;
       break;
      }
     if(index==tasks.length)
      throw new RuntimeException("Register task: too many tasks are running");
    }
    return true;
   }

   public void unregisterTask(OSTask task) {
    int index;

    synchronized(tasks) {
     task.tid=-1;
     for(index=0;index<tasks.length;index++)
      if(tasks[index]==task) {
       tasks[index]=null;
       break;
      }
     if(index==tasks.length)
      throw new RuntimeException("Unregister task: task not found");
     if(countTasks()==0) {
      OS.global.unregisterProcess(this);
      pid=-1;
     }
    }
    if(pid==-1) {
     for(index=0;index<channels.length;index++) {
      if(channels[index]!=null) {
       FSFile file=channels[index].getFile();

       if(file!=null)
        System.err.println("OPEN Channel " + index + ", path " + file.getPath() + " modified: " + file.modified);
      }
     }
    }
   }

   public int nextChannel() {
    int index;

    // we assume the channel array is locked
    for(index=0;index<channels.length;index++) {
     if(channels[index]==null)
      return index;
    }
    return -1;
   }

   public int assignChannel(FSChannel channel) {
    // we assume channels array is locked
    for(int index=0;index<channels.length;index++) {
     if(channels[index]==null) {
      channels[index]=channel;
      return index;
     }
    }
    throw new OSError(OSError.OS_MAXIMUM_NUMBER_OF_FILES_OPEN);
   }

   public FSChannel retrieveChannel(int channel) {
    if(channel<0 || channel>127)
     throw new OSError(OSError.OS_INVALID_CHANNEL_NUMBER);
    if(channels[channel]==null)
     throw new OSError(OSError.OS_INVALID_CHANNEL_NUMBER);
    return channels[channel];
   }

/*
   static public void main(String[] args) {
    OSProcess process=new OSProcess(new FSConsole());
    int       error;

    FS.initializeWithPath(args[0]);
    if(args[0].endsWith(".PR"))
     error=process.launch(args[0]);
    else
     error=process.launch(args[0]+".PR");
    if(error!=SUCCESS)
     System.out.printf("Error code: %04X", error);
    process.machine.run(-1);
   }
*/
}
