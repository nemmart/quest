package os;

import java.io.*;
import java.nio.*;
import java.nio.channels.*;

import hw.*;

public class FSFile implements FSObject {
   static public boolean COMMIT=true;

   public String   path;
   public String   filePath;
   public int      length;
   public int      pageCount;
   public boolean  modified;
   public byte[][] pages;
   public PageSet  pageSet;

   // Memory-mapped fields (null until enableMemoryMapping() is called)
   private RandomAccessFile mappedRAF;
   private FileChannel      mappedChannel;
   private MappedByteBuffer mappedBuffer;
   private boolean          memoryMapped;

   public FSFile() {
    path=null;
    memoryMapped=false;
   }

   public String getPath() {
    return path;
   }

   public void setPath(String path) {
    if(this.path!=null)
     throw new RuntimeException("Attempt to change an FSObject path");
    this.path=path;
   }

   public void modifiedNotification() {
    modified=true;
   }

   // ---------------------------------------------------------------
   // loadPages: always loads into byte[] arrays.
   // This is the original implementation.  Memory mapping happens
   // later, on demand, via enableMemoryMapping().
   // ---------------------------------------------------------------
   public int loadPages(String filePath) {
    byte[] content;
    int    index, offset;

    try {
     this.filePath=filePath;
     content=Utility.readFile(filePath);
    }
    catch(IOException exception) {
     return OSError.FS_ERROR_READING_DATA;
    }
    memoryMapped=false;
    modified=false;
    length=content.length;
    pageCount=(content.length+2047)/2048;
    pages=new byte[pageCount][];
    pageSet=new PageSet();
    for(index=0;index<content.length;index=index+2048) {
     byte[] page=new byte[2048];

     for(offset=0;offset<2048 && index+offset<content.length;offset++)
      page[offset]=content[index+offset];
     pages[index>>>11]=page;
     pageSet.mapPage(new ArrayPage(page, this), index>>>11);
    }
    return OS.SUCCESS;
   }

   // ---------------------------------------------------------------
   // enableMemoryMapping: switch from array-backed to mmap-backed.
   //
   // Called on demand when SPAGE maps this file for shared paged
   // access.  Idempotent -- safe to call multiple times.
   //
   // Creates a MappedByteBuffer over the disk file, copies current
   // page contents into the buffer, then replaces the ArrayPage
   // objects in the PageSet with MappedPage objects.
   //
   // After this call, all reads/writes go through the mmap and are
   // automatically flushed to disk by the OS.
   // ---------------------------------------------------------------
   public synchronized void enableMemoryMapping() {
    if(memoryMapped)
     return;
    if(filePath==null)
     return;

    try {
     mappedRAF=new RandomAccessFile(filePath, "rw");
     mappedChannel=mappedRAF.getChannel();

     // Pad to a page boundary so every MappedPage has a full
     // 2048-byte backing region.
     int mappedLength=pageCount*2048;
     if(mappedLength>(int)mappedRAF.length())
      mappedRAF.setLength(mappedLength);

     mappedBuffer=mappedChannel.map(
      FileChannel.MapMode.READ_WRITE, 0, mappedLength);

     // Copy current array contents into the mapped buffer, then
     // replace each ArrayPage with a MappedPage in the PageSet.
     for(int index=0;index<pageCount;index++) {
      int base=index*2048;
      if(pages!=null && pages[index]!=null) {
       for(int offset=0;offset<2048;offset++)
        mappedBuffer.put(base+offset, pages[index][offset]);
      }
      pageSet.mapPage(new MappedPage(mappedBuffer, base), index);
     }

     // Release the byte arrays -- no longer needed.
     pages=null;
     memoryMapped=true;

     System.err.println("Memory-mapped: " + filePath);
    }
    catch(IOException exception) {
     // If mmap fails, keep running with array-backed pages.
     System.err.println("Memory mapping failed for " + filePath
      + ", continuing with array mode: " + exception.getMessage());
    }
   }

   public boolean isMemoryMapped() {
    return memoryMapped;
   }

   public int length() {
    return length;
   }

   public byte[] fullContents() {
    byte[] bytes=new byte[length];

    if(memoryMapped) {
     synchronized(mappedBuffer) {
      for(int i=0;i<length;i++)
       bytes[i]=mappedBuffer.get(i);
     }
    }
    else {
     for(int page=0;page<pageCount;page++) {
      for(int offset=0;offset<2048;offset++)
       if(page*2048+offset<length)
        bytes[page*2048+offset]=pages[page][offset];
     }
    }
    return bytes;
   }

   public byte[] loadPage(int pageNumber) {
    if(memoryMapped) {
     byte[] page=new byte[2048];
     int    base=pageNumber*2048;
     synchronized(mappedBuffer) {
      for(int i=0;i<2048 && base+i<mappedBuffer.capacity();i++)
       page[i]=mappedBuffer.get(base+i);
     }
     return page;
    }
    return pages[pageNumber];
   }

   public byte[][] allPages() {
    if(memoryMapped) {
     byte[][] snapshot=new byte[pageCount][];
     for(int p=0;p<pageCount;p++)
      snapshot[p]=loadPage(p);
     return snapshot;
    }
    return pages;
   }

   public int pageCount() {
    return pageCount;
   }

   public PageSet pageSet() {
    return pageSet;
   }

   // ---------------------------------------------------------------
   // storePages: for mapped files, just force the buffer.
   // For array-based files, write out as before.
   // ---------------------------------------------------------------
   public int storePages(String filePath) {
    if(memoryMapped) {
     if(modified) {
      try {
       mappedBuffer.force();
       modified=false;
      }
      catch(Exception e) {
       System.err.println("Error forcing mapped buffer for "
        + this.filePath + ": " + e.getMessage());
       return OSError.FS_ERROR_WRITING_DATA;
      }
     }
     return OS.SUCCESS;
    }

    // Original array-based write path
    byte[] content=fullContents();
    if(filePath==null)
     filePath=this.filePath;
    try {
     if(modified && COMMIT)
      Utility.writeFile(filePath, content);
     modified=false;
    }
    catch(IOException exception) {
     return OSError.FS_ERROR_WRITING_DATA;
    }
    return OS.SUCCESS;
   }

   // ---------------------------------------------------------------
   // force: explicitly flush mapped buffer to disk.
   // Safe to call on non-mapped files (no-op).
   // ---------------------------------------------------------------
   public void force() {
    if(memoryMapped && mappedBuffer!=null) {
     mappedBuffer.force();
    }
   }

   // ---------------------------------------------------------------
   // closeMapped: release the mapped resources.
   // Called during orderly shutdown after force().
   // ---------------------------------------------------------------
   public void closeMapped() {
    if(memoryMapped) {
     try {
      if(mappedChannel!=null) {
       mappedChannel.close();
       mappedChannel=null;
      }
      if(mappedRAF!=null) {
       mappedRAF.close();
       mappedRAF=null;
      }
     }
     catch(IOException e) {
      System.err.println("Error closing mapped file "
       + filePath + ": " + e.getMessage());
     }
    }
   }

   public void extend(int newPageCount) {
    if(memoryMapped)
     throw new RuntimeException(
      "Cannot extend a memory-mapped file at runtime: " + filePath);

    byte[][] replacement;
    int      index;

    if(newPageCount<pages.length+16)
     newPageCount=pages.length+16;

    replacement=new byte[newPageCount][];
    for(index=0;index<pages.length;index++)
     replacement[index]=pages[index];
    for(index=pages.length;index<replacement.length;index++)
     replacement[index]=new byte[2048];
    pages=replacement;
   }

   public void setLength(int newLength) {
    if(memoryMapped)
     throw new RuntimeException(
      "Cannot resize a memory-mapped file at runtime: " + filePath);

    int newPageCount=(newLength+2047)/2048;

    if(newPageCount>pages.length)
     extend(newPageCount);
    pageCount=newPageCount;
    length=newLength;
   }
}
