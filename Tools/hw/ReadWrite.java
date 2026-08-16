package hw;

public interface ReadWrite {
   public int readByte(int address);
   public int readWord(int address);
   public int readWide(int address);
   public void writeByte(int address, int value);
   public void writeWord(int address, int value);
   public void writeWide(int address, int value);
}
