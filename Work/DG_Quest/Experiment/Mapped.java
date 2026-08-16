import java.io.*;
import java.nio.*;
import java.nio.channels.*;

public class Mapped {
  public static void main(String[] args) {

   try {
    File        file = new File("WORLD_DATA_FILE");
    FileChannel fc = new RandomAccessFile(file, "rw").getChannel();
    ByteBuffer  bb = fc.map(FileChannel.MapMode.READ_WRITE, 0, fc.size());

    System.out.println("byte0: " + bb.get(0));
    System.out.println("byte1: " + bb.get(1));
   }
   catch (IOException e) {
    System.out.println("Error: " + e.toString());
   }
  }
}
