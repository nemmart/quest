// IRExec — the clone-side IR interpreter.
//
// THE SPEC IS docs/IR.md (consolidated, normative; spec-wins) — ir 4
// since Project 28 (rt_call; ir 3 grammar since Project 26).  The grammar parsed here (goto [labels] e, strict
// 0/1 booleans, s/u comparisons, class-homogeneous chains, the
// statement-root effectful family add/sub/mul/div/cvwn/ash/nadd/nsub/
// nmul, t-places, c/ovr and stack-register reads, ind()), the wp/bp/M8
// semantics (P25 byte addressing), the segment-wrap index rule, the
// refuse-on-anything loader posture, and the terminator discipline are
// all defined there; this file is the implementation.  Every loud
// executor fault (goto index, zero divisor, non-0/1 boolean) is listed
// in IR.md §5.  Census of what is lowered: docs/Project26/Census.md.  Where an operation corresponds to machine
// behavior, call the SAME emulator code paths the instruction would
// (Machine/Memory/EagleInstruction helpers) — never a local formula.
// Byte-EA derivation record: docs/Project25/ByteEA.md.
#include "IRExec.hpp"
#include "Machine.hpp"
#include "Memory.hpp"
#include "Decoder.hpp"
#include "Instruction.hpp"
#include "EagleInstruction.hpp"
#include "BlockSync.hpp"
#include "RTStubs.hpp"
#include "Lockstep.hpp"
#include "../debug/Capture.hpp"
#include "../debug/SymbolTable.hpp"
#include "../debug/Disassembler.hpp"
#include "Mapper.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <climits>
#include <cctype>

// SHA-256 (compact, public-domain style) — provenance check only.
namespace {
struct Sha256 {
  uint32_t h[8]; uint64_t len; uint8_t buf[64]; size_t fill;
  static uint32_t rr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
  Sha256() : len(0), fill(0) {
    static const uint32_t init[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                                     0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memcpy(h, init, sizeof h);
  }
  void block(const uint8_t* p) {
    static const uint32_t k[64] = {
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t w[64], a,b,c,d,e,f,g,hh;
    for (int i = 0; i < 16; i++)
      w[i] = (uint32_t(p[i*4])<<24)|(uint32_t(p[i*4+1])<<16)|(uint32_t(p[i*4+2])<<8)|p[i*4+3];
    for (int i = 16; i < 64; i++) {
      uint32_t s0 = rr(w[i-15],7)^rr(w[i-15],18)^(w[i-15]>>3);
      uint32_t s1 = rr(w[i-2],17)^rr(w[i-2],19)^(w[i-2]>>10);
      w[i] = w[i-16]+s0+w[i-7]+s1;
    }
    a=h[0];b=h[1];c=h[2];d=h[3];e=h[4];f=h[5];g=h[6];hh=h[7];
    for (int i = 0; i < 64; i++) {
      uint32_t s1 = rr(e,6)^rr(e,11)^rr(e,25), ch = (e&f)^(~e&g);
      uint32_t t1 = hh+s1+ch+k[i]+w[i];
      uint32_t s0 = rr(a,2)^rr(a,13)^rr(a,22), mj = (a&b)^(a&c)^(b&c);
      uint32_t t2 = s0+mj;
      hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
  }
  void update(const uint8_t* p, size_t n) {
    len += n;
    while (n) {
      size_t take = std::min(n, size_t(64) - fill);
      memcpy(buf + fill, p, take); fill += take; p += take; n -= take;
      if (fill == 64) { block(buf); fill = 0; }
    }
  }
  std::string hex() {
    uint64_t bits = len * 8;
    uint8_t pad = 0x80; update(&pad, 1);
    uint8_t z = 0;
    while (fill != 56) update(&z, 1);
    uint8_t l[8];
    for (int i = 0; i < 8; i++) l[i] = uint8_t(bits >> (56 - 8*i));
    update(l, 8);
    char out[65];
    for (int i = 0; i < 8; i++) snprintf(out + i*8, 9, "%08x", h[i]);
    return std::string(out, 64);
  }
};
std::string sha256_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return "";
  Sha256 s; char buf[1 << 16];
  while (f.read(buf, sizeof buf) || f.gcount())
    s.update(reinterpret_cast<uint8_t*>(buf), size_t(f.gcount()));
  return s.hex();
}
[[noreturn]] void refuse(const std::string& why) {
  throw std::runtime_error("IRExec: REFUSE: " + why);
}
} // namespace

namespace hw {

IRExec* IRExec::instance = nullptr;

// ------------------------------------------------------------ expression

struct IRExec::Expr {
  enum Kind { CONST, AC, TPLACE, CFLAG, OVRFLAG, WFP, WSP, WSB, WSL,
              MEM16, MEM32, MEM8, RESOLVE, IND,
              ADD, SUB, AND, OR, XOR, MUL, DIVS, DIVU, MODS, MODU,
              EQ, NE, LTS, LES, GTS, GES, LTU, LEU, GTU, GEU,
              LAND, LOR, LNOT, COM,       // && || ! ~
              WP, BP,                     // P25: wp(b,d) bp(b,d) pointer builders
              TF, LSH,                    // P26: tf(e), lsh(e, amount) (pure, flag-free)
              SX16, ZX16, ZX8, TRUNC16 } kind;
  uint32_t value = 0;                     // CONST value / AC index / t index
  std::shared_ptr<Expr> a, b;
};

namespace {
using Expr = IRExec::Expr;
using P = std::shared_ptr<Expr>;

// Operator classes: a flat chain must be homogeneous (docs/IR.md §5 —
// emitters parenthesize; the loader refuses mixed chains rather than
// owning a precedence table).  A comparison chain has exactly one op.
enum OpClass { OC_NONE, OC_ARITH, OC_CMP, OC_BOOL };

struct Parser {
  const char* s;
  uint32_t pc;                            // for messages
  explicit Parser(const char* text, uint32_t at) : s(text), pc(at) {}
  [[noreturn]] void bad(const std::string& why) {
    char buf[32]; snprintf(buf, sizeof buf, " at %08X near '", pc);
    refuse(why + buf + std::string(s).substr(0, 24) + "'");
  }
  void ws() { while (*s == ' ' || *s == '\t') s++; }
  bool lit(const char* t) {
    ws(); size_t n = strlen(t);
    if (strncmp(s, t, n) == 0) { s += n; return true; }
    return false;
  }
  // keyword: literal followed by a non-identifier char (so "wsp" != "wspx")
  bool kw(const char* t) {
    ws(); size_t n = strlen(t);
    if (strncmp(s, t, n) != 0) return false;
    char c = s[n];
    if (isalnum(uchar(c)) || c == '_') return false;
    s += n; return true;
  }
  static unsigned char uchar(char c) { return static_cast<unsigned char>(c); }
  P node(Expr::Kind k, P a = nullptr, P b = nullptr, uint32_t v = 0) {
    P e = std::make_shared<Expr>(); e->kind = k; e->a = a; e->b = b; e->value = v;
    return e;
  }
  P fn1(Expr::Kind k) { P e = expr(); if (!lit(")")) bad("expected )"); return node(k, e); }
  P fn2(Expr::Kind k, const char* name) {
    P a = expr(); if (!lit(",")) bad(std::string(name) + " expects two args");
    P b = expr(); if (!lit(")")) bad("expected )");
    return node(k, a, b);
  }
  bool at_effectful() {                   // effectful op names: root only (§5)
    ws();
    static const char* names[] = {"add(", "sub(", "mul(", "div(", "cvwn(", "ash(",
                                  "nadd(", "nsub(", "nmul(", nullptr};
    for (int i = 0; names[i]; i++)
      if (strncmp(s, names[i], strlen(names[i])) == 0) return true;
    return false;
  }
  P primary() {
    ws();
    if (at_effectful()) bad("effectful op (add/sub/mul/div/cvwn/ash/nadd/nsub/nmul) is statement-root only");
    if (lit("(")) { P e = expr(); if (!lit(")")) bad("expected )"); return e; }
    if (lit("~")) return node(Expr::COM, primary());          // word complement
    if (lit("!")) return node(Expr::LNOT, primary());         // boolean not (0/1 operand)
    if (lit("R[")) { P e = expr(); if (!lit("]")) bad("expected ]"); return node(Expr::RESOLVE, e); }
    if (lit("M32[")) { P e = expr(); if (!lit("]")) bad("expected ]"); return node(Expr::MEM32, e); }
    if (lit("M16[")) { P e = expr(); if (!lit("]")) bad("expected ]"); return node(Expr::MEM16, e); }
    if (lit("M8[")) { P e = expr(); if (!lit("]")) bad("expected ]"); return node(Expr::MEM8, e); }
    if (lit("M1[")) bad("M1 not implemented (IQ3)");
    if (lit("wp(")) return fn2(Expr::WP, "wp");                // P25 word-pointer builder
    if (lit("bp(")) return fn2(Expr::BP, "bp");                // P25 byte-pointer builder
    if (lit("lsh(")) return fn2(Expr::LSH, "lsh");             // P26 pure logical shift (ISA amount)
    if (lit("ind(")) return fn1(Expr::IND);                    // P26 resolve-from-value
    if (lit("tf(")) return fn1(Expr::TF);
    if (lit("sx16(")) return fn1(Expr::SX16);
    if (lit("zx16(")) return fn1(Expr::ZX16);
    if (lit("zx8(")) return fn1(Expr::ZX8);
    if (lit("trunc16(")) return fn1(Expr::TRUNC16);
    if (lit("and(") || lit("or(") || lit("xor(") || lit("com("))
      bad("functional and/or/xor/com are retired — use & | ^ ~");
    if (kw("wfp")) return node(Expr::WFP);
    if (kw("wsp")) return node(Expr::WSP);
    if (kw("wsb")) return node(Expr::WSB);
    if (kw("wsl")) return node(Expr::WSL);
    if (kw("ovr")) return node(Expr::OVRFLAG);
    if (kw("c"))   return node(Expr::CFLAG);
    if (lit("ac")) {
      if (*s < '0' || *s > '3') bad("bad ac index");
      return node(Expr::AC, nullptr, nullptr, uint32_t(*s++ - '0'));
    }
    if (*s == 't' && s[1] >= '1' && s[1] <= '9') {            // t-place t1..t255
      s++; char* end; unsigned long v = strtoul(s, &end, 10);
      if (v < 1 || v > 255) bad("t-place index must be 1..255");
      s = end;
      if (isalnum(uchar(*s)) || *s == '_') bad("bad t-place name");
      return node(Expr::TPLACE, nullptr, nullptr, uint32_t(v));
    }
    ws();
    bool neg = false;
    if (*s == '-') { neg = true; s++; }
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
      char* end; uint64_t v = strtoull(s, &end, 16);
      if (end == s || v > 0xFFFFFFFFull) bad("bad hex constant");
      s = end;
      if (*s == ':') {                  // P25: byte-pointer literal 0xW:b
        s++;                            // (dis fold notation; b = byte select)
        if (neg) bad("negative byte-pointer literal");
        if (*s != '0' && *s != '1') bad("byte select must be 0 or 1");
        uint32_t b = uint32_t(*s++ - '0');
        if (v > 0x7FFFFFFFull) bad("byte-pointer word address exceeds 31 bits");
        return node(Expr::CONST, nullptr, nullptr, uint32_t(v) * 2u + b);
      }
      return node(Expr::CONST, nullptr, nullptr, uint32_t(neg ? -int64_t(v) : int64_t(v)));
    }
    if (*s >= '0' && *s <= '9') {
      char* end; uint64_t v = strtoull(s, &end, 10);
      if (end == s || v > 0xFFFFFFFFull) bad("bad constant");
      s = end; return node(Expr::CONST, nullptr, nullptr, uint32_t(neg ? -int64_t(v) : int64_t(v)));
    }
    bad("expected primary");
  }
  // One flat left-associative chain of a single operator class.
  P expr() {
    P left = primary();
    OpClass cls = OC_NONE; int ncmp = 0;
    for (;;) {
      ws();
      Expr::Kind k; OpClass c;
      if      (lit("#"))   bad("#-ops are retired (ir 3): use add()/sub() at statement root");
      else if (lit("&&"))  { k = Expr::LAND; c = OC_BOOL; }
      else if (lit("||"))  { k = Expr::LOR;  c = OC_BOOL; }
      else if (lit("=="))  { k = Expr::EQ;  c = OC_CMP; }
      else if (lit("!="))  { k = Expr::NE;  c = OC_CMP; }
      else if (lit("<=s")) { k = Expr::LES; c = OC_CMP; }
      else if (lit("<=u")) { k = Expr::LEU; c = OC_CMP; }
      else if (lit(">=s")) { k = Expr::GES; c = OC_CMP; }
      else if (lit(">=u")) { k = Expr::GEU; c = OC_CMP; }
      else if (lit("<s"))  { k = Expr::LTS; c = OC_CMP; }
      else if (lit("<u"))  { k = Expr::LTU; c = OC_CMP; }
      else if (lit(">s"))  { k = Expr::GTS; c = OC_CMP; }
      else if (lit(">u"))  { k = Expr::GTU; c = OC_CMP; }
      else if (lit("<<") || lit(">>")) bad("C shifts are not IR (ir 3): use ash()/lsh()");
      else if (*s == '<' || *s == '>') bad("ordering comparison needs an s/u suffix (<s <=s >s >=s <u <=u >u >=u)");
      else if (lit("/s"))  { k = Expr::DIVS; c = OC_ARITH; }
      else if (lit("/u"))  { k = Expr::DIVU; c = OC_ARITH; }
      else if (lit("%s"))  { k = Expr::MODS; c = OC_ARITH; }
      else if (lit("%u"))  { k = Expr::MODU; c = OC_ARITH; }
      else if (*s == '/' || *s == '%') bad("bare / or % refused: use /s /u %s %u");
      else if (lit("*"))   { k = Expr::MUL; c = OC_ARITH; }
      else if (lit("+"))   { k = Expr::ADD; c = OC_ARITH; }
      else if (lit("-"))   { k = Expr::SUB; c = OC_ARITH; }
      else if (lit("&"))   { k = Expr::AND; c = OC_ARITH; }
      else if (lit("|"))   { k = Expr::OR;  c = OC_ARITH; }
      else if (lit("^"))   { k = Expr::XOR; c = OC_ARITH; }
      else break;
      if (cls != OC_NONE && cls != c)
        bad("mixed operator classes in one chain — parenthesize");
      cls = c;
      if (c == OC_CMP && ++ncmp > 1) bad("chained comparisons refused — parenthesize");
      left = node(k, left, primary());
    }
    return left;
  }
  void end() { ws(); if (*s) bad("trailing text"); }
};

// t-place static discipline (loader): straight-line block, so definite
// assignment is exact — every read must follow its (single) write.
void collect_treads(const P& e, std::vector<uint32_t>& out) {
  if (!e) return;
  if (e->kind == Expr::TPLACE) out.push_back(e->value);
  collect_treads(e->a, out); collect_treads(e->b, out);
}
} // namespace

// --------------------------------------------------------------- loading

IRExec* IRExec::load_from_env() {
  const char* path = getenv("QUEST_IR");
  if (!path)
    return nullptr;
  if (!Lockstep::enabled)
    refuse("QUEST_IR requires -lockstep (only the clone dispatches IR; "
           "a non-lockstep run would silently ignore it)");
  IRExec* ir = new IRExec();
  ir->load(path);
  return ir;
}

void IRExec::load(const std::string& path) {
  std::ifstream f(path);
  if (!f) refuse("cannot open QUEST_IR=" + path);

  // Optional pushmap cross-validation source (call-operand beliefs).
  struct SiteInfo { uint32_t marker; int pushes; };
  std::map<uint32_t, SiteInfo> sites;
  bool have_pushmap = false;
  if (const char* pm = getenv("QUEST_PUSH_MAP")) {
    std::ifstream pf(pm);
    if (pf) {
      have_pushmap = true;
      std::string l; int pending = 0;
      while (std::getline(pf, l)) {
        size_t h = l.find('#');
        if (h != std::string::npos) l = l.substr(0, h);
        std::istringstream is(l);
        std::string kind, pc, slot;
        int wides = 1;
        if (!(is >> kind >> pc >> slot)) continue;
        if (!(is >> wides)) wides = 1;
        if (kind == "push") pending += wides;   // args = elided WIDES
        else if (kind == "call") {
          sites[uint32_t(strtoul(pc.c_str(), nullptr, 16))] =
              SiteInfo{uint32_t(strtoul(slot.c_str(), nullptr, 16)), pending};
          pending = 0;
        }
      }
    }
  }

  std::string line;
  bool got_header = false, got_trailer = false;
  size_t trailer_count = 0;
  bool saw_blocks_sha = false;
  Block* cur = nullptr;
  uint32_t prev_ipc = 0;
  const char* blocks_env = getenv("QUEST_BLOCKS");
  std::vector<bool> tdef(256, false);          // P26: t-places defined so far in cur

  auto check_treads = [&](const P& e, const std::string& body) {
    std::vector<uint32_t> reads; collect_treads(e, reads);
    for (uint32_t t : reads)
      if (!tdef[t]) refuse("t" + std::to_string(t) + " read before its write: " + body);
  };
  auto close_block = [&]() {
    if (!cur) return;
    if (cur->stmts.empty()) refuse("empty block");
    Stmt::Kind k = cur->stmts.back().kind;
    if (k != Stmt::INSTR && k != Stmt::CALL && k != Stmt::RET && k != Stmt::GOTO && k != Stmt::RT_CALL)
      refuse("block does not end in a terminator (instruction/call/rt_call/ret/goto)");
    cur = nullptr;
    std::fill(tdef.begin(), tdef.end(), false);
  };

  while (std::getline(f, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
      line.pop_back();
    // comments: strip from ';' (grammar has no other use of ';')
    size_t sc = line.find(';');
    if (sc != std::string::npos) line = line.substr(0, sc);
    size_t e = line.find_last_not_of(" \t");
    line = (e == std::string::npos) ? "" : line.substr(0, e + 1);
    if (line.empty()) { close_block(); continue; }
    size_t b0 = line.find_first_not_of(" \t");
    std::string body = line.substr(b0);

    if (!got_header) {
      if (body != "ir 4")
        refuse("missing/unknown version header (want 'ir 4'; ir 3 files predate rt_call "
               "and the P28 leftovers — regenerate with tools/lower.py)");
      got_header = true;
      continue;
    }
    if (body.rfind("mode ", 0) == 0) {
      std::string m = body.substr(5);
      bool book_env = getenv("QUEST_ADDRESS_BOOK") && getenv("QUEST_PUSH_MAP");
      if (m == "book" && !book_env)
        refuse("book-mode quest.ir in a stock run (no QUEST_ADDRESS_BOOK/"
               "QUEST_PUSH_MAP): decorated-site lowering would bypass the "
               "stock push path. Re-emit without --book or run with the book.");
      if (m != "book" && m != "stock")
        refuse("unknown mode: " + m);
      continue;
    }
    std::istringstream is(body);
    std::string tok; is >> tok;

    if (tok == "source" || tok == "blocks" || tok == "pushmap" || tok == "argmap") {
      std::string fpath, sha;
      is >> fpath >> sha;
      if (tok == "blocks" && sha.empty() && fpath.find("sha256=") == std::string::npos) {
        // trailer form: "blocks <count>"
        got_trailer = true;
        trailer_count = strtoul(fpath.c_str(), nullptr, 10);
        continue;
      }
      if (sha.rfind("sha256=", 0) != 0) refuse("bad provenance line: " + body);
      sha = sha.substr(7);
      if (tok == "blocks") {
        if (!blocks_env) refuse("QUEST_BLOCKS not set; cannot verify blocks provenance");
        std::string got = sha256_file(blocks_env);
        if (got != sha)
          refuse("blocks provenance mismatch vs QUEST_BLOCKS=" + std::string(blocks_env));
        saw_blocks_sha = true;
      } else {
        std::string got = sha256_file(fpath);
        if (!got.empty() && got != sha)
          refuse(tok + " provenance mismatch for " + fpath);
      }
      continue;
    }
    if (tok == "block") {
      if (cur) refuse("block header inside block (missing blank line)");
      if (got_trailer) refuse("block after trailer");
      std::string pcs, segkw, segs;
      is >> pcs >> segkw >> segs;
      if (segkw != "seg") refuse("block header missing seg: " + body);
      blocks_.push_back(Block());
      cur = &blocks_.back();
      cur->start = uint32_t(strtoul(pcs.c_str(), nullptr, 16));
      cur->seg   = uint32_t(strtoul(segs.c_str(), nullptr, 16));
      if ((cur->start & 0xF0000000) != cur->seg)
        refuse("seg does not match block pc: " + body);
      if (cur->start == 0x7015BD6B)
        refuse("block 7015BD6B is on the exclusion list");
      if (!BlockSync::listed(cur->start))
        refuse("block " + pcs + " is not a listed quest.blocks start");
      prev_ipc = 0;
      std::fill(tdef.begin(), tdef.end(), false);
      continue;
    }
    if (!cur) refuse("content outside any block: " + body);

    Stmt st;
    if (tok[0] == '@') {
      st.kind = Stmt::INSTR;
      st.pc = uint32_t(strtoul(tok.c_str() + 1, nullptr, 16));
      if (prev_ipc && st.pc <= prev_ipc)
        refuse("non-monotonic instruction pc in block");
      prev_ipc = st.pc;
    } else if (tok == "call") {
      st.kind = Stmt::CALL;
      std::string t, a, m, si, r;
      is >> t >> a >> m >> si >> r;
      if (a.rfind("args=",0) || m.rfind("marker=",0) ||
          si.rfind("site=",0) || r.rfind("ret=",0))
        refuse("bad call operands: " + body);
      st.pc     = uint32_t(strtoul(si.c_str()+5, nullptr, 16));  // SITE pc
      st.ret    = uint32_t(strtoul(r.c_str()+4, nullptr, 16));   // validated belief
      st.target = uint32_t(strtoul(t.c_str(), nullptr, 16));
      st.args   = int32_t(strtol(a.c_str()+5, nullptr, 10));
      st.marker = uint32_t(strtoul(m.c_str()+7, nullptr, 16));
      if (st.pc == 0 || st.ret <= st.pc)
        refuse("bad call site/ret: " + body);
      if (have_pushmap) {
        auto it = sites.find(st.pc);
        if (it == sites.end())
          refuse("call site not in QUEST_PUSH_MAP: " + body);
        if (it->second.marker != st.marker || it->second.pushes != st.args)
          refuse("call operands disagree with pushmap: " + body);
      }
    } else if (tok == "rt_call") {
      // P28 (ir 4): `rt_call ?NAME(e1, ..., eN) site=<hex8>` — TERMINATOR.
      // The game -> runtime LCALL at `site` with its N argument pushes
      // folded into pure argument expressions in PL/I order (e1 = arg 1 =
      // the LAST push; the executor pushes eN first — RTBridge::arg_pointer
      // (n) = M32[wsp-2n]).  docs/IR.md §3/§6, docs/Project28/Census.md.
      st.kind = Stmt::RT_CALL;
      std::string rest; std::getline(is, rest);
      size_t b1 = rest.find_first_not_of(" \t");
      if (b1 == std::string::npos) refuse("rt_call without callee: " + body);
      rest = rest.substr(b1);
      size_t lp = rest.find('(');
      if (lp == std::string::npos) refuse("rt_call missing argument list: " + body);
      st.text = rest.substr(0, lp);
      if (st.text.size() < 2 || st.text[0] != '?')
        refuse("rt_call callee must be a `?` runtime symbol: " + body);
      for (char ch : st.text)
        if (!(isalnum(static_cast<unsigned char>(ch)) || ch == '?' || ch == '_' || ch == '.'))
          refuse("rt_call callee has a bad character: " + body);
      // argument list: balanced parens, split on top-level commas
      size_t depth = 0, rp = std::string::npos, argstart = lp + 1;
      std::vector<std::string> argtexts;
      for (size_t i = lp; i < rest.size(); i++) {
        char ch = rest[i];
        if (ch == '(' || ch == '[') depth++;
        else if (ch == ')' || ch == ']') {
          if (depth == 0) refuse("rt_call unbalanced ): " + body);
          depth--;
          if (depth == 0) { rp = i; break; }
        } else if (ch == ',' && depth == 1) {
          argtexts.push_back(rest.substr(argstart, i - argstart));
          argstart = i + 1;
        }
      }
      if (rp == std::string::npos) refuse("rt_call missing ): " + body);
      {
        std::string last = rest.substr(argstart, rp - argstart);
        if (last.find_first_not_of(" \t") != std::string::npos || !argtexts.empty())
          argtexts.push_back(last);
      }
      std::istringstream ts(rest.substr(rp + 1)); std::string si, tail;
      ts >> si;
      if (si.rfind("site=", 0) != 0 || si.size() != 13 ||
          si.find_first_not_of("0123456789ABCDEFabcdef", 5) != std::string::npos)
        refuse("rt_call needs site=<hex8>: " + body);
      if (ts >> tail) refuse("trailing text after rt_call site: " + body);
      st.pc = uint32_t(strtoul(si.c_str() + 5, nullptr, 16));
      if (st.pc == 0) refuse("bad rt_call site: " + body);
      if (prev_ipc && st.pc <= prev_ipc)
        refuse("rt_call site not after the block's last instruction: " + body);
      if (st.pc < cur->start) refuse("rt_call site before its block: " + body);
      if (!BlockSync::listed(st.pc + 4))
        refuse("rt_call return pc (site+4) is not a listed block start: " + body);
      for (const std::string& at : argtexts) {
        if (at.find_first_not_of(" \t") == std::string::npos)
          refuse("rt_call empty argument: " + body);
        Parser pa(at.c_str(), cur->start);
        P e = pa.expr(); pa.end();            // pure: the parser rejects effectful ops
        check_treads(e, body);
        st.argv.push_back(e);
      }
    } else if (tok == "ret") {
      st.kind = Stmt::RET;
    } else if (tok == "goto") {
      // P26 (ir 3): `goto [L0, L1, ...] e` — strict index, false=0/true=1
      // (docs/IR.md §3/§6).  Plain `goto L` is parser sugar for `goto [L] 0`.
      st.kind = Stmt::GOTO;
      std::string rest; std::getline(is, rest);
      size_t b1 = rest.find_first_not_of(" \t");
      if (b1 == std::string::npos) refuse("goto without target: " + body);
      rest = rest.substr(b1);
      if (rest[0] == '[') {
        size_t rb = rest.find(']');
        if (rb == std::string::npos) refuse("goto label list missing ]: " + body);
        std::string lst = rest.substr(1, rb - 1), idx = rest.substr(rb + 1);
        std::istringstream ls(lst); std::string lab;
        while (std::getline(ls, lab, ',')) {
          size_t x = lab.find_first_not_of(" \t"), y = lab.find_last_not_of(" \t");
          if (x == std::string::npos) refuse("empty goto label: " + body);
          lab = lab.substr(x, y - x + 1);
          if (lab.size() != 8 || lab.find_first_not_of("0123456789ABCDEFabcdef") != std::string::npos)
            refuse("goto label must be hex8: " + body);
          uint32_t L = uint32_t(strtoul(lab.c_str(), nullptr, 16));
          if (!BlockSync::listed(L)) refuse("goto target " + lab + " is not a listed block start");
          st.labels.push_back(L);
        }
        if (st.labels.empty()) refuse("goto with empty label list: " + body);
        Parser pe(idx.c_str(), cur->start);
        st.rhs = pe.expr(); pe.end();
        check_treads(st.rhs, body);
        if (st.labels.size() == 1 && !(st.rhs->kind == Expr::CONST && st.rhs->value == 0))
          refuse("single-label goto must use index 0: " + body);
      } else {
        std::istringstream ts(rest); std::string t; ts >> t;
        std::string tail; if (ts >> tail) refuse("trailing text after goto target: " + body);
        if (t.size() != 8 || t.find_first_not_of("0123456789ABCDEFabcdef") != std::string::npos)
          refuse("goto target must be hex8: " + body);
        uint32_t L = uint32_t(strtoul(t.c_str(), nullptr, 16));
        if (!BlockSync::listed(L)) refuse("goto target " + t + " is not a listed block start");
        st.labels.push_back(L);
        st.rhs = std::make_shared<Expr>(); st.rhs->kind = Expr::CONST; st.rhs->value = 0;
      }
    } else if (tok == "save") {
      refuse("'save' is reserved but not implemented this tranche");
    } else if (body.rfind("assert(", 0) == 0) {
      // P25: assert(expr) | assert(expr, "message").  Statement, never a
      // terminator.  The message may not contain '"' (grammar rule) and
      // cannot contain ';' by construction (comment stripping runs first).
      st.kind = Stmt::ASSERT;
      st.text = body;
      const char* p = body.c_str() + 7;          // past "assert("
      Parser pe(p, cur->start);
      P cond = pe.expr();
      std::string msg;
      pe.ws();
      if (*pe.s == ',') {
        pe.s++; pe.ws();
        if (*pe.s != '"') refuse("assert message must be a \"string\": " + body);
        pe.s++;
        const char* mstart = pe.s;
        while (*pe.s && *pe.s != '"') pe.s++;
        if (*pe.s != '"') refuse("assert message missing closing quote: " + body);
        msg.assign(mstart, pe.s - mstart);
        pe.s++;
      }
      pe.ws();
      if (*pe.s != ')') refuse("assert missing closing paren: " + body);
      pe.s++; pe.end();
      check_treads(cond, body);
      st.rhs = cond;
    } else {
      st.kind = Stmt::STMT;
      // The assignment '=' is the first '=' that is not part of '=='/'!='/'<='/'>='.
      size_t eq = std::string::npos;
      for (size_t i = 0; i + 1 < body.size(); i++) {
        if (body[i] == '=' && body[i+1] != '=' &&
            (i == 0 || (body[i-1] != '!' && body[i-1] != '<' && body[i-1] != '>' && body[i-1] != '='))) {
          eq = i; break;
        }
      }
      if (eq == std::string::npos)
        refuse("unrecognized line: " + body);
      std::string lhs = body.substr(0, eq), rhs = body.substr(eq + 1);
      Parser pl(lhs.c_str(), cur->start);
      P l = pl.primary(); pl.end();
      if (l->kind == Expr::WFP || l->kind == Expr::WSP || l->kind == Expr::WSB || l->kind == Expr::WSL)
        refuse("stack-register writes are reserved (P26 emits reads only): " + body);
      if (l->kind != Expr::AC && l->kind != Expr::MEM16 && l->kind != Expr::MEM32
          && l->kind != Expr::MEM8 && l->kind != Expr::TPLACE
          && l->kind != Expr::CFLAG && l->kind != Expr::OVRFLAG)
        refuse("lhs must be an ac, t-place, c, ovr, or memory cell: " + body);
      if (l->kind != Expr::AC && l->kind != Expr::TPLACE && l->kind != Expr::CFLAG
          && l->kind != Expr::OVRFLAG)
        check_treads(l->a, body);
      Parser pr(rhs.c_str(), cur->start);
      // Effectful root op?  (docs/IR.md §5: exactly one, at the root, args pure.)
      static const struct { const char* name; EffOp op; int nargs; } effs[] = {
        {"add(", EFF_ADD, 2}, {"sub(", EFF_SUB, 2}, {"mul(", EFF_MUL, 2}, {"div(", EFF_DIV, 2},
        {"cvwn(", EFF_CVWN, 1}, {"ash(", EFF_ASH, 2},
        {"nadd(", EFF_NADD, 2}, {"nsub(", EFF_NSUB, 2}, {"nmul(", EFF_NMUL, 2}, {nullptr, EFF_NONE, 0}};
      pr.ws();
      EffOp eff = EFF_NONE; int nargs = 0;
      for (int i = 0; effs[i].name; i++)
        if (strncmp(pr.s, effs[i].name, strlen(effs[i].name)) == 0) {
          eff = effs[i].op; nargs = effs[i].nargs; pr.s += strlen(effs[i].name); break;
        }
      if (eff != EFF_NONE) {
        P a = pr.expr();
        P b2;
        if (nargs == 2) { if (!pr.lit(",")) refuse("effectful op expects two args: " + body); b2 = pr.expr(); }
        if (!pr.lit(")")) refuse("effectful op missing ): " + body);
        pr.end();
        check_treads(a, body); if (b2) check_treads(b2, body);
        st.rhs = a; st.rhs2 = b2; st.eff = eff; st.flags = true;
      } else {
        P r = pr.expr(); pr.end();
        check_treads(r, body);
        st.rhs = r;
      }
      st.lhs = l;
      if (l->kind == Expr::TPLACE) {
        if (tdef[l->value]) refuse("t" + std::to_string(l->value) + " assigned twice in one block: " + body);
        tdef[l->value] = true;
      }
    }
    cur->stmts.push_back(st);
  }
  close_block();
  if (!saw_blocks_sha) refuse("missing blocks provenance line");
  if (!got_trailer) refuse("missing 'blocks <count>' trailer");
  if (trailer_count != blocks_.size()) {
    char buf[96];
    snprintf(buf, sizeof buf, "trailer says %zu blocks, file has %zu",
             trailer_count, blocks_.size());
    refuse(buf);
  }
  if (blocks_.empty()) refuse("quest.ir contains no blocks");
  std::sort(blocks_.begin(), blocks_.end(),
            [](const Block& a, const Block& b) { return a.start < b.start; });
  for (size_t i = 1; i < blocks_.size(); i++)
    if (blocks_[i].start == blocks_[i-1].start)
      refuse("duplicate block");
  // F6 fix (user ruling, Aug 29 2026 — Project24 REPORT §10.1, option c):
  // QUEST_INJECT/QUEST_TERMINAL arm a pc that Machine::run_steps tests on
  // ARRIVAL. The emulating master arrives at every instruction pc; an IR
  // clone arrives only at block entries — so an armed MID-BLOCK pc fires
  // one-sided and the pair diverges structurally (task 032 inj/abort
  // reds). Restore the tooling invariant "an armed pc is observed by both
  // engines": drop the IR block whose span contains a non-entry armed pc
  // — absent = emulated = symmetric, the master emulates regardless.
  // Entry-armed pcs stay lowered (task 032 inj3: entry arming pairs
  // correctly). Precision note: without span data the FLOOR block
  // (greatest start <= armed pc) is dropped; if the armed pc actually
  // sits in a listing hole past the block's end this drops one block
  // needlessly — conservative and harmless. An armed pc far past the
  // last game block (e.g. an rt-range QUEST_TERMINAL) is left alone.
  {
    auto drop_for = [&](const char* env_name) {
      const char* v = getenv(env_name);
      if (!v) return;
      uint32_t armed = uint32_t(strtoul(v, nullptr, 16));  // parse stops at ':'
      if (!armed) return;
      auto it = std::upper_bound(blocks_.begin(), blocks_.end(), armed,
                                 [](uint32_t a, const Block& b) { return a < b.start; });
      if (it == blocks_.begin()) return;      // below every game block
      --it;
      if (it->start == armed) return;         // entry-armed: arrival check works
      if (it + 1 == blocks_.end() && armed - it->start > 0x1000)
        return;                               // beyond the game range (rt pc)
      fprintf(stderr, "IRExec: %s=%08X is mid-block — dropping IR block %08X "
                      "(clone emulates it) so both engines observe the armed pc\n",
              env_name, armed, it->start);
      blocks_.erase(it);
    };
    drop_for("QUEST_INJECT");
    drop_for("QUEST_TERMINAL");
  }
  fprintf(stderr, "IRExec: loaded %zu IR blocks from %s\n", blocks_.size(), path.c_str());
}

const IRExec::Block* IRExec::find(uint32_t pc) const {
  auto it = std::lower_bound(blocks_.begin(), blocks_.end(), pc,
                             [](const Block& b, uint32_t v) { return b.start < v; });
  return (it != blocks_.end() && it->start == pc) ? &*it : nullptr;
}

bool IRExec::has(uint32_t pc) const { return find(pc) != nullptr; }

// ------------------------------------------------------------- execution

namespace {
[[noreturn]] void fault(const char* what, uint32_t blk) {
  char buf[128];
  snprintf(buf, sizeof buf, "IRExec: FAULT %s [IR block %08X]", what, blk);
  throw std::runtime_error(buf);          // loud (METHOD §8); never a silent value
}
struct Ctx {
  Machine& m;
  uint32_t seg;
  uint32_t blk;
  uint32_t ac[4];
  uint32_t t[256];                        // t-places (block-local; loader-checked)
  uint32_t wrap(uint32_t e) const { return (e & 0x0FFFFFFF) | seg; }
  uint32_t addr_of(const P& e) {          // MEM index rule: R result raw, else wrapped
    if (e->kind == Expr::RESOLVE)
      return eval(e);
    return wrap(eval(e));
  }
  uint32_t boolean(const P& e) {          // strict 0/1 operand (docs/IR.md §5)
    uint32_t v = eval(e);
    if (v > 1) fault("non-0/1 boolean operand", blk);
    return v;
  }
  uint32_t eval(const P& e) {
    switch (e->kind) {
      case Expr::CONST:   return e->value;
      case Expr::AC:      return ac[e->value];
      case Expr::TPLACE:  return t[e->value];
      case Expr::CFLAG:   return uint32_t(m.c);
      case Expr::OVRFLAG: return uint32_t(m.ovr);
      case Expr::WFP:     return uint32_t(m.wfp);       // LDAFP reads machine.wfp (EagleStack.cpp:527)
      case Expr::WSP:     return uint32_t(m.wsp);       // LDASP reads machine.wsp (:512)
      case Expr::WSB:     return uint32_t(m.wsb);
      case Expr::WSL:     return uint32_t(m.wsl);
      case Expr::MEM32:   return m.memory->read_wide(addr_of(e->a));
      case Expr::MEM16:   return m.memory->read_word(addr_of(e->a)) & 0xFFFF;
      case Expr::MEM8:    return m.memory->read_byte(eval(e->a)) & 0xFF;  // RAW index:
                          // byte pointers carry their own segment (bits 31:29);
                          // the hardware applies no wrap at use (P25 ruling).
      case Expr::RESOLVE: return m.eagle_resolve_indirect(wrap(eval(e->a)) | 0x80000000u);
      case Expr::IND:     return m.eagle_resolve_indirect(eval(e->a));   // P26: from the VALUE
      case Expr::ADD:     return eval(e->a) + eval(e->b);
      case Expr::SUB:     return eval(e->a) - eval(e->b);
      case Expr::AND:     return eval(e->a) & eval(e->b);
      case Expr::OR:      return eval(e->a) | eval(e->b);
      case Expr::XOR:     return eval(e->a) ^ eval(e->b);
      case Expr::MUL:     return eval(e->a) * eval(e->b);   // host mul, no flags (P25)
      case Expr::DIVS: case Expr::MODS: {                    // P26 pure signed divide: total, loud
        int32_t l = int32_t(eval(e->a)), r = int32_t(eval(e->b));
        if (r == 0) fault("zero divisor (/s %s)", blk);
        if (l == INT32_MIN && r == -1) fault("INT_MIN /s -1 overflow", blk);
        return uint32_t(e->kind == Expr::DIVS ? l / r : l % r);
      }
      case Expr::DIVU: case Expr::MODU: {
        uint32_t l = eval(e->a), r = eval(e->b);
        if (r == 0) fault("zero divisor (/u %u)", blk);
        return e->kind == Expr::DIVU ? l / r : l % r;
      }
      case Expr::EQ:  return eval(e->a) == eval(e->b);
      case Expr::NE:  return eval(e->a) != eval(e->b);
      case Expr::LTS: return int32_t(eval(e->a)) <  int32_t(eval(e->b));
      case Expr::LES: return int32_t(eval(e->a)) <= int32_t(eval(e->b));
      case Expr::GTS: return int32_t(eval(e->a)) >  int32_t(eval(e->b));
      case Expr::GES: return int32_t(eval(e->a)) >= int32_t(eval(e->b));
      case Expr::LTU: return eval(e->a) <  eval(e->b);
      case Expr::LEU: return eval(e->a) <= eval(e->b);
      case Expr::GTU: return eval(e->a) >  eval(e->b);
      case Expr::GEU: return eval(e->a) >= eval(e->b);
      case Expr::LAND: { uint32_t l = boolean(e->a), r = boolean(e->b); return l & r; }  // eager
      case Expr::LOR:  { uint32_t l = boolean(e->a), r = boolean(e->b); return l | r; }
      case Expr::LNOT: return boolean(e->a) ^ 1u;
      case Expr::COM:  return ~eval(e->a);
      case Expr::TF:   return eval(e->a) != 0;
      case Expr::LSH: {   // pure: EagleInstruction::logical_shift writes no flag
        uint32_t v = eval(e->a), n = eval(e->b);
        return uint32_t(EagleInstruction::logical_shift(m, int32_t(v), int32_t(n)));
      }
      case Expr::WP:      // wp(b,d): word segment wrap of b+d — Machine::copy_segment
        return ((eval(e->a) + eval(e->b)) & 0x0FFFFFFFu) | seg;
      case Expr::BP:      // bp(b,d): Machine::set_byte_segment(seg, b*2+d)
        return Machine::set_byte_segment((seg >> 28) & 0x7u,
                                         eval(e->a) * 2u + eval(e->b));
      case Expr::SX16:    return uint32_t(int32_t(int16_t(eval(e->a) & 0xFFFF)));
      case Expr::ZX16:    return eval(e->a) & 0xFFFF;
      case Expr::ZX8:     return eval(e->a) & 0xFF;
      case Expr::TRUNC16: return eval(e->a) & 0xFFFF;
    }
    throw std::runtime_error("IRExec: unreachable expr kind");
  }
  // Effectful root op: the SAME EagleInstruction helper the emulated
  // instruction calls (same-helpers principle, P23 ruling).  Arg order:
  // f(a, b) == helper(machine, src=b, dst=a)  (so sub(a,b) == a - b).
  uint32_t effectful(IRExec::EffOp op, uint32_t a, uint32_t b) {
    switch (op) {
      case IRExec::EFF_ADD:  return uint32_t(EagleInstruction::add(m, int64_t(int32_t(b)), int64_t(int32_t(a))));
      case IRExec::EFF_SUB:  return uint32_t(EagleInstruction::sub(m, int64_t(int32_t(b)), int64_t(int32_t(a))));
      case IRExec::EFF_MUL:  return uint32_t(EagleInstruction::mul(m, int64_t(int32_t(b)), int64_t(int32_t(a))));
      case IRExec::EFF_DIV:  return uint32_t(EagleInstruction::div(m, int32_t(b), int32_t(a)));
      case IRExec::EFF_CVWN: return uint32_t(EagleInstruction::cvwn(m, int32_t(a)));
      case IRExec::EFF_ASH:  return uint32_t(EagleInstruction::arithmetic_shift(m, int32_t(a), int32_t(b)));
      case IRExec::EFF_NADD: return uint32_t(EagleInstruction::narrow_add(m, int32_t(b), int32_t(a)));
      case IRExec::EFF_NSUB: return uint32_t(EagleInstruction::narrow_sub(m, int32_t(b), int32_t(a)));
      case IRExec::EFF_NMUL: return uint32_t(EagleInstruction::narrow_mul(m, int32_t(b), int32_t(a)));
      case IRExec::EFF_NONE: break;
    }
    throw std::runtime_error("IRExec: unreachable effectful op");
  }
};
} // namespace

uint32_t IRExec::run_block(Machine& machine, uint32_t pc) {
  Block* blk = const_cast<Block*>(find(pc));
  if (!blk)
    throw std::runtime_error("IRExec: run_block on absent block (dispatch bug)");
  if (!blk->executed) {
    blk->executed = true;
    fprintf(stderr, "IRExec: first execution of block %08X\n", pc);
  }
  Ctx cx{machine, blk->seg, blk->start, {uint32_t(machine.ac[0]), uint32_t(machine.ac[1]),
                             uint32_t(machine.ac[2]), uint32_t(machine.ac[3])}, {}};
  const size_t n = blk->stmts.size();
  const bool dbg = getenv("QUEST_IR_DEBUG_BLOCK") &&
      blk->start == uint32_t(strtoul(getenv("QUEST_IR_DEBUG_BLOCK"), nullptr, 16));

  // one machine instruction through the normal path (all hooks); returns
  // its new_pc. Locals materialized before, re-read after.
  auto run_instr = [&](uint32_t ipc) -> uint32_t {
    for (int r = 0; r < 4; r++) machine.ac[r] = int32_t(cx.ac[r]);
    machine.pc = int32_t(ipc);
    if (machine.rtcov)
      debug::Capture::check(machine);
    uint32_t opcode = machine.memory->read_instruction_word(ipc);
    Instruction* ins = Decoder::decode(machine.segments[(ipc >> 28) & 0x07]->lef, opcode);
    if (!ins) {
      char buf[64];
      snprintf(buf, sizeof buf, "Opcode %04X has not been defined", opcode);
      throw std::runtime_error(buf);
    }
    uint32_t new_pc = ins->execute(machine, ipc, opcode);
    if (new_pc != 0x30000000u) {
      if (machine.ovk > 0 && machine.ovr > 0) {
        char buf[64];
        snprintf(buf, sizeof buf, "Overflow occurred at %08X", ipc);
        throw std::runtime_error(buf);
      }
      machine.instruction_count++;
    }
    for (int r = 0; r < 4; r++) cx.ac[r] = uint32_t(machine.ac[r]);
    // stash decoded format for the continuation check
    last_format_ = &ins->instruction_format;
    return new_pc;
  };

  for (size_t i = 0; i < n; i++) {
    const Stmt& st = blk->stmts[i];
    switch (st.kind) {
      case Stmt::INSTR: {
        uint32_t new_pc = run_instr(st.pc);
        if (new_pc == 0x30000000u)
          return new_pc;                  // syscall sentinel: batch machinery
        if (i + 1 == n)
          return new_pc;                  // terminator instruction
        // Continuation tripwire (IR2.md §3): straight-line by construction;
        // expected next = addr + decoder length. Fault/OS edges exit.
        uint32_t expected = st.pc +
            uint32_t(debug::Disassembler::word_length(*last_format_));
        if (new_pc == expected)
          break;
        if (new_pc < BlockSync::game_start || new_pc >= BlockSync::game_stop)
          return new_pc;                  // fault edge (e.g. WSAVS stack fault)
        {
          char buf[112];
          snprintf(buf, sizeof buf,
                   "IRExec: non-final instruction at %08X continued to %08X, "
                   "expected %08X", st.pc, new_pc, expected);
          throw std::runtime_error(buf);
        }
      }
      case Stmt::ASSERT: {
        // P25: condition true -> free; condition 0 -> the clone's own IR
        // is wrong by its own declaration.  Print the statement and
        // DETACH (user ruling): master (ground truth) continues
        // unverified; the throw ends this clone batch, which the
        // compare_pair detached early-out then skips.
        if (cx.eval(st.rhs) != 0)
          break;
        char rep[512];
        snprintf(rep, sizeof rep,
                 "IR ASSERT FAILED [block %08X stmt %zu]: %s",
                 blk->start, i, st.text.c_str());
        if (machine.lockstep_role == Lockstep::CLONE) {
          Lockstep::assert_detach(&machine, rep);
          throw std::runtime_error("IR assert failed (clone detached)");
        }
        throw std::runtime_error(rep);   // non-lockstep: loud, METHOD S8
      }
      case Stmt::STMT: {
        if (dbg)
          fprintf(stderr, "IRExec DEBUG blk %08X stmt %zu: acs=%08X %08X %08X %08X\n",
                  blk->start, i, cx.ac[0], cx.ac[1], cx.ac[2], cx.ac[3]);
        try {
        uint32_t v;
        if (st.eff != EFF_NONE) {           // pure args first, then the shared helper
          uint32_t a = cx.eval(st.rhs);
          uint32_t b = st.rhs2 ? cx.eval(st.rhs2) : 0;
          v = cx.effectful(st.eff, a, b);
        } else
          v = cx.eval(st.rhs);
        const Expr& l = *st.lhs;
        if (l.kind == Expr::AC)
          cx.ac[l.value] = v;
        else if (l.kind == Expr::TPLACE)
          cx.t[l.value] = v;
        else if (l.kind == Expr::CFLAG || l.kind == Expr::OVRFLAG) {
          if (v > 1) throw std::runtime_error("IRExec: FAULT non-0/1 flag assignment");
          (l.kind == Expr::CFLAG ? machine.c : machine.ovr) = int32_t(v);
        }
        else if (l.kind == Expr::MEM32)
          machine.memory->write_wide(cx.addr_of(l.a), v);
        else if (l.kind == Expr::MEM8)
          machine.memory->write_byte(cx.eval(l.a), v & 0xFF);   // RAW index (P25)
        else
          machine.memory->write_word(cx.addr_of(l.a), v & 0xFFFF);  // M16 store truncates (§5)
        } catch (std::runtime_error& ex) {
          char buf[192];
          bool mem = st.lhs->kind == Expr::MEM32 || st.lhs->kind == Expr::MEM16 || st.lhs->kind == Expr::MEM8;
          uint32_t a = mem ? cx.addr_of(st.lhs->a) : 0;
          snprintf(buf, sizeof buf, "%s [IR block %08X stmt %zu, store addr %08X]",
                   ex.what(), blk->start, i, a);
          throw std::runtime_error(buf);
        }
        if (st.flags && machine.ovk > 0 && machine.ovr > 0) {
          char buf[64];
          snprintf(buf, sizeof buf, "Overflow occurred in block %08X", blk->start);
          throw std::runtime_error(buf);  // attribution: block (IR2.md §6)
        }
        break;
      }
      case Stmt::CALL: {
        if (i + 1 != n)
          throw std::runtime_error("IRExec: interior call (loader bug)");
        // Copied-args accounting batched at the call (IR2.md §4): the
        // block's argpush statements were pure stores; the master pushed.
        machine.pc = int32_t(st.pc);
        if (st.args > 0)
          machine.mapper.note_arg_write(machine, st.args);
        // The instruction at site IS the decorated call (LCALL or XCALL):
        // byte-exact protocol via the shared path. ret= is a validated
        // belief only — ac3 comes from the instruction; a disagreement
        // surfaces at the next pair.
        return run_instr(st.pc);
      }
      case Stmt::RT_CALL: {
        if (i + 1 != n)
          throw std::runtime_error("IRExec: interior rt_call (loader bug)");
        // P28: evaluate every argument first (pure; order unobservable),
        // then push them RIGHT TO LEFT through the SAME helper XPEF/LPEF/
        // WPSH use (Machine::wide_push — owner of the wsp>wsl overflow
        // fault), then run the LCALL at `site` through the normal
        // instruction path exactly as `call` does.  machine.pc = site
        // before the pushes so wide_push's copy_segment(pc, wsp) folds the
        // block's segment and its fault text names a real pc (the master's
        // would name the push's own pc — a recorded difference on a fault
        // that has never fired; Census.md F6).
        std::vector<uint32_t> vals;
        vals.reserve(st.argv.size());
        for (const auto& e : st.argv) vals.push_back(cx.eval(e));
        for (int r = 0; r < 4; r++) machine.ac[r] = int32_t(cx.ac[r]);
        machine.pc = int32_t(st.pc);
        // Declared-belief check against the real instruction words (cheap:
        // two reads).  LCALL opcode pattern 101ii11011001001 (Decoder.cpp);
        // argc field is the word at site+3 (EagleStack.cpp:239).
        {
          uint32_t op = machine.memory->read_instruction_word(st.pc) & 0xFFFF;
          uint32_t argc = machine.memory->read_word(Machine::copy_segment(st.pc, st.pc + 3)) & 0x7FFF;
          if ((op & 0xE7FF) != 0xA6C9 || argc != st.argv.size()) {
            char buf[160];
            snprintf(buf, sizeof buf, "IRExec: rt_call %s at %08X: site word %04X / argc %u disagree "
                     "with the IR's %zu arguments", st.text.c_str(), st.pc, op, argc, st.argv.size());
            throw std::runtime_error(buf);
          }
          if (machine.symbols) {
            uint32_t sym = machine.symbols->address_for_name(st.text);
            uint32_t ii = (op >> 11) & 3;
            uint32_t tgt = machine.eagle_l_resolve_indirect(Machine::copy_segment(st.pc, st.pc + 1), ii);
            if (sym != 0xFFFFFFFF && sym != tgt) {
              char buf[160];
              snprintf(buf, sizeof buf, "IRExec: rt_call %s at %08X resolves to %08X, symbol is %08X",
                       st.text.c_str(), st.pc, tgt, sym);
              throw std::runtime_error(buf);
            }
          }
        }
        for (size_t k = vals.size(); k-- > 0; )
          machine.wide_push(int32_t(vals[k]));
        for (int r = 0; r < 4; r++) cx.ac[r] = uint32_t(machine.ac[r]);
        return run_instr(st.pc);
      }
      case Stmt::RET: {
        if (i + 1 != n)
          throw std::runtime_error("IRExec: interior ret (loader bug)");
        // WRTN is address-independent (abort-message use only): execute
        // its fixed opcode through the normal decode path.
        for (int r = 0; r < 4; r++) machine.ac[r] = int32_t(cx.ac[r]);
        machine.pc = int32_t(blk->start);
        Instruction* ins = Decoder::decode(
            machine.segments[(blk->start >> 28) & 0x07]->lef, 0x87A9);
        if (!ins) throw std::runtime_error("IRExec: WRTN opcode undecodable");
        uint32_t new_pc = ins->execute(machine, blk->start, 0x87A9);
        if (machine.ovk > 0 && machine.ovr > 0) {
          char buf[64];
          snprintf(buf, sizeof buf, "Overflow occurred in block %08X", blk->start);
          throw std::runtime_error(buf);
        }
        machine.instruction_count++;
        return new_pc;
      }
      case Stmt::GOTO: {
        if (i + 1 != n)
          throw std::runtime_error("IRExec: interior goto (loader bug)");
        // P26: strict index into the label table — out of range is a loud
        // fault (no defined arm to take; MathDesign §1: no coercion).
        uint32_t idx = cx.eval(st.rhs);
        if (idx >= st.labels.size()) {
          char buf[128];
          snprintf(buf, sizeof buf, "IRExec: FAULT goto index %u out of range [0,%zu) [IR block %08X]",
                   idx, st.labels.size(), blk->start);
          throw std::runtime_error(buf);
        }
        for (int r = 0; r < 4; r++) machine.ac[r] = int32_t(cx.ac[r]);
        return st.labels[idx];
      }
    }
  }
  throw std::runtime_error("IRExec: block fell off the end (loader bug)");
}

} // namespace hw
