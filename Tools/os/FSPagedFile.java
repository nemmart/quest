package os;

import java.io.*;

import hw.*;

public class FSPagedFile implements FSPagedIO {
   public FSFile file;

   public FSPagedFile(FSFile file) {
    this.file=file;
   }

   public byte[] loadPage(int pageNumber) {
    return file.loadPage(pageNumber);
   }

   public byte[][] allPages() {
    return file.allPages();
   }

   public PageSet pageSet() {
    return file.pageSet();
   }

   public int pageCount() {
    return file.pageCount();
   }

   public void close() {
System.err.println("WRITING: " + file.getPath());
    int error=file.storePages(null);

    if(error!=OS.SUCCESS)
     throw new OSError(error);
   }

   public String getPath() {
    return file.getPath();
   }
}
