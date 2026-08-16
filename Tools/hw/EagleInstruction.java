package hw;

public class EagleInstruction extends Instruction {
   public int add(Machine machine, long src, long dst) {
    long result=dst+src;
    long overflow=((src^result)&~(src^dst))>>>31;
    long carry=(dst & 0xFFFFFFFFl)+(src & 0xFFFFFFFFl)>>31;

    machine.ovr|=(int)(overflow & 0x01);
    machine.c=(int)(carry & 0x01);

    return (int)(result & 0xFFFFFFFFl);
   }

   public int sub(Machine machine, long src, long dst) {
    long result=dst-src;
    long overflow=((src^dst)&(result^dst))>>>31;
    long carry=(dst & 0xFFFFFFFFl)-(src & 0xFFFFFFFFl)>>31;

    machine.ovr|=(int)(overflow & 0x01);
    machine.c=(int)(carry & 0x01);

    return (int)(result & 0xFFFFFFFFl);
   }

   public int mul(Machine machine, long src, long dst) {
    long product=src*dst;
    long overflow=product>>31;

    if(overflow!=0 && overflow!=-1)
     machine.ovr|=1;
    return (int)(product & 0xFFFFFFFFl);
   }

   public int arithmeticShift(Machine machine, int src, int amount) {
    int result=src;

    if(amount>0) {
     if(amount<32)
      result=src<<amount;
     else
      result=0;
    }
    else if(amount<0) {
     if(amount>-32)
      result=src>>-amount;
     else
      result=src>>31;
    }
    machine.ovr|=(result^src)>>>31;
    return result;
   }

   public int logicalShift(Machine machine, int src, int amount) {
    if(amount!=0) {
     if(amount>0 && amount<32)
      src=src<<amount;
     else if(amount<0 && amount>-32)
      src=src>>>-amount;
     else
      src=0;
    }
    return src;
   }

   public int narrowAdd(Machine machine, int src, int dst) {
    int result, overflow, carry;

    src=(src<<16)>>16;
    dst=(dst<<16)>>16;
    result=dst+src;
    overflow=((src^result)&~(src^dst))>>>16;
    carry=(dst & 0xFFFF)+(src & 0xFFFF)>>16;

    machine.ovr|=overflow & 0x01;
    machine.c=carry & 0x01;

    return result;
   }

   public int narrowSub(Machine machine, int src, int dst) {
    int result, overflow, carry;

    src=(src<<16)>>16;
    dst=(dst<<16)>>16;
    result=dst-src;
    overflow=((src^dst)&(result^dst))>>>16;
    carry=(dst & 0xFFFF)+(-src & 0xFFFF)>>16;

    machine.ovr|=overflow & 0x01;
    machine.c=carry & 0x01;

    return result;
   }

   public int narrowMul(Machine machine, int src, int dst) {
    int overflow;

    src=(src<<16)>>16;
    dst=(dst<<16)>>16;
    dst=dst*src;

    overflow=dst>>16;
    if(overflow!=0 && overflow!=-1)
     machine.ovr=1;
    return (dst & 0xFFFF);
   }

   public void validateExponent(Machine machine, double x) {
    int exponent;

    if(x==0.0)
     return;

    exponent=(int)(Double.doubleToLongBits(x)>>52);
    exponent=(exponent & 0x7FF)-1019>>2;

    if(exponent<-64)
     throw new RuntimeException("Floating point underflow");
    if(exponent>63)
     throw new RuntimeException("Floating point overflow");
   }

   public long doubleToEclipseWideFloat(Machine machine, double x) {
    long bits, sign, exponent;

    if(x==0.0)
     return 0;

    bits=Double.doubleToLongBits(x);

    sign=bits & 0x8000000000000000l;
    exponent=((bits>>52) & 0x7FF) - 1019;
    bits=(bits & 0x000FFFFFFFFFFFFFl) | 0x0010000000000000l;
    bits=bits<<(exponent & 0x03);

    exponent=exponent>>2;
    if(exponent<-64)
     throw new RuntimeException("Floating point underflow");
    if(exponent>63)
     throw new RuntimeException("Floating point overflow");

    return sign | (exponent+64<<56) | bits;
   }

   public double eclipseWideFloatToDouble(Machine machine, long x) {
    long mantissa, exponent;
    int  left=0;

    mantissa=x & 0x00FFFFFFFFFFFFFFl;
    if(mantissa==0)
     return 0.0;

    while((mantissa & 0x00F0000000000000l)==0) {
     mantissa=mantissa<<4;
     left=left+1;
    }
    exponent=(((x>>56) & 0x7F)-65-left)*4+1023;

    while((mantissa & 0x00E0000000000000l)!=0) {
     mantissa=mantissa>>1;
     exponent=exponent+1;
    }

    x=(x & 0x8000000000000000l) | (exponent<<52) | (mantissa & 0x000FFFFFFFFFFFFFl);
    return Double.longBitsToDouble(x);
   }

   public double eclipseWideRound(Machine machine, double x) {
    long eclipse=doubleToEclipseWideFloat(machine, x), rounded;
    long mask=0x00FFFFFF00000000l;

    if(machine.fpr!=0) {
     // in reality, 0.5 should sometimes round up and sometimes round down.
     // but that's not implemented
     if((eclipse & mask)==mask) {
      rounded=eclipse & 0xFF00000000000000l;
      rounded=rounded + 0x0110000000000000l;
      if(((rounded^eclipse)>>>63)!=0)
       throw new RuntimeException("Overflow during rounding");
      return eclipseWideFloatToDouble(machine, eclipse);
     }
     else
      eclipse=eclipse+0x0000000080000000l;
    }

    eclipse=eclipse & 0xFFFFFFFF00000000l;
    return eclipseWideFloatToDouble(machine, eclipse);
   }

   public int doubleToEclipseFloat(Machine machine, double x) {
    long eclipse=doubleToEclipseWideFloat(machine, x), rounded;
    long mask=0x00FFFFFF00000000l;

    if(machine.fpr!=0) {
     // in reality, 0.5 should sometimes round up and sometimes round down.
     // but that's not implemented
     if((eclipse & mask)==mask) {
      rounded=eclipse & 0xFF00000000000000l;
      rounded=rounded + 0x0110000000000000l;
      if(((rounded^eclipse)>>>63)!=0)
       throw new RuntimeException("Overflow during rounding");
      return (int)(rounded>>>32);
     }
     else
      eclipse=eclipse+0x0000000080000000l;
    }

    return (int)(eclipse>>>32);
   }

   public double eclipseFloatToDouble(Machine machine, int x) {
    return eclipseWideFloatToDouble(machine, ((long)x)<<32);
   }

/*
   static public void main(String[] args) {
    Machine          machine=new Machine(null);
    EagleInstruction ei=new EagleInstruction();
    double           start=.0004d, round;
    long             eagle;

    machine.fpr=1;
    eagle=ei.doubleToEclipseWideFloat(machine, start);
    System.out.printf("%016X\n", Double.doubleToLongBits(start));
    round=ei.eclipseWideRound(machine, start);
    System.out.printf("%016X\n", Double.doubleToLongBits(round));
   }
*/
}
