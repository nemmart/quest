// src/hw/BlockSync.hpp
//
// Generation-6 block-sync support (docs/Project22/BlockSyncDesign.md).
//
// The Gen-6 sync identity is the (block entry address, per-client block
// ordinal) pair. Master and clone each count arrivals at LISTED game-range
// block-entry addresses; the ordinal is compared at every rendezvous, and
// (Gen-6.0 Stage 1) every K listed entries is itself a rendezvous — the
// heartbeat, replacing the 500-instruction batch.
//
// The SYNC LIST is a TRANSLATION ARTIFACT (user ruling, Aug 28 2026): an
// input file naming the counted addresses, supplied alongside the
// translation. Gen-6.0 ships the IDENTITY list (every quest.blocks start);
// delisting starts with translation projects. Validation on load — the
// loader refuses novelty:
//   1. every listed address must be a block start in quest.blocks;
//   2. every GATE address must be listed: block starts that are the
//      successors of a call (`c`), jump-call (`j`), or SYSCALL-terminated
//      block — the resumption points of gate events — plus any game-range
//      terminal site that is a block start. No merge across a gate.
//
// Counting semantics (both roles, identically): an "entry" is an
// arrival-transition observed inside Machine::run_steps — pc landing on a
// listed address as the result of executing an instruction (or a native
// wrapper returning there). The initial pc of a batch is NOT counted: at a
// break it was already counted when arrived at, and a syscall-return
// resume enters through OS code, which both engines traverse identically.
// The ordinal is therefore the same deterministic function of the
// execution on master and clone; any difference is a divergence.
//
// Loading is env-driven like the address book:
//   QUEST_BLOCKS=<path>      Disassembled/quest.blocks (ground truth CFG)
//   QUEST_SYNC_LIST=<path>   the sync list (hex address per line, # comments)
// Both are REQUIRED under -lockstep for the QUEST program; missing or
// invalid files refuse to run (loud failure over silent).
#pragma once
#include <cstdint>

namespace hw {

class BlockSync {
public:
  static bool active;            // list loaded and validated
  static uint32_t game_start;    // [game_start, game_stop): span of block starts
  static uint32_t game_stop;
  static uint32_t sync_k;        // K: rendezvous every K listed entries (QUEST_SYNC_K, default 50)

  // Gen-6.0 Stage 1: the instruction budget handed to a lockstep client
  // batch is no longer a sync event — the K-block heartbeat is. The
  // budget survives only as a RUNAWAY GUARD: exhausting it without any
  // rendezvous THROWS (loud failure over a silent parallel heartbeat;
  // METHOD §8). Sized far above any legitimate inter-rendezvous stretch
  // (the longest observed emulated RT spans are bounded by the 10M
  // run-to-return guard).
  static constexpr int32_t RUNAWAY_GUARD = 100000000;

  // Load + validate. Called from Launch when -lockstep is given; returns
  // false (after printing why) on any failure. Idempotent.
  static bool load_from_env();

  // True if pc is a listed block-entry address. Hot path: two compares
  // and a byte load.
  static inline bool listed(uint32_t pc) {
    return active && pc >= game_start && pc < game_stop &&
           listed_bits[pc - game_start];
  }

private:
  static uint8_t* listed_bits;   // byte per word over [game_start, game_stop)
};

} // namespace hw
