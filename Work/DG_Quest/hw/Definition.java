package hw;

public class Definition {
   public String match;
   public String name;
   public String instructionClass;
   public String instructionFormat;
   public int    operator;

   public Definition(String match, String name, String instructionClass, String instructionFormat, int operator) {
    this.match=match;
    this.name=name;
    this.instructionClass=instructionClass;
    this.instructionFormat=instructionFormat;
    this.operator=operator;
   }
}
