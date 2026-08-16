// src/emu_quest/SharedData.cpp
#include "SharedData.hpp"
#include "Player.hpp"
#include "../hw/Memory.hpp"

namespace emu_quest {

SharedData::SharedData(hw::Memory& mem) : memory(mem) {}

uint32_t SharedData::sd_base() const {
  return memory.read_wide(SD_PTR_ADDR);
}

uint32_t SharedData::obj_base() const {
  return memory.read_wide(OBJ_PTR_ADDR);
}

uint32_t SharedData::cas_base() const {
  return memory.read_wide(CAS_PTR_ADDR);
}

std::unique_ptr<quest::Player> SharedData::player(int32_t player_num) const {
  return std::make_unique<emu_quest::Player>(memory, sd_base(), player_num);
}

int32_t SharedData::num_players() const {
  return static_cast<int32_t>(memory.read_word(sd_base()+NUM_PLAYERS_OFFSET)&0xFFFF);
}

// --- SD raw access ---

int32_t SharedData::read_sd_word(uint32_t offset) const {
  return static_cast<int32_t>(memory.read_word(sd_base()+offset)&0xFFFF);
}

void SharedData::write_sd_word(uint32_t offset, int32_t value) {
  memory.write_word(sd_base()+offset, static_cast<uint32_t>(value)&0xFFFF);
}

int32_t SharedData::read_sd_wide(uint32_t offset) const {
  return static_cast<int32_t>(memory.read_wide(sd_base()+offset));
}

void SharedData::write_sd_wide(uint32_t offset, int32_t value) {
  memory.write_wide(sd_base()+offset, static_cast<uint32_t>(value));
}

// --- OBJ raw access ---

int32_t SharedData::read_obj_word(uint32_t offset) const {
  return static_cast<int32_t>(memory.read_word(obj_base()+offset)&0xFFFF);
}

void SharedData::write_obj_word(uint32_t offset, int32_t value) {
  memory.write_word(obj_base()+offset, static_cast<uint32_t>(value)&0xFFFF);
}

int32_t SharedData::read_obj_wide(uint32_t offset) const {
  return static_cast<int32_t>(memory.read_wide(obj_base()+offset));
}

void SharedData::write_obj_wide(uint32_t offset, int32_t value) {
  memory.write_wide(obj_base()+offset, static_cast<uint32_t>(value));
}

// --- CAS raw access ---

int32_t SharedData::read_cas_word(uint32_t offset) const {
  return static_cast<int32_t>(memory.read_word(cas_base()+offset)&0xFFFF);
}

void SharedData::write_cas_word(uint32_t offset, int32_t value) {
  memory.write_word(cas_base()+offset, static_cast<uint32_t>(value)&0xFFFF);
}

int32_t SharedData::read_cas_wide(uint32_t offset) const {
  return static_cast<int32_t>(memory.read_wide(cas_base()+offset));
}

void SharedData::write_cas_wide(uint32_t offset, int32_t value) {
  memory.write_wide(cas_base()+offset, static_cast<uint32_t>(value));
}

} // namespace emu_quest
