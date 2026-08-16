#include "EagleSpecial.hpp"
#include "Machine.hpp"




namespace hw {
int32_t EagleSpecial::direction(int32_t count) {
  if(count>0) return -1;
  else if(count<0) return 1;
  else return 0;
}

uint32_t EagleSpecial::execute(Machine& machine, uint32_t address, uint32_t opcode) {
  int32_t dst, src, dst_count, src_count, dst_direction, src_direction;
  int32_t copy, dst_byte, src_byte, segment, result;
  int32_t mask;

  switch(oper) {
   case WBLM:
    segment=get_segment(address);
    src_count=machine.ac[1];
    src_direction=direction(src_count);
    src=machine.ac[2];
    dst=machine.ac[3];
    if((src & 0x80000000)!=0 || (dst & 0x80000000)!=0)
      throw std::runtime_error("WBLM instruction with indirection!");
    while(src_count!=0) {
      if(segment!=get_segment(src) || segment!=get_segment(dst))
        throw std::runtime_error("WBLM: crossing segments not allowed");
      copy=machine.memory->read_word(src);
      machine.memory->write_word(dst, copy);
      src_count+=src_direction;
      src=src-src_direction;
      dst=dst-src_direction;
    }
    machine.ac[1]=0;
    machine.ac[2]=src;
    machine.ac[3]=dst;
    return copy_segment(address, address+1);

   case WCMV:
    segment=get_segment(address);
    dst_count=machine.ac[0];
    src_count=machine.ac[1];
    dst=machine.ac[2];
    src=machine.ac[3];
    dst_direction=direction(dst_count);
    src_direction=direction(src_count);
    while(dst_count!=0) {
      if(src_count==0)
        copy=' ';
      else {
        if(segment!=get_byte_segment(src))
          throw std::runtime_error("WCMV: crossing segments not allowed");
        copy=machine.memory->read_byte(src);
        src_count=src_count+src_direction;
        src=src-src_direction;
      }
      if(segment!=get_byte_segment(dst))
        throw std::runtime_error("WCMV: crossing segments not allowed");
      machine.memory->write_byte(dst, copy);
      dst_count=dst_count+dst_direction;
      dst=dst-dst_direction;
    }
    machine.ac[0]=dst_count;
    machine.ac[1]=src_count;
    machine.ac[2]=dst;
    machine.ac[3]=src;
    if(src_count!=0)
      machine.c=1;
    else
      machine.c=0;
    return copy_segment(address, address+1);

   case WCMP:
    segment=get_segment(address);
    dst_count=machine.ac[0];
    src_count=machine.ac[1];
    dst=machine.ac[2];
    src=machine.ac[3];
    dst_direction=direction(dst_count);
    src_direction=direction(src_count);
    result=0;
    while(dst_count!=0 || src_count!=0) {
      if(src_count==0)
        src_byte=' ';
      else {
        if(segment!=get_byte_segment(src))
          throw std::runtime_error("WCMP: crossing segments not allowed");
        src_byte=machine.memory->read_byte(src);
        src_count=src_count+src_direction;
        src=src-src_direction;
      }
      if(dst_count==0)
        dst_byte=' ';
      else {
        if(segment!=get_byte_segment(dst))
          throw std::runtime_error("WCMP: crossing segments not allowed");
        dst_byte=machine.memory->read_byte(dst);
        dst_count=dst_count+dst_direction;
        dst=dst-dst_direction;
      }
      if(src_byte<dst_byte) { result=-1; break; }
      if(src_byte>dst_byte) { result=1; break; }
    }
    machine.ac[0]=dst_count;
    machine.ac[1]=result;
    machine.ac[2]=dst;
    machine.ac[3]=src;
    return copy_segment(address, address+1);

   case WCST:
    if((machine.ac[0] & 0x80000000)!=0)
      throw std::runtime_error("WCST instruction with indirection");
    src=machine.ac[3];
    src_count=machine.ac[1];
    src_direction=direction(machine.ac[1]);
    while(src_count!=0) {
      src_byte=machine.memory->read_byte(src);
      mask=0x8000>>(src_byte & 0x0F);
      if((machine.memory->read_word(copy_segment(machine.ac[0], machine.ac[0]+(src_byte>>4))) & mask)!=0)
        break;
      src_count=src_count+src_direction;
      src=src-src_direction;
    }
    machine.ac[1]=src_count;
    machine.ac[3]=src;
    return copy_segment(address, address+1);

   case WMESS:
    if((machine.ac[2] & 0x80000000)!=0)
      throw std::runtime_error("WMESS indirection");
    src=machine.memory->read_wide(machine.ac[2]);
    if(((src ^ machine.ac[0]) & machine.ac[3])==0) {
      machine.memory->write_wide(machine.ac[2], machine.ac[1]);
      machine.ac[1]=src;
      return copy_segment(address, address+2);
    }
    else {
      machine.ac[1]=src;
      return copy_segment(address, address+1);
    }

   // ---------------------------------------------------------------
   // Queue instructions (rewritten Aug 2026 from the DG queue-management
   // chapter; see docs/QUEUE_INSTRUCTIONS.md for the derivation).
   //
   // Data element (Table 6-1):  [elem+0] = FORWARD link
   //                            [elem+2] = BACKWARD link
   //   forward  == -1  =>  element is at the TAIL
   //   backward == -1  =>  element is at the HEAD
   // Queue descriptor:         [desc+0] = head element address
   //                            [desc+2] = tail element address
   //   empty queue = both -1.
   //
   // Registers (from the manual's QMOVE / PDEQ examples):
   //   ac0 = queue descriptor address
   //   ac1 = reference element; -1 selects the default position
   //         (ENQT: at the tail, ENQH: at the head, DEQUE: the head element)
   //   ac2 = element to enqueue (ENQH/ENQT only)
   //
   // Skip conventions are NOT stated in that chapter - both worked
   // examples pad with NOP. They are derived instead from LOCK_FILE
   // (0x70169B21 ENQT, 0x70169B55 DEQUE), which consumes them, and they
   // agree with the original Java implementation:
   //   ENQH/ENQT: skip iff the queue was NON-EMPTY before the enqueue
   //              (LOCK_FILE: "someone already holds the lock, go wait")
   //   DEQUE:     skip iff the queue is NON-EMPTY after the dequeue
   //              (LOCK_FILE: carry then says "queue now empty, nobody
   //              left to wake" - see the CRYTZ/DEQUE/CRYTO idiom)
   // The link ORDER and the -1 clearing below are the corrections versus
   // the original: forward/backward were transposed, and DEQUE never
   // reset the removed element's own links.
   // ---------------------------------------------------------------
   case ENQH: case ENQT: {
    int32_t head=machine.memory->read_wide(machine.ac[0]);
    int32_t tail=machine.memory->read_wide(machine.ac[0]+2);
    int32_t element=machine.ac[2];
    bool was_empty=(head==-1 && tail==-1);

    if(was_empty) {
      // Sole element: descriptor points at it both ways, both links -1.
      machine.memory->write_wide(machine.ac[0], element);
      machine.memory->write_wide(machine.ac[0]+2, element);
      machine.memory->write_wide(element, -1);
      machine.memory->write_wide(element+2, -1);
      return copy_segment(address, address+1);
    }

    // Resolve the reference element: -1 means the default end.
    int32_t reference=machine.ac[1];
    if(reference==-1)
      reference=(oper==ENQT) ? tail : head;

    if(oper==ENQT) {
      // Insert AFTER the reference, i.e. towards the tail.
      int32_t next=machine.memory->read_wide(reference);
      machine.memory->write_wide(element, next);
      machine.memory->write_wide(element+2, reference);
      machine.memory->write_wide(reference, element);
      if(next==-1)
        machine.memory->write_wide(machine.ac[0]+2, element);
      else
        machine.memory->write_wide(next+2, element);
    }
    else {
      // Insert BEFORE the reference, i.e. towards the head.
      int32_t prev=machine.memory->read_wide(reference+2);
      machine.memory->write_wide(element+2, prev);
      machine.memory->write_wide(element, reference);
      machine.memory->write_wide(reference+2, element);
      if(prev==-1)
        machine.memory->write_wide(machine.ac[0], element);
      else
        machine.memory->write_wide(prev, element);
    }
    return copy_segment(address, address+2);
   }

   case DEQUE: {
    int32_t element=machine.ac[1];
    if(element==-1)
      element=machine.memory->read_wide(machine.ac[0]);   // dequeue at head
    if(element==-1)
      return copy_segment(address, address+1);            // empty: nothing done
    machine.ac[1]=element;

    int32_t next=machine.memory->read_wide(element);      // forward link
    int32_t prev=machine.memory->read_wide(element+2);    // backward link

    if(prev==-1)
      machine.memory->write_wide(machine.ac[0], next);    // was the head
    else
      machine.memory->write_wide(prev, next);
    if(next==-1)
      machine.memory->write_wide(machine.ac[0]+2, prev);  // was the tail
    else
      machine.memory->write_wide(next+2, prev);

    // "Dequeing a data element sets both forward and backward links to
    // -1." Also makes a double dequeue leave the descriptor empty, as
    // the manual's NOTES describe.
    machine.memory->write_wide(element, -1);
    machine.memory->write_wide(element+2, -1);

    if(next==-1 && prev==-1)
      return copy_segment(address, address+1);            // queue now empty
    return copy_segment(address, address+2);
   }
  }
  throw std::runtime_error("Internal error - some case is not returning");
}

} // namespace hw
