// src/quest/SharedData.hpp
#pragma once
#include <cstdint>
#include <memory>

namespace quest {

class Player;

// Abstract factory for accessing the three shared memory regions:
//   SHARED_DATA_FILE (SD_PTR)  — player records, world state
//   WORLD_DATA_FILE  (OBJ_PTR) — object/item data, world map
//   CASTLE_DATA_FILE (CAS_PTR) — castle data
//
// Created after INIT_SHARED_DATA maps the files. Accessed via ctx.shared.

class SharedData {
public:
  virtual ~SharedData() = default;

  // --- Player access (SHARED_DATA_FILE) ---

  virtual std::unique_ptr<Player> player(int32_t player_num) const = 0;
  virtual int32_t num_players() const = 0;

  // --- Raw SD access (escape hatch) ---

  virtual int32_t read_sd_word(uint32_t offset) const = 0;
  virtual void write_sd_word(uint32_t offset, int32_t value) = 0;
  virtual int32_t read_sd_wide(uint32_t offset) const = 0;
  virtual void write_sd_wide(uint32_t offset, int32_t value) = 0;

  // --- Raw OBJ access (escape hatch) ---

  virtual int32_t read_obj_word(uint32_t offset) const = 0;
  virtual void write_obj_word(uint32_t offset, int32_t value) = 0;
  virtual int32_t read_obj_wide(uint32_t offset) const = 0;
  virtual void write_obj_wide(uint32_t offset, int32_t value) = 0;

  // --- Raw CAS access (escape hatch) ---

  virtual int32_t read_cas_word(uint32_t offset) const = 0;
  virtual void write_cas_word(uint32_t offset, int32_t value) = 0;
  virtual int32_t read_cas_wide(uint32_t offset) const = 0;
  virtual void write_cas_wide(uint32_t offset, int32_t value) = 0;
};

} // namespace quest
