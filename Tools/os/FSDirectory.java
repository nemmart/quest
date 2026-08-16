package os;

import java.util.*;

public class FSDirectory implements FSObject {
   public String               path;
   public Set<String>          list;
   public Map<String,FSObject> map;

   public FSDirectory() {
    path=null;
    list=new TreeSet<String>();
    map=new HashMap<String,FSObject>();
   }

   public String getPath() {
    return path;
   }

   public void setPath(String path) {
    if(this.path!=null)
     throw new RuntimeException("Attempt to change an FSObject path");
    this.path=path;
   }

   public List<String> contents() {
    List<String>     contents=new ArrayList<String>();
    Iterator<String> iterator=list.iterator();

    while(iterator.hasNext()) {
     String file=iterator.next();

     contents.add(path + ":" + iterator.next());
    }
    return contents;
   }

   public void insert(String name, FSObject object) {
    map.put(name, object);
   }

   public void delete(String name) {
    map.remove(name);
   }

   public int fileCount() {
    return map.size();
   }
}
