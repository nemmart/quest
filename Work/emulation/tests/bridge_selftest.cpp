// tests/bridge_selftest.cpp — Project 54 (THE CALLING BRIDGE) self-test:
// calls OUT OF a symbolic block execute, and control returns INTO a 0x77
// block. Spec: docs/IR.md §5.10.6, §6; design: docs/Project54/q001-plan-gate.md
// (a001 granted all seven rulings). Model: tests/vform_selftest.cpp.
//
// Legs (§4 of the gate):
//  1. teeth (load)  — a naive call whose callee has no b0 REFUSES (R1)
//  2. teeth (run)   — a symbolic rt_call with no symbol table / an unknown
//                     symbol THROWS naming the callee
//  3. rt_call -> NATIVE  (a test native on RTBridge): arguments on the real
//                     stack in the right order, write-back through arg 3,
//                     result in ac0, the continuation runs, stack balanced
//  4. rt_call -> EMULATED (a hand-assembled leaf: WSAVS; NLDAI; XWSTA
//                     0,[ac3-8]; WRTN): WRTN into a 0x77 label through a REAL
//                     WSAVS frame, callee-pops argc, ac0 = the F4 slotpatch —
//                     the path ?RANDOM_NUMBER takes in the game
//  5. game->game, void — a cells + arg_count, the callee writes through the
//                     caller's pointer; ovk from the addrbook variant (WSAVS
//                     entry -> 1, WSAVR entry -> 0), observed by a probe native
//  6. game->game, valued — the callee writes <CALLEE>.ret; the caller reads it;
//                     ac0 after the call is the CALLER's, not the result
//  7. nested        — caller -> game callee -> rt_call native -> back -> ret
//  8. PICK_X_Y end to end with a REPOSITION-shaped caller and a host oracle:
//                     identity-green (caller's cells, seed, call count agree),
//                     identity-red (a) an extra ?RANDOM_NUMBER call -> FAIL,
//                     identity-red (b) the *x write-back off by one -> FAIL
//  9. (script) -DP54_BROKEN_BRIDGE must go RED on the stack-balance check
//  1b/1c (P54 reopening, docs/Project54/a003): the argument-KIND check at a
//     naive call, and a game->game call on a symbol-less Machine executing
//
// Every program's final wsp is compared with a baseline program's — WRTN
// pops a fixed six wides, so any bridge that leaves or takes stack shows
// up as a wsp delta, whichever leg it hides in.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <stdexcept>
#include <memory>
#include <fstream>
#include "hw/Machine.hpp"
#include "hw/Memory.hpp"
#include "hw/IRExec.hpp"
#include "hw/Lockstep.hpp"
#include "hw/Decoder.hpp"
#include "hw/Permissions.hpp"
#include "hw/RTBridge.hpp"
#include "hw/NativeRegistry.hpp"
#include "os/ArrayPage.hpp"
#include "debug/SymbolTable.hpp"
using namespace hw;

static int fails = 0, cases = 0;
static void fail(const char* what, const std::string& detail) {
  if(fails++ < 60) std::printf("FAIL %s: %s\n", what, detail.c_str());
}
static void expect(bool ok, const char* what, const std::string& detail = "") { cases++; if(!ok) fail(what, detail); }
static std::string hex(uint32_t v) { char b[16]; snprintf(b, sizeof b, "%08X", v); return b; }

static const char* ADDRBOOK = "/tmp/bridge_selftest.addrbook";
static const char* SCRATCH  = "/tmp/bridge_selftest_case.ir";
static void write_file(const char* path, const std::string& text) { std::ofstream f(path); f << text; }

// Synthetic addrbook (real format). DELTA is the one WSAVR entry so the
// variant column is exercised both ways. PICK_X_Y / REPOSITION carry the
// game's own names so the leg-8 program reads like P53's output would.
static const char* ADDRBOOK_TEXT =
  "# quest.addrbook — SYNTHETIC (tests/bridge_selftest.cpp); not the game's\n"
  "# base 74000000  total_words 96  pages 1  entries 6  live 6  borrow_slots 0\n"
  "# entry     name                          alloc_base wfp_base   argc frame  variant flags\n"
  "borrow_slots 0\n"
  "70100000    ALPHA                         74000000   7400000A   0    0x04   WSAVS   -\n"
  "70100100    GAMMA                         74000010   7400001A   0    0x01   WSAVS   -\n"
  "70100200    DELTA                         74000020   7400002A   0    0x01   WSAVR   -\n"
  "70100300    BETA                          74000030   7400003A   1    0x02   WSAVS   -\n"
  "701761E7    PICK_X_Y                      74000040   7400004A   2    0x05   WSAVS   -\n"
  "70176FC1    REPOSITION                    74000050   7400005A   1    0x01   WSAVS   -\n";

// ---- the test runtime ------------------------------------------------------
// Symbols: four `?` runtime routines in the rig's code page. TEST_ADD,
// OVK_PROBE and RANDOM_NUMBER are natives on RTBridge (the RTStubs
// translation shape); TEST_LEAF is NOT registered, so the bridge transfers
// to its entry and the EMULATOR runs the hand-assembled words there.
static constexpr uint32_t TEST_ADD_PC = 0x70100600u, TEST_LEAF_PC = 0x70100610u,
                          OVK_PROBE_PC = 0x70100620u, RANDOM_PC = 0x70100630u;
static int  g_native_calls = 0;      // TEST_ADD invocations
static int  g_probe_ovk = -1;        // machine.ovk seen by OVK_PROBE
static int  g_random_calls = 0;      // ?RANDOM_NUMBER invocations
static std::string g_native_note;    // TEST_ADD's own checks (empty = all held)

static uint32_t test_add(Machine& m) {
  RTBridge b(m);
  g_native_calls++;
  // the pushed VALUES are what arg_pointer(n) returns: e1 at wsp-2, eN at wsp-2N
  if (b.arg_count() != 3) g_native_note += "argc " + std::to_string(b.arg_count()) + "; ";
  if (b.arg_pointer(1) != 11u) g_native_note += "arg1 " + hex(b.arg_pointer(1)) + "; ";
  if (b.arg_pointer(2) != 22u) g_native_note += "arg2 " + hex(b.arg_pointer(2)) + "; ";
  if ((b.arg_pointer(3) >> 24) != 0x76u) g_native_note += "arg3 not a 0x76 cell " + hex(b.arg_pointer(3)) + "; ";
  if (b.entry_ac(1) != 0x1111 || b.entry_ac(2) != 0x2222) g_native_note += "entry ac1/ac2; ";
  b.set_arg_wide(3, 99);                                   // write THROUGH the pointer argument
  b.set_return_ac(0, 42);                                  // the result, restored into ac0 by native_return
  return b.native_return();                                // -> return_addr = the ac3 the bridge set
}
static uint32_t ovk_probe(Machine& m) {
  RTBridge b(m);
  g_probe_ovk = m.ovk;
  return b.native_return();
}
// ?RANDOM_NUMBER, the LCG of rt/random_number.cpp:9-19 verbatim, on the seed
// word the third argument points at (a001 R7: the seed comparison is still
// real — same memory word, same recurrence, advanced once per call).
static constexpr double LCG_MULT = 31415933.0, LCG_INC = 14181771.0, LCG_MOD = 67108864.0;
static int32_t lcg_draw(double& seed, int32_t lower, int32_t upper) {
  seed = std::fmod(seed * LCG_MULT + LCG_INC, LCG_MOD);
  double range = static_cast<double>(upper - lower + 1);
  return lower + static_cast<int32_t>(seed * range / LCG_MOD);
}
static uint32_t random_number(Machine& m) {
  RTBridge b(m);
  g_random_calls++;
  int32_t lower = b.arg_wide(1), upper = b.arg_wide(2);
  if (getenv("BRIDGE_DEBUG")) fprintf(stderr, "RANDOM_NUMBER(%d,%d) seed %08X ret %08X\n", lower, upper, m.memory->read_wide(b.arg_pointer(3)), uint32_t(m.ac[3]));
  double seed = static_cast<double>(m.memory->read_wide(b.arg_pointer(3)));
  int32_t r = lcg_draw(seed, lower, upper);
  m.memory->write_wide(b.arg_pointer(3), static_cast<uint32_t>(static_cast<int32_t>(seed)));
  b.set_return_ac(0, r);
  return b.native_return();
}

struct Rig {
  Memory memory;
  debug::SymbolTable symbols;
  NativeRegistry registry;
  Machine machine;
  std::vector<os::ArrayPage*> pages;
  static constexpr uint32_t STACK = 0x70000000u;    // 16 pages (also SD/OBJ scratch for leg 8)
  static constexpr uint32_t CODE  = 0x70100000u;    // 2 pages: the test runtime lives here
  static constexpr int32_t  WFP = 0x70001000, SAVED_WFP = 0x70000F00;
  Rig(bool with_symbols = true, bool with_registry = true)
    : machine(nullptr, nullptr, with_symbols ? &symbols : nullptr, &memory) {
    memory.process_name = "bridge";
    map(STACK, 16); map(CODE, 2);
    machine.zero_claims = false;
    machine.lockstep_role = Lockstep::CLONE; machine.lockstep_ordinal = 0;
    machine.wsb = STACK; machine.wsl = STACK + 0x3F00;
    machine.wfp = WFP; machine.wsp = WFP + 0x40;
    // a WSAVS-shaped frame image at WFP: ret, saved wfp, ac2, ac1, ac0, frame word (argc 0)
    memory.write_wide(WFP, 0x70123456u);
    memory.write_wide(WFP - 2, static_cast<uint32_t>(SAVED_WFP));
    memory.write_wide(WFP - 4, 0x22222222u); memory.write_wide(WFP - 6, 0x11111111u); memory.write_wide(WFP - 8, 0u);
    memory.write_wide(WFP - 10, 0u);
    symbols.add_symbol("?TEST_ADD", TEST_ADD_PC);
    symbols.add_symbol("?TEST_LEAF", TEST_LEAF_PC);
    symbols.add_symbol("?OVK_PROBE", OVK_PROBE_PC);
    symbols.add_symbol("?RANDOM_NUMBER", RANDOM_PC);
    if (with_registry) {
      registry.register_by_address(TEST_ADD_PC, test_add);
      registry.register_by_address(OVK_PROBE_PC, ovk_probe);
      registry.register_by_address(RANDOM_PC, random_number);
    }
    // ?TEST_LEAF, hand-assembled (hw/Decoder.cpp patterns):
    //   WSAVS 0x0000        A739 0000   1010011100111001, frame word
    //   NLDAI 77,0          C629 004D   110yy11000101001 (yy=00), immediate
    //   XWSTA 0,[ac3+0x7FF8] E319 7FF8  1iiyy01100011001 (ii=11 ac3, yy=00); disp -8 = the saved-ac0 image (Salvage F4)
    //   WRTN                87A9        1000011110101001
    const uint16_t leaf[] = { 0xA739, 0x0000, 0xC629, 0x004D, 0xE319, 0x7FF8, 0x87A9 };
    for (size_t i = 0; i < sizeof leaf / sizeof leaf[0]; i++) memory.write_word(TEST_LEAF_PC + uint32_t(i), leaf[i]);
  }
  void map(uint32_t word_base, uint32_t n) {
    for(uint32_t i = 0; i < n; i++) {
      os::ArrayPage* p = new os::ArrayPage();
      pages.push_back(p);
      memory.map_page(p, (word_base >> 10) + i, Permissions::PERMISSIONS_READ_WRITE_EXECUTE);
    }
  }
  // Drive the program the way the emulator does: each native return ends
  // the batch at the 0x77 continuation (Machine::run native_break), so the
  // next run_steps re-enters there. Returns the exception text that ended
  // the run ("Empty call stack" is the top-level ret's WRTN, the normal end).
  std::string drive(uint32_t start, int max_batches = 500) {
    uint32_t pc = start;
    for (int i = 0; i < max_batches; i++) {
      try { pc = machine.run_steps(pc, 200); }
      catch (const std::exception& e) { return e.what(); }
    }
    return "batch limit";
  }
};
#define RIG(name, ...) std::unique_ptr<Rig> name##_p(new Rig(__VA_ARGS__)); Rig& name = *name##_p

static std::string load_throws(const std::string& text) {
  write_file(SCRATCH, text);
  try { IRExec* ir = IRExec::load_file(SCRATCH, ADDRBOOK); delete ir; return ""; }
  catch(const std::exception& e) { return e.what(); }
}
static void refuses(const char* what, const std::string& text, const char* needle) {
  std::string got = load_throws(text);
  bool ok = !got.empty() && got.find(needle) != std::string::npos;
  expect(ok, what, got.empty() ? std::string("LOADED (wanted a refusal mentioning '") + needle + "')" : "got '" + got + "', wanted '" + needle + "'");
}
static IRExec* load_ok(const char* what, const std::string& text) {
  write_file(SCRATCH, text);
  try { return IRExec::load_file(SCRATCH, ADDRBOOK); }
  catch(const std::exception& e) { fail(what, e.what()); return nullptr; }
}

static const std::string HEAD = "ir 8\nmode stock\n";

// ---- programs ----------------------------------------------------------------
// baseline: one ret. Every other program must leave wsp where this one does.
static const std::string BASELINE = HEAD + "\nblock ALPHA.b0\n  ret\n\nblocks 1\n";

// leg 3: rt_call -> native
static const std::string RT_NATIVE = HEAD +
  "v ALPHA.v0 u32          ; what ?TEST_ADD writes through arg 3\n"
  "v ALPHA.v1 u32          ; the result, from ac0\n"
  "v ALPHA.v2 u32          ; ac1 after the call\n"
  "v ALPHA.v3 u32          ; ac2 after the call\n"
  "v ALPHA.v4 u32          ; ac3 after the call\n"
    "v ALPHA.v20 u32          ; wsp before the call\n"
  "v ALPHA.v21 u32          ; wsp in the continuation\n"
"\n"
  "block ALPHA.b0\n"
  "  ac1 = 0x1111\n"
  "  ac2 = 0x2222\n"
  "  ALPHA.v20 = wsp\n"
  "  rt_call ?TEST_ADD(11, 22, wp(ALPHA.v0, 0)) ret=ALPHA.b1\n"
  "\n"
  "block ALPHA.b1 ; the valued-call split: the continuation opens with ac0\n"
  "  ALPHA.v1 = ac0\n"
  "  ALPHA.v21 = wsp\n"
  "  ALPHA.v2 = ac1\n"
  "  ALPHA.v3 = ac2\n"
  "  ALPHA.v4 = ac3\n"
  "  ret\n"
  "\n"
  "blocks 2\n";

// leg 4: rt_call -> emulated leaf (WSAVS ... WRTN)
static const std::string RT_EMULATED = HEAD +
  "v ALPHA.v0 u32\n"
  "v ALPHA.v1 u32\n"
  "v ALPHA.v2 u32\n"
    "v ALPHA.v20 u32          ; wsp before the call\n"
  "v ALPHA.v21 u32          ; wsp in the continuation\n"
"\n"
  "block ALPHA.b0\n"
  "  ac1 = 0x3333\n"
  "  ALPHA.v20 = wsp\n"
  "  rt_call ?TEST_LEAF(5, 6) ret=ALPHA.b1\n"
  "\n"
  "block ALPHA.b1\n"
  "  ALPHA.v0 = ac0 ; 77, via the slotpatch WRTN restored\n"
  "  ALPHA.v21 = wsp\n"
  "  ALPHA.v1 = ac1 ; 0x3333, WRTN restored the caller's\n"
  "  ALPHA.v2 = ac3 ; wfp\n"
  "  ret\n"
  "\n"
  "blocks 2\n";

// leg 5: game->game void, two callees (WSAVS and WSAVR entries), each
// probing its ovk through a nested rt_call
static const std::string GG_VOID = HEAD +
  "a ALPHA.a1 *i16\n"
  "a ALPHA.arg_count u16\n"
  "a DELTA.a1 *i16\n"
  "a DELTA.arg_count u16\n"
  "v GAMMA.v0 i16\n"
  "v GAMMA.v1 i16\n"
  "v GAMMA.v2 u32\n"
  "v GAMMA.v3 u32\n"
    "v GAMMA.v20 u32          ; wsp before the call\n"
  "v GAMMA.v21 u32          ; wsp in the continuation\n"
"\n"
  "block GAMMA.b0\n"
  "  ac0 = 0xAAAA\n"
  "  ALPHA.a1 = wp(GAMMA.v0, 0)\n"
  "  ALPHA.arg_count = 1\n"
  "  GAMMA.v20 = wsp\n"
  "  call ALPHA args=1 ret=GAMMA.b1\n"
  "\n"
  "block ALPHA.b0 ; WSAVS entry: ovk 1 inside\n"
  "  M16[ALPHA.a1] = 0 - 5 ; -5 through the caller's pointer\n"
  "  rt_call ?OVK_PROBE() ret=ALPHA.b1\n"
  "\n"
  "block ALPHA.b1\n"
  "  ret\n"
  "\n"
  "block GAMMA.b1\n"
  "  GAMMA.v21 = wsp\n"
  "  GAMMA.v2 = ac0 ; the caller's ac0, restored\n"
  "  DELTA.a1 = wp(GAMMA.v1, 0)\n"
  "  DELTA.arg_count = 1\n"
  "  call DELTA args=1 ret=GAMMA.b2\n"
  "\n"
  "block DELTA.b0 ; WSAVR entry: ovk 0 inside\n"
  "  M16[DELTA.a1] = 9\n"
  "  rt_call ?OVK_PROBE() ret=DELTA.b1\n"
  "\n"
  "block DELTA.b1\n"
  "  ret\n"
  "\n"
  "block GAMMA.b2\n"
  "  GAMMA.v3 = ac0\n"
  "  ret\n"
  "\n"
  "blocks 7\n";

// leg 6: game->game valued — BETA.ret carries the value; ac0 is the caller's
static const std::string GG_VALUED = HEAD +
  "a BETA.a1 i32\n"
  "a BETA.arg_count u16\n"
  "a BETA.ret i32\n"
  "v GAMMA.v0 i32\n"
  "v GAMMA.v1 u32\n"
    "v GAMMA.v20 u32          ; wsp before the call\n"
  "v GAMMA.v21 u32          ; wsp in the continuation\n"
"\n"
  "block GAMMA.b0\n"
  "  ac0 = 0xC0C0\n"
  "  BETA.a1 = 20\n"
  "  BETA.arg_count = 1\n"
  "  GAMMA.v20 = wsp\n"
  "  call BETA args=1 ret=GAMMA.b1\n"
  "\n"
  "block BETA.b0\n"
  "  ac0 = BETA.a1 * 2 ; the callee's ac0 is NOT how the value travels\n"
  "  BETA.ret = ac0 + 2 ; 42\n"
  "  ret\n"
  "\n"
  "block GAMMA.b1\n"
  "  GAMMA.v21 = wsp\n"
  "  GAMMA.v0 = BETA.ret\n"
  "  GAMMA.v1 = ac0\n"
  "  ret\n"
  "\n"
  "blocks 3\n";

// leg 7: nested — GAMMA -> ALPHA (game) -> ?TEST_ADD (native) -> back -> ret -> GAMMA
static const std::string NESTED = HEAD +
  "a ALPHA.a1 *u32\n"
  "a ALPHA.arg_count u16\n"
  "v ALPHA.v0 u32\n"
  "v GAMMA.v0 u32\n"
  "v GAMMA.v1 u32\n"
  "v GAMMA.v2 u32\n"
    "v GAMMA.v20 u32          ; wsp before the call\n"
  "v GAMMA.v21 u32          ; wsp in the continuation\n"
"\n"
  "block GAMMA.b0\n"
  "  ac0 = 0xD0D0\n"
  "  ALPHA.a1 = wp(GAMMA.v0, 0)\n"
  "  ALPHA.arg_count = 1\n"
  "  GAMMA.v20 = wsp\n"
  "  call ALPHA args=1 ret=GAMMA.b1\n"
  "\n"
  "block ALPHA.b0\n"
  "  ac1 = 0x1111\n"
  "  ac2 = 0x2222\n"
  "  rt_call ?TEST_ADD(11, 22, wp(ALPHA.v0, 0)) ret=ALPHA.b1\n"
  "\n"
  "block ALPHA.b1\n"
  "  M32[ALPHA.a1] = ac0 + ALPHA.v0 ; 42 + 99 into the caller's cell\n"
  "  ret\n"
  "\n"
  "block GAMMA.b1\n"
  "  GAMMA.v21 = wsp\n"
  "  GAMMA.v1 = ac0\n"
  "  GAMMA.v2 = ac3\n"
  "  ret\n"
  "\n"
  "blocks 4\n";

// leg 8: PICK_X_Y, hand-written in ir 8 from game/routines/PICK_X_Y.c (P53's
// emitter is not ready; said so in the REPORT). Naive: one temp pair, no
// hoisting. The REPOSITION-shaped caller passes wp() of its two locals.
// OBJ_PTR = M32[0x70000212], SD_PTR = M32[0x70000210]; region_count at
// OBJ_PTR+0x2CEE (32-bit), REGION[r] at OBJ_PTR + 9r: x +0x2CE7, y +0x2CE8,
// type +0x2CE9 (16-bit); seed at SD_PTR+0x28 — the listing's own constants.
// variant: 0 = as written; 1 = identity-red (a): one extra ?RANDOM_NUMBER
// draw after the write-backs (right coordinates, wrong seed — the prompt's
// case); 2 = identity-red (b): the *x write-back off by one; 3 = an extra
// DISCARDED draw inside the loop head — MEASURED, not asserted: see the
// REPORT, the loop re-synchronises and seed + count can coincide.
static std::string pick_x_y(int variant) {
  const bool extra_call = variant == 3;
  const char* xstore = variant == 2 ? "ac0 + 1" : "ac0";
  std::string s = HEAD +
  "a PICK_X_Y.a1 *i16\n"
  "a PICK_X_Y.a2 *i16\n"
  "a PICK_X_Y.arg_count u16\n"
  "v PICK_X_Y.v0 i32       ; r\n"
  "v PICK_X_Y.v1 i32       ; TMP lo (32-bit: ?RANDOM_NUMBER's parameters are FIXED BIN(31))\n"
  "v PICK_X_Y.v2 i32       ; TMP hi\n"
  "v REPOSITION.v0 i16     ; x\n"
  "v REPOSITION.v1 i16     ; y\n"
  "v REPOSITION.v20 u32    ; wsp before the call\n"
  "v REPOSITION.v21 u32    ; wsp in the continuation\n"
  "\n"
  "block REPOSITION.b0\n"
  "  PICK_X_Y.a1 = wp(REPOSITION.v0, 0)\n"
  "  PICK_X_Y.a2 = wp(REPOSITION.v1, 0)\n"
  "  PICK_X_Y.arg_count = 2\n"
  "  REPOSITION.v20 = wsp\n"
  "  call PICK_X_Y args=2 ret=REPOSITION.b1\n"
  "\n"
  "block REPOSITION.b1\n"
  "  REPOSITION.v21 = wsp\n"
  "  ret\n"
  "\n"
  "block PICK_X_Y.b0 ; loop head: r = RANDOM_NUMBER(TMP(1), TMP(region_count), &seed)\n"
  "  PICK_X_Y.v1 = 1\n"
  "  ac2 = M32[0x70000212]\n"
  "  PICK_X_Y.v2 = M32[wp(ac2, 0x2CEE)]\n"
  "  ac2 = M32[0x70000210]\n";
  if (extra_call)   // identity-red (a): one draw too many, result discarded
    s += "  rt_call ?RANDOM_NUMBER(wp(PICK_X_Y.v1, 0), wp(PICK_X_Y.v2, 0), wp(ac2, 0x28)) ret=PICK_X_Y.b10\n"
         "\n"
         "block PICK_X_Y.b10\n"
         "  ac2 = M32[0x70000210]\n";
  s +=
  "  rt_call ?RANDOM_NUMBER(wp(PICK_X_Y.v1, 0), wp(PICK_X_Y.v2, 0), wp(ac2, 0x28)) ret=PICK_X_Y.b1\n"
  "\n"
  "block PICK_X_Y.b1\n"
  "  PICK_X_Y.v0 = ac0\n"
  "  assert(((ac0 >=s 0) && (ac0 <=s 100000)), \"RANGE_CHECK(r, 100000)\")\n"
  "  ac2 = M32[0x70000212] + (PICK_X_Y.v0 * 9)\n"
  "  ac0 = sx16(M16[wp(ac2, 0x2CE7)]) ; REGION[r].x\n"
  "  goto [PICK_X_Y.b2, PICK_X_Y.b0] (ac0 == 0)\n"
  "\n"
  "block PICK_X_Y.b2\n"
  "  ac2 = M32[0x70000212] + (PICK_X_Y.v0 * 9)\n"
  "  ac0 = sx16(M16[wp(ac2, 0x2CE9)]) ; REGION[r].type\n"
  "  ac0 = (ac0 /s 100) + 1\n"
  "  goto [PICK_X_Y.b3, PICK_X_Y.b0] (ac0 != 3)\n"
  "\n"
  "block PICK_X_Y.b3 ; *x = RANDOM_NUMBER(TMP(x-20), TMP(x+20), &seed)\n"
  "  ac2 = M32[0x70000212] + (PICK_X_Y.v0 * 9)\n"
  "  ac0 = sx16(M16[wp(ac2, 0x2CE7)])\n"
  "  PICK_X_Y.v1 = ac0 - 20\n"
  "  PICK_X_Y.v2 = ac0 + 20\n"
  "  ac2 = M32[0x70000210]\n"
  "  rt_call ?RANDOM_NUMBER(wp(PICK_X_Y.v1, 0), wp(PICK_X_Y.v2, 0), wp(ac2, 0x28)) ret=PICK_X_Y.b4\n"
  "\n"
  "block PICK_X_Y.b4 ; *y = RANDOM_NUMBER(TMP(y-20), TMP(y+20), &seed)\n"
  "  M16[PICK_X_Y.a1] = trunc16(" + std::string(xstore) + ") ; *x (16-bit store through the parameter)\n"
  "  ac2 = M32[0x70000212] + (PICK_X_Y.v0 * 9)\n"
  "  ac0 = sx16(M16[wp(ac2, 0x2CE8)])\n"
  "  PICK_X_Y.v1 = ac0 - 20\n"
  "  PICK_X_Y.v2 = ac0 + 20\n"
  "  ac2 = M32[0x70000210]\n"
  "  rt_call ?RANDOM_NUMBER(wp(PICK_X_Y.v1, 0), wp(PICK_X_Y.v2, 0), wp(ac2, 0x28)) ret=PICK_X_Y.b5\n"
  "\n"
  "block PICK_X_Y.b5 ; the four-term gate, one test per block (a skip chain in the original)\n"
  "  M16[PICK_X_Y.a2] = trunc16(ac0)\n"
  "  ac0 = sx16(M16[PICK_X_Y.a1])\n"
  "  goto [PICK_X_Y.b0, PICK_X_Y.b6] (ac0 >s 0x3BF5)\n"
  "\n"
  "block PICK_X_Y.b6\n"
  "  goto [PICK_X_Y.b0, PICK_X_Y.b7] (ac0 <=s 0x3FAC)\n"
  "\n"
  "block PICK_X_Y.b7\n"
  "  ac0 = sx16(M16[PICK_X_Y.a2])\n"
  "  goto [PICK_X_Y.b0, PICK_X_Y.b8] (ac0 >s 0x3B73)\n"
  "\n"
  "block PICK_X_Y.b8\n"
  "  goto [PICK_X_Y.b0, PICK_X_Y.b9] (ac0 <=s 0x3FDE)\n"
  "\n"
  "block PICK_X_Y.b9\n";
  if (variant == 1)   // identity-red (a): a draw too many, AFTER the coordinates are final
    s += "  ac2 = M32[0x70000210]\n"
         "  rt_call ?RANDOM_NUMBER(wp(PICK_X_Y.v1, 0), wp(PICK_X_Y.v2, 0), wp(ac2, 0x28)) ret=PICK_X_Y.b11\n"
         "\n"
         "block PICK_X_Y.b11\n";
  s +=
  "  ret\n"
  "\n"
  "blocks " + std::string((extra_call || variant == 1) ? "13" : "12") + "\n";
  return s;
}

// The shared-data scratch for leg 8 and the host ORACLE (the same C, on the
// host, over the same table and seed).
static constexpr uint32_t SD_AREA = 0x70000800u, OBJ_AREA = 0x70000A00u;
struct Region { int16_t x, y, type; };
static const Region REGIONS[4] = {
  { 0, 0, 0 },                     // [0] never drawn (r >= 1)
  { 0, 0x3C00, 250 },              // [1] unused (x == 0)           -> bail 1
  { 0x3D00, 0x3D00, 150 },         // [2] class 2, not 3            -> bail 2
  { 0x3BF5 + 10, 0x3FDE - 10, 220 } // [3] good, near two edges: ±20 draws fall outside about half the time
};
static constexpr int32_t REGION_COUNT = 3;
struct Oracle { int16_t x, y; int32_t seed; int calls; int bails[4]; };
static Oracle oracle_pick_x_y(double seed) {
  Oracle o{}; 
  for (;;) {
    int32_t r = lcg_draw(seed, 1, REGION_COUNT); o.calls++;
    if (REGIONS[r].x == 0) { o.bails[0]++; continue; }
    if (REGIONS[r].type / 100 + 1 != 3) { o.bails[1]++; continue; }
    int16_t x = int16_t(lcg_draw(seed, REGIONS[r].x - 20, REGIONS[r].x + 20)); o.calls++;
    int16_t y = int16_t(lcg_draw(seed, REGIONS[r].y - 20, REGIONS[r].y + 20)); o.calls++;
    if (x > 0x3BF5 && x <= 0x3FAC && y > 0x3B73 && y <= 0x3FDE) { o.x = x; o.y = y; o.seed = int32_t(seed); return o; }
    o.bails[2]++;
  }
}
static void seed_tables(Rig& r, int32_t seed) {
  r.memory.write_wide(0x70000210u, SD_AREA);
  r.memory.write_wide(0x70000212u, OBJ_AREA);
  r.memory.write_wide(SD_AREA + 0x28, uint32_t(seed));
  r.memory.write_wide(OBJ_AREA + 0x2CEE, uint32_t(REGION_COUNT));
  for (int i = 1; i <= REGION_COUNT; i++) {
    uint32_t base = OBJ_AREA + 9u * uint32_t(i);
    r.memory.write_word(base + 0x2CE7, uint16_t(REGIONS[i].x));
    r.memory.write_word(base + 0x2CE8, uint16_t(REGIONS[i].y));
    r.memory.write_word(base + 0x2CE9, uint16_t(REGIONS[i].type));
  }
}
// THE COMPARATOR — one function, used by the green leg and by both red legs.
// Compares the CALLER's cells, the seed word and the call count against the
// oracle; returns "" when they agree, else what differs.
static std::string compare_pick(Rig& r, IRExec* ir, const Oracle& o, int calls) {
  std::string diff;
  uint16_t x = r.memory.read_word(ir->v_address("REPOSITION.v0")), y = r.memory.read_word(ir->v_address("REPOSITION.v1"));
  int32_t seed = int32_t(r.memory.read_wide(SD_AREA + 0x28));
  if (x != uint16_t(o.x)) diff += "x " + hex(x) + " want " + hex(uint16_t(o.x)) + "; ";
  if (y != uint16_t(o.y)) diff += "y " + hex(y) + " want " + hex(uint16_t(o.y)) + "; ";
  if (seed != o.seed) diff += "seed " + hex(uint32_t(seed)) + " want " + hex(uint32_t(o.seed)) + "; ";
  if (calls != o.calls) diff += "calls " + std::to_string(calls) + " want " + std::to_string(o.calls) + "; ";
  return diff;
}

static int run() {
  Decoder::initialize();
  write_file(ADDRBOOK, ADDRBOOK_TEXT);

  // ---- baseline: where a program that only rets leaves wsp -------------------
  int32_t base_wsp = 0;
  {
    IRExec* ir = load_ok("baseline loads", BASELINE);
    if (!ir) return 1;
    IRExec::instance = ir; RIG(r); ir->map_pages(r.memory);
    std::string got = r.drive(ir->block_address("ALPHA.b0"));
    expect(got == "Empty call stack", "baseline ends at its ret", got);
    base_wsp = r.machine.wsp;
    expect(base_wsp == Rig::WFP - 12, "baseline wsp = WFP - 12 (WRTN's six pops, argc 0)", hex(uint32_t(base_wsp)));
    IRExec::instance = nullptr; delete ir;
  }
  // THE STACK-BALANCE CHECK, at the CONTINUATION: <entry>.v20 = wsp before
  // the call, .v21 = wsp in the ret= block. (The end-of-program wsp is NOT
  // evidence: WRTN begins with wsp = wfp, so the top-level ret erases any
  // drift — the first version of this check was blind and the
  // -DP54_BROKEN_BRIDGE build proved it.)
  auto balanced = [&](Rig& r, IRExec* ir, const char* entry, const char* leg) {
    uint32_t before = r.memory.read_wide(ir->v_address(std::string(entry) + ".v20"));
    uint32_t after  = r.memory.read_wide(ir->v_address(std::string(entry) + ".v21"));
    expect(before != 0 && before == after, leg, std::string("stack NOT balanced at the continuation: wsp before ") + hex(before) + " after " + hex(after));
    expect(r.machine.wsp == base_wsp && r.machine.wfp == Rig::SAVED_WFP && r.machine.ac[3] == Rig::SAVED_WFP, leg, "final wsp/wfp/ac3 not the baseline's");
  };

  // ---- 1. teeth (load) --------------------------------------------------------
  refuses("teeth: naive call to an entry with no b0",
          HEAD + "a ALPHA.a1 i16\na ALPHA.arg_count u16\n\nblock GAMMA.b0\n  ALPHA.a1 = 1\n  ALPHA.arg_count = 1\n  call ALPHA args=1 ret=GAMMA.b1\n\nblock GAMMA.b1\n  ret\n\nblocks 2\n",
          "ALPHA.b0 is not a block of this file");
  refuses("teeth: naive call to an un-compiled entry (PICK_X_Y with no blocks)",
          HEAD + "\nblock GAMMA.b0\n  call PICK_X_Y args=0 ret=GAMMA.b1\n\nblock GAMMA.b1\n  ret\n\nblocks 2\n",
          "PICK_X_Y.b0 is not a block of this file");

  // ---- 1b. THE ARGUMENT-KIND CHECK (a003 item 2): where the kind of the
  //          value written to a pointer `a` cell is manifest, it must match ---
  {
    const std::string DECL = HEAD + "a GAMMA.a1 *char\na GAMMA.a2 *i16\na GAMMA.arg_count u16\nv ALPHA.v0 char 4\nv ALPHA.v1 i16\nv ALPHA.v2 *char\nv ALPHA.v3 *u32\n\n";
    const std::string TAIL = "  GAMMA.arg_count = 2\n  call GAMMA args=2 ret=ALPHA.b1\n\nblock ALPHA.b1\n  ret\n\nblock GAMMA.b0\n  ret\n\nblocks 3\n";
    refuses("kind: wp() into a *char cell",       DECL + "block ALPHA.b0\n  GAMMA.a1 = wp(ALPHA.v1, 0)\n  GAMMA.a2 = wp(ALPHA.v1, 0)\n" + TAIL, "GAMMA.a1 is a BYTE pointer but the value written to it in ALPHA.b0 is a WORD pointer");
    refuses("kind: bp() into a *i16 cell",        DECL + "block ALPHA.b0\n  GAMMA.a1 = bp(ALPHA.v0, 0)\n  GAMMA.a2 = bp(ALPHA.v0, 0)\n" + TAIL, "GAMMA.a2 is a WORD pointer but the value written to it in ALPHA.b0 is a BYTE pointer");
    refuses("kind: a *char cell into a *i16 cell",DECL + "block ALPHA.b0\n  GAMMA.a1 = bp(ALPHA.v0, 0)\n  GAMMA.a2 = ALPHA.v2\n" + TAIL, "GAMMA.a2 is a WORD pointer but the value written to it in ALPHA.b0 is a BYTE pointer");
    refuses("kind: a *u32 cell into a *char cell",DECL + "block ALPHA.b0\n  GAMMA.a1 = ALPHA.v3\n  GAMMA.a2 = wp(ALPHA.v1, 0)\n" + TAIL, "GAMMA.a1 is a BYTE pointer but the value written to it in ALPHA.b0 is a WORD pointer");
    refuses("kind: the LAST write counts",        DECL + "block ALPHA.b0\n  GAMMA.a1 = bp(ALPHA.v0, 0)\n  GAMMA.a1 = wp(ALPHA.v1, 0)\n  GAMMA.a2 = wp(ALPHA.v1, 0)\n" + TAIL, "GAMMA.a1 is a BYTE pointer");
    {   // the matching kinds load; so do the two the check does not claim: an unknowable value and a write in an earlier block
      std::string ok = DECL + "block ALPHA.b0\n  GAMMA.a1 = bp(ALPHA.v0, 0)\n  GAMMA.a2 = wp(ALPHA.v1, 0)\n" + TAIL;
      expect(load_throws(ok).empty(), "kind: bp() into *char and wp() into *i16 load", load_throws(ok));
      std::string ok2 = DECL + "block ALPHA.b0\n  GAMMA.a1 = ALPHA.v2\n  GAMMA.a2 = ALPHA.v3\n" + TAIL;
      expect(load_throws(ok2).empty(), "kind: pointer cells of the matching kinds load (pointee width is advisory)", load_throws(ok2));
      std::string unk = DECL + "block ALPHA.b0\n  GAMMA.a1 = 0x70000400\n  GAMMA.a2 = M32[wp(ALPHA.v1, 0)]\n" + TAIL;
      expect(load_throws(unk).empty(), "kind: a constant / a memory read are of unknowable kind and are NOT checked (the spec says so)", load_throws(unk));
      std::string earlier = DECL + "block ALPHA.b2\n  GAMMA.a1 = wp(ALPHA.v1, 0)\n  goto [ALPHA.b0] 0\n\nblock ALPHA.b0\n  GAMMA.a2 = wp(ALPHA.v1, 0)\n" + std::string(TAIL).replace(TAIL.find("blocks 3"), 8, "blocks 4");
      expect(load_throws(earlier).empty(), "kind: a write in an EARLIER block is not checked (the check is per calling block; the spec says so)", load_throws(earlier));
    }
  }

  // ---- 1c. a003 item 1: a game->game call on a Machine with NO symbol table
  //          EXECUTES (CallStack::call used to dereference the null table and
  //          the process died with no diagnostic — P53's 40/40 segfault) ------
  {
    IRExec* ir = load_ok("no-symbols program loads", GG_VALUED);
    if (ir) {
      IRExec::instance = ir; RIG(r, /*with_symbols=*/false); ir->map_pages(r.memory);
      std::string got = r.drive(ir->block_address("GAMMA.b0"));
      expect(got == "Empty call stack", "no-symbols rig: the game->game call runs to the program's ret (no crash, no diagnostic needed)", got);
      expect(r.memory.read_wide(ir->v_address("GAMMA.v0")) == 42u, "no-symbols rig: the call round-tripped", hex(r.memory.read_wide(ir->v_address("GAMMA.v0"))));
      IRExec::instance = nullptr; delete ir;
    }
  }

  // ---- 2. teeth (run) ---------------------------------------------------------
  {
    std::string prog = HEAD + "\nblock ALPHA.b0\n  rt_call ?TEST_ADD(1, 2, 3) ret=ALPHA.b1\n\nblock ALPHA.b1\n  ret\n\nblocks 2\n";
    IRExec* ir = load_ok("run-teeth program loads", prog);
    if (ir) {
      IRExec::instance = ir;
      { RIG(r, /*with_symbols=*/false); ir->map_pages(r.memory);
        std::string got = r.drive(ir->block_address("ALPHA.b0"));
        expect(got.find("needs a symbol table") != std::string::npos && got.find("?TEST_ADD") != std::string::npos,
               "teeth: symbolic rt_call with no symbol table throws by name", got); }
      IRExec::instance = nullptr; delete ir;
    }
    std::string prog2 = HEAD + "\nblock ALPHA.b0\n  rt_call ?NO_SUCH(1) ret=ALPHA.b1\n\nblock ALPHA.b1\n  ret\n\nblocks 2\n";
    ir = load_ok("run-teeth program 2 loads", prog2);
    if (ir) {
      IRExec::instance = ir;
      { RIG(r); ir->map_pages(r.memory);
        std::string got = r.drive(ir->block_address("ALPHA.b0"));
        expect(got.find("not in the symbol table") != std::string::npos && got.find("?NO_SUCH") != std::string::npos,
               "teeth: symbolic rt_call to an unknown symbol throws by name", got); }
      IRExec::instance = nullptr; delete ir;
    }
  }

  // ---- 3. rt_call -> native ---------------------------------------------------
  {
    IRExec* ir = load_ok("rt_call native program loads", RT_NATIVE);
    if (ir) {
      IRExec::instance = ir; RIG(r); ir->map_pages(r.memory);
      IRExec::rt_registry_override = &r.registry;
      g_native_calls = 0; g_native_note.clear();
      std::string got = r.drive(ir->block_address("ALPHA.b0"));
      expect(got == "Empty call stack", "rt_call native: program ends at its ret", got);
      expect(g_native_calls == 1, "rt_call native: ?TEST_ADD ran exactly once", std::to_string(g_native_calls));
      expect(g_native_note.empty(), "rt_call native: arguments in order on the real stack, entry registers seen", g_native_note);
      auto V = [&](const char* n) { return r.memory.read_wide(ir->v_address(n)); };
      expect(V("ALPHA.v0") == 99u, "rt_call native: write-back through arg 3 into the 0x76 cell", hex(V("ALPHA.v0")));
      expect(V("ALPHA.v1") == 42u, "rt_call native: result arrived in ac0 in the continuation block", hex(V("ALPHA.v1")));
      expect(V("ALPHA.v2") == 0x1111u && V("ALPHA.v3") == 0x2222u, "rt_call native: ac1/ac2 preserved across the call");
      expect(V("ALPHA.v4") == uint32_t(Rig::WFP), "rt_call native: ac3 = wfp after the return", hex(V("ALPHA.v4")));
      balanced(r, ir, "ALPHA", "rt_call native: stack balanced");
      IRExec::rt_registry_override = nullptr; IRExec::instance = nullptr; delete ir;
    }
  }

  // ---- 4. rt_call -> emulated leaf --------------------------------------------
  {
    IRExec* ir = load_ok("rt_call emulated program loads", RT_EMULATED);
    if (ir) {
      IRExec::instance = ir; RIG(r); ir->map_pages(r.memory);
      IRExec::rt_registry_override = &r.registry;          // ?TEST_LEAF is not in it: the emulator runs the words
      std::string got = r.drive(ir->block_address("ALPHA.b0"));
      expect(got == "Empty call stack", "rt_call emulated: program ends at its ret", got);
      auto V = [&](const char* n) { return r.memory.read_wide(ir->v_address(n)); };
      expect(V("ALPHA.v0") == 77u, "rt_call emulated: WRTN restored ac0 from the leaf's slotpatch (wp(ac3,-8))", hex(V("ALPHA.v0")));
      expect(V("ALPHA.v1") == 0x3333u, "rt_call emulated: WRTN restored the caller's ac1", hex(V("ALPHA.v1")));
      expect(V("ALPHA.v2") == uint32_t(Rig::WFP), "rt_call emulated: ac3 = wfp after WRTN", hex(V("ALPHA.v2")));
      balanced(r, ir, "ALPHA", "rt_call emulated: callee-pops argc 2, stack balanced");
      IRExec::rt_registry_override = nullptr; IRExec::instance = nullptr; delete ir;
    }
  }

  // ---- 5. game->game void, ovk from the variant ------------------------------
  {
    IRExec* ir = load_ok("game->game void program loads", GG_VOID);
    if (ir) {
      IRExec::instance = ir; RIG(r); ir->map_pages(r.memory);
      IRExec::rt_registry_override = &r.registry;
      r.machine.ovk = 0;
      // the probe records ovk on each call; drive stops at each native return, so read it after each batch
      std::vector<int> ovks;
      uint32_t pc = ir->block_address("GAMMA.b0"); std::string got;
      for (int i = 0; i < 50; i++) {
        try { g_probe_ovk = -1; pc = r.machine.run_steps(pc, 200); if (g_probe_ovk >= 0) ovks.push_back(g_probe_ovk); }
        catch (const std::exception& e) { got = e.what(); break; }
      }
      expect(got == "Empty call stack", "game->game void: program ends at its ret", got);
      expect(ovks.size() == 2 && ovks[0] == 1 && ovks[1] == 0, "game->game void: ovk 1 inside the WSAVS callee, 0 inside the WSAVR callee (addrbook variant)",
             ovks.size() == 2 ? std::to_string(ovks[0]) + "," + std::to_string(ovks[1]) : std::to_string(ovks.size()) + " probes");
      expect(r.memory.read_word(ir->v_address("GAMMA.v0")) == 0xFFFB, "game->game void: ALPHA wrote -5 through the caller's pointer", hex(r.memory.read_word(ir->v_address("GAMMA.v0"))));
      expect(r.memory.read_word(ir->v_address("GAMMA.v1")) == 9, "game->game void: DELTA wrote 9 through the caller's pointer");
      expect(r.memory.read_wide(ir->v_address("GAMMA.v2")) == 0xAAAAu, "game->game void: the caller's ac0 restored after ALPHA", hex(r.memory.read_wide(ir->v_address("GAMMA.v2"))));
      expect(r.machine.ovk == 0, "game->game void: the caller's psr (ovk 0) restored by the top-level WRTN", std::to_string(r.machine.ovk));
      balanced(r, ir, "GAMMA", "game->game void: stack balanced");
      IRExec::rt_registry_override = nullptr; IRExec::instance = nullptr; delete ir;
    }
  }

  // ---- 6. game->game valued ---------------------------------------------------
  {
    IRExec* ir = load_ok("game->game valued program loads", GG_VALUED);
    if (ir) {
      IRExec::instance = ir; RIG(r); ir->map_pages(r.memory);
      std::string got = r.drive(ir->block_address("GAMMA.b0"));
      expect(got == "Empty call stack", "game->game valued: program ends at its ret", got);
      expect(r.memory.read_wide(ir->v_address("GAMMA.v0")) == 42u, "game->game valued: the caller read 42 from BETA.ret", hex(r.memory.read_wide(ir->v_address("GAMMA.v0"))));
      expect(r.memory.read_wide(ir->v_address("GAMMA.v1")) == 0xC0C0u, "game->game valued: ac0 after the call is the CALLER's, not the result (no slotpatch at L1)", hex(r.memory.read_wide(ir->v_address("GAMMA.v1"))));
      balanced(r, ir, "GAMMA", "game->game valued: stack balanced");
      IRExec::instance = nullptr; delete ir;
    }
  }

  // ---- 7. nested --------------------------------------------------------------
  {
    IRExec* ir = load_ok("nested program loads", NESTED);
    if (ir) {
      IRExec::instance = ir; RIG(r); ir->map_pages(r.memory);
      IRExec::rt_registry_override = &r.registry;
      g_native_calls = 0; g_native_note.clear();
      std::string got = r.drive(ir->block_address("GAMMA.b0"));
      expect(got == "Empty call stack", "nested: program ends at its ret", got);
      expect(g_native_calls == 1 && g_native_note.empty(), "nested: the native ran once with the right stack", g_native_note);
      expect(r.memory.read_wide(ir->v_address("GAMMA.v0")) == 141u, "nested: 42 + 99 landed in the outer caller's cell", hex(r.memory.read_wide(ir->v_address("GAMMA.v0"))));
      expect(r.memory.read_wide(ir->v_address("GAMMA.v1")) == 0xD0D0u, "nested: outer caller's ac0 restored", hex(r.memory.read_wide(ir->v_address("GAMMA.v1"))));
      expect(r.memory.read_wide(ir->v_address("GAMMA.v2")) == uint32_t(Rig::WFP), "nested: ac3 = the outer wfp after both returns", hex(r.memory.read_wide(ir->v_address("GAMMA.v2"))));
      balanced(r, ir, "GAMMA", "nested: stack balanced at depth 2");
      IRExec::rt_registry_override = nullptr; IRExec::instance = nullptr; delete ir;
    }
  }

  // ---- 8. PICK_X_Y end to end: identity-green, identity-red (a), (b) ----------
  {
    const int32_t SEEDS[] = { 12345, 7, 60000001 };
    for (int32_t seed0 : SEEDS) {
      Oracle o = oracle_pick_x_y(double(seed0));
      std::printf("PICK_X_Y oracle seed %d: x %04X y %04X seed' %08X calls %d bails unused/class/rect %d/%d/%d\n",
                  seed0, uint16_t(o.x), uint16_t(o.y), uint32_t(o.seed), o.calls, o.bails[0], o.bails[1], o.bails[2]);
      auto run_variant = [&](const char* what, int variant, std::string& diff, int& calls) -> bool {
        IRExec* ir = load_ok(what, pick_x_y(variant));
        if (!ir) return false;
        IRExec::instance = ir; RIG(r); ir->map_pages(r.memory);
        IRExec::rt_registry_override = &r.registry;
        seed_tables(r, seed0);
        g_random_calls = 0;
        std::string got = r.drive(ir->block_address("REPOSITION.b0"), 2000);
        expect(got == "Empty call stack", what, "program did not end at REPOSITION.b1's ret: " + got);
        calls = g_random_calls;
        diff = compare_pick(r, ir, o, calls);
        std::printf("  %s: x %04X y %04X seed' %08X calls %d%s%s\n", what,
                    r.memory.read_word(ir->v_address("REPOSITION.v0")), r.memory.read_word(ir->v_address("REPOSITION.v1")),
                    r.memory.read_wide(SD_AREA + 0x28), calls, diff.empty() ? "" : " DIFF: ", diff.c_str());
        balanced(r, ir, "REPOSITION", what);
        IRExec::rt_registry_override = nullptr; IRExec::instance = nullptr; delete ir;
        return true;
      };
      std::string diff; int calls = 0;
      // identity-green
      if (run_variant("PICK_X_Y identity-green", 0, diff, calls))
        expect(diff.empty(), "PICK_X_Y identity-green: caller's x, y, the seed and the call count agree with the oracle", diff);
      // identity-red (a): one draw too many after the coordinates are final — right x/y, wrong seed
      if (run_variant("PICK_X_Y identity-red (a)", 1, diff, calls))
        expect(!diff.empty() && diff.find("seed") != std::string::npos && diff.find("calls") != std::string::npos && diff.find("x ") == std::string::npos,
               "PICK_X_Y identity-red (a): an extra ?RANDOM_NUMBER call FAILS the comparison on seed and count with x/y right", diff.empty() ? "comparison PASSED (no teeth)" : diff);
      // identity-red (b): the *x write-back off by one — x differs, or the gate takes a different path and the seed differs
      if (run_variant("PICK_X_Y identity-red (b)", 2, diff, calls))
        expect(!diff.empty(), "PICK_X_Y identity-red (b): *x off by one FAILS the comparison", diff.empty() ? "comparison PASSED (no teeth)" : diff);
      // MEASURED (not asserted): an extra DISCARDED draw inside the loop head.
      // The loop re-synchronises on the seed sequence and the accepted x/y are
      // always the last two draws, so whenever the total count coincides the
      // seed and the coordinates coincide too — seed + count comparison is
      // blind to it. Recorded for the REPORT (and P50 Q3).
      if (run_variant("PICK_X_Y in-loop discarded draw (measured)", 3, diff, calls))
        std::printf("  in-loop discarded draw, seed %d: %s\n", seed0, diff.empty() ? "INVISIBLE to seed+count comparison" : ("caught: " + diff).c_str());
    }
  }
  return 0;
}

int main() {
  try { run(); }
  catch (const std::exception& e) { fail("unexpected exception", e.what()); }
  std::printf("BRIDGE SELFTEST %s (%d cases, %d failures)\n", fails ? "RED" : "GREEN", cases, fails);
  return fails ? 1 : 0;
}
