package os;

import hw.*;

public class ArrayPage extends Page {
   public byte[] bytes;
   public FSFile file;

   public ArrayPage() {
    this.bytes=new byte[2048];
    this.file=null;
   }

   public ArrayPage(byte[] bytes) {
    this.bytes=bytes;
    this.file=null;
   }

   public ArrayPage(byte[] bytes, FSFile file) {
    this.bytes=bytes;
    this.file=file;
   }

   public void setFile(FSFile file) {
    this.file=file;
   }

   public int read(int offset) {
    return (int)(bytes[offset] & 0xFF);
   }

   public void write(int offset, int value) {
    bytes[offset]=(byte)(value & 0xFF);
   }

   public void writeByte(int offset, int value) {
    super.writeByte(offset, value);
    if(file!=null)
     file.modifiedNotification();
   }

   public void writeWord(int offset, int value) {
    super.writeWord(offset, value);
    if(file!=null)
     file.modifiedNotification();
   }

   public void writeWide(int offset, int value) {
    super.writeWide(offset, value);
    if(file!=null)
     file.modifiedNotification();
   }
}
