#pragma once
// P23 (Gen-6.1) — quest.ir loader + block interpreter; P26 = ir 3; P28 = ir 4; P31 = ir 5.
// Spec: docs/IR.md (normative). Dispatch rule: a block PRESENT in
// quest.ir (QUEST_IR env) is executed as IR by the CLONE; absent =
// emulated; the master always emulates. The executor is an interpreter;
// Effectful ops (add sub mul div cvwn ash nadd nsub nmul) call the SAME
// EagleInstruction helpers the emulated instructions call (single source
// of truth — WideCarry.md ruling), and every effectful statement ends
// with the loop's ovk/ovr check, identical throw string. Embedded statement = full barrier: materialize locals,
// run the single instruction through the normal decode/execute path
// with all hooks, re-read locals. Any impossibility THROWS (METHOD §8).
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace hw {
class Machine;

class IRExec {
public:
  // Loads QUEST_IR if set. Returns nullptr (and prints why) only when
  // the env is absent; every validation failure REFUSES (throws) — a
  // present-but-bad quest.ir must never silently fall back to emulation.
  static IRExec* load_from_env();
  // P46 (ir 7): load a file directly (the self-tests; no QUEST_IR, no
  // -lockstep gate). `addrbook` supplies the entry NAMES a v-form program
  // needs (§5.10.2; nullptr = QUEST_ADDRESS_BOOK from the environment).
  static IRExec* load_file(const std::string& path, const char* addrbook = nullptr);
  static IRExec* instance;         // set at launch; nullptr = no QUEST_IR

  bool has(uint32_t pc) const;
  // P46 (ir 7): the placement queries and the 0x76 page mapping (§5.10.3/.6).
  // v_address / block_address return 0 for an unknown name.
  uint32_t v_address(const std::string& qualified) const;
  uint32_t block_address(const std::string& qualified) const;
  size_t v_count() const { return vars_.size(); }
  size_t symbolic_block_count() const { return nsymbolic_; }
  void map_pages(class Memory& memory) const;   // clone process: the touched 0x76 pages, RW, no exec
  static constexpr uint32_t V_BASE = 0x76000000u, B_BASE = 0x77000000u, ENTRY_STRIDE = 0x10000u;
  static bool is_synthetic(uint32_t word) { return word >= V_BASE && word < B_BASE + 0x01000000u; }
  // Runs one IR block; machine.pc is at the block entry. Returns the
  // exit pc chosen by the embedded terminator (or the 0x30000000
  // syscall sentinel propagated from an embedded instruction).
  uint32_t run_block(Machine& machine, uint32_t pc);

  struct Expr;                       // opaque AST node
  // P26 (ir 3): effectful ops sit only at statement root and name the
  // shared EagleInstruction helper they call (docs/IR.md §5).
  enum EffOp { EFF_NONE = 0, EFF_ADD, EFF_SUB, EFF_MUL, EFF_DIV, EFF_CVWN,
               EFF_ASH, EFF_NADD, EFF_NSUB, EFF_NMUL };
  // P31 (ir 5): a string PIECE — literal (contents known, address in the
  // image, verified lazily against memory at first use), located fixed
  // [@a, n] (byte address, n bytes), located varying [@a, n varying] /
  // [@a, varying] (word address of the length word).
  struct Piece {
    enum Kind { LIT, FIXED, VARYING, VARYING_NOCAP } kind = FIXED;
    std::shared_ptr<Expr> addr;      // FIXED: byte-pointer value; VARYING*: word address (wrapped)
    int32_t n = 0;                   // LIT: byte count; FIXED/VARYING: n when constant
    std::shared_ptr<Expr> n_expr;    // FIXED/VARYING (P32, ir 5): the count as a pure
                                     //   expression (a register, a length-word read, a sum);
                                     //   null when n is the constant above
    uint32_t lit_bp = 0;             // LIT: byte pointer in the image
    std::string bytes;               // LIT: the text (unescaped)
    bool verified = false;           // LIT: bytes checked against memory (once)
  };
  struct StrOp {
    enum Kind { ASSIGN_FIXED, ASSIGN_VARYING, CMP, WORDS } kind = ASSIGN_FIXED;
    Piece dst, src;                  // ASSIGN: dst/src; CMP: src=string 1 (ac3/ac1), dst=string 2 (ac2/ac0)
    std::shared_ptr<Expr> k;         // WORDS: word count
    bool executed = false;           // first-execution log (coverage)
  };
  struct Stmt {
    enum Kind { INSTR, STMT, CALL, RET, GOTO, ASSERT, RT_CALL, STRING, CLAIM, RELEASE } kind = STMT;
                                     // CLAIM/RELEASE (P33-B, ir 6): target = the twin's word address,
                                     //   args = capacity (CLAIM) ; marker = the size register (CLAIM)
    uint32_t pc = 0;                 // INSTR: address; CALL/RT_CALL: site pc
    uint32_t target = 0;             // CALL: callee
    uint32_t ret = 0;                // CALL: declared return pc (belief)
    uint32_t marker = 0;             // CALL: marker slot (validated belief)
    int32_t  args = 0;               // CALL: elided arg-push count
    std::shared_ptr<Expr> lhs, rhs;  // STMT: lhs/rhs (rhs = arg a of an effectful op);
                                     // ASSERT: rhs = condition; GOTO: rhs = index expr
    std::shared_ptr<Expr> rhs2;      // STMT: second arg of a binary effectful op
    EffOp eff = EFF_NONE;            // STMT: effectful root op (flags written)
    std::vector<uint32_t> labels;    // GOTO: exit table (goto [L0..Lk] e)
    std::string text;                // ASSERT: source text for the failure report;
                                     // RT_CALL: callee symbol (`?NAME`)
    std::vector<std::shared_ptr<Expr>> argv;   // RT_CALL: e1..eN in PL/I order (P28, ir 4)
    std::shared_ptr<StrOp> str;      // STRING (P31, ir 5): the located-string statement
    bool flags = false;              // STMT: writes c/ovr (effectful) -> ovk/ovr check
  };
  // P46 (ir 7): a declared v (§5.10.1) — its type, size and placement.
  struct Var {
    enum Type { I16, U16, I32, U32, CHAR, VARYING, WORDS } type;
    std::string name;                    // <ENTRY>.v<k>
    uint32_t n = 0;                      // char/varying/words: the declared n
    uint32_t words = 0;                  // size in words
    uint32_t addr = 0;                   // placed word address (0x76……)
  };
  struct Block {
    uint32_t start, seg;
    std::string name;                    // P46: <ENTRY>.b<k> for a symbolic block; empty for a numeric one
    uint32_t fall = 0;                   // fall-through exit (0 = terminator embed)
    bool executed = false;               // first-execution logged (coverage)
    std::vector<Stmt> stmts;
  };

private:
  IRExec() = default;
  void load(const std::string& path, const char* addrbook);
  std::vector<Block> blocks_;        // sorted by start
  std::vector<Var> vars_;            // P46: declaration order
  std::map<std::string, size_t> var_by_name_;
  std::map<std::string, uint32_t> block_by_name_;   // P46: symbolic block name -> 0x77 address
  size_t nsymbolic_ = 0;
  const std::string* last_format_ = nullptr;  // decoded fmt of last instr
  const Block* find(uint32_t pc) const;
};

} // namespace hw
