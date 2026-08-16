package os;

import java.io.*;
import hw.*;

public class OSContext {
   static public final int HIGH=0;
   static public final int LOW=1;
   static public final int SUCCESS=0;

   static public final int CREATE=000;
   static public final int ISEND=025;
   static public final int IREC=026;
   static public final int ISR=0142;
   static public final int ILKUP=027;
   static public final int CON=0167;
   static public final int DCON=0170;

   static public final int TASK=0500;
   static public final int REC=0525;
   static public final int KILAD=0505;

   static public final int MEM=003;
   static public final int MEMI=014;
   static public final int SSHPT=044;
   static public final int SOPEN=063;
   static public final int SPAGE=060;
   static public final int GSHPT=073;
   static public final int GTOD=036;
   static public final int RECREATE=0336;
   static public final int OPEN=0300;
   static public final int CLOSE=0301;
   static public final int READ=0302;
   static public final int WRITE=0303;
   static public final int UPDATE=0232;
   static public final int UIDSTAT=0333;

   static public final int PNAME=0116;
   static public final int SERVE=0171;
   static public final int RETURN=0310;
   static public final int IXIT=0542;
   static public final int INTWT=016;
   static public final int DADID=0127;
   static public final int WDELAY=0263;

   public OSProcess process;
   public OSTask    task;
   public Memory    memory;
   public Machine   machine;

   public int       ac0, ac1, ac2, ac3;

   static public OSContext contextForCall(int call, OSProcess process, OSTask task, Memory memory, Machine machine) {
    switch(call) {
     case MEM: case MEMI: case GTOD: case RETURN: case PNAME: case DADID: case RECREATE: case IXIT: case INTWT:
     case WDELAY:
      return new OSContextSystem(process, task, memory, machine);

     case GSHPT: case SSHPT: case SOPEN: case SPAGE:
      return new OSContextShared(process, task, memory, machine);

     case CREATE: case SERVE: case ILKUP: case IREC: case ISEND: case ISR: case CON: case DCON:
      return new OSContextIPC(process, task, memory, machine);

     case OPEN: case CLOSE: case READ: case WRITE: case UPDATE:
      return new OSContextFS(process, task, memory, machine);

     case TASK: case REC: case KILAD: case UIDSTAT:
      return new OSContextTask(process, task, memory, machine);
    }
    throw new RuntimeException("Unimplemented system call " + String.format("%04o", call));
   }

   public int aosError(String name) {
    Integer value=AOSVSSymbols.symbols.get(name);

    if(value==null)
     throw new RuntimeException("Undefined AOS/VS symbol '" + name + "'");
    return value.intValue();
   }

   public int aosSymbol(String name) {
    Integer value=AOSVSSymbols.symbols.get(name);

    if(value==null)
     throw new RuntimeException("Undefined AOS/VS symbol '" + name + "'");
    return value.intValue();
   }

   public OSContext(OSProcess process, OSTask task, Memory memory, Machine machine) {
    this.process=process;
    this.task=task;
    this.memory=memory;
    this.machine=machine;
   }

   public String fullPath(String name) {
    return task.fullPath(name);
   }

   public byte[] readByteArray(int address, int length) {
    byte[] bytes;
    int    index;

    if(length<0) {
     length=0;
     while(memory.readByte(address+length)!=0)
      length++;
    }
    bytes=new byte[length];
    for(index=0;index<length;index++)
     bytes[index]=(byte)memory.readByte(address+index);
    return bytes;
   }

   public String readString(int address, int length) {
    try {
     return new String(readByteArray(address, length), "ASCII");
    }
    catch(UnsupportedEncodingException exception) {
     throw new RuntimeException("ASCII encoding is not supported");
    }
   }

   public String readString(int address) {
    return readString(address, -1);
   }

   public void writeByteArray(byte[] bytes, int address, int length) {
    for(int index=0;index<length;index++)
     memory.writeByte(address+index, (int)(bytes[index] & 0xFF));
   }

   public void writeByteArray(byte[] bytes, int address) {
    writeByteArray(bytes, address, bytes.length);
   }

   public void writeString(String string, int address) {
    try {
     writeByteArray(string.getBytes("ASCII"), address);
    }
    catch(UnsupportedEncodingException exception) {
     throw new RuntimeException("ASCII encoding is not supported");
    }
   }

   public boolean readPacketFlag(String symbol, String flag, int location) {
    Integer tableOffset=aosSymbol(symbol), flagMask=aosSymbol(flag);

    return (flagMask.intValue() & memory.readWord(location+tableOffset.intValue()))!=0;
   }

   public int readPacketByte(String symbol, int highLow, int location) {
    Integer tableOffset=aosSymbol(symbol);

    return memory.readByte((location+tableOffset.intValue())*2+highLow);
   }

   public int readPacketWord(String symbol, int location) {
    Integer tableOffset=aosSymbol(symbol);

    return memory.readWord(location+tableOffset.intValue());
   }

   public int readPacketWide(String symbol, int location) {
    Integer tableOffset=aosSymbol(symbol);

    return memory.readWide(location+tableOffset.intValue());
   }

   public boolean readPacketFlag(String symbol, String flag) {
    return readPacketFlag(symbol, flag, ac2);
   }

   public int readPacketByte(String symbol, int highLow) {
    return readPacketByte(symbol, highLow, ac2);
   }

   public int readPacketWord(String symbol) {
    return readPacketWord(symbol, ac2);
   }

   public int readPacketWide(String symbol) {
    return readPacketWide(symbol, ac2);
   }

   public void writePacketByte(String symbol, int highLow, int value, int location) {
    Integer tableOffset=aosSymbol(symbol);

    memory.writeByte((location+tableOffset.intValue())*2+highLow, value);
   }

   public void writePacketWord(String symbol, int value, int location) {
    Integer tableOffset=aosSymbol(symbol);

    memory.writeWord(location+tableOffset.intValue(), value);
   }

   public void writePacketWide(String symbol, int value, int location) {
    Integer tableOffset=aosSymbol(symbol);

    memory.writeWide(location+tableOffset.intValue(), value);
   }

   public void writePacketByte(String symbol, int highLow, int value) {
    writePacketByte(symbol, highLow, value, ac2);
   }

   public void writePacketWord(String symbol, int value) {
    writePacketWord(symbol, value, ac2);
   }

   public void writePacketWide(String symbol, int value) {
    writePacketWide(symbol, value, ac2);
   }

   public int dispatchSystemCall(int call) {
    throw new RuntimeException("Dispatch system call must be implemented by the subclass");
   }

/*
   public int dispatchSystemCall() {
    int address=memory.readWide(machine.wsp-2);
    int call=memory.readWord(address);
    int returnAddress=machine.ac[3];
    int error;

    ac0=machine.ac[0];
    ac1=machine.ac[1];
    ac2=machine.ac[2];

 System.out.printf("System Call %o, called from %08X\n", call, address-2);
 System.out.printf(" ac0=%08X\n", machine.ac[0]);
 System.out.printf(" ac1=%08X\n", machine.ac[1]);
 System.out.printf(" ac2=%08X\n", machine.ac[2]);
 System.out.printf(" ac3=%08X\n", machine.ac[3]);


    switch(call) {
     case ILKUP:
      error=ipcContext().ILKUP();
      break;

     case MEM:
      error=systemContext().MEM();
      break;

     case MEMI:
      error=systemContext().MEMI();
      break;

     case GSHPT:
      error=sharedContext().GSHPT();
      break;

     case SSHPT:
      error=sharedContext().SSHPT();
      break;

     case SOPEN:
      error=sharedContext().SOPEN();
      break;

     case SPAGE:
      error=sharedContext().SPAGE();
      break;

     case GTOD:
      error=systemContext().GTOD();
      break;

     case CREATE:
      error=ipcContext().CREATE();
      break;

     case RECREATE:
      error=RECREATE();
      break;

     case OPEN:
      error=fsContext().OPEN();
      break;

     case CLOSE:
      error=fsContext().CLOSE();
      break;

     case READ:
      error=fsContext().READ();
      break;

     case WRITE:
      error=fsContext().WRITE();
      break;

     case PNAME:
      error=systemContext().PNAME();
      break;

     case SERVE:
      error=ipcContext().SERVE();
      break;

     case RETURN:
      error=systemContext().RETURN();
      break;

     case IXIT:
      error=systemContext().SUCCESS;
      break;

     case DADID:
      ac1=1;
      error=SUCCESS;
      break;

     case CON:
      // ignored
      error=SUCCESS;
      break;

     case DCON:
      // ignored
      error=SUCCESS;
      break;

     case ISEND:
      error=ipcContext().ISEND();
      break;

     case IREC:
      error=ipcContext().IREC();
      break;

     case ISR:
      error=ipcContext().ISR();
      break;

     case TASK:
      error=taskContext().TASK();
      break;

     case REC:
      error=taskContext().REC();
      break;

     default:
      throw new RuntimeException("Unimplemented system call");
//      error=INVALID;
    }

    machine.setPSR(machine.widePop()>>>16);
System.out.println("RETURNING AC1=" + ac1);
    if(error==SUCCESS) {
     machine.ac[0]=ac0;
     machine.ac[1]=ac1;
     machine.ac[2]=ac2;
     return returnAddress+1;
    }
    else {
System.out.println("****** ERROR RETURN ******  CODE=" + String.format("%04X", error));
System.out.println();
     machine.ac[0]=error;
     return returnAddress;
    }
   }
*/
}
