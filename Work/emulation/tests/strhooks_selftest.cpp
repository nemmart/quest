// tests/strhooks_selftest.cpp — Project 33-A self-test for the string
// checker hooks (hw/strings/StrHooks). Not part of the emulator build; see
// tests/run_strhooks_selftest.sh (which also builds ClaimDelta with
// -DP33_BROKEN_DELTA_SIGN and requires THAT run to go RED — teeth).
//
// Two scratch Machines play master (ordinal 0) and clone (ordinal 0). The
// hooks are driven by the REAL instruction arms: WMSP/STASP/WRTN words are
// written into a scratch code page at the pcs a synthetic quest.strhooks
// names, decoded by the Decoder and executed by EagleStack — so the wiring
// in the arms is under test, not just the hook bodies.
//
//  1. the loader's refusals; the decode-at-attach check
//  2. one group (DIED shape, 3 claims): Δ arithmetic, bind + rebind at every
//     claim, the STASP restore/Δ checks, the clone drain, equivalent()
//  3. two sequential groups in one frame; the symmetric-Δ compare identity
//  4. ordinary WRTN: rows of the popped frame unmapped, Δ erased; a return
//     with a claim outstanding aborts (loud)
//  5. unwind cut (I.GOTO-shape WRTN / ON-pop STASP): >= rule, discarded
//     claims tolerated and counted
//  6. the §6.5 loop-rebind edge: a dead register from iteration 1 mismatches
//     after iteration 2 rebinds at a different depth
//  7. the loud checks: claim out of order, STASP restoring the wrong wsp,
//     STASP for a block bound in another frame
//  8. F2-b: Lockstep::terminal_abort_pending consumes the pending assert and
//     fires only for a kind-2 terminal master half
//  9. QUEST_POKE :CLONE parsing is exercised by the battery (leg d), not here
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <stdexcept>
#include <functional>
#include <memory>
#include "hw/Machine.hpp"
#include "hw/Memory.hpp"
#include "hw/Decoder.hpp"
#include "hw/Instruction.hpp"
#include "hw/EagleStack.hpp"
#include "hw/Mapper.hpp"
#include "hw/Lockstep.hpp"
#include "hw/QueueEntry.hpp"
#include "hw/RTStubs.hpp"
#include "hw/strings/StrHooks.hpp"
#include "hw/strings/Arena.hpp"
#include "os/ArrayPage.hpp"
using namespace hw;
using namespace hw::strings;

static int fails = 0, cases = 0;
static void fail(const char* what, const char* detail) {
  if(fails++ < 40) std::printf("FAIL %s: %s\n", what, detail);
}
static void expect(bool ok, const char* what, const char* detail = "") { cases++; if(!ok) fail(what, detail); }

// ---- fixture ---------------------------------------------------------------

static constexpr uint32_t CODE  = 0x70100000u;   // scratch code page (segment 7)
static constexpr uint32_t STACK = 0x70000000u;   // 8 pages: 0x70000000..0x70001FFF

// the synthetic table's pcs
static constexpr uint32_t BLK_A = 0x70100010u, A1 = 0x70100012u, A2 = 0x70100020u, A3 = 0x70100028u, AS = 0x70100030u;
static constexpr uint32_t BLK_B = 0x70100040u, B1 = 0x70100042u, BS = 0x70100050u;
static constexpr uint32_t ONPOP = 0x70100060u, UNWIND = 0x70100070u, PLAIN_WRTN = 0x70100080u;
// P33-B: one twin per claim — A.1/A.2/A.3 and B.1 (quest.arena)
static constexpr uint32_t ARENA_A1 = 0x75000000u, ARENA_A2 = 0x75001000u, ARENA_A3 = 0x75002000u, ARENA_B1 = 0x75003000u;
static constexpr uint32_t ARENA_A = ARENA_A3, ARENA_B = ARENA_B1;   // the group results (p@b ≡ s@b.last)

// opcodes (Decoder tables): WMSP yy = 111yy11001001001, STASP yy = 101yy11001011001, WRTN = 1000011110101001
static uint16_t WMSP(int r)  { return static_cast<uint16_t>(0xE649u | (r << 11)); }
static uint16_t STASP(int r) { return static_cast<uint16_t>(0xA659u | (r << 11)); }
static constexpr uint16_t WRTN = 0x87A9u;

struct Rig {
  Memory memory;
  Machine machine;
  std::vector<os::ArrayPage*> pages;
  Rig(int32_t role, int32_t ordinal) : machine(nullptr, nullptr, nullptr, &memory) {
    map(CODE, 1); map(STACK, 8);
    machine.zero_claims = false;
    machine.lockstep_role = role; machine.lockstep_ordinal = ordinal;
    machine.wsb = STACK; machine.wsl = STACK + 0x1F00;
    frame(0x70001000, 0x70001040);
    // the code words at the hooked pcs
    w16(A1, WMSP(0)); w16(A2, WMSP(2)); w16(A3, WMSP(2)); w16(AS, STASP(1));
    w16(B1, WMSP(0)); w16(BS, STASP(1));
    w16(ONPOP, STASP(0)); w16(UNWIND, WRTN); w16(PLAIN_WRTN, WRTN);
  }
  void map(uint32_t word_base, uint32_t n) {
    for(uint32_t i = 0; i < n; i++) {
      os::ArrayPage* p = new os::ArrayPage();
      pages.push_back(p);
      memory.map_page(p, (word_base >> 10) + i, Permissions::PERMISSIONS_READ_WRITE_EXECUTE);
    }
  }
  void w16(uint32_t a, uint16_t v) { memory.write_word(a, v); }
  void frame(int32_t wfp, int32_t wsp) { machine.wfp = wfp; machine.wsp = wsp; }
  // a WSAVS-shaped frame image at F: ret, saved wfp, ac2, ac1, ac0, frame word (argc 0)
  void frame_image(int32_t F, int32_t saved_wfp, int32_t frame_words = 0) {
    memory.write_wide(F, 0x70123456u);
    memory.write_wide(F - 2, static_cast<uint32_t>(saved_wfp));
    memory.write_wide(F - 4, 0x22222222u); memory.write_wide(F - 6, 0x11111111u); memory.write_wide(F - 8, 0u);
    memory.write_wide(F - 10, static_cast<uint32_t>(frame_words));
  }
  uint32_t exec(uint32_t pc) {
    uint32_t opcode = memory.read_instruction_word(pc);
    Instruction* ins = Decoder::decode(machine.segments[(pc >> 28) & 7]->lef, opcode);
    if(!ins) throw std::runtime_error("no decode");
    machine.pc = static_cast<int32_t>(pc);
    return ins->execute(machine, pc, opcode);
  }
  // WRTN through the real arm: the hook fires before CallStack::call_return,
  // which throws "Empty call stack" in this rig (no shadow calls) — swallowed.
  void exec_wrtn(uint32_t pc) {
    try { exec(pc); }
    catch(const std::runtime_error& e) { if(std::strcmp(e.what(), "Empty call stack") != 0) throw; }
  }
  MachineHooks& H() { return *machine.strhooks; }
};

// Rigs live on the heap: Memory carries a 2 MB permissions array inline (P30 note).
#define RIG(name, role, ord) std::unique_ptr<Rig> name##_p(new Rig(role, ord)); Rig& name = *name##_p

static std::string throws(const char* what, std::function<void()> f) {
  cases++;
  try { f(); } catch(const std::exception& e) { return e.what(); }
  fail(what, "did not throw");
  return "";
}

static const char* TABLE = "/tmp/strhooks_selftest.strhooks";

static void write_table(const char* path, const char* body) {
  FILE* f = fopen(path, "w");
  fputs("# synthetic P33-A table for the self-test\n", f);
  fputs(body, f);
  fclose(f);
}

static const char* ARENA_FILE = "/tmp/strhooks_selftest.arena";
static const char* ARENA_GOOD =
  "temp 1 s@70100010.1 arena=75000000 cap=4096 bound=unbounded wmsp=70100012 routine=GROUP_A size=x\n"
  "temp 2 s@70100010.2 arena=75001000 cap=4096 bound=unbounded wmsp=70100020 routine=GROUP_A size=y\n"
  "temp 3 s@70100010.3 arena=75002000 cap=4096 bound=unbounded wmsp=70100028 routine=GROUP_A size=z\n"
  "temp 4 s@70100040.1 arena=75003000 cap=4096 bound=unbounded wmsp=70100042 routine=GROUP_B size=w\n";

static const char* GOOD =
  "row 1 70100010 GROUP_A first=70100012 last=70100028 nclaims=3\n"
  "wmsp 70100012 row=1 n=1 size=x\n"
  "wmsp 70100020 row=1 n=2 size=y\n"
  "wmsp 70100028 row=1 n=3 size=z\n"
  "stasp 70100030 row=1\n"
  "row 2 70100040 GROUP_B first=70100042 last=70100042 nclaims=1\n"
  "wmsp 70100042 row=2 n=1 size=w\n"
  "stasp 70100050 row=2\n"
  "onpop 70100060\n"
  "unwind 70100070\n";

static bool load(const char* body, std::string* err) {
  write_table(TABLE, body);
  return StrHooks::load_file(TABLE, err);
}

// the compare_pair identity (symmetric Δ): claim-free wsps agree
static bool wsp_agrees(Rig& m, Rig& c) {
  int32_t dm = m.H().outstanding(), dc = c.H().outstanding();
  return (m.machine.wsp - dm) == (c.machine.shadow_wsp() + c.machine.mapper.checkpoint_offset() - dc);
}

static int run() {
  Decoder::initialize();
  std::string err;

  // ---- 0. the arena layout (P33-B) ------------------------------------------
  write_table(ARENA_FILE, ARENA_GOOD);
  expect(Arena::load_file(ARENA_FILE, &err), "arena.load", err.c_str());
  expect(Arena::temps().size() == 4 && Arena::find(BLK_A, 3) && Arena::find(BLK_A, 3)->addr == ARENA_A3, "arena.lookup");
  expect(Arena::by_wmsp(A2) && Arena::by_wmsp(A2)->claim == 2, "arena.by_wmsp");
  {
    std::string e2;
    write_table(ARENA_FILE, "temp 1 s@70100010.1 arena=75000000 cap=4096 bound=unbounded wmsp=70100012 routine=A size=x\n"
                            "temp 2 s@70100010.2 arena=75000800 cap=4096 bound=unbounded wmsp=70100020 routine=A size=y\n");
    expect(!Arena::load_file(ARENA_FILE, &e2) && e2.find("overlaps") != std::string::npos, "arena.overlap refused", e2.c_str());
    write_table(ARENA_FILE, "temp 1 s@70100010.2 arena=75000000 cap=4096 bound=unbounded wmsp=70100020 routine=A size=y\n");
    expect(!Arena::load_file(ARENA_FILE, &e2) && e2.find("lacks claim 1") != std::string::npos, "arena.missing claim refused", e2.c_str());
    write_table(ARENA_FILE, ARENA_GOOD);
    expect(Arena::load_file(ARENA_FILE, &e2), "arena.reload", e2.c_str());
  }

  // ---- 1. loader ----------------------------------------------------------
  expect(load(GOOD, &err), "load.good", err.c_str());
  expect(StrHooks::rows().size() == 2, "load.rows");
  expect(StrHooks::lookup(A2) && StrHooks::lookup(A2)->kind == HookKind::Wmsp && StrHooks::lookup(A2)->n == 2, "load.lookup");
  expect(StrHooks::onpop_pc() == ONPOP && StrHooks::is_unwind_wrtn(UNWIND) && !StrHooks::is_unwind_wrtn(PLAIN_WRTN), "load.onpop/unwind");
  expect(!load("row 1 70100010 A first=70100012 last=70100012 nclaims=1\n"
               "wmsp 70100012 row=1 n=1 size=x\nwmsp 70100012 row=1 n=1 size=x\nstasp 70100030 row=1\nonpop 70100060\n", &err)
         && err.find("duplicate") != std::string::npos, "load.duplicate pc", err.c_str());
  expect(!load("row 1 70100010 A first=70100012 last=70100020 nclaims=2\n"
               "wmsp 70100012 row=1 n=1 size=x\nwmsp 70100020 row=1 n=1 size=x\nstasp 70100030 row=1\nonpop 70100060\n", &err),
         "load.two first claims", err.c_str());
  expect(!load("row 1 70100010 A first=70100012 last=70100012 nclaims=1\n"
               "wmsp 70100012 row=1 n=1 size=x\nonpop 70100060\n", &err) && err.find("no STASP") != std::string::npos,
         "load.no stasp", err.c_str());
  expect(!load("row 1 70100010 A first=70100012 last=70100012 nclaims=1\n"
               "wmsp 70100012 row=1 n=1 size=x\nstasp 70100030 row=1\n", &err) && err.find("onpop") != std::string::npos,
         "load.no onpop", err.c_str());
  expect(!load("row 1 70100010 A first=70100012 last=70100012 nclaims=1\n"
               "row 2 70100010 B first=70100042 last=70100042 nclaims=1\n", &err)
         && err.find("duplicate block") != std::string::npos, "load.duplicate block", err.c_str());
  expect(!load("bogus 1\n", &err), "load.unknown kind");
  expect(load(GOOD, &err), "load.good again", err.c_str());
  StrHooks::active = true;

  // ---- 2. one group on the master, drained into the clone ---------------------
  RIG(M, Lockstep::MASTER, 0); RIG(C, Lockstep::CLONE, 0);
  StrHooks::attach(M.machine);           // decode check + arena rows
  StrHooks::attach(C.machine);
  expect(M.machine.strhooks && C.machine.strhooks, "attach.hooks");
  expect(C.machine.mapper.arena_rows() == 4 && M.machine.mapper.arena_rows() == 4, "attach.arena rows (one per twin)");
  const int32_t F1 = 0x70001000, S0 = 0x70001040;
  M.frame(F1, S0); C.frame(F1, S0);
  M.machine.ac[0] = 3;  M.exec(A1);       // claim 1: 3 wides
  expect(M.machine.wsp == S0 + 6, "group.wsp after claim 1");
  expect(M.H().delta(F1) == 6, "group.delta 6");
  expect(StrHooks::queued(0) == 2, "group.bind + claim-insertion queued");
  M.machine.ac[2] = 5;  M.exec(A2);       // claim 2: 5 wides
  M.machine.ac[2] = 4;  M.exec(A3);       // claim 3: 4 wides (the result temp)
  expect(M.H().delta(F1) == 24, "group.delta 24");
  expect(M.H().max_delta == 24, "group.max_delta");
  expect(StrHooks::queued(0) == 6 && M.H().n_bind == 1 && M.H().n_rebind == 0, "group.two events per claim (bind, insertion), no rebind");
  // the clone claims too (today): same instructions, same Δ
  C.machine.ac[0] = 3; C.exec(A1); C.machine.ac[2] = 5; C.exec(A2); C.machine.ac[2] = 4; C.exec(A3);
  expect(C.H().delta(F1) == 24 && StrHooks::queued(0) == 6, "group.clone claims, no events from the clone");
  expect(wsp_agrees(M, C), "group.compare identity mid-group (both claim)");
  // drain: the row maps to the LAST claim's base (wsp_before + 2)
  StrHooks::attach(C.machine);
  expect(StrHooks::queued(0) == 0, "drain.empties");
  const uint32_t last_base = static_cast<uint32_t>(S0 + 6 + 10 + 2);   // wsp before claim 3, +2
  Mapper::Verdict v = C.machine.equivalent(last_base + 3, ARENA_A3 + 3);
  expect(v.kind == Mapper::Kind::MAPPED, "drain.twin A.3 mapped to claim 3's base");
  expect(C.machine.equivalent(static_cast<uint32_t>(S0 + 2) + 3, ARENA_A1 + 3).kind == Mapper::Kind::MAPPED,
         "drain.twin A.1 mapped to claim 1's base");
  expect(C.machine.equivalent(static_cast<uint32_t>(S0 + 6 + 2) + 1, ARENA_A2 + 1).kind == Mapper::Kind::MAPPED,
         "drain.twin A.2 mapped to claim 2's base (the intermediate temp — INIT_OBJ_TBL's survivor)");
  expect(C.machine.equivalent(static_cast<uint32_t>(S0 + 2) + 3, ARENA_A3 + 3).kind == Mapper::Kind::MISMATCH,
         "drain.twin A.3 does not map to claim 1's base");
  expect(C.machine.equivalent(0x70001234u, ARENA_B).kind == Mapper::Kind::MISMATCH, "drain.twin B.1 unmapped");
  // the STASP: restore + Δ back to 0; the row stays mapped
  M.machine.ac[1] = S0; M.exec(AS);
  C.machine.ac[1] = S0; C.exec(AS);
  expect(M.machine.wsp == S0 && M.H().delta(F1) == 0 && C.H().delta(F1) == 0, "stasp.delta 0");
  expect(M.H().n_release == 1, "stasp.release counted");
  expect(C.machine.equivalent(last_base + 3, ARENA_A + 3).kind == Mapper::Kind::MAPPED, "stasp.row stays mapped");
  expect(wsp_agrees(M, C), "stasp.compare identity");
  // the S1 point: a clone that does NOT claim (P33-B) still agrees, because Δ_c = 0 and its wsp did not move
  {
    RIG(C2, Lockstep::CLONE, 1); StrHooks::attach(C2.machine); C2.frame(F1, S0);
    RIG(M2, Lockstep::MASTER, 1); StrHooks::attach(M2.machine); M2.frame(F1, S0);
    M2.machine.ac[0] = 3; M2.exec(A1);
    expect(wsp_agrees(M2, C2), "S1.non-claiming clone agrees mid-group");
    C2.machine.wsp += 2;                  // a clone that moved wsp for any other reason: caught
    expect(!wsp_agrees(M2, C2), "S1.any other wsp drift is a mismatch");
    M2.machine.ac[2] = 5; M2.exec(A2); M2.machine.ac[2] = 4; M2.exec(A3); M2.machine.ac[1] = S0; M2.exec(AS);
    StrHooks::attach(C2.machine);        // drain ordinal 1
  }

  // ---- 3. two sequential groups in one frame ------------------------------------
  M.machine.ac[0] = 4; M.exec(B1);       // group B, same frame, same base
  expect(M.H().delta(F1) == 8 && M.H().n_bind == 2, "seq.group B claim");
  M.machine.ac[1] = S0; M.exec(BS);
  expect(M.H().delta(F1) == 0, "seq.group B released");
  StrHooks::attach(C.machine);
  expect(C.machine.equivalent(static_cast<uint32_t>(S0 + 2), ARENA_B).kind == Mapper::Kind::MAPPED, "seq.row B mapped");
  expect(C.machine.equivalent(last_base, ARENA_A).kind == Mapper::Kind::MAPPED, "seq.row A still mapped");

  // ---- 4. ordinary WRTN --------------------------------------------------------
  M.frame_image(F1, 0x70000800);
  M.frame(F1, S0);
  M.exec_wrtn(PLAIN_WRTN);
  expect(M.machine.wfp == 0x70000800, "wrtn.popped");
  expect(M.H().n_frame_exit == 1 && M.H().n_unmap == 1, "wrtn.frame exit counted once (one frame, all four twins)");
  StrHooks::attach(C.machine);
  expect(C.machine.equivalent(last_base, ARENA_A).kind == Mapper::Kind::MISMATCH &&
         C.machine.equivalent(static_cast<uint32_t>(S0 + 2), ARENA_A1).kind == Mapper::Kind::MISMATCH &&
         C.machine.equivalent(static_cast<uint32_t>(S0 + 2), ARENA_B).kind == Mapper::Kind::MISMATCH,
         "wrtn.every twin unmapped on the clone");
  expect(M.H().delta(F1) == 0 && M.H().claims().frames() == 0, "wrtn.delta erased");
  {   // loud: a return with a claim outstanding
    RIG(L, Lockstep::MASTER, 2); StrHooks::attach(L.machine);
    L.frame(F1, S0); L.machine.ac[0] = 3; L.exec(A1);
    L.frame_image(F1, 0x70000800);
    std::string m = throws("wrtn.claim outstanding", [&] { L.exec_wrtn(PLAIN_WRTN); });
    expect(m.find("discards frame") != std::string::npos, "wrtn.claim outstanding message", m.c_str());
    { RIG(D, Lockstep::CLONE, 2); StrHooks::attach(D.machine); }   // drain ordinal 2
  }

  // ---- 5. unwind cut and ON-pop (>= rule, discarded claims tolerated) ------------------
  {
    RIG(U, Lockstep::MASTER, 3); RIG(UC, Lockstep::CLONE, 3); StrHooks::attach(U.machine); StrHooks::attach(UC.machine);
    const int32_t F2 = 0x70001100;
    U.frame(F1, S0); U.machine.ac[0] = 3; U.exec(A1); U.machine.ac[2] = 5; U.exec(A2); U.machine.ac[2] = 4; U.exec(A3);
    U.machine.ac[1] = S0; U.exec(AS);                  // group A complete in F1
    U.frame(F2, 0x70001140); U.machine.ac[0] = 4; U.exec(B1);   // group B claimed in F2, NOT released (signal inside it)
    StrHooks::attach(UC.machine);
    expect(UC.machine.equivalent(last_base, ARENA_A).kind == Mapper::Kind::MAPPED &&
           UC.machine.equivalent(0x70001142u, ARENA_B).kind == Mapper::Kind::MAPPED, "unwind.both bound");
    // the I.GOTO-shape WRTN through F2 (the cursor), tolerated: F2's claim is discarded
    U.frame_image(F2, F1);
    U.frame(F2, 0x70001148);
    U.exec_wrtn(UNWIND);
    expect(U.machine.wfp == F1 && U.H().n_unwind == 1 && U.H().n_discarded_claims == 1, "unwind.cut through a live group");
    // the landing stub: STASP 0 with ac0 = the snapshot; restored wfp F1 → F1's rows go too (>=)
    U.machine.ac[0] = S0; U.exec(ONPOP);
    expect(U.H().n_onpop == 1 && U.machine.wsp == S0, "onpop.fired");
    StrHooks::attach(UC.machine);
    expect(UC.machine.equivalent(last_base, ARENA_A).kind == Mapper::Kind::MISMATCH &&
           UC.machine.equivalent(0x70001142u, ARENA_B).kind == Mapper::Kind::MISMATCH, "onpop.rows at/above the restored frame unmapped");
    expect(U.H().claims().frames() == 0, "onpop.delta erased for both frames");
    // the clone twin: frame_exit(pre_wfp, unwind=true) is what frames.cpp's i_goto passes
    UC.frame(F1, S0); UC.machine.ac[0] = 3; UC.exec(A1);
    UC.H().frame_exit(F1, /*unwind=*/true);
    expect(UC.H().n_unwind == 1 && UC.H().n_discarded_claims == 1 && UC.H().delta(F1) == 0, "unwind.clone twin");
  }

  // ---- 6. the §6.5 loop-rebind edge -----------------------------------------------
  {
    RIG(R, Lockstep::MASTER, 4); RIG(RC, Lockstep::CLONE, 4); StrHooks::attach(R.machine); StrHooks::attach(RC.machine);
    R.frame(F1, S0); R.machine.ac[0] = 3; R.exec(A1); R.machine.ac[2] = 5; R.exec(A2); R.machine.ac[2] = 4; R.exec(A3);
    R.machine.ac[1] = S0; R.exec(AS);
    StrHooks::attach(RC.machine);
    const uint32_t dead_master = last_base + 2, dead_clone = ARENA_A + 2;   // a dead ac2 after iteration 1
    expect(RC.machine.equivalent(dead_master, dead_clone).kind == Mapper::Kind::MAPPED, "edge.iteration 1 pointer maps");
    // iteration 2 at a different depth (a WPSH'd temp below the group, say)
    R.frame(F1, S0 + 4); R.machine.ac[0] = 3; R.exec(A1); R.machine.ac[2] = 5; R.exec(A2); R.machine.ac[2] = 4; R.exec(A3);
    R.machine.ac[1] = S0 + 4; R.exec(AS);
    expect(R.H().n_rebind == 1, "edge.rebind counted (the block re-opened in the same frame)");
    StrHooks::attach(RC.machine);
    v = RC.machine.equivalent(dead_master, dead_clone);
    expect(v.kind == Mapper::Kind::MISMATCH && v.mapped == dead_master + 4,
           "edge.iteration 1's dead pointer mismatches after the rebind (maps 4 words higher) — exactly §6.5");
  }

  // ---- 7. the loud checks --------------------------------------------------------
  {
    RIG(L, Lockstep::MASTER, 6); StrHooks::attach(L.machine); L.frame(F1, S0);
    std::string m = throws("loud.claim 2 before claim 1", [&] { L.machine.ac[2] = 5; L.exec(A2); });
    expect(m.find("without claim 1") != std::string::npos, "loud.claim order message", m.c_str());
  }
  {
    RIG(L, Lockstep::MASTER, 7); StrHooks::attach(L.machine); L.frame(F1, S0);
    L.machine.ac[0] = 3; L.exec(A1); L.machine.ac[2] = 5; L.exec(A2); L.machine.ac[2] = 4; L.exec(A3);
    std::string m = throws("loud.stasp wrong restore", [&] { L.machine.ac[1] = S0 + 2; L.exec(AS); });
    expect(m.find("restores wsp") != std::string::npos, "loud.stasp restore message", m.c_str());
  }
  {
    RIG(L, Lockstep::MASTER, 8); StrHooks::attach(L.machine); L.frame(F1, S0);
    L.machine.ac[0] = 3; L.exec(A1); L.machine.ac[2] = 5; L.exec(A2); L.machine.ac[2] = 4; L.exec(A3);
    L.machine.wfp = 0x70001100;                                    // the STASP arrives in another frame
    std::string m = throws("loud.stasp other frame", [&] { L.machine.ac[1] = S0; L.exec(AS); });
    expect(m.find("bound in frame") != std::string::npos, "loud.stasp frame message", m.c_str());
  }
  {
    RIG(L, Lockstep::MASTER, 9); StrHooks::attach(L.machine); L.frame(F1, S0);
    L.machine.ac[0] = 3; L.exec(A1); L.machine.ac[2] = 5; L.exec(A2);
    std::string m = throws("loud.stasp before the last claim", [&] { L.machine.ac[1] = S0; L.exec(AS); });
    expect(m.find("after 2 of 3 claims") != std::string::npos, "loud.stasp incomplete group", m.c_str());
  }

  // ---- 7b. the twin's capacity (P33-B) --------------------------------------------
  {
    RIG(L, Lockstep::MASTER, 11); StrHooks::attach(L.machine); L.frame(F1, S0);
    std::string m = throws("cap.claim over capacity", [&] { L.machine.ac[0] = 1030; L.exec(A1); });   // 4120 bytes > 4096
    expect(m.find("quest.arena undersized") != std::string::npos, "cap.message", m.c_str());
  }

  // ---- 8. F2-b -----------------------------------------------------------------
  {
    RIG(T, Lockstep::MASTER, 10);
    QueueEntry master(&T.machine, 0x70155555u, 1);
    RTStubs::terminal_test_pc = 0x70155555u; RTStubs::terminal_test_kind = 2;
    std::string msg;
    expect(!Lockstep::terminal_abort_pending(&master, &msg), "f2b.nothing pending");
    Lockstep::pending_assert[10].set = true; Lockstep::pending_assert[10].pc = 0x7015C48Bu;
    Lockstep::pending_assert[10].report = "IR ASSERT FAILED [block 7015C48B stmt 0]";
    master.terminal = false;
    expect(!Lockstep::terminal_abort_pending(&master, &msg) && !Lockstep::pending_assert[10].set,
           "f2b.non-terminal master: consumed, no abort");
    Lockstep::pending_assert[10].set = true; Lockstep::pending_assert[10].pc = 0x7015C48Bu;
    Lockstep::pending_assert[10].report = "IR ASSERT FAILED [block 7015C48B stmt 0]";
    master.terminal = true; RTStubs::terminal_test_kind = 1;
    expect(!Lockstep::terminal_abort_pending(&master, &msg), "f2b.kind-1 terminal: no abort");
    Lockstep::pending_assert[10].set = true; Lockstep::pending_assert[10].pc = 0x7015C48Bu;
    Lockstep::pending_assert[10].report = "IR ASSERT FAILED [block 7015C48B stmt 0]";
    RTStubs::terminal_test_kind = 2;
    expect(Lockstep::terminal_abort_pending(&master, &msg) && !Lockstep::pending_assert[10].set,
           "f2b.kind-2 terminal: TERMINAL-ABORT, consumed");
    expect(msg.find("TERMINAL-ABORT at 70155555") != std::string::npos && msg.find("assert at 7015C48B") != std::string::npos &&
           msg.find("IR ASSERT FAILED") != std::string::npos, "f2b.message names both pcs", msg.c_str());
    expect(!Lockstep::terminal_abort_pending(&master, &msg), "f2b.one-shot");
    RTStubs::terminal_test_pc = 0;
  }

  return 0;
}

int main() {
  try { run(); }
  catch(const std::exception& e) { fail("unexpected exception", e.what()); }
  std::printf("STRHOOKS SELFTEST %s (%d cases, %d failures)\n", fails ? "RED" : "GREEN", cases, fails);
  return fails ? 1 : 0;
}
