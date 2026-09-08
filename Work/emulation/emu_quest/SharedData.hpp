// src/emu_quest/SharedData.hpp
#pragma once
#include "../quest/SharedData.hpp"

namespace hw { class Memory; }

namespace emu_quest {

// Memory-backed SharedData. Reads base pointers (SD_PTR, OBJ_PTR,
// CAS_PTR) from fixed global addresses in Memory.

class SharedData : public quest::SharedData {
public:
  explicit SharedData(hw::Memory& mem);

  std::unique_ptr<quest::Player> player(int32_t player_num) const override;
  int32_t num_players() const override;

  int32_t read_sd_word(uint32_t offset) const override;
  void write_sd_word(uint32_t offset, int32_t value) override;
  int32_t read_sd_wide(uint32_t offset) const override;
  void write_sd_wide(uint32_t offset, int32_t value) override;

  int32_t read_obj_word(uint32_t offset) const override;
  void write_obj_word(uint32_t offset, int32_t value) override;
  int32_t read_obj_wide(uint32_t offset) const override;
  void write_obj_wide(uint32_t offset, int32_t value) override;

  int32_t read_cas_word(uint32_t offset) const override;
  void write_cas_word(uint32_t offset, int32_t value) override;
  int32_t read_cas_wide(uint32_t offset) const override;
  void write_cas_wide(uint32_t offset, int32_t value) override;

private:
  hw::Memory& memory;

  static constexpr uint32_t SD_PTR_ADDR=0x70000210;
  static constexpr uint32_t OBJ_PTR_ADDR=0x70000212;
  static constexpr uint32_t CAS_PTR_ADDR=0x70000214;
  static constexpr uint32_t NUM_PLAYERS_OFFSET=0x002B;

  uint32_t sd_base() const;
  uint32_t obj_base() const;
  uint32_t cas_base() const;
};

} // namespace emu_quest
