// Unit test for the queue instructions, transcribed directly from the
// DG queue-management chapter's worked figures (6-2 .. 6-6).
//
// Build:  g++ -std=c++17 -I. -o /tmp/qtest /tmp/queue_test.cpp
// (self-contained: replicates the ENQH/ENQT/DEQUE logic against a plain
// memory map, so it can run without linking the whole emulator.)
#include <cstdio>
#include <cstdint>
#include <map>
#include <string>

static std::map<uint32_t,int32_t> mem;
static int32_t rd(uint32_t a){ auto i=mem.find(a); return i==mem.end()?0:i->second; }
static void wr(uint32_t a,int32_t v){ mem[a]=v; }

static const uint32_t DESC=0x1000, A=0x2000, B=0x3000, C=0x4000;

// --- the implementation under test (mirrors hw/EagleSpecial.cpp) ---
enum { ENQH, ENQT };
static bool enq(int op,int32_t desc,int32_t ref,int32_t element){
  int32_t head=rd(desc), tail=rd(desc+2);
  if(head==-1 && tail==-1){
    wr(desc,element); wr(desc+2,element); wr(element,-1); wr(element+2,-1);
    return false;                     // no skip: queue was empty
  }
  int32_t reference = (ref==-1) ? ((op==ENQT)?tail:head) : ref;
  if(op==ENQT){
    int32_t next=rd(reference);
    wr(element,next); wr(element+2,reference); wr(reference,element);
    if(next==-1) wr(desc+2,element); else wr(next+2,element);
  } else {
    int32_t prev=rd(reference+2);
    wr(element+2,prev); wr(element,reference); wr(reference+2,element);
    if(prev==-1) wr(desc,element); else wr(prev,element);
  }
  return true;                        // skip: queue was non-empty
}
static bool deq(int32_t desc,int32_t elem){
  int32_t element = (elem==-1) ? rd(desc) : elem;
  if(element==-1) return false;
  int32_t next=rd(element), prev=rd(element+2);
  if(prev==-1) wr(desc,next); else wr(prev,next);
  if(next==-1) wr(desc+2,prev); else wr(next+2,prev);
  wr(element,-1); wr(element+2,-1);
  return !(next==-1 && prev==-1);
}

// --- checking ---
static int failures=0;
static void check(const char* what,int32_t got,int32_t want){
  if(got!=want){ printf("  FAIL %-28s got %08X want %08X\n",what,got,want); failures++; }
  else          printf("  ok   %-28s %08X\n",what,got);
}
static void empty_queue(){ mem.clear(); wr(DESC,-1); wr(DESC+2,-1); }

int main(){
  printf("Figure 6-2/6-3: enqueue A into an empty queue\n");
  empty_queue();
  bool skip=enq(ENQT,DESC,-1,A);
  check("desc.head",rd(DESC),A);
  check("desc.tail",rd(DESC+2),A);
  check("A.forward",rd(A),-1);
  check("A.backward",rd(A+2),-1);
  check("skip (queue was empty)",skip,0);

  printf("\nFigure 6-4: enqueue B at the HEAD, before A\n");
  skip=enq(ENQH,DESC,-1,B);
  check("desc.head",rd(DESC),B);
  check("desc.tail",rd(DESC+2),A);
  check("B.forward (-> A)",rd(B),A);
  check("B.backward (head)",rd(B+2),-1);
  check("A.forward (tail)",rd(A),-1);
  check("A.backward (-> B)",rd(A+2),B);
  check("skip (queue non-empty)",skip,1);

  printf("\nFigure 6-5: enqueue C at the TAIL, after A\n");
  skip=enq(ENQT,DESC,-1,C);
  check("desc.head",rd(DESC),B);
  check("desc.tail",rd(DESC+2),C);
  check("B.forward (-> A)",rd(B),A);
  check("B.backward (head)",rd(B+2),-1);
  check("A.forward (-> C)",rd(A),C);
  check("A.backward (-> B)",rd(A+2),B);
  check("C.forward (tail)",rd(C),-1);
  check("C.backward (-> A)",rd(C+2),A);
  check("skip (queue non-empty)",skip,1);

  printf("\nFigure 6-6: dequeue B (the head); A becomes head, C unchanged\n");
  skip=deq(DESC,B);
  check("desc.head",rd(DESC),A);
  check("desc.tail",rd(DESC+2),C);
  check("A.forward (-> C)",rd(A),C);
  check("A.backward (head)",rd(A+2),-1);
  check("C.forward (tail)",rd(C),-1);
  check("C.backward (-> A)",rd(C+2),A);
  check("B.forward cleared",rd(B),-1);
  check("B.backward cleared",rd(B+2),-1);
  check("skip (others remain)",skip,1);

  printf("\nNOTES: dequeuing the same element twice empties the descriptor\n");
  empty_queue();
  enq(ENQT,DESC,-1,A);
  deq(DESC,A);
  check("desc empty after 1st",rd(DESC),-1);
  deq(DESC,A);                       // second dequeue of the same element
  check("desc.head still -1",rd(DESC),-1);
  check("desc.tail still -1",rd(DESC+2),-1);

  printf("\nDEQUE of the only element does not skip\n");
  empty_queue();
  enq(ENQT,DESC,-1,A);
  check("skip (queue now empty)",deq(DESC,A),0);

  printf("\nDEQUE with ac1 = -1 removes the head\n");
  empty_queue();
  enq(ENQT,DESC,-1,A); enq(ENQT,DESC,-1,B);
  deq(DESC,-1);
  check("desc.head is B",rd(DESC),B);
  check("A cleared",rd(A),-1);

  printf("\n%s (%d failures)\n", failures?"FAILED":"ALL PASS", failures);
  return failures!=0;
}
