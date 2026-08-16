package hw;

// The page class is designed to be subclassed.  The idea is that you could have two subclasses, one
// which uses a java byte[] array to store the page data.  This would be used to store code pages and
// private data pages like the stack.   The second subclass could use MappedByteBuffer, which would
// automatically store the data on disk when the page was swapped out.  This could be used to handle
// the DG shared files, although the current implementation doesn't do that right now.

public abstract class Page implements ReadWrite {
   public Page() {
   }

   abstract public int read(int offset);
   abstract public void write(int offset, int value);

   public int readByte(int offset) {
    return read(offset);
   }

   public int readWord(int offset) {
    return read(offset*2+1) + (read(offset*2)<<8);
   }

   public int readWide(int offset) {
    return read(offset*2+3) + (read(offset*2+2)<<8) + (read(offset*2+1)<<16) + (read(offset*2)<<24);
   }

   public void writeByte(int offset, int value) {
    write(offset, value);
   }

   public void writeWord(int offset, int value) {
    write(offset*2, value>>8);
    write(offset*2+1, value);
   }

   public void writeWide(int offset, int value) {
    write(offset*2, value>>24);
    write(offset*2+1, value>>16);
    write(offset*2+2, value>>8);
    write(offset*2+3, value);
   }
}
