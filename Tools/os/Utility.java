package os;

import java.io.*;

import hw.*;

public class Utility {
/*
   static public Page[] pagesForBytes(byte[] bytes) {
    Page[] pages;
    int    index, offset, count=(bytes.length+2047)/2048;

    pages=new Page[count];
    for(index=0;index<bytes.length;index=index+2048) {
     Page page=new Page();

     for(offset=0;offset<2048 && index+offset<bytes.length;offset++)
      page.bytes[offset]=bytes[index+offset];
     pages[index>>>11]=page;
    }
    return pages;
   }

   static public Page[] readFileAsPages(String fileName) throws IOException {
    return pagesForBytes(readFile(fileName));
   }
*/

   static public byte[] readFile(String fileName) throws IOException {
    RandomAccessFile file=new RandomAccessFile(fileName, "r");
    byte[]           bytes;
    int              length;

    length=(int)file.length();
    bytes=new byte[length];
    file.readFully(bytes);
    file.close();
    return bytes;
   }

   static public void writeFile(String fileName, byte[] content) throws IOException {
    RandomAccessFile file=new RandomAccessFile(fileName, "rw");

    file.setLength(0);
    file.write(content);
    file.close();
   }
}
