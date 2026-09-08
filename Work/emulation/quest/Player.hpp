// src/quest/Player.hpp
#pragma once
#include <cstdint>
#include "PlayerFlag.hpp"

namespace quest {

class DisplayCache;  // forward declaration

// Abstract view of a single player's record in SHARED_DATA_FILE.
// Reads/writes go through to shared memory on every call.
//
// Named accessors are added lazily as game functions are ported.
// The escape hatches (read_word, read_bit, etc.) cover everything
// else using raw disassembly offsets.

class Player {
public:
  virtual ~Player() = default;

  // --- Display cache ---
  DisplayCache get_display_cache();

  // --- Named fields (added lazily as discovered) ---

  virtual int32_t get_x() const = 0;
  virtual int32_t get_y() const = 0;
  virtual void set_x(int32_t x) = 0;
  virtual void set_y(int32_t y) = 0;

  // Inventory: 10 slots, 0-indexed. Returns 0 for empty slot.
  virtual int32_t get_inventory(int slot) const = 0;

  // Core stats
  virtual int32_t get_current_hp() const = 0;
  virtual int32_t get_intelligence() const = 0;
  virtual int32_t get_experience() const = 0;
  virtual int32_t get_vision() const = 0;
  virtual int32_t get_perception() const = 0;
  virtual int32_t get_wealth() const = 0;
  virtual int32_t get_castle_count() const = 0;
  virtual int32_t get_player_class() const = 0;
  virtual int32_t get_quest_level() const = 0;
  virtual int32_t get_dragons_slain() const = 0;
  virtual int32_t get_arrows() const = 0;
  virtual int32_t get_poison() const = 0;

  // --- Player flags ---

  virtual bool has_player_flag(PlayerFlag flag) const = 0;
  virtual void set_player_flag(PlayerFlag flag) = 0;
  virtual void clear_player_flag(PlayerFlag flag) = 0;
  virtual void set_player_flag(PlayerFlag flag, bool value) = 0;

  // --- Escape hatches for undiscovered fields ---

  // Word offset: raw 15-bit value from disassembly (e.g. 0x7DB3).
  virtual int32_t read_word(uint32_t offset) const = 0;
  virtual void write_word(uint32_t offset, int32_t value) = 0;
  virtual int32_t read_wide(uint32_t offset) const = 0;
  virtual void write_wide(uint32_t offset, int32_t value) = 0;

  // Bit offset: raw 16-bit value from disassembly (e.g. 0xDB1F).
  virtual int32_t read_bit(uint32_t bit_offset) const = 0;
  virtual void write_bit(uint32_t bit_offset, int32_t value) = 0;
};

} // namespace quest
