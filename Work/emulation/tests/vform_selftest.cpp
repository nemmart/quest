// tests/vform_selftest.cpp — Project 46 (ir 7) self-test: a hand-written,
// NAIVE, UNPLACED IR program — declared `v`s with no address, symbolic
// blocks with no address — loads, is placed by the loader (every v at
// 0x76……, every block at 0x77……), and EXECUTES correctly through the real
// dispatch path (Machine::run_steps with lockstep_role = CLONE and
// IRExec::instance set — the Machine.cpp IR dispatch, not only run_block).
// Spec: docs/IR.md §5.10. Not part of the emulator build; see
// tests/run_vform_selftest.sh.
//
// Legs:
//  1. teeth — each malformed program REFUSES at load with the named message
//     (docs/IR.md §5.10.1/.2/.4/.5; the a001 R5 width tripwire)
//  2. placement — the name → address functions, 0x76 pages mapped RW/no
//     exec, NO 0x77 page mapped
//  3. execution — the program (every vtype; sx16/zx16/trunc16 conventions; an
//     effectful store to a v; a loop with a backward symbolic edge and a
//     forward reference; a three-way goto table; a cross-entry reference; a
//     varying v assigned from a longer literal (truncation), a fixed char v,
//     a cmp, a bp() byte read; `ret` from a symbolic block over a rig-built
//     WSAVS frame) — final v memory and the WRTN residues checked against
//     hand-computed expectations
//  4. faults — a goto index out of range is a loud executor FAULT
//
// NOT covered (docs/Project46/REPORT.md): the Mapper/checker's view of 0x76
// pointers at a rendezvous; ordinal counting for 0x77 arrivals; call /
// rt_call / @addr from a symbolic block (refused in ir 7); placement to 0x74
// (no placement input exists); the compiler's emission shape (P47);
// substitution (P48).
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <stdexcept>
#include <functional>
#include <memory>
#include <fstream>
#include "hw/Machine.hpp"
#include "hw/Memory.hpp"
#include "hw/IRExec.hpp"
#include "hw/Lockstep.hpp"
#include "hw/Decoder.hpp"
#include "hw/Permissions.hpp"
#include "os/ArrayPage.hpp"
using namespace hw;

static int fails = 0, cases = 0;
static void fail(const char* what, const std::string& detail) {
  if(fails++ < 60) std::printf("FAIL %s: %s\n", what, detail.c_str());
}
static void expect(bool ok, const char* what, const std::string& detail = "") { cases++; if(!ok) fail(what, detail); }
static std::string hex(uint32_t v) { char b[16]; snprintf(b, sizeof b, "%08X", v); return b; }

static const char* ADDRBOOK = "/tmp/vform_selftest.addrbook";
static const char* PROGRAM  = "/tmp/vform_selftest.ir";
static const char* SCRATCH  = "/tmp/vform_selftest_case.ir";

static void write_file(const char* path, const std::string& text) {
  std::ofstream f(path); f << text;
}

// A synthetic addrbook in the real format: header comments, a
// `borrow_slots` line, migrated entries, a `.N@ADDR` nested entry and a
// `#`-commented (unmigrated, nocall) one — all four are namespace entries
// (docs/IR.md §5.10.2): ALPHA=0, ALPHA.1=1, BETA.2=2, GAMMA=3.
static const char* ADDRBOOK_TEXT =
  "# quest.addrbook — SYNTHETIC (tests/vform_selftest.cpp); not the game's\n"
  "# base 74000000  total_words 64  pages 1  entries 4  live 3  borrow_slots 0\n"
  "# entry     name                          alloc_base wfp_base   argc frame  variant flags\n"
  "borrow_slots 0\n"
  "70100000    ALPHA                         74000000   7400000A   0    0x04   WSAVS   -\n"
  "70100100    ALPHA.1@70100100              74000020   7400002A   0    0x02   WSAVS   nested\n"
  "#70100200    BETA.2@70100200               74000040   7400004A   0    0x00   WSAVS   nested,nocall\n"
  "70100300    GAMMA                         74000060   7400006A   0    0x01   WSAVS   -\n";

// The program (docs/IR.md §5.10.7 is the short form of this shape).
static const char* PROGRAM_TEXT =
  "ir 7\n"
  "mode stock\n"
  "; declarations: every vtype, three entries incl. a nested and an unmigrated one\n"
  "v ALPHA.v0 i16          ; loop counter\n"
  "v ALPHA.v1 u32          ; accumulator\n"
  "v ALPHA.v2 varying 8    ; a varying string, capacity 8\n"
  "v ALPHA.v3 char 6       ; a fixed CHAR(6)\n"
  "v ALPHA.v4 words 4      ; an aggregate\n"
  "v ALPHA.v5 u16\n"
  "v ALPHA.1.v0 i32        ; the nested procedure's local\n"
  "v BETA.2.v0 i16         ; an unmigrated (#) entry's local\n"
  "v GAMMA.v0 u16\n"
  "\n"
  "block ALPHA.b0\n"
  "  M16[ALPHA.v0] = 5 ; counter = 5\n"
  "  M32[ALPHA.v1] = 0\n"
  "  M16[ALPHA.v5] = 0xFFFF\n"
  "  goto [ALPHA.b1] 0\n"
  "\n"
  "block ALPHA.b1 ; loop: v1 += counter; counter -= 1; until counter == 0\n"
  "  ac0 = sx16(M16[ALPHA.v0])\n"
  "  M32[ALPHA.v1] = add(M32[ALPHA.v1], ac0) ; effectful store to a v\n"
  "  t1 = ac0 - 1\n"
  "  M16[ALPHA.v0] = trunc16(t1)\n"
  "  goto [ALPHA.b2, ALPHA.b1] (t1 >s 0) ; backward edge; ALPHA.b2 is a forward reference\n"
  "\n"
  "block ALPHA.b2\n"
  "  ac1 = zx16(M16[ALPHA.v5]) ; 0x0000FFFF\n"
  "  ac2 = sx16(M16[ALPHA.v5]) ; 0xFFFFFFFF\n"
  "  M32[ALPHA.v4 + 2] = ac1 + ac2 ; words 2,3 of the aggregate := 0x0000FFFE (wraps)\n"
  "  M16[ALPHA.v4] = 7 ; words admits any width directly\n"
  "  ac0 = M32[ALPHA.v1] - 13 ; 15 - 13 = 2\n"
  "  goto [ALPHA.b3, ALPHA.b4, ALPHA.b5] ac0 ; three-way table\n"
  "\n"
  "block ALPHA.b3\n"
  "  M16[GAMMA.v0] = 0x3333\n"
  "  goto [ALPHA.b6] 0\n"
  "\n"
  "block ALPHA.b4\n"
  "  M16[GAMMA.v0] = 0x4444\n"
  "  goto ALPHA.b6 ; sugar\n"
  "\n"
  "block ALPHA.b5\n"
  "  M16[GAMMA.v0] = 0x5555 ; the expected arm\n"
  "  goto [ALPHA.1.b0] 0\n"
  "\n"
  "block ALPHA.1.b0 ; the nested procedure reads its parent's local (cross-entry)\n"
  "  ac3 = M32[ALPHA.v1] ; 15\n"
  "  M32[ALPHA.1.v0] = ac3 * 3 ; 45\n"
  "  M16[BETA.2.v0] = trunc16(0 - 7) ; 0xFFF9\n"
  "  goto [ALPHA.b6] 0\n"
  "\n"
  "block ALPHA.b6 ; strings\n"
  "  [@ALPHA.v2, 8 varying] = [@0x70100400:0, \"HELLO WORLD\"] ; length 8, HELLO WO\n"
  "  [@bp(ALPHA.v3, 0), 6] = [@0x70100400:0, \"HELLO WORLD\"] ; HELLO_ (truncated, c = 1)\n"
  "  ac1 = cmp([@ALPHA.v2, varying], [@bp(ALPHA.v3, 0), 6]) ; W > blank: +1\n"
  "  M16[ALPHA.v5] = trunc16(ac1)\n"
  "  ac0 = zx8(M8[bp(ALPHA.v3, 4)]) ; 'O' = 0x4F\n"
  "  M16[ALPHA.v4 + 1] = trunc16(ac0)\n"
  "  goto [ALPHA.b7] 0\n"
  "\n"
  "block ALPHA.b7\n"
  "  assert((M16[ALPHA.v5] == 1), \"cmp result\")\n"
  "  ret\n"
  "\n"
  "blocks 9\n";

struct Rig {
  Memory memory;
  Machine machine;
  std::vector<os::ArrayPage*> pages;
  static constexpr uint32_t STACK = 0x70000000u;    // 8 pages
  static constexpr uint32_t CODE  = 0x70100000u;    // the literal's page
  static constexpr int32_t  WFP = 0x70001000, SAVED_WFP = 0x70000F00;
  Rig() : machine(nullptr, nullptr, nullptr, &memory) {
    memory.process_name = "vform";
    map(STACK, 8); map(CODE, 2);
    machine.zero_claims = false;
    machine.lockstep_role = Lockstep::CLONE; machine.lockstep_ordinal = 0;
    machine.wsb = STACK; machine.wsl = STACK + 0x1F00;
    machine.wfp = WFP; machine.wsp = WFP + 0x40;
    // a WSAVS-shaped frame image at WFP: ret, saved wfp, ac2, ac1, ac0, frame word (argc 0)
    memory.write_wide(WFP, 0x70123456u);
    memory.write_wide(WFP - 2, static_cast<uint32_t>(SAVED_WFP));
    memory.write_wide(WFP - 4, 0x22222222u); memory.write_wide(WFP - 6, 0x11111111u); memory.write_wide(WFP - 8, 0u);
    memory.write_wide(WFP - 10, 0u);
    // the literal's bytes in the image at word 0x70100400
    const char* lit = "HELLO WORLD";
    for(size_t i = 0; i < strlen(lit); i++) memory.write_byte((0x70100400u << 1) + uint32_t(i), uint32_t(lit[i]));
  }
  void map(uint32_t word_base, uint32_t n) {
    for(uint32_t i = 0; i < n; i++) {
      os::ArrayPage* p = new os::ArrayPage();
      pages.push_back(p);
      memory.map_page(p, (word_base >> 10) + i, Permissions::PERMISSIONS_READ_WRITE_EXECUTE);
    }
  }
  std::string bytes(uint32_t word, uint32_t n) {
    std::string s;
    for(uint32_t i = 0; i < n; i++) s.push_back(char(memory.read_byte((word << 1) + i)));
    return s;
  }
};
#define RIG(name) std::unique_ptr<Rig> name##_p(new Rig()); Rig& name = *name##_p

static std::string load_throws(const std::string& text, const char* addrbook = ADDRBOOK) {
  write_file(SCRATCH, text);
  try { IRExec* ir = IRExec::load_file(SCRATCH, addrbook); delete ir; return ""; }
  catch(const std::exception& e) { return e.what(); }
}
static void refuses(const char* what, const std::string& text, const char* needle) {
  std::string got = load_throws(text);
  bool ok = !got.empty() && got.find(needle) != std::string::npos;
  expect(ok, what, got.empty() ? std::string("LOADED (wanted a refusal mentioning '") + needle + "')" : "got '" + got + "', wanted '" + needle + "'");
}

static const std::string HEAD = "ir 7\nmode stock\n";
static const std::string ONE_BLOCK_TAIL = "\nblock ALPHA.b0\n  ac0 = 1\n  ret\n\nblocks 1\n";

static int run() {
  Decoder::initialize();                 // WRTN's fixed opcode decodes through the real table
  write_file(ADDRBOOK, ADDRBOOK_TEXT);
  write_file(PROGRAM, PROGRAM_TEXT);

  // ---- 1. teeth ---------------------------------------------------------
  // first: no addrbook at all (the entry table is cached once loaded, so this must run first)
  unsetenv("QUEST_ADDRESS_BOOK");
  {
    std::string got = load_throws(HEAD + "v ALPHA.v0 i16\n" + ONE_BLOCK_TAIL, nullptr);
    expect(got.find("QUEST_ADDRESS_BOOK is not set") != std::string::npos, "teeth: no addrbook", got);
  }
  refuses("teeth: lowercase entry",       HEAD + "v alpha.v0 i16\n" + ONE_BLOCK_TAIL, "UPPERCASE");
  refuses("teeth: .x local",              HEAD + "v ALPHA.x0 i16\n" + ONE_BLOCK_TAIL, "malformed qualified name");
  refuses("teeth: no digits",             HEAD + "v ALPHA.v i16\n" + ONE_BLOCK_TAIL, "malformed qualified name");
  refuses("teeth: unknown entry",         HEAD + "v DELTA.v0 i16\n" + ONE_BLOCK_TAIL, "unknown addrbook entry");
  refuses("teeth: v before declaration",  HEAD + "\nblock ALPHA.b0\n  ac0 = M16[ALPHA.v9]\n  ret\n\nblocks 1\n", "before its declaration");
  refuses("teeth: duplicate v",           HEAD + "v ALPHA.v0 i16\nv ALPHA.v0 u16\n" + ONE_BLOCK_TAIL, "duplicate v declaration");
  refuses("teeth: duplicate block",       HEAD + "\nblock ALPHA.b0\n  ac0 = 1\n  ret\n\nblock ALPHA.b0\n  ac0 = 2\n  ret\n\nblocks 2\n", "duplicate symbolic block");
  refuses("teeth: undefined label",       HEAD + "\nblock ALPHA.b0\n  goto [ALPHA.b9] 0\n\nblocks 1\n", "no header defines");
  refuses("teeth: hex8 label in 0x77",    HEAD + "\nblock ALPHA.b0\n  goto [77000001] 0\n\nblocks 1\n", "hex8 goto label in the 0x76/0x77");
  refuses("teeth: literal 0x76 constant", HEAD + "\nblock ALPHA.b0\n  ac0 = M16[0x76000010]\n  ret\n\nblocks 1\n", "not authorable");
  refuses("teeth: literal 0x77 byte ptr", HEAD + "\nblock ALPHA.b0\n  ac0 = 0x77000010:0\n  ret\n\nblocks 1\n", "not authorable");
  refuses("teeth: @addr in symbolic",     HEAD + "\nblock ALPHA.b0\n  @70100000 WRTN\n\nblocks 1\n", "@addr instruction inside a symbolic block");
  refuses("teeth: rt_call in symbolic",   HEAD + "\nblock ALPHA.b0\n  rt_call ?WRITE_SCREEN(0x70000260) site=70100000\n\nblocks 1\n", "rt_call inside a symbolic block");
  refuses("teeth: call in symbolic",      HEAD + "\nblock ALPHA.b0\n  call 70100000 args=0 marker=70100000 site=70100000 ret=70100004\n\nblocks 1\n", "call inside a symbolic block");
  refuses("teeth: ir 6 header",           "ir 6\nmode stock\n" + ONE_BLOCK_TAIL, "want 'ir 7'");
  refuses("teeth: v inside a block",      HEAD + "\nblock ALPHA.b0\n  v ALPHA.v0 i16\n  ret\n\nblocks 1\n", "v declaration inside a block");
  refuses("teeth: width M32 on i16",      HEAD + "v ALPHA.v0 i16\n\nblock ALPHA.b0\n  ac0 = M32[ALPHA.v0]\n  ret\n\nblocks 1\n", "width tripwire");
  refuses("teeth: width M8 on u32",       HEAD + "v ALPHA.v0 u32\n\nblock ALPHA.b0\n  M8[ALPHA.v0] = 1\n  ret\n\nblocks 1\n", "width tripwire");
  refuses("teeth: width M16 on char",     HEAD + "v ALPHA.v0 char 4\n\nblock ALPHA.b0\n  ac0 = M16[ALPHA.v0]\n  ret\n\nblocks 1\n", "width tripwire");
  refuses("teeth: width M32 on varying",  HEAD + "v ALPHA.v0 varying 4\n\nblock ALPHA.b0\n  ac0 = M32[ALPHA.v0]\n  ret\n\nblocks 1\n", "width tripwire");
  refuses("teeth: fixed piece on raw v",  HEAD + "v ALPHA.v0 char 6\n\nblock ALPHA.b0\n  [@ALPHA.v0, 6] = [@0x70100400:0, \"HELLO\"]\n  ret\n\nblocks 1\n", "needs a BYTE pointer");
  refuses("teeth: varying capacity mismatch", HEAD + "v ALPHA.v0 varying 8\n\nblock ALPHA.b0\n  [@ALPHA.v0, 4 varying] = [@0x70100400:0, \"HELLO\"]\n  ret\n\nblocks 1\n", "declared varying 8");
  refuses("teeth: varying piece on i32",  HEAD + "v ALPHA.v0 i32\n\nblock ALPHA.b0\n  ac1 = cmp([@ALPHA.v0, varying], [@0x70100400:0, \"HELLO\"])\n  ret\n\nblocks 1\n", "not `varying`");
  refuses("teeth: seg on symbolic",       HEAD + "\nblock ALPHA.b0 seg 0x70000000\n  ac0 = 1\n  ret\n\nblocks 1\n", "takes no seg");
  refuses("teeth: char 0",                HEAD + "v ALPHA.v0 char 0\n" + ONE_BLOCK_TAIL, "1..32767");
  refuses("teeth: words 40000",           HEAD + "v ALPHA.v0 words 40000\n" + ONE_BLOCK_TAIL, "1..32767");
  refuses("teeth: bad type",              HEAD + "v ALPHA.v0 float\n" + ONE_BLOCK_TAIL, "unknown v type");
  refuses("teeth: block ordinal too big", HEAD + "\nblock ALPHA.b65536\n  ac0 = 1\n  ret\n\nblocks 1\n", "reserved range");
  refuses("teeth: trailer counts symbolic", HEAD + "\nblock ALPHA.b0\n  ac0 = 1\n  ret\n\nblocks 2\n", "trailer says");
  refuses("teeth: block name in expr",    HEAD + "\nblock ALPHA.b0\n  ac0 = ALPHA.b0\n  ret\n\nblocks 1\n", "block name where a v was expected");
  refuses("teeth: v range overflow",      HEAD + "v ALPHA.v0 words 32767\nv ALPHA.v1 words 32767\nv ALPHA.v2 words 3\n" + ONE_BLOCK_TAIL, "overflows its reserved 0x76 range");
  refuses("teeth: numeric block needs blocks provenance", HEAD + "\nblock 70100000 seg 0x70000000\n  ac0 = 1\n  ret\n\nblocks 1\n", "not a listed quest.blocks start");

  // ---- 2. placement -----------------------------------------------------
  IRExec* ir = nullptr;
  try { ir = IRExec::load_file(PROGRAM, ADDRBOOK); }
  catch(const std::exception& e) {
    // the -DP46_BROKEN_ALLOC build (run_vform_selftest.sh teeth) must land here:
    // the disjointness assertion, unreachable in ir 7 by construction, fires
    // when the allocator is broken on purpose
    fail("load program", e.what()); return 1;
  }
  expect(ir->v_count() == 9, "9 v's", std::to_string(ir->v_count()));
  expect(ir->symbolic_block_count() == 9, "9 symbolic blocks", std::to_string(ir->symbolic_block_count()));
  struct { const char* name; uint32_t addr; } vs[] = {
    {"ALPHA.v0", 0x76000000u}, {"ALPHA.v1", 0x76000001u}, {"ALPHA.v2", 0x76000003u}, {"ALPHA.v3", 0x76000008u},
    {"ALPHA.v4", 0x7600000Bu}, {"ALPHA.v5", 0x7600000Fu}, {"ALPHA.1.v0", 0x76010000u}, {"BETA.2.v0", 0x76020000u},
    {"GAMMA.v0", 0x76030000u}};
  for(auto& v : vs) expect(ir->v_address(v.name) == v.addr, "v placement", std::string(v.name) + " at " + hex(ir->v_address(v.name)) + " want " + hex(v.addr));
  struct { const char* name; uint32_t addr; } bs[] = {
    {"ALPHA.b0", 0x77000000u}, {"ALPHA.b1", 0x77000001u}, {"ALPHA.b2", 0x77000002u}, {"ALPHA.b7", 0x77000007u},
    {"ALPHA.1.b0", 0x77010000u}};
  for(auto& b : bs) expect(ir->block_address(b.name) == b.addr, "block placement", std::string(b.name) + " at " + hex(ir->block_address(b.name)) + " want " + hex(b.addr));
  expect(ir->v_address("ALPHA.v9") == 0 && ir->block_address("GAMMA.b0") == 0, "unknown names query 0");
  for(auto& b : bs) expect(ir->has(b.addr), "has(0x77 block)", b.name);

  RIG(r);
  ir->map_pages(r.memory);
  auto perm = [&](uint32_t word) { return r.memory.permissions[(word >> 10) & 0x1FFFFF]; };
  for(auto& v : vs) {
    uint8_t p = perm(v.addr);
    expect((p & Permissions::PERMISSION_READ) && (p & Permissions::PERMISSION_WRITE) && !(p & Permissions::PERMISSION_EXECUTE),
           "0x76 page RW no-exec", std::string(v.name) + " perm " + std::to_string(p));
  }
  expect(perm(0x77000000u) == 0 && perm(0x77010000u) == 0, "no 0x77 page mapped");
  expect(perm(0x76040000u) == 0, "untouched 0x76 range not mapped");

  // ---- 3. execution through Machine::run_steps (the real dispatch) --------
  IRExec::instance = ir;
  std::string ended;
  try { r.machine.run_steps(0x77000000u, 200); ended = "returned"; }
  catch(const std::exception& e) { ended = e.what(); }
  // `ret` runs WRTN; in this rig the shadow call stack is empty, so the arm
  // throws AFTER the frame pop (EagleStack.cpp WRTN: pops, fixup, then
  // call_return) — that throw is the expected end of the program.
  expect(ended == "Empty call stack", "program ends at the ret's WRTN", ended);
  Memory& M = r.memory;
  expect(M.read_word(0x76000000u) == 0, "v0 counter ran to 0", hex(M.read_word(0x76000000u)));
  expect(M.read_wide(0x76000001u) == 15, "v1 = 5+4+3+2+1", hex(M.read_wide(0x76000001u)));
  expect(M.read_word(0x76000003u) == 8, "v2 length = min(11, 8)", hex(M.read_word(0x76000003u)));
  expect(r.bytes(0x76000004u, 8) == "HELLO WO", "v2 data truncated", r.bytes(0x76000004u, 8));
  expect(r.bytes(0x76000008u, 6) == "HELLO ", "v3 fixed char 6", r.bytes(0x76000008u, 6));
  expect(M.read_word(0x7600000Bu) == 7, "v4 word 0", hex(M.read_word(0x7600000Bu)));
  expect(M.read_word(0x7600000Cu) == 0x4F, "v4 word 1 = 'O'", hex(M.read_word(0x7600000Cu)));
  expect(M.read_wide(0x7600000Du) == 0x0000FFFEu, "v4 words 2,3 = 0xFFFF + 0xFFFFFFFF", hex(M.read_wide(0x7600000Du)));
  expect(M.read_word(0x7600000Fu) == 1, "v5 = cmp result +1", hex(M.read_word(0x7600000Fu)));
  expect(M.read_wide(0x76010000u) == 45, "ALPHA.1.v0 = 15*3 (cross-entry)", hex(M.read_wide(0x76010000u)));
  expect(M.read_word(0x76020000u) == 0xFFF9, "BETA.2.v0 = trunc16(-7)", hex(M.read_word(0x76020000u)));
  expect(M.read_word(0x76030000u) == 0x5555, "GAMMA.v0: table arm 2 taken", hex(M.read_word(0x76030000u)));
  // WRTN residues: the frame popped, ac0..ac2 from the image, ac3 = restored wfp, c = bit 31 of the return
  expect(r.machine.wfp == Rig::SAVED_WFP, "ret: wfp restored", hex(uint32_t(r.machine.wfp)));
  expect(uint32_t(r.machine.ac[0]) == 0 && uint32_t(r.machine.ac[1]) == 0x11111111u && uint32_t(r.machine.ac[2]) == 0x22222222u,
         "ret: ac0..ac2 from the frame image");
  expect(r.machine.ac[3] == Rig::SAVED_WFP, "ret: ac3 = wfp");
  expect(r.machine.c == 0, "ret: c from the return word");

  // ---- 4. a goto index out of range is a loud FAULT -----------------------
  {
    write_file(SCRATCH, HEAD + "\nblock ALPHA.b0\n  ac0 = 3\n  goto [ALPHA.b1, ALPHA.b2] ac0\n\nblock ALPHA.b1\n  ret\n\nblock ALPHA.b2\n  ret\n\nblocks 3\n");
    IRExec* bad = IRExec::load_file(SCRATCH, ADDRBOOK);
    IRExec::instance = bad;
    RIG(q);
    std::string got;
    try { q.machine.run_steps(0x77000000u, 10); got = "returned"; }
    catch(const std::exception& e) { got = e.what(); }
    expect(got.find("FAULT goto index 3 out of range") != std::string::npos, "goto index fault", got);
    IRExec::instance = nullptr;
    delete bad;
  }
  delete ir;
  return 0;
}

int main() {
  try { run(); }
  catch(const std::exception& e) { fail("unexpected exception", e.what()); }
  std::printf("VFORM SELFTEST %s (%d cases, %d failures)\n", fails ? "RED" : "GREEN", cases, fails);
  return fails ? 1 : 0;
}
