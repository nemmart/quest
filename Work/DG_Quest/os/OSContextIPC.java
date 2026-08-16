package os;

import hw.*;

public class OSContextIPC extends OSContext {
   public OSContextIPC(OSProcess process, OSTask task, Memory memory, Machine machine) {
    super(process, task, memory, machine);
   }

   public int dispatchSystemCall(int call) {
    switch(call) {
     case CREATE:
      return CREATE();

     case SERVE:
      return SERVE();

     case CON:
      return CON();

     case DCON:
      return DCON();

     case ILKUP:
      return ILKUP();

     case ISEND:
      return ISEND();

     case IREC:
      return IREC();

     case ISR:
      return ISR();
    }
    throw new RuntimeException("Dispatch system call - missing case");
   }

   public int CREATE() {
    String name=readString(ac0);
    int    type=readPacketWord("?CFTYP") & 0xFF, timePacket=readPacketWide("?CTIM"), aclPacket=readPacketWide("?CACP");

    System.out.println();
    System.out.println("CREATE:");
    System.out.printf("   name = %s\n", name);
    System.out.printf("   type = %04X\n", type);
    System.out.printf("   time packet address = %08X\n", timePacket);
    System.out.printf("   acl = %08X\n", aclPacket);

    if(type==aosSymbol("?FIPC")) {
     int localPort=readPacketWord("?CPOR");

     System.out.printf("   local port = %04X\n", localPort);
     System.out.println();
     try {
      OS.global.registerService(process, name, localPort);
     }
     catch(OSError error) {
      return error.error();
     }

     return SUCCESS;
    }
    else
     throw new RuntimeException("BOMB");
   }

   public int SERVE() {
    System.out.println();
    System.out.println("SERVE:");
    System.out.printf("   servmsg packet = %08X\n", ac2);
    System.out.println();
    System.out.println("Server call ignored");
    System.out.println();
    return SUCCESS;
   }

   public int ILKUP() {
    String name=readString(ac0);

    System.out.println();
    System.out.println("ILKUP:");
    System.out.printf("   service = %s\n", name);
    System.out.println();

    try {
     ac1=OS.global.retrieveService(name);
     ac2=aosSymbol("?FIPC");
     System.out.printf("   service port = %08X\n", ac1);
     return SUCCESS;
    }
    catch(OSError error) {
     return error.error();
    }
   }

   public int ISEND() {
    int       systemFlags=readPacketWord("?ISFL"), userFlags=readPacketWord("?IUFL"), destinationPort=readPacketWide("?IDPH");
    int       originPort=readPacketWord("?IOPN"), length=readPacketWord("?ILTH"), data=readPacketWide("?IPTR");
    OSMessage message;
    int[]     content;

    System.out.println();
    System.out.println("ISEND:");
    System.out.printf("   system flags = %04X\n", systemFlags);
    System.out.printf("   user flags = %04X\n", userFlags);
    System.out.printf("   destination = %08X\n", destinationPort);
    System.out.printf("   origin port = %04X\n", originPort);
    System.out.printf("   length = %04X\n", length);
    System.out.printf("   data pointer = %08X\n", data);
    System.out.println();

    try {
System.err.println("SENDING MESSAGE");
     content=new int[length];
     for(int index=0;index<length;index++)
      content[index]=memory.readWord(data+index);
     message=new OSMessage((process.pid<<16)|originPort, destinationPort, userFlags, data, content);
     OS.global.sendMessage(message);
    }
    catch(OSError error) {
     return error.error();
    }
    return SUCCESS;
   }

public void irecReturn() {
    int       systemFlags=readPacketWord("?ISFL"), userFlags=readPacketWord("?IUFL"), originPort=readPacketWide("?IOPH");
    int       destinationPort=readPacketWord("?IDPN"), length=readPacketWord("?ILTH"), data=readPacketWide("?IPTR");
    OSMessage message;
    int[]     content;

    System.err.println();
    System.err.println("IREC RETURN:");
    System.err.printf("   system flags = %04X\n", systemFlags);
    System.err.printf("   user flags = %04X\n", userFlags);
    System.err.printf("   origin port = %08X\n", originPort);
    System.err.printf("   destination port = %04X\n", destinationPort);
    System.err.printf("   length = %04X\n", length);
    System.err.printf("   data pointer = %08X\n", data);
    System.err.println();
}

   public int IREC() {
    int       systemFlags=readPacketWord("?ISFL"), userFlags=readPacketWord("?IUFL"), originPort=readPacketWide("?IOPH");
    int       destinationPort=readPacketWord("?IDPN"), length=readPacketWord("?ILTH"), data=readPacketWide("?IPTR");
    OSMessage message;
    int[]     content;

    System.out.println();
    System.out.println("IREC:");
    System.out.printf("   system flags = %04X\n", systemFlags);
    System.out.printf("   user flags = %04X\n", userFlags);
    System.out.printf("   origin port = %08X\n", originPort);
    System.out.printf("   destination port = %04X\n", destinationPort);
    System.out.printf("   length = %04X\n", length);
    System.out.printf("   data pointer = %08X\n", data);
    System.out.println();

    try {
     message=OS.global.receiveMessage(process.pid);
     if(message==null)
      throw new RuntimeException("System call 'IREC' interrupted");
     content=message.content();
     if(length<content.length)
      throw new RuntimeException("Insufficient space to receive message");

System.err.println("RECEIVED MESSAGE");
     writePacketWord("?IUFL", message.userFlags());
     writePacketWide("?IOPH", message.origin());
     writePacketWord("?IDPN", message.destinationPort());
     for(int index=0;index<content.length;index++)
      memory.writeWord(data+index, content[index]);
     writePacketWord("?ILTH", content.length);
     if(content.length==0)
      writePacketWide("?IPTR", message.pointer());
irecReturn();
     return SUCCESS;
    }
    catch(OSError error) {
     return error.error();
    }
   }

   public int ISR() {
    int       systemFlags=readPacketWord("?ISFL"), userFlags=readPacketWord("?IUFL"), destinationPort=readPacketWide("?IDPH");
    int       originPort=readPacketWord("?IOPN"), sendLength=readPacketWord("?ILTH"), sendData=readPacketWide("?IPTR");
    int       receiveLength=readPacketWord("?IRLT"), receiveData=readPacketWide("?IRPT");
    int[]     content;
    OSMessage message;

    System.out.println();
    System.out.println("ISR:");
    System.out.printf("   system flags = %04X\n", systemFlags);
    System.out.printf("   user flags = %04X\n", userFlags);
    System.out.printf("   destination = %08X\n", destinationPort);
    System.out.printf("   origin port = %04X\n", originPort);
    System.out.printf("   send length = %04X\n", sendLength);
    System.out.printf("   send pointer = %08X\n", sendData);
    System.out.printf("   receive length = %04X\n", receiveLength);
    System.out.printf("   receive pointer = %08X\n", receiveData);

    try {
System.err.println("ISR SENDING MESSAGE");
     content=new int[sendLength];
     for(int index=0;index<sendLength;index++)
      content[index]=memory.readWord(sendData+index);
     message=new OSMessage((process.pid<<16)|originPort, destinationPort, userFlags, sendData, content);
     OS.global.sendMessage(message);

     message=OS.global.receiveMessage(process.pid);
     if(message==null)
      throw new RuntimeException("System call 'ISR' interrupted");
     content=message.content();
     if(receiveLength<content.length)
      throw new RuntimeException("Insufficient space to receive message");

System.err.println("ISR RECEIVED MESSAGE");
//     writePacketWide("?IOPH", message.origin());
//     writePacketWord("?IDPN", message.destinationPort());
     writePacketWord("?IUFL", message.userFlags());
     for(int index=0;index<content.length;index++)
      memory.writeWord(receiveData+index, content[index]);
     writePacketWord("?IRLT", content.length);
     if(content.length==0)
      writePacketWide("?IPTR", message.pointer());
    }
    catch(OSError error) {
     return error.error();
    }

    return SUCCESS;
   }

   public int CON() {
    System.out.println();
    System.out.println("CON:");
    System.out.printf("   pid = %04X\n", ac0);
    System.out.println();

    OS.global.connect(process, ac0);
    return SUCCESS;
   }

   public int DCON() {
    System.out.println();
    System.out.println("DCON call ignored");
    System.out.println();
    return SUCCESS;
   }
}
