package os;

public class FSStreamedFile implements FSStreamIO {
   public FSFile  file;
   public int     position;
   public boolean updated;

   public FSStreamedFile(FSFile file) {
    this.file=file;
    position=0;
    updated=false;
   }

   public int available() {
    return file.length()-position;
   }

   public void setPosition(int position) {
    if(position>file.length())
     position=file.length();
    this.position=position;
   }

   public int read(byte[] bytes) {
    byte[] content=null;
    int    amount=bytes.length, index, page=-1;

    if(amount>available())
     amount=available();
    for(index=0;index<amount;index++) {
     if(position>>11!=page) {
      page=position>>11;
      content=file.loadPage(page);
     }
     bytes[index]=content[position & 0x7FF];
     position++;
    }
    return amount;
   }

   public void write(byte[] bytes) {
    byte[] content=null;
    int    index, page=-1;

    file.modified=true;
    if(position+bytes.length>file.length())
     file.setLength(position+bytes.length);

    for(index=0;index<bytes.length;index++) {
     if(position>>11!=page) {
      page=position>>11;
      content=file.loadPage(page);
     }
     content[position & 0x7FF]=bytes[index];
     position++;
    }
    updated=true;
   }

   public void close() {
    int error;

    if(updated) {
     error=file.storePages(null);
     if(error!=OS.SUCCESS)
      throw new OSError(error);
    }
   }
}
