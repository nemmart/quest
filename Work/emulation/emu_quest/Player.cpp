// src/emu_quest/Player.cpp
#include "Player.hpp"
#include "../hw/Memory.hpp"

namespace emu_quest {

Player::Player(hw::Memory& mem, uint32_t base, int32_t pnum)
  : memory(mem), sd_base(base), player_num(pnum) {}

static int32_t sign_extend_15(uint32_t raw) {
  if(raw&0x4000)
    return static_cast<int32_t>(raw|0xFFFF8000);
  return static_cast<int32_t>(raw);
}

static int32_t sign_extend_16(uint32_t raw) {
  if(raw&0x8000)
    return static_cast<int32_t>(raw|0xFFFF0000);
  return static_cast<int32_t>(raw);
}

uint32_t Player::word_addr(uint32_t offset) const {
  int32_t signed_offset=sign_extend_15(offset);
  return static_cast<uint32_t>(
    static_cast<int32_t>(sd_base)+player_num*STRIDE+signed_offset);
}

// --- Known fields ---

static constexpr uint32_t OFF_X=0x7DB3;
static constexpr uint32_t OFF_Y=0x7DB4;
static constexpr uint32_t OFF_INVENTORY=0x7E7A;
static constexpr uint32_t OFF_STATUS_BITS_0=0x7DB1;
static constexpr uint32_t OFF_CURRENT_HP=0x7E87;
static constexpr uint32_t OFF_INTELLIGENCE=0x7E85;
static constexpr uint32_t OFF_EXPERIENCE=0x7E86;
static constexpr uint32_t OFF_VISION=0x7E89;
static constexpr uint32_t OFF_PERCEPTION=0x7E8A;
static constexpr uint32_t OFF_WEALTH=0x7E8C;
static constexpr uint32_t OFF_CASTLE_COUNT=0x7FA8;
static constexpr uint32_t OFF_QUEST_LEVEL=0x7FA7;
static constexpr uint32_t OFF_DRAGONS_SLAIN=0x7E8E;
static constexpr uint32_t OFF_ARROWS=0x7F02;
static constexpr uint32_t OFF_POISON=0x7F03;
static constexpr uint32_t OFF_PLAYER_CLASS=0x002A;

int32_t Player::get_x() const {
  return static_cast<int32_t>(memory.read_word(word_addr(OFF_X))&0xFFFF);
}

int32_t Player::get_y() const {
  return static_cast<int32_t>(memory.read_word(word_addr(OFF_Y))&0xFFFF);
}

void Player::set_x(int32_t x) {
  memory.write_word(word_addr(OFF_X), static_cast<uint32_t>(x)&0xFFFF);
}

void Player::set_y(int32_t y) {
  memory.write_word(word_addr(OFF_Y), static_cast<uint32_t>(y)&0xFFFF);
}

int32_t Player::get_inventory(int slot) const {
  return static_cast<int32_t>(
    memory.read_word(word_addr(OFF_INVENTORY+static_cast<uint32_t>(slot)+1))&0xFFFF);
}

int32_t Player::get_current_hp() const {
  return static_cast<int32_t>(memory.read_word(word_addr(OFF_CURRENT_HP))&0xFFFF);
}

int32_t Player::get_intelligence() const {
  return static_cast<int32_t>(memory.read_word(word_addr(OFF_INTELLIGENCE))&0xFFFF);
}

int32_t Player::get_experience() const {
  return static_cast<int32_t>(memory.read_word(word_addr(OFF_EXPERIENCE))&0xFFFF);
}

int32_t Player::get_vision() const {
  return static_cast<int32_t>(memory.read_word(word_addr(OFF_VISION))&0xFFFF);
}

int32_t Player::get_perception() const {
  return static_cast<int32_t>(memory.read_word(word_addr(OFF_PERCEPTION))&0xFFFF);
}

int32_t Player::get_wealth() const {
  return static_cast<int32_t>(memory.read_word(word_addr(OFF_WEALTH))&0xFFFF);
}

int32_t Player::get_castle_count() const {
  return static_cast<int32_t>(memory.read_word(word_addr(OFF_CASTLE_COUNT))&0xFFFF);
}

int32_t Player::get_player_class() const {
  return static_cast<int32_t>(memory.read_word(word_addr(OFF_PLAYER_CLASS))&0xFFFF);
}

int32_t Player::get_quest_level() const {
  return static_cast<int32_t>(memory.read_word(word_addr(OFF_QUEST_LEVEL))&0xFFFF);
}

int32_t Player::get_dragons_slain() const {
  return static_cast<int32_t>(memory.read_word(word_addr(OFF_DRAGONS_SLAIN))&0xFFFF);
}

int32_t Player::get_arrows() const {
  return static_cast<int32_t>(memory.read_word(word_addr(OFF_ARROWS))&0xFFFF);
}

int32_t Player::get_poison() const {
  return static_cast<int32_t>(memory.read_word(word_addr(OFF_POISON))&0xFFFF);
}

// --- Player flags ---

bool Player::has_player_flag(quest::PlayerFlag flag) const {
  int f=static_cast<int>(flag);
  uint32_t addr=word_addr(OFF_STATUS_BITS_0)+static_cast<uint32_t>(f/16);
  int bit=f%16;
  uint32_t word=memory.read_word(addr);
  return (word>>(15-bit))&1;
}

void Player::set_player_flag(quest::PlayerFlag flag) {
  int f=static_cast<int>(flag);
  uint32_t addr=word_addr(OFF_STATUS_BITS_0)+static_cast<uint32_t>(f/16);
  int bit=f%16;
  uint32_t word=memory.read_word(addr);
  word|=1u<<(15-bit);
  memory.write_word(addr, word);
}

void Player::clear_player_flag(quest::PlayerFlag flag) {
  int f=static_cast<int>(flag);
  uint32_t addr=word_addr(OFF_STATUS_BITS_0)+static_cast<uint32_t>(f/16);
  int bit=f%16;
  uint32_t word=memory.read_word(addr);
  word&=~(1u<<(15-bit));
  memory.write_word(addr, word);
}

void Player::set_player_flag(quest::PlayerFlag flag, bool value) {
  if(value) set_player_flag(flag);
  else clear_player_flag(flag);
}

// --- Escape hatches ---

int32_t Player::read_word(uint32_t offset) const {
  return static_cast<int32_t>(memory.read_word(word_addr(offset))&0xFFFF);
}

void Player::write_word(uint32_t offset, int32_t value) {
  memory.write_word(word_addr(offset), static_cast<uint32_t>(value)&0xFFFF);
}

int32_t Player::read_wide(uint32_t offset) const {
  return static_cast<int32_t>(memory.read_wide(word_addr(offset)));
}

void Player::write_wide(uint32_t offset, int32_t value) {
  memory.write_wide(word_addr(offset), static_cast<uint32_t>(value));
}

int32_t Player::read_bit(uint32_t bit_offset) const {
  int32_t signed_bit=sign_extend_16(bit_offset);
  int32_t abs_bit=player_num*STRIDE*16+signed_bit;
  uint32_t addr=static_cast<uint32_t>(
    static_cast<int32_t>(sd_base)+abs_bit/16);
  int pos=abs_bit%16;
  if(pos<0) { addr--; pos+=16; }
  uint32_t word=memory.read_word(addr);
  return static_cast<int32_t>((word>>(15-pos))&1);
}

void Player::write_bit(uint32_t bit_offset, int32_t value) {
  int32_t signed_bit=sign_extend_16(bit_offset);
  int32_t abs_bit=player_num*STRIDE*16+signed_bit;
  uint32_t addr=static_cast<uint32_t>(
    static_cast<int32_t>(sd_base)+abs_bit/16);
  int pos=abs_bit%16;
  if(pos<0) { addr--; pos+=16; }
  uint32_t word=memory.read_word(addr);
  uint32_t mask=1u<<(15-pos);
  if(value)
    word|=mask;
  else
    word&=~mask;
  memory.write_word(addr, word);
}

} // namespace emu_quest
