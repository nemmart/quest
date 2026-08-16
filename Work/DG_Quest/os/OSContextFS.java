package os;

import hw.*;

public class OSContextFS extends OSContext {

   public OSContextFS(OSProcess process, OSTask task, Memory memory, Machine machine) {
    super(process, task, memory, machine);
   }

   public int dispatchSystemCall(int call) {
    switch(call) {
     case OPEN:
      return OPEN();

     case CLOSE:
      return CLOSE();

     case READ:
      return READ();

     case WRITE:
      return WRITE();

     case UPDATE:
      return UPDATE();
    }
    throw new RuntimeException("Dispatch system call - missing case");
   }

   public int OPEN() {
    int    options=readPacketWord("?ISTI"), fileType=readPacketWord("?ISTO"), blockSize=readPacketWord("?IMRS");
    int    bufferPointer=readPacketWide("?IBAD"), densityMode=readPacketWord("?IRES"), recordLength=readPacketWord("?IRCL");
    int    namePointer=readPacketWide("?IFNP");
    String file=fullPath(readString(namePointer));
    int    slot=-1;

    if(process.systemCallLogging) {
     System.out.println();
     System.out.println("OPEN (fs op):");
     System.out.printf("   (fs op) options = %04X\n", options);
     System.out.printf("   (fs op) file type = %04X\n", blockSize);
     System.out.printf("   (fs op) block size = %04X\n", densityMode);
     System.out.printf("   (fs op) record length = %04X\n", recordLength);
     System.out.printf("   (fs op) buffer pointer = %08X\n", bufferPointer);
     System.out.printf("   (fs op) name pointer = %08X\n", namePointer);
     System.out.printf("   (fs op) file = %s\n", file);
    }

    try {
     synchronized(process.channels) {
      slot=process.assignChannel(OS.global.openFile(process, file, options));
     }
    }
    catch(OSError error) {
     return error.error();
    }
    if(process.systemCallLogging)
     System.out.println("Assigning channel: " + slot);
    writePacketWord("?ICH", slot);
    return SUCCESS;
   }

   public int CLOSE() {
    int channel=readPacketWord("?ICH");

    if(process.systemCallLogging) {
     System.out.println();
     System.out.println("CLOSE (fs cl):");
     System.out.printf("   (fs cl) channel = %04X\n", channel);
     System.out.println();
    }

    try {
     synchronized(process.channels) {
      if(process.channels[channel]==null)
       return OSError.OS_INVALID_CHANNEL_NUMBER;
      process.channels[channel].close();
      process.channels[channel]=null;
     }
    }
    catch(OSError error) {
     writePacketWord("?IRLR", 0);
     return error.error();
    }
    return SUCCESS;
   }

   public int READ() {
    int    channel=readPacketWord("?ICH"), options=readPacketWord("?ISTI"), bufferPointer=readPacketWide("?IBAD"), byteCount=readPacketWord("?IRCL");
    int    recordNumber=readPacketWide("?IRNH");
    byte[] bytes;

    if(process.systemCallLogging) {
     System.out.println();
     System.out.println("READ (fs rd):");
     System.out.printf("   (fs rd) channel = %04X\n", channel);
     System.out.printf("   (fs rd) options = %04X\n", options);
     System.out.printf("   (fs rd) record number = %d\n", recordNumber);
     System.out.printf("   (fs rd) buffer pointer = %08X\n", bufferPointer);
     System.out.printf("   (fs rd) byte count = %04X\n", byteCount);
    }

    if(readPacketFlag("?ISTI", "?IPKL")) {
     int packet=readPacketWide("?ETSP");

     if(process.systemCallLogging) {
      System.out.println("READ EXTENSION");
      System.out.printf("   location = %08X\n", packet);
      if((packet & 0x7FFFFFFF)!=0) {
       System.out.printf("   options = %04X\n", readPacketWord("?ESFC", packet & 0x7FFFFFFF));
       System.out.printf("   rel pos = %04X\n", readPacketWord("?ESEP", packet & 0x7FFFFFFF));
       System.out.printf("   init pos = %04X\n", readPacketWord("?ESCR", packet & 0x7FFFFFFF));
      }
     }
    }

    bytes=new byte[byteCount];
    try {
     FSChannel file;
     int       amount;

     synchronized(process.channels) {
      if(process.channels[channel]==null)
       return OSError.OS_INVALID_CHANNEL_NUMBER;
      file=process.channels[channel];
     }
     if(readPacketFlag("?ISTI", "?IPST"))
      file.setPosition(recordNumber);
     amount=file.read(bytes, !readPacketFlag("?ISTI", "?IBIN"));
     if(amount==-1)
      return aosError("EREOF");
     if(amount>0)
      writeByteArray(bytes, bufferPointer, amount);
     writePacketWord("?IRLR", amount);
    }
    catch(OSError error) {
     writePacketWord("?IRLR", 0);
     return error.error();
    }
    return SUCCESS;
   }

   public int WRITE() {
    int       channelNumber=readPacketWord("?ICH"), options=readPacketWord("?ISTI"), bufferPointer=readPacketWide("?IBAD"), byteCount=readPacketWord("?IRCL");
    int       recordNumber=readPacketWide("?IRNH");
    int       screenOptions=0, initialPosition=0, relativePosition=0;
    int       index;
    byte[]    bytes;
    FSChannel channel;

    if(process.systemCallLogging) {
     System.out.println();
     System.out.println("WRITE (fs wt):");
     System.out.printf("   (fs wt) channel = %04X\n", channelNumber);
     System.out.printf("   (fs wt) options = %04X\n", options);
     System.out.printf("   (fs wt) record number = %d\n", recordNumber);
     System.out.printf("   (fs wt) buffer pointer = %08X\n", bufferPointer);
     System.out.printf("   (fs wt) byte count = %04X\n", byteCount);

     System.out.println(readPacketFlag("?ISTI", "?IPKL"));
    }

    if(readPacketFlag("?ISTI", "?IPKL")) {
     int packet=readPacketWide("?ETSP");

     screenOptions=readPacketWord("?ESFC", packet & 0x7FFFFFFF);
     relativePosition=readPacketWord("?ESEP", packet & 0x7FFFFFFF);
     initialPosition=readPacketWord("?ESCR", packet & 0x7FFFFFFF);
     if(process.systemCallLogging) {
      System.out.println("WRITE EXTENSION");
      System.out.printf("   location = %08X\n", packet);
      System.out.printf("   options = %04X\n", screenOptions);
      System.out.printf("   rel pos = %04X\n", relativePosition);
      System.out.printf("   init pos = %04X\n", readPacketWord("?ESCR", packet));
     }
    }

    if(byteCount==0xFFFF) {
     bytes=readByteArray(bufferPointer, -1);
     System.out.println(readString(bufferPointer, -1));
     byteCount=bytes.length;
    }
    else {
     bytes=readByteArray(bufferPointer, byteCount);
     System.out.println(readString(bufferPointer, byteCount));
    }

    try {
     synchronized(process.channels) {
      channel=process.retrieveChannel(channelNumber);
     }
     if((screenOptions & aosSymbol("?ESCP"))!=0) {
      byte[] cursor=new byte[]{020, (byte)(initialPosition>>8), (byte)(initialPosition & 0xFF)};

      channel.write(cursor);
     }
     channel.write(bytes);
    }
    catch(OSError error) {
     writePacketWord("?IRLR", 0);
     return error.error();
    }

    writePacketWord("?IRLR", byteCount);
    return SUCCESS;
   }


   public int UPDATE() {
    System.out.println();
    System.out.println("UPDATE:");
    System.out.printf("   channel = %04X\n", ac1);
    System.out.println();

    return SUCCESS;
   }
}
