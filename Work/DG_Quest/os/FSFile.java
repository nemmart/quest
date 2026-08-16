package os;

import java.io.*;

import hw.*;

public class FSFile implements FSObject {
   static public boolean COMMIT=true;

   public String   path;
   public String   filePath;
   public int      length;
   public int      pageCount;
   public boolean  modified;
   public byte[][] pages;

   public FSFile() {
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

   public void modifiedNotification() {
    modified=true;
   }

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
    modified=false;
    length=content.length;
    pageCount=(content.length+2047)/2048;
    pages=new byte[pageCount][];
    for(index=0;index<content.length;index=index+2048) {
     byte[] page=new byte[2048];

     for(offset=0;offset<2048 && index+offset<content.length;offset++)
      page[offset]=content[index+offset];
     pages[index>>>11]=page;
    }
    return OS.SUCCESS;
   }

   public int length() {
    return length;
   }

   public byte[] fullContents() {
    byte[] bytes;

    bytes=new byte[length];
    for(int page=0;page<pageCount;page++) {
     for(int offset=0;offset<2048;offset++)
      if(page*2048+offset<length)
       bytes[page*2048+offset]=pages[page][offset];
    }
    return bytes;
   }

   public byte[] loadPage(int pageNumber) {
    return pages[pageNumber];
   }

   public int pageCount() {
    return pageCount;
   }

   public int storePages(String filePath) {
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

   public void extend(int newPageCount) {
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
    int newPageCount=(newLength+2047)/2048;

    if(newPageCount>pages.length)
     extend(newPageCount);
    pageCount=newPageCount;
    length=newLength;
   }
}