// src/emu_quest/Player.hpp
#pragma once
#include "../quest/Player.hpp"

namespace hw { class Memory; }

namespace emu_quest {

// Memory-backed Player. All reads/writes go through hw::Memory
// to the shared data region.

class Player : public quest::Player {
public:
  static constexpr int32_t STRIDE=686;

  Player(hw::Memory& mem, uint32_t sd_base, int32_t player_num);

  int32_t get_x() const override;
  int32_t get_y() const override;
  void set_x(int32_t x) override;
  void set_y(int32_t y) override;

  int32_t get_inventory(int slot) const override;

  int32_t get_current_hp() const override;
  int32_t get_intelligence() const override;
  int32_t get_experience() const override;
  int32_t get_vision() const override;
  int32_t get_perception() const override;
  int32_t get_wealth() const override;
  int32_t get_castle_count() const override;
  int32_t get_player_class() const override;
  int32_t get_quest_level() const override;
  int32_t get_dragons_slain() const override;
  int32_t get_arrows() const override;
  int32_t get_poison() const override;

  bool has_player_flag(quest::PlayerFlag flag) const override;
  void set_player_flag(quest::PlayerFlag flag) override;
  void clear_player_flag(quest::PlayerFlag flag) override;
  void set_player_flag(quest::PlayerFlag flag, bool value) override;

  int32_t read_word(uint32_t offset) const override;
  void write_word(uint32_t offset, int32_t value) override;
  int32_t read_wide(uint32_t offset) const override;
  void write_wide(uint32_t offset, int32_t value) override;
  int32_t read_bit(uint32_t bit_offset) const override;
  void write_bit(uint32_t bit_offset, int32_t value) override;

private:
  hw::Memory& memory;
  uint32_t sd_base;
  int32_t player_num;

  uint32_t word_addr(uint32_t offset) const;
};

} // namespace emu_quest
