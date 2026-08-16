package os;

import java.util.*;
import java.io.*;

public class FS implements Runnable {
   static public final int SUCCESS=0;

   static public FSDirectory          root=new FSDirectory();
   static public Map<String,FSObject> files=new HashMap<String,FSObject>();

   static public String validatePath(String fsPath) {
    char current;

    if(!fsPath.startsWith(":"))
     return null;
    for(int index=0;index<fsPath.length();index++) {
     current=fsPath.charAt(index);
     if((current<'A' || current>'Z') && (current<'a' || current>'z') &&
        (current<'0' || current>'9') && (current!='.') && (current!=':') && (current!='_')) {
      return null;
     }
    }
    for(int index=1;index<fsPath.length();index++)
     if(fsPath.charAt(index-1)==':' && fsPath.charAt(index)==':')
      return null;
    if(fsPath.length()>1 && fsPath.endsWith(":"))
     return null;
    return fsPath.toUpperCase();
   }

   static public String parentPath(String fsPath) {
    int index=fsPath.lastIndexOf(':');

    if(index==0)
     return ":";
    return fsPath.substring(0, index);
   }

   static public String fileName(String fsPath) {
    int index=fsPath.lastIndexOf(':');

    if(index<=0)
     return null;
    return fsPath.substring(index+1);
   }

   static public void initializeWithPath(String loadPath) {
    File file=new File(loadPath);

    if(!file.exists())
     throw new RuntimeException("FS initialization failed - the load path '" + loadPath + "' does not exist");
    if(validatePath(":" + file.getName())==null)
     throw new RuntimeException("FS initialization failed - invalid file name");
    synchronized(root) {
     if(files.size()!=0)
      throw new RuntimeException("FS has already been initialized");

     files.put(":", root);
     FS.mkdir(":PER");
     FS.mkdir(":SYSTEM");

     if(file.isDirectory())
      FS.load(loadPath, "");
     else
      FS.load(loadPath, ":" + file.getName());

     for(String name : files.keySet())
      System.err.println(name);
    }
    Runtime.getRuntime().addShutdownHook(new Thread(new FS()));
   }

   static public void load(String loadPath, String fsPath) {
    File   file=new File(loadPath);
    String validated;
    int    error=SUCCESS;

    synchronized(root) {
     if(file.isDirectory()) {
      String[] list=file.list();

      if(fsPath.length()>0)
       error=mkdir(fsPath);
      if(error!=SUCCESS)
       System.out.println("FS load - failed to create FS directory '" + fsPath + "', error code " + error);
      else {
       for(int index=0;index<list.length;index++) {
        if(list[index].equals(".") || list[index].equals(".."))
         continue;
        validated=validatePath(fsPath + ":" + list[index]);
        if(validated==null)
         System.out.println("FS load - skipping '" + list[index] + "', invalid file name");
        else
         load(loadPath + "/" + list[index], validated);
       }
      }
     }
     else {
      FSFile fsFile=new FSFile();

      error=fsFile.loadPages(loadPath);
      if(error==SUCCESS)
       error=insert(fsPath, fsFile);
      if(error!=SUCCESS)
       System.out.println("FS load - failed to load/insert FS file '" + fsPath + "', error code " + error);
     }
    }
   }

   static public FSObject retrieve(String fsPath) {
    synchronized(root) {
     return files.get(fsPath);
    }
   }

   static public int insert(String fsPath, FSObject object) {
    FSObject parent;

    fsPath=validatePath(fsPath);
    if(fsPath==null)
     return OSError.FS_INVALID_PATH;

    synchronized(root) {
     if(files.size()==0)
      return OSError.FS_NOT_INITIALIZED;
     if(files.get(fsPath)!=null)
      return OSError.FS_ALREADY_EXISTS;
     parent=retrieve(parentPath(fsPath));
     if(parent==null || !(parent instanceof FSDirectory))
      return OSError.FS_PARENT_DIRECTORY_DOES_NOT_EXIST;
     object.setPath(fsPath);
     files.put(fsPath, object);
     ((FSDirectory)parent).insert(fileName(fsPath), object);
     return SUCCESS;
    }
   }

   static public int mkdir(String fsPath) {
    FSObject directory;

    fsPath=validatePath(fsPath);
    if(fsPath==null)
     return OSError.FS_INVALID_PATH;

    synchronized(root) {
     directory=retrieve(fsPath);
     if(directory==null)
      return insert(fsPath, new FSDirectory());
     if(directory instanceof FSDirectory)
      return SUCCESS;
     else
      return OSError.FS_ALREADY_EXISTS;
    }
   }

   static public int delete(String fsPath) {
    FSObject found;

    fsPath=validatePath(fsPath);
    if(fsPath==null)
     return OSError.FS_INVALID_PATH;

    if(fsPath.equals(":") || fsPath.equals(":PER") || fsPath.equals(":SYSTEM"))
     return OSError.FS_PROTECTED_DIRECTORY;

    synchronized(root) {
     found=retrieve(fsPath);
     if(found==null)
      return OSError.FS_DOES_NOT_EXIST;
     if((found instanceof FSDirectory) && ((FSDirectory)found).fileCount()!=0)
      return OSError.FS_DIRECTORY_IS_NOT_EMPTY;
     files.remove(fsPath);
     found=retrieve(parentPath(fsPath));
     ((FSDirectory)found).delete(fileName(fsPath));
     return SUCCESS;
    }
   }

   // MODIFIED: handles both memory-mapped and array-based files.
   // For mapped files: force the buffer to disk, then close the channel.
   // For array-based files: write out as before (fallback path).
   static public void shutdown() {
    Iterator<String> iterator=files.keySet().iterator();
    String           path;
    FSObject         object;

    while(iterator.hasNext()) {
     path=iterator.next();
     object=files.get(path);
     if(object instanceof FSFile) {
      FSFile file=(FSFile)object;

      if(file.isMemoryMapped()) {
       System.err.println("Flushing mapped file: " + path);
       file.force();
       file.closeMapped();
      }
      else if(file.modified) {
       System.err.println("Writing: " + path);
       file.storePages(null);
      }
     }
    }
   }

   public void run() {
    FS.shutdown();
   }

   static public void main(String[] args) {
    initializeWithPath(args[0]);
   }

}
