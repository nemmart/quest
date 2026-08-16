import hw.*;
import os.*;

public class HexDump {

   static public void dump(ReadWrite memory, int start, int stop) {
    char[] toHex = "0123456789ABCDEF".toCharArray();
    int    index = start;
    int    zeroStart = -1;

    while (index < stop) {
     // Check for runs of zeros (compress)
     int zeroCount = 0;
     while (index + zeroCount < stop && memory.readWord(index + zeroCount) == 0)
      zeroCount++;

     if (zeroCount >= 16) {
      // Skip large zero regions, print marker
      zeroCount = zeroCount - (zeroCount % 8);
      System.out.printf("%08X: *zero %d words*\n", index, zeroCount);
      index += zeroCount;
      continue;
     }

     // Print one line: 8 words
     int wordsThisLine = Math.min(8, stop - index);
     char[] ascii = new char[wordsThisLine * 2];

     System.out.printf("%08X:", index);
     for (int w = 0; w < wordsThisLine; w++) {
      int word = memory.readWord(index + w);
      System.out.printf(" %04X", word);
      int b1 = (word >> 8) & 0xFF, b2 = word & 0xFF;
      ascii[w * 2]     = (b1 > 32 && b1 < 127) ? (char) b1 : '.';
      ascii[w * 2 + 1] = (b2 > 32 && b2 < 127) ? (char) b2 : '.';
     }
     // Pad if short line
     for (int w = wordsThisLine; w < 8; w++)
      System.out.print("     ");
     System.out.print("  ");
     System.out.println(new String(ascii));
     index += wordsThisLine;
    }
   }

   static public void main(String[] args) {
    if (args.length < 3 || args.length > 4) {
     System.err.println("Usage: java HexDump <dir> <PR file> <start hex> [end hex]");
     System.err.println("  e.g. java HexDump /path/to/game QUEST 70000272 70000596");
     System.err.println("  If end is omitted, dumps 256 words from start.");
     System.exit(1);
    }

    FS.initializeWithPath(args[0]);

    String file = args[1].toUpperCase();
    if (file.endsWith(".PR"))
     file = file.substring(0, file.length() - 3);

    OSProcess process = new OSProcess(":", file);
    Memory memory = process.memory;

    int start = (int) Long.parseLong(args[2], 16);
    int stop;
    if (args.length == 4)
     stop = (int) Long.parseLong(args[3], 16);
    else
     stop = start + 256;

    dump(memory, start, stop);
   }
}