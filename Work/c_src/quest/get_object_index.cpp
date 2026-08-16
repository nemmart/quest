// src/quest/get_object_index.cpp
#include "get_object_index.hpp"
#include "../types/Context.hpp"
#include "SharedData.hpp"

namespace quest {

// Object table layout
static constexpr uint32_t OBJ_X_OFFSET=0x2CE7;
static constexpr uint32_t OBJ_LINKED_OFFSET=0x2CEA;
static constexpr uint32_t OBJ_COUNT_OFFSET=0x2CEE;
static constexpr int32_t OBJ_STRIDE=9;

// 4 type arrays: each entry is wide (2 words), indexed by k*2
struct TypeArray {
  uint32_t base_offset;
  int32_t max_entries;
};

static constexpr TypeArray TYPE_ARRAYS[]={
  {0xE6977, 10000},
  {0xE4267,  5000},
  {0xEB797,  5000},
  {0xEDEA7, 10000},
};

// Check if any entry in a type array references the given object index,
// either directly or via the linked field (OBJ_LINKED_OFFSET).
// Returns true if a reference is found (slot is NOT free).
static bool is_referenced_in(quest::SharedData& sd, const TypeArray& ta, int32_t index) {
  for(int32_t k=1; k<=ta.max_entries; k++) {
    int32_t entry=sd.read_obj_wide(static_cast<uint32_t>(k)*2+ta.base_offset);
    if(entry==0) continue;

    // Direct reference?
    if(entry==index) return true;

    // Indirect reference via linked field?
    int32_t linked=sd.read_obj_wide(static_cast<uint32_t>(entry)*OBJ_STRIDE+OBJ_LINKED_OFFSET);
    if(linked==index) return true;
  }
  return false;
}

void get_object_index(types::Context& ctx, int32_t& index) {
  int32_t obj_count=ctx.shared->read_obj_wide(OBJ_COUNT_OFFSET);

  index=11;

  if(obj_count<11) {
    // Table too small — allocate new slot
    obj_count=ctx.shared->read_obj_wide(OBJ_COUNT_OFFSET);
    obj_count++;
    ctx.shared->write_obj_wide(OBJ_COUNT_OFFSET, obj_count);
    index=obj_count;
    return;
  }

  // Search for a truly free slot (index 12..obj_count)
  for(index=12; index<=obj_count; index++) {
    // Check if slot is empty (obj.x == 0)
    int32_t obj_x=ctx.shared->read_obj_word(
      static_cast<uint32_t>(index)*OBJ_STRIDE+OBJ_X_OFFSET);
    if(obj_x!=0) continue;

    // Slot appears empty — verify no type array references it
    bool referenced=false;
    for(int i=0; i<4 && !referenced; i++)
      referenced=is_referenced_in(*ctx.shared, TYPE_ARRAYS[i], index);

    if(!referenced) return;  // truly free — index is set
  }

  // No free slot found — allocate new
  obj_count=ctx.shared->read_obj_wide(OBJ_COUNT_OFFSET);
  obj_count++;
  ctx.shared->write_obj_wide(OBJ_COUNT_OFFSET, obj_count);
  index=obj_count;
}

} // namespace quest
