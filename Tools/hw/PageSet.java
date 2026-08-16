package hw;

public class PageSet implements ReadWrite {
   public Block[] blocks;

   public PageSet() {
    blocks=new Block[4096];
   }

   public void mapPage(Page page, int pageNumber) {
    int block=(pageNumber>>9)&0xFFF;

    if(blocks[block]==null)
     blocks[block]=new Block();

    blocks[block].pages[pageNumber & 0x1FF]=page;
   }

   public void unmapPage(int pageNumber) {
    int block=(pageNumber>>9)&0xFFF;

    blocks[block].pages[pageNumber & 0x1FF]=null;
   }

   public Page findPage(int pageNumber) {
    int block=(pageNumber>>9)&0xFFF;

    return blocks[block].pages[pageNumber & 0x1FF];
   }

/*
   // this looks buggy

   public boolean valid(int address) {
    int block=(address>>>20) & 0xFFF;
    int page=(address>>11) & 0x1FF;

    if(blocks[block]==null)
     return false;
    if(blocks[block].pages[page]==null)
     return false;
    return true;
   }
*/

   public int readByte(int address) {
    int block=(address>>>20)&0xFFF;
    int page=(address>>11)&0x1FF;
    int offset=address&0x7FF;

    if(blocks[block]==null)
     throw new RuntimeException("Segment fault - block " + block + " not loaded");
    if(blocks[block].pages[page]==null)
     throw new RuntimeException("Segment fault - block " + block + ", page " + page + " not loaded");

    return blocks[block].pages[page].readByte(offset);
   }

   public int readWord(int address) {
    int block=(address>>>19)&0xFFF;
    int page=(address>>10)&0x1FF;
    int offset=address&0x3FF;

    if(blocks[block]==null)
     throw new RuntimeException("Segment fault - block " + block + " not loaded");
    if(blocks[block].pages[page]==null)
     throw new RuntimeException("Segment fault - block " + block + ", page " + page + " not loaded");

    return blocks[block].pages[page].readWord(offset);
   }

   public int readWide(int address) {
    int block=(address>>>19)&0xFFF;
    int page=(address>>10)&0x1FF;
    int offset=address&0x3FF;
    int valud;

    if(offset==1023) {
     int high, low;

     // read might cross a page boundary
     high=readWord(address);
     low=readWord(address+1);
     return (high<<16) | (low & 0xFFFF);
    }
    if(blocks[block]==null)
     throw new RuntimeException("Segment fault - block " + block + " not loaded");
    if(blocks[block].pages[page]==null)
     throw new RuntimeException("Segment fault - block " + block + ", page " + page + " not loaded");

    return blocks[block].pages[page].readWide(offset);
   }

   public long readQuad(int address) {
    long high=readWide(address), low=readWide(address+2);

    return (high<<32)+(low & 0xFFFFFFFFl);
   }

   public void writeByte(int address, int value) {
    int block=(address>>>20)&0xFFF;
    int page=(address>>11)&0x1FF;
    int offset=address&0x7FF;

    if(blocks[block]==null)
     throw new RuntimeException("Segment fault - block " + block + " not loaded");
    if(blocks[block].pages[page]==null)
     throw new RuntimeException("Segment fault - block " + block + ", page " + page + " not loaded");

    blocks[block].pages[page].writeByte(offset, value);
   }

   public void writeWord(int address, int value) {
    int block=(address>>>19)&0xFFF;
    int page=(address>>10)&0x1FF;
    int offset=address&0x3FF;

    if(blocks[block]==null)
     throw new RuntimeException("Segment fault - block " + block + " not loaded");
    if(blocks[block].pages[page]==null)
     throw new RuntimeException("Segment fault - block " + block + ", page " + page + " not loaded");

    blocks[block].pages[page].writeWord(offset, value);
   }

   public void writeWide(int address, int value) {
    int block=(address>>>19)&0xFFF;
    int page=(address>>10)&0x1FF;
    int offset=address&0x3FF;

    if(offset==1023) {
     // write crosses a page boundary
     writeWord(address, value>>>16);
     writeWord(address+1, value & 0xFFFF);
     return;
    }
    if(blocks[block]==null)
     throw new RuntimeException("Segment fault - block " + block + " not loaded");
    if(blocks[block].pages[page]==null)
     throw new RuntimeException("Segment fault - block " + block + ", page " + page + " not loaded");

    blocks[block].pages[page].writeWide(offset, value);
   }

   public void writeQuad(int address, long value) {
    writeWide(address, (int)(value>>>32));
    writeWide(address+2, (int)(value & 0xFFFFFFFFl));
   }
}