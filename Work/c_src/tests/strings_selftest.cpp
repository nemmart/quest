// tests/strings_selftest.cpp — Project 30 self-test for hw/strings/ and
// the Mapper's arena form. Not part of the emulator build; see
// tests/run_strings_selftest.sh (which also builds the -DP30_BROKEN_RESIDUE
// variant and requires it to go RED — the test has teeth).
//
// Part 1: every library operation is run against EagleSpecial's own
// WCMV/WCMP/WBLM arm on a SECOND Machine+Memory with identical contents,
// over a brute-forced operand space (lengths 0..300 both sides, equal /
// shorter / longer, blank padding, truncation, overlap, descending counts;
// WCMP equal / prefix / mismatch-at-k; WBLM copies and the self-overlapping
// fill). After each case ac0-ac3, c, ovr and EVERY byte of the scratch
// region are compared field by field. Exceptions (G-2, G-3) must match by
// text and leave identical state.
// Part 2: the arena form — codec rows, closed-end containment, the two
// events, the refusals (clone_location / frame_precedes), the bind and
// layout invariants, and that non-arena verdicts are untouched.
// Part 3: the Δ accumulator.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include "hw/Machine.hpp"
#include "hw/Memory.hpp"
#include "hw/EagleSpecial.hpp"
#include "hw/Mapper.hpp"
#include "hw/strings/EagleString.hpp"
#include "hw/strings/ClaimDelta.hpp"
#include "os/ArrayPage.hpp"
using namespace hw;
using namespace hw::strings;

static int fails = 0, cases = 0;
static void fail(const char* what, const char* detail) {
  if(fails++ < 30) std::printf("FAIL %s: %s\n", what, detail);
}
static void expect(bool ok, const char* what, const char* detail = "") { if(!ok) fail(what, detail); }

// ---- fixture ---------------------------------------------------------------

// Scratch region: 8 pages (8,192 words / 16,384 bytes) at word 0x70100000,
// segment 7 in both the word form (>>28) and the byte form (>>29). Plus one
// page at 0x70000000 (a descending copy that runs below byte 0xE0000000
// leaves byte-segment 7) and one page in segment 6 (0x60100000) for the
// immediate-throw crossing cases.
static constexpr uint32_t REGION = 0x70100000u, REGION_WORDS = 8u * 1024u;
static constexpr uint32_t LOWPAGE = 0x70000000u;
static constexpr uint32_t SEG6    = 0x60100000u;
static constexpr uint32_t SITE    = 0x70160000u;   // a code address in segment 7

struct Rig {
  Memory memory;
  Machine machine;
  std::vector<os::ArrayPage*> pages;
  Rig() : machine(nullptr, nullptr, nullptr, &memory) {
    map(REGION, REGION_WORDS / 1024); map(LOWPAGE, 1); map(SEG6, 1);
    machine.zero_claims = false;
    machine.wfp = 0x70001000; machine.wsp = 0x70001040; machine.wsl = 0x70010000; machine.wsb = 0x70000000;
  }
  void map(uint32_t word_base, uint32_t n) {
    for(uint32_t i = 0; i < n; i++) {
      os::ArrayPage* p = new os::ArrayPage();
      pages.push_back(p);
      memory.map_page(p, (word_base >> 10) + i, Permissions::PERMISSIONS_READ_WRITE_EXECUTE);
    }
  }
  void seed(uint32_t s) {
    for(os::ArrayPage* p : pages)
      for(uint8_t& b : p->bytes) { s = s * 1103515245u + 12345u; b = static_cast<uint8_t>(s >> 16); }
  }
  void copy_from(const Rig& o) {
    for(size_t i = 0; i < pages.size(); i++) pages[i]->bytes = o.pages[i]->bytes;
    machine.ac[0] = o.machine.ac[0]; machine.ac[1] = o.machine.ac[1];
    machine.ac[2] = o.machine.ac[2]; machine.ac[3] = o.machine.ac[3];
    machine.c = o.machine.c; machine.ovr = o.machine.ovr;
  }
  void regs(int32_t a0, int32_t a1, int32_t a2, int32_t a3, int32_t c, int32_t ovr) {
    machine.ac[0] = a0; machine.ac[1] = a1; machine.ac[2] = a2; machine.ac[3] = a3;
    machine.c = c; machine.ovr = ovr;
  }
};

static EagleSpecial make(int32_t oper, const char* name) {
  EagleSpecial e; e.setup(0, name, "", oper); return e;
}

// Run `lib` on B and the oracle instruction on A (registers preloaded by the
// caller), then compare everything. Exceptions must agree by text.
// Returns the (agreed) exception text, empty when neither side threw.
template<class Lib>
static std::string agree(const char* what, Rig& A, Rig& B, EagleSpecial& ins, Lib lib) {
  cases++;
  std::string ea, eb;
  try { ins.execute(A.machine, SITE, 0); } catch(std::exception& e) { ea = e.what(); }
  try { lib(); } catch(std::exception& e) { eb = e.what(); }
  char buf[200];
  if(ea != eb) {
    snprintf(buf, sizeof(buf), "exception oracle='%s' lib='%s'", ea.c_str(), eb.c_str());
    fail(what, buf); return ea;
  }
  const char* fields[6] = {"ac0", "ac1", "ac2", "ac3", "c", "ovr"};
  int32_t va[6] = {A.machine.ac[0], A.machine.ac[1], A.machine.ac[2], A.machine.ac[3], A.machine.c, A.machine.ovr};
  int32_t vb[6] = {B.machine.ac[0], B.machine.ac[1], B.machine.ac[2], B.machine.ac[3], B.machine.c, B.machine.ovr};
  for(int i = 0; i < 6; i++)
    if(va[i] != vb[i]) {
      snprintf(buf, sizeof(buf), "%s oracle=%08X lib=%08X", fields[i], (uint32_t)va[i], (uint32_t)vb[i]);
      fail(what, buf);
    }
  for(size_t p = 0; p < A.pages.size(); p++)
    if(A.pages[p]->bytes != B.pages[p]->bytes) {
      size_t k = 0; while(A.pages[p]->bytes[k] == B.pages[p]->bytes[k]) k++;
      snprintf(buf, sizeof(buf), "memory differs page %zu byte %zu oracle=%02X lib=%02X", p, k,
               A.pages[p]->bytes[k], B.pages[p]->bytes[k]);
      fail(what, buf); break;
    }
  return ea;
}

static const int32_t LENS[] = {0, 1, 2, 3, 5, 7, 8, 15, 16, 17, 31, 32, 63, 64, 100, 127, 128, 200, 255, 256, 300};
static constexpr uint32_t BASE_BP = REGION * 2u;   // byte pointer of the region

int main() {
  // Memory carries a 2 MB permissions array inline: Rigs live on the heap.
  Rig& A = *new Rig; Rig& B = *new Rig;
  EagleSpecial wcmv = make(EagleSpecial::WCMV, "WCMV");
  EagleSpecial wcmp = make(EagleSpecial::WCMP, "WCMP");
  EagleSpecial wblm = make(EagleSpecial::WBLM, "WBLM");
  uint32_t seed = 1;
  auto fresh = [&](int32_t a0, int32_t a1, int32_t a2, int32_t a3) {
    A.seed(++seed); A.regs(a0, a1, a2, a3, seed & 1, (seed >> 1) & 1); B.copy_from(A);
  };

  // ---- 1a. copy: every (n, len) pair, four layouts each ----
  for(int32_t n : LENS)
    for(int32_t len : LENS)
      for(int layout = 0; layout < 4; layout++) {
        uint32_t dst, src;
        switch(layout) {
          case 0: dst = BASE_BP + 1000; src = BASE_BP + 5000; break;              // disjoint
          case 1: dst = BASE_BP + 5001; src = BASE_BP + 5000 + 1; src -= 0; dst = src + 1; break; // dst = src+1 (smear forward)
          case 2: src = BASE_BP + 7001; dst = src - 1; break;                      // dst = src-1
          default: dst = BASE_BP + 3000; src = dst; break;                         // identical
        }
        fresh(n, len, (int32_t)dst, (int32_t)src);
        char what[64]; snprintf(what, sizeof(what), "copy n=%d len=%d layout=%d", n, len, layout);
        agree(what, A, B, wcmv, [&] { copy(B.machine, SITE, dst, n, EagleString::literal(src, len)); });
      }
  // 1b. copy with descending counts (the emulator's direction() model)
  for(int32_t n : {-1, -2, -7, -16, -100, 0, 3, 20})
    for(int32_t len : {-1, -3, -20, -100, 0, 5, 50}) {
      uint32_t dst = BASE_BP + 4000, src = BASE_BP + 6000;
      fresh(n, len, (int32_t)dst, (int32_t)src);
      char what[64]; snprintf(what, sizeof(what), "copy-desc n=%d len=%d", n, len);
      agree(what, A, B, wcmv, [&] { copy(B.machine, SITE, dst, n, EagleString::literal(src, len)); });
    }
  // 1c. pieces: located varying and substr as sources; fixed as destination
  for(int32_t cap : LENS)
    for(int32_t len : LENS) {
      uint32_t lw = REGION + 100;           // length word; data at REGION+101
      fresh(0, 0, 0, 0);
      A.memory.write_word(lw, len); B.memory.write_word(lw, len);
      uint32_t dst = BASE_BP + 2000;
      A.regs(cap, len, (int32_t)dst, (int32_t)((lw + 1) * 2), 0, 0); B.copy_from(A);
      char what[64]; snprintf(what, sizeof(what), "assign_fixed<-varying cap=%d len=%d", cap, len);
      agree(what, A, B, wcmv, [&] { assign_fixed(B.machine, SITE, dst, cap, EagleString::varying(B.memory, lw)); });
      // substr(piece, i, m) with the count clipped like the compiler does
      int32_t i = len / 3, msub = len - i;
      A.regs(cap, msub, (int32_t)dst, (int32_t)((lw + 1) * 2 + i), 1, 0); B.copy_from(A);
      snprintf(what, sizeof(what), "assign_fixed<-substr cap=%d len=%d i=%d", cap, len, i);
      agree(what, A, B, wcmv, [&] { assign_fixed(B.machine, SITE, dst, cap, EagleString::varying(B.memory, lw).substr(i, msub)); });
    }
  // 1d. assign_varying: length word = min(len, cap), copy of min(len, cap)
  for(int32_t cap : LENS)
    for(int32_t len : LENS) {
      uint32_t src = BASE_BP + 9000, lw = REGION + 200;
      int32_t count = len < cap ? len : cap;
      fresh(count, len, (int32_t)((lw + 1) * 2), (int32_t)src);
      A.memory.write_word(lw, count);                        // the master's XNSTA before its WCMV
      char what[64]; snprintf(what, sizeof(what), "assign_varying cap=%d len=%d", cap, len);
      agree(what, A, B, wcmv, [&] {
        int32_t got = assign_varying(B.machine, SITE, lw, cap, EagleString::literal(src, len));
        expect(got == count, what, "returned count");
        expect((int32_t)B.memory.read_word(lw) == count, what, "length word");
      });
    }
  // 1e. append_char is one write_byte
  {
    fresh(0, 0, 0, 0);
    uint32_t bp = BASE_BP + 1234;
    append_char(B.machine, bp, 0x0A);
    expect(B.memory.read_byte(bp) == 0x0A, "append_char", "byte");
    expect(B.memory.read_byte(bp + 1) == A.memory.read_byte(bp + 1) && B.memory.read_byte(bp - 1) == A.memory.read_byte(bp - 1), "append_char", "neighbours");
    expect(B.machine.ac[0] == A.machine.ac[0] && B.machine.ac[2] == A.machine.ac[2] && B.machine.c == A.machine.c, "append_char", "residues untouched");
    cases++;
  }

  // ---- 2. compare: equal / prefix / mismatch-at-k / both zero ----
  auto put = [&](Rig& r, uint32_t bp, const std::string& s) { for(size_t i = 0; i < s.size(); i++) r.memory.write_byte(bp + i, (uint8_t)s[i]); };
  for(int32_t n1 : LENS)
    for(int32_t n2 : LENS) {
      uint32_t s1 = BASE_BP + 1000, s2 = BASE_BP + 4000;
      int32_t nmax = n1 > n2 ? n1 : n2;
      // the k positions to perturb: each byte for short strings, a spread for long ones
      std::vector<int32_t> ks = {-1};
      for(int32_t k = 0; k < nmax; k += (nmax > 40 ? 13 : 1)) ks.push_back(k);
      if(nmax > 0) ks.push_back(nmax - 1);
      for(int32_t k : ks)
        for(int dir = 0; dir < 2; dir++) {
          std::string a(n1, 'x'), b(n2, 'x');
          for(int32_t i = 0; i < nmax; i++) { char c = (char)('A' + (i * 7) % 26); if(i < n1) a[i] = c; if(i < n2) b[i] = c; }
          // beyond the shorter: blanks in the longer → equal after padding unless perturbed
          for(int32_t i = n2; i < n1; i++) a[i] = ' ';
          for(int32_t i = n1; i < n2; i++) b[i] = ' ';
          if(k >= 0) {   // mismatch at k: raise one side (dir picks which)
            if(k < n1 && dir == 0) a[k] = (char)(a[k] + 1);
            else if(k < n2 && dir == 1) b[k] = (char)(b[k] + 1);
            else if(k < n1) a[k] = (char)(a[k] - 1);
            else b[k] = (char)(b[k] - 1);
          }
          fresh(n2, n1, (int32_t)s2, (int32_t)s1);
          put(A, s1, a); put(A, s2, b); put(B, s1, a); put(B, s2, b);
          char what[80]; snprintf(what, sizeof(what), "compare n1=%d n2=%d k=%d dir=%d", n1, n2, k, dir);
          bool ref = pad_equal(B.memory, EagleString::literal(s1, n1), EagleString::literal(s2, n2));
          agree(what, A, B, wcmp, [&] {
            int32_t r = residues_after_compare(B.machine, SITE, s2, n2, s1, n1);
            expect(r == B.machine.ac[1], what, "return value == ac1");
            expect((r == 0) == ref, what, "pad_equal disagrees with WCMP == 0");
          });
          if(k < 0 && (n1 || n2)) expect(A.machine.ac[1] == 0, what, "equal case should compare equal");
          if(k >= 0 && k < nmax) expect(A.machine.ac[1] != 0, what, "perturbed case should mismatch");
        }
    }
  // 2b. descending counts and unsigned byte order (0x80 vs 0x7F)
  for(int32_t n1 : {-3, -1, 0, 2}) for(int32_t n2 : {-2, 0, 1, 4}) {
    uint32_t s1 = BASE_BP + 1500, s2 = BASE_BP + 4500;
    fresh(n2, n1, (int32_t)s2, (int32_t)s1);
    char what[64]; snprintf(what, sizeof(what), "compare-desc n1=%d n2=%d", n1, n2);
    agree(what, A, B, wcmp, [&] { residues_after_compare(B.machine, SITE, s2, n2, s1, n1); });
  }
  {
    uint32_t s1 = BASE_BP + 1600, s2 = BASE_BP + 4600;
    fresh(1, 1, (int32_t)s2, (int32_t)s1);
    A.memory.write_byte(s1, 0x80); A.memory.write_byte(s2, 0x7F); B.copy_from(A);
    agree("compare-unsigned", A, B, wcmp, [&] { residues_after_compare(B.machine, SITE, s2, 1, s1, 1); });
    expect(A.machine.ac[1] == 1, "compare-unsigned", "0x80 > 0x7F unsigned");
  }

  // ---- 3. block move: copies and the self-overlapping fills ----
  for(int32_t k : {0, 1, 2, 3, 4, 10, 64, 100, 300})
    for(int layout = 0; layout < 5; layout++) {
      uint32_t dst = REGION + 2000, src;
      switch(layout) {
        case 0: src = REGION + 4000; break;            // disjoint copy
        case 1: src = dst - 2; break;                  // the fill idiom (Census F6): smears two words forward
        case 2: src = dst - 1; break;                  // one-word smear
        case 3: src = dst + 1; break;                  // overlap the other way
        default: src = dst; break;
      }
      fresh(0x11111111, k, (int32_t)src, (int32_t)dst);
      char what[64]; snprintf(what, sizeof(what), "blm k=%d layout=%d", k, layout);
      agree(what, A, B, wblm, [&] { block_move(B.machine, SITE, dst, src, k); });
      if(layout == 1 && k >= 4) {   // the fill really is a fill
        uint32_t w0 = B.memory.read_word(dst - 2), w1 = B.memory.read_word(dst - 1);
        bool ok = true;
        for(int32_t i = 0; i < k; i++) ok = ok && (B.memory.read_word(dst + i) == ((i & 1) ? w1 : w0));
        expect(ok, what, "fill pattern");
      }
    }
  for(int32_t k : {-1, -2, -5, -100}) {
    uint32_t dst = REGION + 3000, src = REGION + 5000;
    fresh(0, k, (int32_t)src, (int32_t)dst);
    char what[64]; snprintf(what, sizeof(what), "blm-desc k=%d", k);
    agree(what, A, B, wblm, [&] { block_move(B.machine, SITE, dst, src, k); });
  }

  // ---- 4. negatives: the emulator's guards, reproduced ----
  {
    // G-3: source in segment 6 while the instruction is in segment 7 → throws before any byte
    uint32_t dst = BASE_BP + 100, src = SEG6 * 2u + 10;
    fresh(10, 10, (int32_t)dst, (int32_t)src);
    std::string t;
    t = agree("G-3 copy src seg6", A, B, wcmv, [&] { copy(B.machine, SITE, dst, 10, EagleString::literal(src, 10)); });
    expect(t == "WCMV: crossing segments not allowed", "G-3 copy src seg6", t.c_str());
    fresh(10, 10, (int32_t)src, (int32_t)dst);
    t = agree("G-3 copy dst seg6", A, B, wcmv, [&] { copy(B.machine, SITE, src, 10, EagleString::literal(dst, 10)); });
    expect(t == "WCMV: crossing segments not allowed", "G-3 copy dst seg6", t.c_str());
    fresh(5, 5, (int32_t)dst, (int32_t)src);
    t = agree("G-3 compare seg6", A, B, wcmp, [&] { residues_after_compare(B.machine, SITE, dst, 5, src, 5); });
    expect(t == "WCMP: crossing segments not allowed", "G-3 compare seg6", t.c_str());
    fresh(0, 5, (int32_t)SEG6, (int32_t)(REGION + 50));
    t = agree("G-3 blm seg6", A, B, wblm, [&] { block_move(B.machine, SITE, REGION + 50, SEG6, 5); });
    expect(t == "WBLM: crossing segments not allowed", "G-3 blm seg6", t.c_str());
    // G-3 mid-copy: a descending copy that walks below byte 0xE0000000 (segment 7 → 6)
    // after writing three bytes — both sides throw with the same bytes already written
    uint32_t low = LOWPAGE * 2u + 2;   // byte 0xE0000002
    fresh(-6, -6, (int32_t)low, (int32_t)(BASE_BP + 300));
    t = agree("G-3 copy mid-run", A, B, wcmv, [&] { copy(B.machine, SITE, low, -6, EagleString::literal(BASE_BP + 300, -6)); });
    expect(t == "WCMV: crossing segments not allowed", "G-3 copy mid-run", t.c_str());
    expect(A.machine.ac[0] == -6, "G-3 copy mid-run", "oracle threw before writing registers");
    expect(A.memory.read_byte(low) == A.memory.read_byte(BASE_BP + 300) && A.memory.read_byte(low - 2) == A.memory.read_byte(BASE_BP + 298),
           "G-3 copy mid-run", "three bytes written before the throw");
    // G-2: WBLM with bit 31
    fresh(0, 3, (int32_t)(REGION | 0x80000000u), (int32_t)(REGION + 10));
    t = agree("G-2 blm src@", A, B, wblm, [&] { block_move(B.machine, SITE, REGION + 10, REGION | 0x80000000u, 3); });
    expect(t == "WBLM instruction with indirection!", "G-2 blm src@", t.c_str());
    fresh(0, 3, (int32_t)(REGION + 10), (int32_t)(REGION | 0x80000000u));
    t = agree("G-2 blm dst@", A, B, wblm, [&] { block_move(B.machine, SITE, REGION | 0x80000000u, REGION + 10, 3); });
    expect(t == "WBLM instruction with indirection!", "G-2 blm dst@", t.c_str());
    // a char() piece is not a copy source
    bool threw = false;
    try { copy(B.machine, SITE, dst, 1, EagleString::chr('x')); } catch(std::exception&) { threw = true; }
    expect(threw, "copy(chr)", "must refuse");
    cases++;
    // W2/G-4: zero count with garbage pointers proceeds silently on both sides
    fresh(0, 0, (int32_t)0xE0000000u, (int32_t)0x12345678);
    t = agree("G-4 zero count", A, B, wcmv, [&] { copy(B.machine, SITE, 0xE0000000u, 0, EagleString::literal(0x12345678, 0)); });
    expect(t.empty(), "G-4 zero count", "must not throw");
  }

  // ---- 5. the Mapper arena form ----
  {
    Rig& R = *new Rig;
    Mapper& mp = R.machine.mapper;
    mp.configure(&R.machine, nullptr, true);
    // a 19-row fixture in the pushmap tradition: block, arena address, capacity
    std::vector<Mapper::ArenaLayout> layout;
    uint32_t a = Mapper::ARENA_BASE + 0x10;
    for(uint32_t i = 0; i < 19; i++) {
      uint32_t cap = 20 + i * 17;                       // 20..326 bytes, odd and even
      layout.push_back({0x70166144u + i * 0x100u, a, cap});
      a += 1 + (cap + 1) / 2 + 3;                       // strictly past the closed end
    }
    mp.configure_arena(layout);
    expect(mp.arena_rows() == 19, "arena.configure", "row count");
    auto row5 = layout[5];
    uint32_t base = row5.arena_addr, end = base + 1 + (row5.capacity + 1) / 2;
    // unbound: every form of an arena address is a MISMATCH, whatever the master holds
    expect(mp.equivalent(0x70001234u, base).kind == Mapper::Kind::MISMATCH, "arena.unbound", "word");
    expect(mp.equivalent(0xE0002468u, base * 2).kind == Mapper::Kind::MISMATCH, "arena.unbound", "byte");
    expect(mp.equivalent(0x70001234u | 0x80000000u, base | 0x80000000u).kind == Mapper::Kind::MISMATCH, "arena.unbound", "@word");
    // an unmapped row must not map through the 0 sentinel: master == the bare offset is still a MISMATCH
    expect(mp.equivalent(7u, base + 7).kind == Mapper::Kind::MISMATCH, "arena.unbound", "offset-through-0 sentinel");
    // bind row 5 to the master's stack address; the machine's wfp is the bind wfp
    R.machine.wfp = 0x70001000;
    uint32_t master = 0x70001052u;
    mp.arena_bind(base, R.machine.wfp, master);
    Mapper::Verdict v = mp.equivalent(master, base);
    expect(v.kind == Mapper::Kind::MAPPED && v.mapped == master, "arena.bound", "base word");
    v = mp.equivalent(master + 7, base + 7);
    expect(v.kind == Mapper::Kind::MAPPED, "arena.bound", "interior word");
    v = mp.equivalent((master + 7) * 2 + 1, (base + 7) * 2 + 1);
    expect(v.kind == Mapper::Kind::MAPPED && v.clone_form == Mapper::Form::Byte, "arena.bound", "interior byte, low bit kept");
    v = mp.equivalent((master + 3) | 0x80000000u, (base + 3) | 0x80000000u);
    expect(v.kind == Mapper::Kind::MAPPED && v.clone_form == Mapper::Form::AtWord, "arena.bound", "@word");
    v = mp.equivalent(master + (end - base), end);
    expect(v.kind == Mapper::Kind::MAPPED, "arena.bound", "closed right end is inside");
    v = mp.equivalent(master + (end - base) + 1, end + 1);
    expect(v.kind == Mapper::Kind::MISMATCH, "arena.bound", "one past the closed end is outside");
    v = mp.equivalent(master + 1, base);
    expect(v.kind == Mapper::Kind::MISMATCH && v.mapped == master, "arena.bound", "wrong master value mismatches, verdict carries the mapping");
    v = mp.equivalent(base, base);
    expect(v.kind == Mapper::Kind::RAW, "arena.bound", "raw equality still RAW");
    // a second row in another frame; frame exit unmaps only its own
    auto row9 = layout[9];
    R.machine.wfp = 0x70002000;
    mp.arena_bind(row9.arena_addr, R.machine.wfp, 0x70002030u);
    expect(mp.equivalent(0x70002030u, row9.arena_addr).kind == Mapper::Kind::MAPPED, "arena.row9", "bound");
    mp.arena_unmap_frame(0x70002000);
    expect(mp.equivalent(0x70002030u, row9.arena_addr).kind == Mapper::Kind::MISMATCH, "arena.unmap", "row 9 unmapped");
    expect(mp.equivalent(3u, row9.arena_addr + 3).kind == Mapper::Kind::MISMATCH, "arena.unmap", "unmapped row does not map through 0");
    expect(mp.equivalent(master, base).kind == Mapper::Kind::MAPPED, "arena.unmap", "row 5 still mapped");
    mp.arena_unmap_frame(0x70001000);
    expect(mp.equivalent(master, base).kind == Mapper::Kind::MISMATCH, "arena.unmap", "row 5 unmapped");
    // rebind at block re-entry: length resets, mapping refreshed
    R.machine.wfp = 0x70001000;
    mp.arena_bind(base, R.machine.wfp, master + 0x20);
    mp.arena_set_length(base, (int32_t)row5.capacity);
    expect(mp.arena_row(base)->length == (int32_t)row5.capacity, "arena.length", "set to capacity");
    mp.arena_bind(base, R.machine.wfp, master);
    expect(mp.arena_row(base)->length == 0 && mp.arena_row(base)->master_addr == master, "arena.rebind", "length 0, new master");
    // binary search: every row found at base, interior and end; the gap between rows is nobody's
    for(size_t i = 0; i < layout.size(); i++) {
      uint32_t b = layout[i].arena_addr, e = b + 1 + (layout[i].capacity + 1) / 2;
      expect(mp.arena_row(b) && mp.arena_row(b)->block == layout[i].block, "arena.search", "base");
      expect(mp.arena_row(e) && mp.arena_row(e)->block == layout[i].block, "arena.search", "end");
      expect(mp.arena_row(e + 1) == nullptr, "arena.search", "gap");
    }
    expect(mp.arena_row(Mapper::ARENA_BASE) == nullptr && mp.arena_row(Mapper::ARENA_BASE + 0x7FFFFF) == nullptr, "arena.search", "outside all rows");
    // refusals and invariants (Lockstep disabled → mapper_abort throws)
    auto throws = [&](const char* what, auto f) {
      bool t = false; std::string msg;
      try { f(); } catch(std::exception& e) { t = true; msg = e.what(); }
      expect(t, what, "expected an abort");
      cases++;
      return msg;
    };
    std::string msg = throws("clone_location(arena)", [&] { mp.clone_location(base + 2); });
    expect(msg.find("arena") != std::string::npos, "clone_location(arena)", msg.c_str());
    throws("clone_location(arena byte)", [&] { mp.clone_location((base + 2) * 2); });
    throws("clone_location(arena @)", [&] { mp.clone_location((base + 2) | 0x80000000u); });
    throws("frame_precedes(arena)", [&] { mp.frame_precedes(base, 0x70001000u); });
    throws("bind unknown row", [&] { mp.arena_bind(base + 1, R.machine.wfp, master); });
    throws("bind wrong wfp", [&] { mp.arena_bind(base, R.machine.wfp + 2, master); });
    throws("bind master 0", [&] { mp.arena_bind(base, R.machine.wfp, 0); });
    throws("bind master in arena", [&] { mp.arena_bind(base, R.machine.wfp, layout[0].arena_addr); });
    throws("length overflow", [&] { mp.arena_set_length(base, (int32_t)row5.capacity + 1); });
    throws("length negative", [&] { mp.arena_set_length(base, -1); });
    throws("length unknown row", [&] { mp.arena_set_length(base + 1, 0); });
    {
      Rig& R2 = *new Rig; Mapper& m2 = R2.machine.mapper; m2.configure(&R2.machine, nullptr, true);
      std::vector<Mapper::ArenaLayout> bad = layout;
      bad[3].arena_addr = bad[2].arena_addr + 1 + (bad[2].capacity + 1) / 2;   // touches the closed end
      throws("layout: closed ends touch", [&] { m2.configure_arena(bad); });
      bad = layout; bad[0].arena_addr = Mapper::ARENA_BASE - 4;
      throws("layout: below segment", [&] { m2.configure_arena(bad); });
      bad = layout; bad[18].arena_addr = Mapper::ARENA_BASE + Mapper::ARENA_WORDS - 2;
      throws("layout: end past segment", [&] { m2.configure_arena(bad); });
      m2.configure_arena(layout);   // the good layout still loads after the bad ones
      expect(m2.arena_rows() == 19, "layout", "reload");
    }
    // non-arena behaviour untouched (stock mode: no records)
    expect(mp.equivalent(0x70001234u, 0x70001234u).kind == Mapper::Kind::RAW, "mapper.plain", "raw");
    expect(mp.equivalent(0x70001234u, 0x70001236u).kind == Mapper::Kind::MISMATCH, "mapper.plain", "mismatch");
    expect(mp.clone_location(0x70001234u) == 0x70001234u, "mapper.plain", "clone_location identity");
    expect(mp.equivalent(0x70001234u, 0x12345678u).kind == Mapper::Kind::MISMATCH, "mapper.plain", "unlisted form");
    cases += 40;
  }

  // ---- 6. the Δ accumulator ----
  {
    ClaimDelta d;
    int32_t f = 0x70001000, g = 0x70002000;
    expect(d.check(f, 0x70001040, 0x70001040), "delta", "empty: equal wsps");
    d.claim(f, 8); d.claim(f, 8); d.claim(f, 9);          // a 3-claim group: +16 +16 +18 = +50
    expect(d.delta(f) == 50, "delta", "claims sum");
    expect(d.check(f, 0x70001040 + 50, 0x70001040), "delta", "master ahead by the claims");
    expect(!d.check(f, 0x70001040 + 48, 0x70001040), "delta", "anything else breaks it");
    d.claim(g, 5);                                         // another frame's claim is separate
    expect(d.delta(f) == 50 && d.delta(g) == 10, "delta", "keyed by wfp");
    d.release(f, 0x70001040 + 50, 0x70001040);             // the STASP: old − new
    expect(d.delta(f) == 0, "delta", "released to 0");
    d.claim(f, 6); d.frame_exit(f);
    expect(d.delta(f) == 0 && d.frames() == 1, "delta", "frame_exit erases");
    d.frame_exit(g);
    expect(d.frames() == 0, "delta", "all gone");
    cases += 9;
  }

  std::printf("%s (%d cases, %d failures)\n", fails ? "STRINGS SELFTEST RED" : "STRINGS SELFTEST GREEN", cases, fails);
  return fails ? 1 : 0;
}
