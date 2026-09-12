// tests/lowerc_rig.cpp — Project 48, Stage A: the IR side of the differential
// tester.  Loads a NAIVE v-form program produced by compiler/lower_c.py, runs
// it through the REAL dispatch path (Machine::run_steps with
// lockstep_role = CLONE and IRExec::instance set — P46 F10's lesson: run_block
// is not the dispatch the program will meet), and prints the final value of
// every observable `v` in the same `name=%08X` form the native harness prints.
//
// Not part of the emulator build; see tests/run_lowerc_difftest.sh.
//
// Observables and rendering come from the compiler's own .vmap manifest, so
// the thing under test states where it put everything and the comparison
// checks that claim too: a wrong .vmap goes red, which is correct.
//
// TRAPS.  A failed `assert` is the compiler's DERR 17 subscript check
// (quest_rt.h's quest_sub_check is the native twin).  IRExec routes an assert
// failure through Lockstep::assert_detach, which prints the report to STDERR
// and then throws, so the rig captures its own stderr for the run, extracts
// the assert's message, and prints `TRAP <message>` on stdout — the same line
// the native side prints.  "Both trapped at the same site" is then a string
// compare, and "one trapped" is a disagreement (a001 R5).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <unistd.h>
#include "hw/Machine.hpp"
#include "hw/Memory.hpp"
#include "hw/IRExec.hpp"
#include "hw/Lockstep.hpp"
#include "hw/Decoder.hpp"
#include "hw/Permissions.hpp"
#include "os/ArrayPage.hpp"
using namespace hw;

// ------------------------------------------------------------- the .vmap --

struct VRow {
  std::string vname, kind, cname, ctype, vtype, render, anchor;
  uint32_t words = 0, nelem = 0, elemwords = 0;
  int line = 0;
};

struct VMap {
  std::string entry, entryblock, routine, src;
  std::vector<VRow> rows;
};

static std::vector<std::string> split_tabs(const std::string& s) {
  std::vector<std::string> out;
  size_t p = 0;
  for (;;) {
    size_t q = s.find('\t', p);
    if (q == std::string::npos) { out.push_back(s.substr(p)); break; }
    out.push_back(s.substr(p, q - p));
    p = q + 1;
  }
  return out;
}

static VMap read_vmap(const char* path) {
  std::ifstream f(path);
  if (!f) throw std::runtime_error(std::string("cannot open vmap ") + path);
  VMap m;
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;
    if (line[0] == '#') {
      std::istringstream is(line.substr(1));
      std::string k, v;
      is >> k >> v;
      if (k == "entry") m.entry = v;
      else if (k == "entryblock") m.entryblock = v;
      else if (k == "routine") m.routine = v;
      else if (k == "src") m.src = v;
      continue;
    }
    std::vector<std::string> c = split_tabs(line);
    if (c.size() < 11)
      throw std::runtime_error("malformed vmap row: " + line);
    VRow r;
    r.vname = c[0]; r.kind = c[1]; r.cname = c[2]; r.ctype = c[3];
    r.vtype = c[4];
    r.words = uint32_t(std::strtoul(c[5].c_str(), nullptr, 10));
    r.nelem = uint32_t(std::strtoul(c[6].c_str(), nullptr, 10));
    r.elemwords = uint32_t(std::strtoul(c[7].c_str(), nullptr, 10));
    r.render = c[8]; r.anchor = c[9];
    r.line = atoi(c[10].c_str());
    m.rows.push_back(r);
  }
  if (m.entryblock.empty())
    throw std::runtime_error("vmap has no `# entryblock` line");
  return m;
}

// ------------------------------------------------------------- the world --

struct Rig {
  Memory memory;
  Machine machine;
  std::vector<os::ArrayPage*> pages;
  static constexpr uint32_t STACK = 0x70000000u;      // 8 pages
  static constexpr int32_t WFP = 0x70001000, SAVED_WFP = 0x70000F00;

  Rig() : machine(nullptr, nullptr, nullptr, &memory) {
    memory.process_name = "lowerc";
    map(STACK, 8);
    machine.zero_claims = false;
    machine.lockstep_role = Lockstep::CLONE;          // the IR dispatch path
    machine.lockstep_ordinal = 0;
    machine.wsb = STACK; machine.wsl = STACK + 0x1F00;
    machine.wfp = WFP; machine.wsp = WFP + 0x40;
    // A WSAVS-shaped frame image, so the program's `ret` runs a real WRTN
    // (vform_selftest's shape: ret, saved wfp, ac2, ac1, ac0, frame word).
    memory.write_wide(WFP, 0x70123456u);
    memory.write_wide(WFP - 2, uint32_t(SAVED_WFP));
    memory.write_wide(WFP - 4, 0x22222222u);
    memory.write_wide(WFP - 6, 0x11111111u);
    memory.write_wide(WFP - 8, 0u);
    memory.write_wide(WFP - 10, 0u);
  }
  void map(uint32_t word_base, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
      os::ArrayPage* p = new os::ArrayPage();
      pages.push_back(p);
      memory.map_page(p, (word_base >> 10) + i,
                      Permissions::PERMISSIONS_READ_WRITE_EXECUTE);
    }
  }
};

// A "fixture" file is the whole of a hand-written end-to-end test:
//
//   map  <hex word base> <npages>      map memory before writing it
//   <hex word addr> <hex value> 16|32  seed a word / wide
//   setv <ENTRY.vN> <hex value> 16|32  seed a `v` BY NAME — the loader owns
//                                      the 0x76 address, so a by-reference
//                                      parameter can only be seeded this way
//                                      (a001 R4: a param is a u32 v holding
//                                      the argument's word address)
//   expect <hex word addr> <hex> 16|32 [label]   checked AFTER the run
//
// Generated programs need no fixture; UPDATE_SCREENS needs SD_PTR, a player
// table, its three arguments, and its expectations.
struct Expect { uint32_t addr, want; int width; std::string label; };
static std::vector<Expect> expects;

static void apply_fixture(Rig& r, const char* path, IRExec* ir) {
  std::ifstream f(path);
  if (!f) throw std::runtime_error(std::string("cannot open fixture ") + path);
  std::string line;
  while (std::getline(f, line)) {
    size_t h = line.find(';');
    if (h != std::string::npos) line = line.substr(0, h);
    std::istringstream is(line);
    std::string tok;
    if (!(is >> tok)) continue;
    if (tok == "expect") {
      std::string addr, val, width, label;
      is >> addr >> val >> width;
      std::getline(is, label);
      while (!label.empty() && label[0] == ' ') label.erase(0, 1);
      Expect e;
      e.addr = uint32_t(std::strtoul(addr.c_str(), nullptr, 16));
      e.want = uint32_t(std::strtoul(val.c_str(), nullptr, 16));
      e.width = (width == "16") ? 16 : 32;
      e.label = label;
      expects.push_back(e);
    } else if (tok == "setv") {
      std::string name, val, width;
      is >> name >> val >> width;
      uint32_t at = ir->v_address(name);
      if (at == 0)
        throw std::runtime_error("fixture setv: no such v " + name);
      uint32_t v = uint32_t(std::strtoul(val.c_str(), nullptr, 16));
      if (width == "16") r.memory.write_word(at, v & 0xFFFFu);
      else r.memory.write_wide(at, v);
    } else if (tok == "map") {
      std::string base, n;
      is >> base >> n;
      r.map(uint32_t(std::strtoul(base.c_str(), nullptr, 16)),
            uint32_t(std::strtoul(n.c_str(), nullptr, 10)));
    } else {
      std::string val, width;
      is >> val >> width;
      uint32_t a = uint32_t(std::strtoul(tok.c_str(), nullptr, 16));
      uint32_t v = uint32_t(std::strtoul(val.c_str(), nullptr, 16));
      if (width == "16") r.memory.write_word(a, v & 0xFFFFu);
      else r.memory.write_wide(a, v);
    }
  }
}

// --------------------------------------------------------------- stderr --

// IRExec's placement diagnostics and an assert report both go to stderr.  We
// want the assert text, and we do not want the diagnostics polluting the
// comparison, so the whole run's stderr goes to a scratch file we read back.
struct StderrCapture {
  std::string path;
  int saved = -1;
  explicit StderrCapture(const std::string& p) : path(p) {
    fflush(stderr);
    saved = dup(fileno(stderr));
    FILE* f = freopen(path.c_str(), "w+", stderr);
    (void)f;
  }
  std::string restore() {
    fflush(stderr);
    if (saved >= 0) { dup2(saved, fileno(stderr)); close(saved); saved = -1; }
    std::ifstream in(path);
    std::stringstream ss; ss << in.rdbuf();
    return ss.str();
  }
};

// `IR ASSERT FAILED [block 77000003 stmt 4]: assert((..), "DERR17 prog.c:12")`
// -> `DERR17 prog.c:12`.  The message is the LAST double-quoted run on the
// line; an assert with no message yields the whole statement text.
static bool assert_message(const std::string& err, std::string& out) {
  size_t p = err.find("IR ASSERT FAILED");
  if (p == std::string::npos) return false;
  size_t e = err.find('\n', p);
  std::string line = err.substr(p, e == std::string::npos ? e : e - p);
  size_t q2 = line.rfind('"');
  if (q2 == std::string::npos) { out = line; return true; }
  size_t q1 = line.rfind('"', q2 - 1);
  if (q1 == std::string::npos) { out = line; return true; }
  out = line.substr(q1 + 1, q2 - q1 - 1);
  return true;
}

// ----------------------------------------------------------------- main --

int main(int argc, char** argv) {
  const char* program = nullptr;
  const char* vmap_path = nullptr;
  const char* addrbook = nullptr;
  const char* fixture = nullptr;
  long steps = 20000000L;
  bool dump_all = false;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto next = [&]() -> const char* {
      if (i + 1 >= argc) { fprintf(stdout, "missing value for %s\n", a.c_str()); exit(2); }
      return argv[++i];
    };
    if (a == "--program") program = next();
    else if (a == "--vmap") vmap_path = next();
    else if (a == "--addrbook") addrbook = next();
    else if (a == "--fixture") fixture = next();
    else if (a == "--steps") steps = atol(next());
    else if (a == "--dump-all") dump_all = true;
    else { printf("unknown argument %s\n", a.c_str()); return 2; }
  }
  if (!program || !vmap_path || !addrbook) {
    printf("usage: lowerc_rig --program X.ir --vmap X.vmap --addrbook A "
           "[--fixture F] [--steps N] [--dump-all]\n");
    return 2;
  }

  Decoder::initialize();       // WRTN's fixed opcode decodes through the table

  VMap vm;
  IRExec* ir = nullptr;
  std::unique_ptr<Rig> rig;
  std::string captured;

  try {
    vm = read_vmap(vmap_path);
    ir = IRExec::load_file(program, addrbook);
  } catch (const std::exception& e) {
    printf("RIGERROR load: %s\n", e.what());
    return 1;
  }

  uint32_t entry_pc = ir->block_address(vm.entryblock);
  if (entry_pc == 0) {
    printf("RIGERROR: entry block %s not in the program\n", vm.entryblock.c_str());
    return 1;
  }

  rig.reset(new Rig());
  try {
    ir->map_pages(rig->memory);
    if (fixture) apply_fixture(*rig, fixture, ir);
  } catch (const std::exception& e) {
    printf("RIGERROR setup: %s\n", e.what());
    return 1;
  }
  IRExec::instance = ir;

  std::string tmp = std::string(program) + ".rigerr";
  StderrCapture cap(tmp);
  std::string ended;
  try {
    rig->machine.run_steps(entry_pc, steps);
    ended = "step limit";                 // ran out of steps without returning
  } catch (const std::exception& e) {
    ended = e.what();
  }
  captured = cap.restore();
  remove(tmp.c_str());

  // A failed assert is a TRAP and is the whole output, exactly as the native
  // side's quest_sub_check exits before printing anything.
  std::string msg;
  if (assert_message(captured, msg)) {
    printf("TRAP %s\n", msg.c_str());
    return 0;
  }
  // `ret` runs WRTN over the rig's frame; the shadow call stack is empty, so
  // the arm throws AFTER the frame pop.  That throw IS the normal end of a
  // v-form program in this rig (vform_selftest establishes the same shape).
  if (ended != "Empty call stack") {
    printf("RIGERROR run: %s\n", ended.c_str());
    fputs(captured.c_str(), stderr);
    return 1;
  }

  Memory& M = rig->memory;
  int failed = 0;
  for (const Expect& e : expects) {
    uint32_t got = (e.width == 16) ? (M.read_word(e.addr) & 0xFFFFu)
                                   : M.read_wide(e.addr);
    if (got != e.want) {
      if (failed < 20)
        printf("EXPECT FAIL %08X = %08X want %08X  %s\n",
               e.addr, got, e.want, e.label.c_str());
      failed++;
    }
  }
  if (!expects.empty())
    printf("EXPECT: %zu checked, %d failed\n", expects.size(), failed);
  for (const VRow& r : vm.rows) {
    bool observable = (r.kind == "local" || r.kind == "param" || r.kind == "ret");
    if (!dump_all && (!observable || r.cname.empty())) continue;
    uint32_t base = ir->v_address(r.vname);
    if (base == 0) {
      printf("RIGERROR: %s not placed\n", r.vname.c_str());
      return 1;
    }
    uint32_t n = r.nelem ? r.nelem : 1;
    for (uint32_t i = 0; i < n; i++) {
      uint32_t at = base + i * (r.elemwords ? r.elemwords : 1);
      uint32_t v = (r.render == "w16") ? (M.read_word(at) & 0xFFFFu)
                                       : M.read_wide(at);
      if (r.nelem) printf("%s[%u]=%08X\n", r.cname.c_str(), i, v);
      else printf("%s=%08X\n", r.cname.c_str(), v);
    }
  }
  fflush(stdout);
  return failed ? 1 : 0;
}
