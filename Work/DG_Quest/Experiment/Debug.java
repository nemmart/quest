public class Debug {
   static public int sum_carry(int a, int b) {
    return (a+b)>>4;
   }

   static public int sub_carry(int a, int b) {
    return ((b-a)>>4)&1;
   }

   static public int sum_overflow(int a, int b) {
    if(a>=8 && b>=8)
     if(a+b-16<8)
      return 1;
    if(a<8 && b<8)
     if(a+b>=8)
      return 1;
    return 0;
   }

   static public int sub_overflow(int a, int b) {
    if(a>=8 && b<8)
     if(b-a+16>=8)
      return 1;
    if(a<8 && b>=8)
     if(b-a<8)
      return 1;
    return 0;
   }

   static public void doubleTest() {
    double x=1e-200;

    System.out.printf("%016X\n", Double.doubleToRawLongBits(x*x));
    System.out.printf("%016X\n", Double.doubleToRawLongBits(0.0*x));
   }

   static public void testing() {
    byte a, b;

    for(int i=0;i<256;i++)
     for(int j=0;j<256;j++) {
      a=(byte)i; b=(byte)j;

      if(i<j) {
       if(b-a>=0)
        System.out.println("FAILED!");
      }
      if(i>j) {
       if(a-b>=0)
        System.out.println("FAILED");
      }
     }
    System.out.println("done");
   }


   static public void main(String[] args) {
    int sum, sub, comp, sum_ovr, sub_ovr, sum_car, sub_car;

//    doubleTest();

//    testing();

    sum=-20;

System.out.println(sum%6);
System.out.println(sum/6);

/*
    for(int src=0;src<16;src++)
     for(int dst=0;dst<16;dst++) {
      sum=(src+dst) & 0x0F;
      sub=(dst-src) & 0x0F;
      sum_ovr=(((src^sum)&~(src^dst))>>>3)&1;
      sub_ovr=(((src^dst)&(dst^sub))>>>3)&1;

      sum_car=(src>>3)+(dst>>3)-(sum>>3);
      if(sum_car>0)
       sum_car=1;
      else
       sum_car=0;

      sub_car=(sub>>3)-(dst>>3)+(src>>3);
      if(sub_car<0)
       sub_car=0;
      else
       sub_car=1;


      if(sum_carry(src, dst)!=sum_car)
       System.out.println("Failed for " + src + " " + dst + " " + sum_carry(src, dst) + " " + sum_car);
//      if(sum_overflow(src, dst)!=sum_ovr)
//       System.out.println("Failed for " + src + " " + dst);
//      if(sub_overflow(src, dst)!=sub_ovr)
//       System.out.println("Failed for " + src + " " + dst);
      if(sub_carry(src, dst)!=sub_car)
       System.out.println("Failed for " + src + " " + dst + " " + sub_carry(src, dst) + " " + sub_car);
     }
*/
   }
}
