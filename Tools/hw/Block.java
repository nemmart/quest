package hw;

public class Block {
   public int    base;
   public Page[] pages;

   public Block() {
    pages=new Page[512];
   }
}
