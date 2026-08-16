package os;

import java.nio.MappedByteBuffer;
import hw.*;

// MappedPage is a Page subclass backed by a region of a MappedByteBuffer.
// Reads and writes go directly to the memory-mapped file -- no explicit
// save/flush is needed.  The OS will write dirty pages to disk on its own
// schedule, and we call force() at shutdown for safety.
//
// Each MappedPage covers 2048 bytes (one DG page) starting at baseOffset
// within the buffer.

public class MappedPage extends Page {
  private MappedByteBuffer buffer;
  private int              baseOffset;

  public MappedPage(MappedByteBuffer buffer, int baseOffset) {
    this.buffer = buffer;
    this.baseOffset = baseOffset;
  }

  public int read(int offset) {
    return buffer.get(baseOffset + offset) & 0xFF;
  }

  public void write(int offset, int value) {
    buffer.put(baseOffset + offset, (byte)(value & 0xFF));
  }

  public MappedByteBuffer getMappedBuffer() {
    return buffer;
  }
}
