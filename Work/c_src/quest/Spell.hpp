// src/quest/Spell.hpp
#pragma once
#include <cstdint>

namespace quest {

enum class Spell : int32_t {
  LIGHTNING_BOLT         =  1,
  MAGIC_SHIELD           =  2,
  CREATE_SMOKE_SCREEN    =  3,
  FAHRENHEIT_FOILER      =  4,
  OBTAIN_KNOWLEDGE       =  5,
  DUMBFOUND_ATTACKER     =  6,
  ACTIVATE_CLOAK         =  7,
  HEALING_AURA           =  8,
  CAMOUFLAGE             =  9,
  INVISIBILITY           = 10,
  WIELD_THORS_HAMMER     = 11,
  TELEPORT               = 12,
  GAZE_CRYSTAL_BALL      = 13,
  FIELD_OF_PROTECTION    = 14,
  EYE_OF_ARGUS           = 15,
  ACTIVATE_CADUCEUS      = 16,
  STRENGTH_OF_ATLAS      = 17,
  WIELD_STAFF_OF_DEATH   = 18,
  CREATE_A_BOAT          = 19,
  SUMMON_PEGASUS         = 20,
  CALL_HEALER            = 21,
  CAUSE_FEAR             = 22,
  CLEAR_VISION           = 23,
  PANIC                  = 24,
  WEAKEN_OPPONENT        = 25,
  CONJOUR_STORM          = 26,
  SUMMON_SUN             = 27,
  DISPATCH_FAMILIAR      = 28,
  RECALL_FAMILIAR        = 29,
  FAMILIAR_CLAIRVOYANCE  = 30,
  CHARM_FAMILIAR         = 31,
  CREATE_FOOD            = 32,
  CREATE_WATER           = 33,
  FIRE_BALL              = 34,
  RUN_LIKE_A_HORSE       = 35,
  POLYMORPH              = 36,
  CREATE_FOG             = 37,
};

static constexpr int32_t SPELL_COUNT = 37;

} // namespace quest
