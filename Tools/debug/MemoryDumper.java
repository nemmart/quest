package debug;

import hw.*;

public class MemoryDumper {
   static public int count(ReadWrite memory, int start, int stop) {
    int index;

    for(index=start;index<stop;index++) {
     if(memory.readWord(index)!=0)
      break;
    }
    return index-start;
   }

   static public void dump(ReadWrite memory, int start, int stop) {
    int              index, current, count;
    char[]           line=new char[59];
    char[]           toHex=new char[]{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    index=start;
    while(index<stop) {
     count=count(memory, index, stop);
     if(count>48) {
      count=count-count%8;
      System.out.printf("%06X    0000 0000 0000 0000 0000 0000 0000 0000  [                ]\n", index);
      System.out.printf("%06X    0000 0000 0000 0000 0000 0000 0000 0000  [                ]\n", index+8);
      System.out.println("*");
      System.out.println("*");
      System.out.printf("%06X    0000 0000 0000 0000 0000 0000 0000 0000  [                ]\n", index+count-8);
      index=index+count;
     }
     else {
      System.out.printf("%06X    ", index);
      for(current=0;current<58;current++)
       line[current]=' ';
      line[41]='[';
      line[58]=']';

      for(current=0;current<8 && index+current<stop;current++) {
       int word=memory.readWord(current+index);
       int byte1=(word>>8) & 0xFF, byte2=word & 0xFF;

       line[current*5+0]=toHex[(word>>12) & 0x0F];
       line[current*5+1]=toHex[(word>>8) & 0x0F];
       line[current*5+2]=toHex[(word>>4) & 0x0F];
       line[current*5+3]=toHex[word & 0x0F];

       if(byte1>32 && byte1<127)
        line[current*2+42]=(char)byte1;
       if(byte2>32 && byte2<127)
        line[current*2+43]=(char)byte2;
      }
      System.out.println(line);
      index=index+8;
     }
    }
   }
}
