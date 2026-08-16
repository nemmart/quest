package os;

public class FSChannel {
   static public final int PAGED_IO=0x00;
   static public final int STREAM_IO=0x01;

   static public final int READ_PERMISSION=0x10;
   static public final int WRITE_PERMISSION=0x20;
   static public final int READ_WRITE_PERMISSION=0x30;

   public int    mode;
   public Object file;

   static public FSChannel openForPagedIO(String fsPath, boolean readOnly) {
    FSObject  object=FS.retrieve(fsPath);
    int       mode;

    if(object==null)
     throw new OSError(OSError.FS_FILE_NOT_FOUND);
    if(!(object instanceof FSFile))
     throw new OSError(OSError.FS_WRONG_FILE_TYPE);

    if(readOnly)
     mode=PAGED_IO | READ_PERMISSION;
    else
     mode=PAGED_IO | READ_PERMISSION | WRITE_PERMISSION;

    return new FSChannel(mode, new FSPagedFile((FSFile)object));
   }

   static public FSChannel openForStreamedIO(String fsPath, boolean readOnly) {
    FSObject object=FS.retrieve(fsPath);
    int      mode;

    if(object==null)
     throw new OSError(OSError.FS_FILE_NOT_FOUND);
    if(!(object instanceof FSFile))
     throw new OSError(OSError.FS_WRONG_FILE_TYPE);

    if(readOnly)
     mode=STREAM_IO | READ_PERMISSION;
    else
     mode=STREAM_IO | READ_PERMISSION | WRITE_PERMISSION;

    return new FSChannel(mode, new FSStreamedFile((FSFile)object));
   }

   static public FSChannel openForStreamedIO(FSStreamIO stream, int permission) {
    return new FSChannel(permission | STREAM_IO, stream);
   }

   public FSChannel(int mode, Object file) {
    this.mode=mode;
    this.file=file;
   }

   public boolean pagedMode() {
    return (mode&0x01)==PAGED_IO;
   }

   public boolean streamMode() {
    return (mode&0x01)==STREAM_IO;
   }

   public boolean readOnly() {
    return (mode&WRITE_PERMISSION)==0;
   }

   public void close() {
    if(file instanceof FSStreamedFile)
     ((FSStreamedFile)file).close();
//    if(file instanceof FSPagedFile)
//     ((FSPagedFile)file).close();
   }

   public byte[] readPage(int pageNumber) {
    FSPagedFile pagedFile;

    if((mode&0x01)!=PAGED_IO)
     throw new OSError(OSError.FS_PAGING_NOT_ALLOWED_ON_FILE);
    pagedFile=(FSPagedFile)file;
    if(pageNumber<0 || pageNumber>=pagedFile.pageCount())
     throw new OSError(OSError.FS_INVALID_PAGE_NUMBER);
    return pagedFile.loadPage(pageNumber);
   }

   public int pageCount() {
    FSPagedFile pagedFile;

    if((mode&0x01)!=PAGED_IO)
     throw new OSError(OSError.FS_PAGING_NOT_ALLOWED_ON_FILE);
    pagedFile=(FSPagedFile)file;
    return pagedFile.pageCount();
   }

   public void setPosition(int offset) {
    if(file instanceof FSStreamedFile)
     ((FSStreamedFile)file).setPosition(offset);
    else
     throw new OSError(OSError.FS_SET_POSITION_NOT_ALLOWED_ON_FILE);
   }

   public int read(byte[] bytes, boolean lineMode) {
    if((mode&0x01)!=STREAM_IO)
     throw new OSError(OSError.FS_STREAMING_NOT_ALLOWED_ON_FILE);
    if(lineMode && (file instanceof FSTerminal))
     return ((FSTerminal)file).read(bytes, lineMode);
    else
     return ((FSStreamIO)file).read(bytes);
   }

   public void write(byte[] bytes) {
    FSStreamIO streamFile;

    if((mode&0x01)!=STREAM_IO)
     throw new OSError(OSError.FS_STREAMING_NOT_ALLOWED_ON_FILE);
    streamFile=(FSStreamIO)file;
    streamFile.write(bytes);
   }

   public FSFile getFile() {
    if(file instanceof FSPagedFile)
     return ((FSPagedFile)file).file;
    if(file instanceof FSStreamedFile)
     return ((FSStreamedFile)file).file;
    return null;
   }
}
