package os;

import java.util.*;

public class OSError extends RuntimeException {
   static public final int SUCCESS=0;
   static public final int FS_INVALID_PATH=0x1001;
   static public final int FS_NOT_INITIALIZED=0x1002;
   static public final int FS_PARENT_DIRECTORY_DOES_NOT_EXIST=0x1003;
   static public final int FS_ALREADY_EXISTS=0x1004;
   static public final int FS_DOES_NOT_EXIST=0x1005;
   static public final int FS_DIRECTORY_IS_NOT_EMPTY=0x1006;
   static public final int FS_PROTECTED_DIRECTORY=0x1007;
   static public final int FS_ERROR_READING_DATA=0x1008;
   static public final int FS_ERROR_WRITING_DATA=0x1009;
   static public final int FS_FILE_NOT_FOUND=0x100A;
   static public final int FS_WRONG_FILE_TYPE=0x100B;
   static public final int FS_PAGING_NOT_ALLOWED_ON_FILE=0x100C;
   static public final int FS_STREAMING_NOT_ALLOWED_ON_FILE=0x100D;
   static public final int FS_SET_POSITION_NOT_ALLOWED_ON_FILE=0x100E;
   static public final int FS_INVALID_PAGE_NUMBER=0x100F;
   static public final int FS_INVALID_OPEN_ON_DIRECTORY=0x1010;

   static public final int OS_NOT_IMPLEMENTED=0x2000;
   static public final int OS_UNHANDLED_SYSTEM_CALL=0x2001;
   static public final int OS_INVALID_CALL_ARGUMENT=0x2002;
   static public final int OS_MAXIMUM_NUMBER_OF_FILES_OPEN=0x2003;
   static public final int OS_INVALID_CHANNEL_NUMBER=0x2004;
   static public final int OS_SERVICE_ALREADY_EXISTS=0x2005;
   static public final int OS_SERVICE_DOES_NOT_EXIST=0x2006;

   static public Map<Integer,String> errors=errors();

   public int error;

   static public Map<Integer,String> errors() {
    Map<Integer,String> map=new HashMap<Integer,String>();

    map.put(FS_INVALID_PATH, "FS - invalid path");
    map.put(FS_NOT_INITIALIZED, "FS - not initialized");
    map.put(FS_PARENT_DIRECTORY_DOES_NOT_EXIST, "FS - parent directory does not exist");
    map.put(FS_ALREADY_EXISTS, "FS - file/directory already exists");
    map.put(FS_DIRECTORY_IS_NOT_EMPTY, "FS - directory is not empty");
    map.put(FS_PROTECTED_DIRECTORY, "FS - protected directory");
    map.put(FS_ERROR_READING_DATA, "FS - error reading data");
    map.put(FS_ERROR_WRITING_DATA, "FS - error writing data");
    map.put(FS_FILE_NOT_FOUND, "FS - file not found");
    map.put(FS_WRONG_FILE_TYPE, "FS - wrong file type");
    map.put(FS_PAGING_NOT_ALLOWED_ON_FILE, "FS - paging is not allowed on this file");
    map.put(FS_STREAMING_NOT_ALLOWED_ON_FILE, "FS - streaming is not allowed on this file");
    map.put(FS_SET_POSITION_NOT_ALLOWED_ON_FILE, "FS - set position is not allowed on this file");
    map.put(FS_INVALID_PAGE_NUMBER, "FS - invalid page number");
    map.put(FS_INVALID_OPEN_ON_DIRECTORY, "FS - invalid open on a directory");

    map.put(OS_NOT_IMPLEMENTED, "OS - not implemented yet");
    map.put(OS_UNHANDLED_SYSTEM_CALL, "OS - unhandled system call");
    map.put(OS_INVALID_CALL_ARGUMENT, "OS - invalid call argument");
    map.put(OS_MAXIMUM_NUMBER_OF_FILES_OPEN, "OS - maximum number of files open");
    map.put(OS_INVALID_CHANNEL_NUMBER, "OS - invalid channel number");
    map.put(OS_SERVICE_ALREADY_EXISTS, "OS - service already exists");
    map.put(OS_SERVICE_DOES_NOT_EXIST, "OS - service does not exist");

    return map;
   }

   static public String messageForError(int error) {
    String message=errors.get(error);

    if(message==null)
     throw new RuntimeException("Message not found for error number " + String.format("0x%X", error));
    return message;
   }

   public OSError(int error) {
    super(messageForError(error));
    this.error=error;
   }

   public int error() {
    return error;
   }
}
