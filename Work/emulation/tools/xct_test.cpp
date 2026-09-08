// Unit test for XCT (Execute Accumulator), checking the three
// continuation rules from the DG description:
//
//   * first location after XCT        (one-word instruction executed)
//   * second location after XCT       (accumulator held word 1 of two)
//   * the effective address           (jump or skip)
//
// Also checks that a multiword instruction fetches its extra words from
// the words immediately FOLLOWING the XCT, and that the real Quest use
// (a constructed ENQT opcode with a skip) lands correctly.
//
// Build:
//   g++ -std=c++17 -I. -o /tmp/xcttest tools/xct_test.cpp \
//       hw/Decoder.o hw/Instruction.o hw/EagleGeneral.o ... (see below)
// Simpler: this test drives the emulator's own Decoder + Machine, so
// link it against the object files produced by `make`:
//   make && g++ -std=c++17 -I. -o /tmp/xcttest tools/xct_test.cpp \
//       $(ls hw/*.o os/*.o debug/*.o runtime/*.o | grep -v Launch) -lpthread
#include "../hw/Machine.hpp"
#include "../hw/Decoder.hpp"
#include "../hw/Memory.hpp"
#include "../hw/EagleGeneral.hpp"
#include "../hw/Permissions.hpp"
#include "../os/ArrayPage.hpp"
#include <cstdio>
#include <cstdint>

static int failures=0;

// A Memory with one writable/executable page mapped at the test address.
static hw::Memory* fresh_memory(uint32_t at) {
  hw::Memory* mem=new hw::Memory();
  uint32_t page=(at*2)>>11;              // byte address -> 2KB page
  for(uint32_t p=page; p<=page+1; p++)
    mem->map_page(new os::ArrayPage(), p,
                  hw::Permissions::PERMISSIONS_READ_WRITE_EXECUTE);
  return mem;
}

static void check(const char* what, uint32_t got, uint32_t want) {
  if(got!=want) { printf("  FAIL %-34s got %08X want %08X\n", what, got, want); failures++; }
  else          printf("  ok   %-34s %08X\n", what, got);
}
static void check_i(const char* what, int32_t got, int32_t want) {
  if(got!=want) { printf("  FAIL %-34s got %08X want %08X\n", what, got, want); failures++; }
  else          printf("  ok   %-34s %08X\n", what, got);
}

// Build an opcode from a Decoder-style pattern with xx/yy fields filled.
static uint32_t build(const char* pat, uint32_t xx, uint32_t yy) {
  uint32_t v=0; int xi=0, yi=0;
  for(const char* p=pat; *p; ++p) if(*p=='x') xi++; else if(*p=='y') yi++;
  int xseen=0, yseen=0;
  for(const char* p=pat; *p; ++p) {
    uint32_t bit=0;
    if(*p=='1') bit=1;
    else if(*p=='x') { bit=(xx>>(xi-1-xseen))&1; xseen++; }
    else if(*p=='y') { bit=(yy>>(yi-1-yseen))&1; yseen++; }
    v=(v<<1)|bit;
  }
  return v;
}

int main() {
  hw::Decoder::initialize();

  // XCT with accumulator field yy
  auto XCT=[&](uint32_t ac){ return build("101yy11011111000", 0, ac); };
  // WMOV xx,yy  — one word, no continuation change
  auto WMOV=[&](uint32_t s, uint32_t d){ return build("1xxyy01101111001", s, d); };
  // WLDAI yy,<wide> — three words (opcode + 2 immediate)
  auto WLDAI=[&](uint32_t d){ return build("110yy11010001001", 0, d); };
  // WSEQ xx,yy — one word, SKIPS when equal
  auto WSEQ=[&](uint32_t a, uint32_t b){ return build("1xxyy00010111001", a, b); };

  printf("XCT opcode (ac1) = %04X\n", XCT(1));
  printf("WMOV 2,0 = %04X   WSEQ 0,0 = %04X\n\n", WMOV(2,0), WSEQ(0,0));

  // ---- case 1: one-word instruction -> pc = address+1 ----
  printf("Case 1: one-word instruction (WMOV 2,0) via XCT\n");
  {
    uint32_t at=0x00001000;
    hw::Machine m(nullptr, nullptr, nullptr, fresh_memory(at));
    m.ac[1]=static_cast<int32_t>(WMOV(2,0));   // instruction to execute
    m.ac[2]=0x12345678;
    m.ac[0]=0;
    hw::Instruction* xct=hw::Decoder::decode(false, XCT(1));
    uint32_t next=xct->execute(m, at, XCT(1));
    check("pc after one-word XCT", next, at+1);
    check_i("WMOV executed (ac0 = ac2)", m.ac[0], 0x12345678);
  }

  // ---- case 2: two/three-word instruction -> extra words follow XCT ----
  printf("\nCase 2: multiword instruction (WLDAI) via XCT\n");
  {
    uint32_t at=0x00001000;
    hw::Memory* mem=fresh_memory(at);
    hw::Machine m(nullptr, nullptr, nullptr, mem);
    // The immediate lives in the words immediately FOLLOWING the XCT.
    mem->write_wide(at+1, 0x0BADF00D);
    m.ac[1]=static_cast<int32_t>(WLDAI(3));
    m.ac[3]=0;
    hw::Instruction* xct=hw::Decoder::decode(false, XCT(1));
    uint32_t next=xct->execute(m, at, XCT(1));
    check("pc after 3-word XCT", next, at+3);
    check_i("immediate fetched after XCT", m.ac[3], 0x0BADF00D);
  }

  // ---- case 3: skip instruction -> effective address ----
  printf("\nCase 3: skip instruction (WSEQ) via XCT\n");
  {
    uint32_t at=0x00001000;
    hw::Machine m(nullptr, nullptr, nullptr, fresh_memory(at));
    m.ac[1]=static_cast<int32_t>(WSEQ(0,2));
    m.ac[0]=5; m.ac[2]=5;                     // equal -> skip
    hw::Instruction* xct=hw::Decoder::decode(false, XCT(1));
    check("pc when skip taken", xct->execute(m, at, XCT(1)), at+2);
    m.ac[2]=6;                                // unequal -> no skip
    check("pc when skip not taken", xct->execute(m, at, XCT(1)), at+1);
  }

  printf("\n%s (%d failures)\n", failures?"FAILED":"ALL PASS", failures);
  return failures!=0;
}
