package os;

public interface FSStreamIO {
   public int available();
   public int read(byte[] bytes);
   public void write(byte[] bytes);
   public void close();
}
