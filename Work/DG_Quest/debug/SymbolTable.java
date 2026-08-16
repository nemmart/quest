package debug;

import java.util.*;
import java.io.*;

public class SymbolTable {
   public Map<String,Integer> nameToAddress;
   public Map<Integer,String> addressToName;

   static public int four(byte[] bytes, int offset) {
    return ((bytes[offset] & 0xFF)<<24) + ((bytes[offset+1] & 0xFF)<<16) + ((bytes[offset+2] & 0xFF)<<8) + (bytes[offset+3] & 0xFF);
   }

   static public String name(byte[] bytes, int offset, int length) {
    char[] chars=new char[length];
    int    index;

    for(index=0;index<length;index++)
     chars[index]=(char)bytes[offset+index];
    return new String(chars);
   }

   static public SymbolTable parseSTFormat(byte[] bytes) {
    SymbolTable symbolTable=new SymbolTable();
    int         index=0, length=bytes.length;
    int         tag, size, address;
    String      symbol;

    while(index<length) {
     if(bytes[index]==0)
      index+=2;
     else {
      tag=bytes[index] & 0xFF;
      size=bytes[index+1] & 0xFF;
      address=four(bytes, index+2);
      symbol=name(bytes, index+20, size);
      symbolTable.addSymbol(symbol, address);
      index=index+20+size+(size%2);
     }
    }
    return symbolTable;
   }

   static public SymbolTable parseSYMFormat(byte[] bytes) {
    SymbolTable symbolTable=new SymbolTable();
    int         index=0, length=bytes.length;
    int         start, address;
    String      symbol;

    while(index<length) {
     if(bytes[index]<=' ')
      index++;
     else {
      start=index;
      while(index<length && bytes[index]>' ' && bytes[index]<127)
       index++;
      if(bytes[index]!=' ')
       throw new RuntimeException("SYM file format error");
      symbol=name(bytes, start, index-start);
      while(index<length && bytes[index]==' ')
       index++;
      address=0;
      while(index<length) {
       if(bytes[index]>='0' && bytes[index]<='9')
        address=address*16+bytes[index]-'0';
       else if(bytes[index]>='a' && bytes[index]<='f')
        address=address*16+bytes[index]-'a'+10;
       else if(bytes[index]>='A' && bytes[index]<='F')
        address=address*16+bytes[index]-'A'+10;
       else
        break;
       index++;
      }
      if(bytes[index]>=' ')
       throw new RuntimeException("SYM file format error");
      symbolTable.addSymbol(symbol, address);
     }
    }
    return symbolTable;
   }

   public SymbolTable() {
    nameToAddress=new HashMap<String,Integer>();
    addressToName=new HashMap<Integer,String>();
   }

   public String nameForAddress(int address) {
    return addressToName.get(address);
   }

   public int addressForName(String name) {
    return nameToAddress.get(name);
   }

   public int firstAddress(int address) {
    Iterator<Integer> iterator=addressToName.keySet().iterator();
    int               max=-1, check;

    while(iterator.hasNext()) {
     check=iterator.next();
     if(check<=address) {
      if(max==-1 || check>max)
       max=check;
     }
    }
    return max;
   }

   public int lastAddress(int address) {
    Iterator<Integer> iterator=addressToName.keySet().iterator();
    int               min=-1, check;

    while(iterator.hasNext()) {
     check=iterator.next();
     if(check>address) {
      if(min==-1 || check<min)
       min=check;
     }
    }
    return min;
   }

   public void addSymbol(String name, int address) {
    String current;

    nameToAddress.put(name, address);
    current=addressToName.get(address);
    if(current==null)
     addressToName.put(address, name);
    else {
     if(!current.startsWith("("))
      current="(" + current + "/" + name + ")";
     else
      current=current.substring(0, current.length()-1) + "/" + name + ")";
     addressToName.put(address, current);
    }
   }

/*
   static public void main(String[] args) throws Exception {
	RandomAccessFile file=new RandomAccessFile("Quest.ST", "r");
    byte[]           bytes;
    int              length;
    SymbolTable      st;

    length=(int)file.length();
    bytes=new byte[length];
    file.readFully(bytes);
    st=parseSTFormat(bytes);
    System.out.println(st.nameForAddress(0x7017FDB9));
   }
*/
}
