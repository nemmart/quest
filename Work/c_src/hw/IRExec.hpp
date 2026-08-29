#pragma once
// P23 (Gen-6.1) — quest.ir loader + block interpreter.
// Spec: docs/Project23/IRPhase1.md. Dispatch rule: a block PRESENT in
// quest.ir (QUEST_IR env) is executed as IR by the CLONE; absent =
// emulated; the master always emulates. The executor is an interpreter;
// #-ops call the SAME EagleInstruction helpers the emulated
// instructions call (single source of truth — WideCarry.md ruling), and
// every #-statement ends with the loop's ovk/ovr check, identical
// throw string. Embedded statement = full barrier: materialize locals,
// run the single instruction through the normal decode/execute path
// with all hooks, re-read locals. Any impossibility THROWS (METHOD §8).
#include <cstdint>
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
  static IRExec* instance;         // set at launch; nullptr = no QUEST_IR

  bool has(uint32_t pc) const;
  // Runs one IR block; machine.pc is at the block entry. Returns the
  // exit pc chosen by the embedded terminator (or the 0x30000000
  // syscall sentinel propagated from an embedded instruction).
  uint32_t run_block(Machine& machine, uint32_t pc);

  struct Expr;                       // opaque AST node
  struct Stmt {
    enum Kind { INSTR, STMT, CALL, RET, GOTO, ASSERT } kind = STMT;
    uint32_t pc = 0;                 // INSTR: address; CALL: ret pc
    uint32_t target = 0;             // CALL: callee; GOTO: exit
    uint32_t ret = 0;                // CALL: declared return pc (belief)
    uint32_t marker = 0;             // CALL: marker slot (validated belief)
    int32_t  args = 0;               // CALL: elided arg-push count
    std::shared_ptr<Expr> lhs, rhs;  // STMT: lhs/rhs; ASSERT: rhs = condition
    std::string text;                // ASSERT: source text for the failure report
    bool flags = false;              // STMT: rhs contains a #-op
  };
  struct Block {
    uint32_t start, seg;
    uint32_t fall = 0;                   // fall-through exit (0 = terminator embed)
    bool executed = false;               // first-execution logged (coverage)
    std::vector<Stmt> stmts;
  };

private:
  IRExec() = default;
  void load(const std::string& path);
  std::vector<Block> blocks_;        // sorted by start
  const std::string* last_format_ = nullptr;  // decoded fmt of last instr
  const Block* find(uint32_t pc) const;
};

} // namespace hw
