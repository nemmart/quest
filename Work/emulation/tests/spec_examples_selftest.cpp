// tests/spec_examples_selftest.cpp — P54 reopening, a003 item 2b (P53 q006):
// docs/IR.md's worked examples are RUN, not read. The text is extracted from
// the spec FILE at run time (QUEST_IR_SPEC, default ../docs/IR.md), so this
// leg tests whatever the document currently says, and a spec example that
// does not load fails a test rather than a reader.
//
//  1. §5.10.10, VERBATIM: the indented program is extracted, loaded with the
//     REAL quest.addrbook (QUEST is idx 0 — the prose depends on it), and
//     every placement fact the prose states is checked; then it is EXECUTED
//     with a1 pointed at a rig word, and the prose's outcome — v1 holds 6,
//     v2 holds length 8 and HELLO WO, the pointed i16 holds 6 — is checked.
//  2. §5.10.4's illustration lines are NOT a program (they reference cells
//     the table never declares, and one line reads an arena twin), so each
//     is wrapped in a minimal program with the declarations the prose
//     implies and must LOAD; the twin line is skipped and said so.
//
// Own binary rather than a bridge_selftest leg because the loader caches the
// addrbook's entry table once per process and the other self-tests use a
// synthetic one. Runs from emulation/ (tests/run_bridge_selftest.sh).
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <memory>
#include <stdexcept>
#include "hw/Machine.hpp"
#include "hw/Memory.hpp"
#include "hw/IRExec.hpp"
#include "hw/Lockstep.hpp"
#include "hw/Decoder.hpp"
#include "hw/Permissions.hpp"
#include "os/ArrayPage.hpp"
#include "debug/SymbolTable.hpp"
using namespace hw;

static int fails = 0, cases = 0;
static void fail(const char* what, const std::string& d) { if (fails++ < 40) std::printf("FAIL %s: %s\n", what, d.c_str()); }
static void expect(bool ok, const char* what, const std::string& d = "") { cases++; if (!ok) fail(what, d); }
static std::string hex(uint32_t v) { char b[16]; snprintf(b, sizeof b, "%08X", v); return b; }

static const char* ADDRBOOK = "quest.addrbook";
static const char* SCRATCH  = "/tmp/spec_example_case.ir";

// The indented block under a `#### <section>` heading: 4-space lines (blank
// lines kept), up to the first non-blank line that is not indented.
static std::string extract_block(const std::string& spec, const std::string& heading) {
  std::ifstream f(spec);
  if (!f) throw std::runtime_error("cannot open the spec: " + spec);
  std::string line, out; bool in = false, started = false;
  while (std::getline(f, line)) {
    if (!in) { if (line.rfind(heading, 0) == 0) in = true; continue; }
    bool indented = line.rfind("    ", 0) == 0, blank = line.find_first_not_of(" \t") == std::string::npos;
    if (indented) { out += line.substr(4) + "\n"; started = true; }
    else if (blank) { if (started) out += "\n"; }
    else if (started) break;
  }
  if (out.empty()) throw std::runtime_error("no indented block under " + heading);
  return out;
}

struct Rig {
  Memory memory;
  debug::SymbolTable symbols;
  Machine machine;
  std::vector<os::ArrayPage*> pages;
  static constexpr uint32_t STACK = 0x70000000u;
  static constexpr int32_t WFP = 0x70001000, SAVED_WFP = 0x70000F00;
  Rig() : machine(nullptr, nullptr, &symbols, &memory) {
    memory.process_name = "spec";
    for (uint32_t i = 0; i < 8; i++) { os::ArrayPage* p = new os::ArrayPage(); pages.push_back(p);
      memory.map_page(p, (STACK >> 10) + i, Permissions::PERMISSIONS_READ_WRITE_EXECUTE); }
    machine.zero_claims = false;
    machine.lockstep_role = Lockstep::CLONE; machine.lockstep_ordinal = 0;
    machine.wsb = STACK; machine.wsl = STACK + 0x1F00;
    machine.wfp = WFP; machine.wsp = WFP + 0x40;
    memory.write_wide(WFP, 0x70123456u); memory.write_wide(WFP - 2, uint32_t(SAVED_WFP));
    memory.write_wide(WFP - 4, 0x22222222u); memory.write_wide(WFP - 6, 0x11111111u);
    memory.write_wide(WFP - 8, 0u); memory.write_wide(WFP - 10, 0u);
  }
  std::string bytes(uint32_t word, uint32_t n) { std::string s; for (uint32_t i = 0; i < n; i++) s.push_back(char(memory.read_byte((word << 1) + i))); return s; }
};

static std::string load_throws(const std::string& text) {
  { std::ofstream f(SCRATCH); f << text; }
  try { IRExec* ir = IRExec::load_file(SCRATCH, ADDRBOOK); delete ir; return ""; }
  catch (const std::exception& e) { return e.what(); }
}

int main() {
  Decoder::initialize();
  const char* spec_env = getenv("QUEST_IR_SPEC");
  std::string spec = spec_env ? spec_env : "../docs/IR.md";

  // ---- 1. §5.10.10 verbatim: load, placement facts, execution, outcome ------
  try {
    std::string prog = extract_block(spec, "#### 5.10.10");
    { std::ofstream f(SCRATCH); f << prog; }
    IRExec* ir = nullptr;
    try { ir = IRExec::load_file(SCRATCH, ADDRBOOK); }
    catch (const std::exception& e) { fail("5.10.10 loads VERBATIM", e.what()); }
    if (ir) {
      expect(ir->symbolic_block_count() == 3, "5.10.10: three blocks", std::to_string(ir->symbolic_block_count()));
      struct { const char* n; uint32_t a; } P[] = {
        {"QUEST.v0", 0x76000000u}, {"QUEST.v1", 0x76000001u}, {"QUEST.v2", 0x76000003u}, {"QUEST.v3", 0x76000008u},
        {"QUEST.v4", 0x7600000Eu}, {"QUEST.a1", 0x76000010u}, {"QUEST.arg_count", 0x76000012u} };
      for (auto& p : P) expect(ir->v_address(p.n) == p.a, "5.10.10: the placement the prose states", std::string(p.n) + " at " + hex(ir->v_address(p.n)) + " want " + hex(p.a));
      for (uint32_t k = 0; k < 3; k++) expect(ir->block_address("QUEST.b" + std::to_string(k)) == 0x77000000u + k, "5.10.10: block placement", "b" + std::to_string(k));
      IRExec::instance = ir;
      std::unique_ptr<Rig> r(new Rig());
      ir->map_pages(r->memory);
      // "the i16 the caller pointed a1 at": the caller is this rig
      const uint32_t POINTED = 0x70000800u;
      r->memory.write_word(POINTED, 0xDEAD);
      r->memory.write_wide(ir->v_address("QUEST.a1"), POINTED);
      r->memory.write_word(ir->v_address("QUEST.arg_count"), 1);
      std::string got;
      try { r->machine.run_steps(0x77000000u, 100); got = "returned"; } catch (const std::exception& e) { got = e.what(); }
      expect(got == "Empty call stack", "5.10.10: runs to its ret", got);
      expect(r->memory.read_wide(0x76000001u) == 6, "5.10.10: 'v1 holds 6'", hex(r->memory.read_wide(0x76000001u)));
      expect(r->memory.read_word(0x76000003u) == 8, "5.10.10: 'v2 holds length 8'", hex(r->memory.read_word(0x76000003u)));
      expect(r->bytes(0x76000004u, 8) == "HELLO WO", "5.10.10: 'and HELLO WO'", r->bytes(0x76000004u, 8));
      expect(r->memory.read_word(POINTED) == 6, "5.10.10: 'the i16 the caller pointed a1 at holds 6'", hex(r->memory.read_word(POINTED)));
      expect(r->memory.read_word(0x76000000u) == 0, "5.10.10: 'the loop runs three times' (counter reached 0)", hex(r->memory.read_word(0x76000000u)));
      IRExec::instance = nullptr; delete ir;
    }
  } catch (const std::exception& e) { fail("5.10.10 extraction", e.what()); }

  // ---- 2. §5.10.4's illustration lines, each wrapped so it can LOAD --------
  try {
    std::string lines = extract_block(spec, "#### 5.10.4 THE VARIABLE FORM");
    // declarations the prose implies: v0 i16, v1 u32, v2 a char/varying, v3
    // varying 27, v4 words 10, v6 *i16, v7 *i32, and the two a1 pointers
    const std::string DECL = "ir 8\nmode stock\n"
      "v QUEST.v0 i16\nv QUEST.v1 u32\nv QUEST.v2 char 4\nv QUEST.v3 varying 27\nv QUEST.v4 words 10\n"
      "v QUEST.v6 *i16\nv QUEST.v7 *i32\na FAKE_OCEAN.a1 *i16\na FAKE_OCEAN.arg_count u16\na INIT_SCREEN.a1 *i16\na INIT_SCREEN.arg_count u16\n\n";
    std::istringstream is(lines); std::string line; int n = 0, skipped = 0;
    while (std::getline(is, line)) {
      if (line.find_first_not_of(" \t") == std::string::npos) continue;
      std::string stmt = line.substr(0, line.find(';'));
      if (stmt.find("s@") != std::string::npos) { skipped++; continue; }   // an arena twin: needs QUEST_ARENA, not a load-only shape
      bool is_goto = stmt.find("goto") != std::string::npos;
      std::string prog = DECL + "block QUEST.b0\n  " + stmt + "\n" + (is_goto ? "" : "  ret\n") +
                         "\nblock QUEST.b3\n  ret\n\nblock QUEST.b7\n  ret\n\nblocks 3\n";
      std::string got = load_throws(prog);
      expect(got.empty(), "5.10.4 illustration loads", stmt + " -> " + got);
      n++;
    }
    expect(n >= 9, "5.10.4: the illustration lines were found", std::to_string(n) + " lines");
    std::printf("5.10.4: %d illustration lines loaded, %d skipped (arena twin)\n", n, skipped);
  } catch (const std::exception& e) { fail("5.10.4 extraction", e.what()); }

  std::printf("SPEC EXAMPLES SELFTEST %s (%d cases, %d failures) — %s\n", fails ? "RED" : "GREEN", cases, fails, spec.c_str());
  return fails ? 1 : 0;
}
