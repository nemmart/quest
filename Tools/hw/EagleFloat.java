package hw;

// We implement the eagle floating point format with a java Double.
// While the results will be close, they won't be exact.  The Eagle floating
// point format has more bits of mantissa than a double, but fewer bits
// of exponent.

// This could be replaced with an exact simulation using two integers, one to
// represent the mantissa and one for the exponent.  The MV floating point
// format exactly matches the Eclipse floating point format.  Which is
// implemented in http://simh.trailing-edge.com/

public class EagleFloat extends EagleInstruction {
   static public final int FTD=10;
   static public final int FTE=11;

   static public final int FMOV=100;
   static public final int WFLAD=101;
   static public final int WFFAD=102;
   static public final int XFLDS=103;
   static public final int LFLDS=104;
   static public final int XFSTS=105;
   static public final int LFSTS=106;
   static public final int XFLDD=107;
   static public final int LFLDD=108;
   static public final int XFSTD=109;
   static public final int LFSTD=110;

   static public final int FRDS=150;
   static public final int FHLV=151;
   static public final int FINT=152;
   static public final int FRH=153;
   static public final int FEXP=154;
   static public final int FSCAL=155;

   static public final int FAS=200;
   static public final int FSS=201;
   static public final int FMS=202;
   static public final int FDS=203;

   static public final int FAD=250;
   static public final int FSD=251;
   static public final int FMD=252;
   static public final int FDD=253;
   static public final int FCMP=254;

   static public final int FSEQ=270;
   static public final int FSNE=271;
   static public final int FSGE=272;
   static public final int FSGT=273;
   static public final int FSLE=274;
   static public final int FSLT=275;

   static public final int XFAMS=300;
   static public final int LFAMS=301;
   static public final int XFAMD=302;
   static public final int LFAMD=303;
   static public final int XFMMS=304;
   static public final int LFMMS=305;
   static public final int XFMMD=306;
   static public final int LFMMD=307;

   public int XX, YY;

   public void setup(int opcode, String name, String instructionFormat, int operation) {
    super.setup(opcode, name, instructionFormat, operation);

    opcode=opcode>>11;
    YY=opcode & 0x03;
    opcode=opcode>>2;
    XX=opcode & 0x03;
   }

   public int execute(Machine machine, int address, int opcode) {
    int  src, dst, resolved, exp;
    long quad, mantissa;

    switch(operator) {
     case FTD: case FTE:
      System.out.println("Ignoring FTE/FTD");
      return copySegment(address, address+1);

     case FMOV:
      machine.fpac[YY]=machine.fpac[XX];
      machine.quads[YY]=machine.quads[XX];
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+1);

     case WFLAD:
      machine.fpac[YY]=(double)machine.ac[XX];
      machine.quads[YY]=0;
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+1);

     case WFFAD:
      // does not update N and Z
      quad=(long)machine.fpac[YY];
      machine.ac[XX]=(int)quad;
      quad=quad>>31;
      if(quad!=0 && quad!=-1)
       throw new RuntimeException("Floating point conversion overflow");
      return copySegment(address, address+1);

     case FRDS:
      machine.fpac[YY]=eclipseWideRound(machine, machine.fpac[XX]);
      machine.quads[YY]=0;
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+1);

     case FHLV:
      machine.fpac[YY]=machine.fpac[YY]*0.5;
      machine.quads[YY]=0;
      validateExponent(machine, machine.fpac[YY]);
      return copySegment(address, address+1);

     case FINT:
      if(machine.fpac[YY]>0)
       machine.fpac[YY]=Math.floor(machine.fpac[YY]);
      else
       machine.fpac[YY]=Math.ceil(machine.fpac[YY]);
      machine.quads[YY]=0;
      return copySegment(address, address+1);

     case FRH:
      // does not update N and Z
      quad=doubleToEclipseWideFloat(machine, machine.fpac[YY]);
      machine.ac[0]=(int)(quad>>>48);
      return copySegment(address, address+1);

     case FEXP:
      quad=doubleToEclipseWideFloat(machine, machine.fpac[YY]);
      quad=quad & 0x80FFFFFFFFFFFFFFl;
      quad=quad | (((long)(machine.ac[0] & 0x00007F00))<<48);
      machine.fpac[YY]=eclipseWideFloatToDouble(machine, quad);
      machine.quads[YY]=0;
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+1);

     case FSCAL:
      // this is such a messy instruction.  It leaves the data
      // in the FP denormalized.
      exp=(machine.ac[0]>>8) & 0x7F;
      quad=doubleToEclipseWideFloat(machine, machine.fpac[YY]);
      exp=exp-(int)((quad>>56) & 0x7F);
      mantissa=quad & 0x00FFFFFFFFFFFFFFl;
      if(exp>0)
       mantissa=mantissa>>exp*4;
      else if(exp<0)
       mantissa=mantissa<<-exp*4;
      mantissa=mantissa & 0x00FFFFFFFFFFFFFFl;
      if(mantissa==0) {
       machine.fpac[YY]=0.0;
       machine.fplr=0.0;
      }
      else {
       quad=((quad>>56) & 0x80) + ((machine.ac[0]>>8) & 0x7F);
       quad=(quad<<56) + mantissa;
       machine.fpac[YY]=eclipseWideFloatToDouble(machine, quad);
       machine.quads[YY]=quad;
       System.err.printf("FSCAL Quad: %016X\n", quad);
      }
      return copySegment(address, address+1);

     case XFLDS:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      src=machine.memory.readWide(resolved);
      machine.fpac[YY]=eclipseFloatToDouble(machine, src);
      machine.quads[YY]=0;
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+2);

     case LFLDS:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      src=machine.memory.readWide(resolved);
      machine.fpac[YY]=eclipseFloatToDouble(machine, src);
      machine.quads[YY]=0;
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+3);

     case XFSTS:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      if(machine.quads[YY]!=0)
       src=(int)(machine.quads[YY]>>>32);
      else
       src=doubleToEclipseFloat(machine, machine.fpac[YY]);
      machine.memory.writeWide(resolved, src);
      return copySegment(address, address+2);

     case LFSTS:
      // does not update N and Z
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      if(machine.quads[YY]!=0)
       src=(int)(machine.quads[YY]>>>32);
      else
       src=doubleToEclipseFloat(machine, machine.fpac[YY]);
      machine.memory.writeWide(resolved, src);
      return copySegment(address, address+3);

     case XFLDD:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      quad=machine.memory.readQuad(resolved);
      machine.fpac[YY]=eclipseWideFloatToDouble(machine, quad);
      machine.quads[YY]=0;
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+2);

     case LFLDD:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      quad=machine.memory.readQuad(resolved);
      machine.fpac[YY]=eclipseWideFloatToDouble(machine, quad);
      machine.quads[YY]=0;
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+3);

     case XFSTD:
      // does not update N and Z
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      if(machine.quads[YY]!=0)
       quad=machine.quads[YY];
      else
       quad=doubleToEclipseWideFloat(machine, machine.fpac[YY]);
      machine.memory.writeQuad(resolved, quad);
      return copySegment(address, address+2);

     case LFSTD:
      // does not update N and Z
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      if(machine.quads[YY]!=0)
       quad=machine.quads[YY];
      else
       quad=doubleToEclipseWideFloat(machine, machine.fpac[YY]);
      machine.memory.writeQuad(resolved, quad);
      return copySegment(address, address+3);

     case FAS:
     case FAD:
      machine.fpac[YY]=machine.fpac[XX]+machine.fpac[YY];
      machine.quads[YY]=0;
      validateExponent(machine, machine.fpac[YY]);
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+1);

     case FSS:
     case FSD:
      machine.fpac[YY]=machine.fpac[YY]-machine.fpac[XX];
      machine.quads[YY]=0;
      validateExponent(machine, machine.fpac[YY]);
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+1);

     case FMS:
     case FMD:
      machine.fpac[YY]=machine.fpac[XX]*machine.fpac[YY];
      machine.quads[YY]=0;
      validateExponent(machine, machine.fpac[YY]);
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+1);

     case FDS:
     case FDD:
      if(machine.fpac[XX]==0.0)
       throw new RuntimeException("Division by zero");
      machine.fpac[YY]=machine.fpac[YY]/machine.fpac[XX];
      machine.quads[YY]=0;
      validateExponent(machine, machine.fpac[YY]);
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+1);

     case FCMP:
      machine.fplr=machine.fpac[YY]-machine.fpac[XX];
      return copySegment(address, address+1);

     case FSEQ:
      return copySegment(address, address+((machine.fplr==0.0)?2:1));

     case FSNE:
      return copySegment(address, address+((machine.fplr!=0.0)?2:1));

     case FSGE:
      return copySegment(address, address+((machine.fplr>=0.0)?2:1));

     case FSGT:
      return copySegment(address, address+((machine.fplr>0.0)?2:1));

     case FSLE:
      return copySegment(address, address+((machine.fplr<=0.0)?2:1));

     case FSLT:
      return copySegment(address, address+((machine.fplr<0.0)?2:1));

     case XFAMS:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      src=machine.memory.readWide(resolved);
      machine.fpac[YY]=machine.fpac[YY]+eclipseFloatToDouble(machine, src);
      machine.quads[YY]=0;
      validateExponent(machine, machine.fpac[YY]);
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+2);

     case LFAMS:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      quad=machine.memory.readQuad(resolved);
      machine.fpac[YY]=machine.fpac[YY]+eclipseWideFloatToDouble(machine, quad);
      machine.quads[YY]=0;
      validateExponent(machine, machine.fpac[YY]);
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+3);

     case XFAMD:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      quad=machine.memory.readQuad(resolved);
      machine.fpac[YY]=machine.fpac[YY]+eclipseWideFloatToDouble(machine, quad);
      machine.quads[YY]=0;
      validateExponent(machine, machine.fpac[YY]);
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+2);

     case LFAMD:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      quad=machine.memory.readQuad(resolved);
      machine.fpac[YY]=machine.fpac[YY]+eclipseWideFloatToDouble(machine, quad);
      machine.quads[YY]=0;
      validateExponent(machine, machine.fpac[YY]);
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+3);

     case XFMMS:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      src=machine.memory.readWide(resolved);
      machine.fpac[YY]=machine.fpac[YY]*eclipseFloatToDouble(machine, src);
      machine.quads[YY]=0;
      validateExponent(machine, machine.fpac[YY]);
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+2);

     case LFMMS:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      quad=machine.memory.readQuad(resolved);
      machine.fpac[YY]=machine.fpac[YY]*eclipseWideFloatToDouble(machine, quad);
      machine.quads[YY]=0;
      validateExponent(machine, machine.fpac[YY]);
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+3);

     case XFMMD:
      resolved=machine.eagleXResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      quad=machine.memory.readQuad(resolved);
      machine.fpac[YY]=machine.fpac[YY]*eclipseWideFloatToDouble(machine, quad);
      machine.quads[YY]=0;
      validateExponent(machine, machine.fpac[YY]);
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+2);

     case LFMMD:
      resolved=machine.eagleLResolveIndirect(copySegment(address, address+1), XX);   // index bits are in XX
      quad=machine.memory.readQuad(resolved);
      machine.fpac[YY]=machine.fpac[YY]*eclipseWideFloatToDouble(machine, quad);
      machine.quads[YY]=0;
      validateExponent(machine, machine.fpac[YY]);
      machine.fplr=machine.fpac[YY];
      return copySegment(address, address+3);
    }

    throw new RuntimeException("Internal error - some case is not returning");
   }
}

