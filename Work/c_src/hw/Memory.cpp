#include "Memory.hpp"
#include "../os/Trace.hpp"
#include <cstdio>
#include <cstring>




namespace hw {
Memory::Memory() : PageSet() {
  std::memset(permissions, 0, sizeof(permissions));
}

uint8_t Memory::ever_mapped[4096*512];

void Memory::dump_ever_mapped(FILE* out) {
  fprintf(out, "pagemap census (ever-mapped page ranges, word addresses):\n");
  uint32_t n = 4096*512, i = 0;
  while(i < n) {
    if(!ever_mapped[i]) { i++; continue; }
    uint32_t j = i;
    while(j < n && ever_mapped[j]) j++;
    fprintf(out, "  %08X..%08X  (%u pages)%s\n", i<<10, ((j<<10)-1), j-i,
            (i<<10) >= 0x74000000u ? "  <-- AT/ABOVE 0x74000000" : "");
    i = j;
  }
}

void Memory::map_page(Page* page, uint32_t page_number, uint32_t permission) {
  uint32_t block = (page_number>>9)&0xFFF;
  ever_mapped[page_number] = 1;
  if(os::Trace::enabled("pagemap")) {
    char buf[96];
    snprintf(buf, sizeof(buf), "map page=%06X addr=%08X perm=%X%s", page_number, page_number<<10,
             permission, permissions[page_number] & Permissions::PERMISSION_MAPPED ? " (remap)" : "");
    os::Trace::line("pagemap", process_name, buf);
  }
  permissions[page_number] = static_cast<uint8_t>((permission&Permissions::PERMISSIONS_READ_WRITE_EXECUTE) | Permissions::PERMISSION_MAPPED);
  if (blocks[block] == nullptr)
    blocks[block] = new Block();
  blocks[block]->pages[page_number&0x1FF] = page;
}

void Memory::unmap_page(uint32_t page_number) {
  uint32_t block = (page_number>>9)&0xFFF;
  if(os::Trace::enabled("pagemap")) {
    char buf[64];
    snprintf(buf, sizeof(buf), "unmap page=%06X addr=%08X", page_number, page_number<<10);
    os::Trace::line("pagemap", process_name, buf);
  }
  permissions[page_number] = 0;
  blocks[block]->pages[page_number&0x1FF] = nullptr;
}

Page* Memory::find_page(uint32_t page_number) {
  uint32_t block = (page_number>>9)&0xFFF;
  return blocks[block]->pages[page_number&0x1FF];
}

uint32_t Memory::read_instruction_word(uint32_t address) {
  uint32_t page_number = (address>>10)&0x1FFFFF;
  uint32_t block = (page_number>>9)&0xFFF;
  uint32_t page = page_number&0x1FF;
  uint32_t offset = address&0x3FF;

  if ((permissions[page_number]&Permissions::PERMISSION_EXECUTE) == 0) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Page does not have execute permission, address=%08X", address);
    throw std::runtime_error(buf);
  }
  if (blocks[block] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + " not loaded");
  if (blocks[block]->pages[page] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + ", page " + std::to_string(page) + " not loaded");

  return blocks[block]->pages[page]->read_word(offset);
}

uint32_t Memory::read_instruction_wide(uint32_t address) {
  uint32_t page_number = (address>>10)&0x1FFFFF;
  uint32_t block = (page_number>>9)&0xFFF;
  uint32_t page = page_number&0x1FF;
  uint32_t offset = address&0x3FF;

  if (offset == 1023) {
    uint32_t high = read_word(address);
    uint32_t low = read_word(address+1);
    return (high<<16) | (low&0xFFFF);
  }

  if ((permissions[page_number]&Permissions::PERMISSION_EXECUTE) == 0) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Page does not have execute permission, address=%08X", address);
    throw std::runtime_error(buf);
  }
  if (blocks[block] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + " not loaded");
  if (blocks[block]->pages[page] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + ", page " + std::to_string(page) + " not loaded");

  return blocks[block]->pages[page]->read_wide(offset);
}

uint32_t Memory::read_byte(uint32_t address) {
  uint32_t page_number = address>>11;
  uint32_t block = (page_number>>9)&0xFFF;
  uint32_t page = page_number&0x1FF;
  uint32_t offset = address&0x7FF;

  if ((permissions[page_number]&Permissions::PERMISSION_READ) == 0) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Page does not have read permission (byte), address=%08X page=%06X perm=%02X", address, page_number, permissions[page_number]);
    throw std::runtime_error(buf);
  }
  if (blocks[block] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + " not loaded");
  if (blocks[block]->pages[page] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + ", page " + std::to_string(page) + " not loaded");

  return blocks[block]->pages[page]->read_byte(offset);
}

void Memory::check_reserved_access(uint32_t address, bool is_write) {
  if (sd_ptr_cache == 0) return;
  
  // Is this address in the player array?  (SD_PTR + 44 .. SD_PTR + 44 + 686*10 - 1)
  if (address < sd_ptr_cache + 44) return;
  if (address >= sd_ptr_cache + 44 + 686 * 10) return;
  
  uint32_t offset = address - sd_ptr_cache - 44;
  uint32_t player = offset / 686;
  uint32_t rec = offset % 686;
  
  // Reserved regions (no code accesses found in either QUEST or QUEST_SERVER)
  bool reserved = false;
  if (rec >= 310 && rec <= 372) reserved = true;
  else if (rec >= 426 && rec <= 469) reserved = true;
  else if (rec >= 473 && rec <= 551) reserved = true;
  else if (rec >= 555 && rec <= 564) reserved = true;
  else if (rec >= 660 && rec <= 683) reserved = true;
  else if (rec == 685) reserved = true;
  
  if (reserved) {
    std::fprintf(stderr, "*** RESERVED ACCESS [%s]: %s player[%u] rec %u (addr 0x%08X)\n",
                 process_name.c_str(), is_write ? "WRITE" : "READ", player, rec, address);
  }
}

uint32_t Memory::read_word(uint32_t address) {
  check_reserved_access(address, false);

  uint32_t page_number = (address>>10)&0x1FFFFF;
  uint32_t block = (page_number>>9)&0xFFF;
  uint32_t page = page_number&0x1FF;
  uint32_t offset = address&0x3FF;

  if ((permissions[page_number]&Permissions::PERMISSION_READ) == 0) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Page does not have read permission (word), address=%08X page=%06X perm=%02X", address, page_number, permissions[page_number]);
    throw std::runtime_error(buf);
  }
  if (blocks[block] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + " not loaded");
  if (blocks[block]->pages[page] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + ", page " + std::to_string(page) + " not loaded");

  return blocks[block]->pages[page]->read_word(offset);
}

uint32_t Memory::read_wide(uint32_t address) {
  check_reserved_access(address, false);
  check_reserved_access(address + 1, false);

  uint32_t page_number = (address>>10)&0x1FFFFF;
  uint32_t block = (page_number>>9)&0xFFF;
  uint32_t page = page_number&0x1FF;
  uint32_t offset = address&0x3FF;

  if (offset == 1023) {
    uint32_t high = read_word(address);
    uint32_t low = read_word(address+1);
    return (high<<16) | (low&0xFFFF);
  }

  if ((permissions[page_number]&Permissions::PERMISSION_READ) == 0) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Page does not have read permission (wide), address=%08X page=%06X perm=%02X", address, page_number, permissions[page_number]);
    throw std::runtime_error(buf);
  }
  if (blocks[block] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + " not loaded");
  if (blocks[block]->pages[page] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + ", page " + std::to_string(page) + " not loaded");

  return blocks[block]->pages[page]->read_wide(offset);
}

uint64_t Memory::read_quad(uint32_t address) {
  uint64_t high = read_wide(address), low = read_wide(address+2);
  return (high<<32) + (low&0xFFFFFFFF);
}

void Memory::write_byte(uint32_t address, uint32_t value) {
  uint32_t page_number = address>>11;
  uint32_t block = (page_number>>9)&0xFFF;
  uint32_t page = page_number&0x1FF;
  uint32_t offset = address&0x7FF;

  if ((permissions[page_number]&Permissions::PERMISSION_WRITE) == 0)
    throw std::runtime_error("Page does not have write permission");
  if (blocks[block] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + " not loaded");
  if (blocks[block]->pages[page] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + ", page " + std::to_string(page) + " not loaded");

  blocks[block]->pages[page]->write_byte(offset, value);
}

void Memory::write_word(uint32_t address, uint32_t value) {
  check_reserved_access(address, true);

  uint32_t page_number = (address>>10)&0x1FFFFF;
  uint32_t block = (page_number>>9)&0xFFF;
  uint32_t page = page_number&0x1FF;
  uint32_t offset = address&0x3FF;

  if ((permissions[page_number]&Permissions::PERMISSION_WRITE) == 0)
    throw std::runtime_error("Page does not have write permission");
  if (blocks[block] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + " not loaded");
  if (blocks[block]->pages[page] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + ", page " + std::to_string(page) + " not loaded");

  blocks[block]->pages[page]->write_word(offset, value);
}

void Memory::write_wide(uint32_t address, uint32_t value) {
  check_reserved_access(address, true);
  check_reserved_access(address + 1, true);

  // Watch for wide writes to SD_PTR
  if (address == 0x70000210) {
    sd_ptr_cache = value;
    std::fprintf(stderr, "*** SD_PTR [%s] set to 0x%08X\n", process_name.c_str(), sd_ptr_cache);
  }

  uint32_t page_number = (address>>10)&0x1FFFFF;
  uint32_t block = (page_number>>9)&0xFFF;
  uint32_t page = page_number&0x1FF;
  uint32_t offset = address&0x3FF;

  if (offset == 1023) {
    write_word(address, value>>16);
    write_word(address+1, value&0xFFFF);
    return;
  }

  if ((permissions[page_number]&Permissions::PERMISSION_WRITE) == 0)
    throw std::runtime_error("Page does not have write permission");
  if (blocks[block] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + " not loaded");
  if (blocks[block]->pages[page] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + ", page " + std::to_string(page) + " not loaded");

  blocks[block]->pages[page]->write_wide(offset, value);
}

void Memory::write_quad(uint32_t address, uint64_t value) {
  write_wide(address, static_cast<uint32_t>(value>>32));
  write_wide(address+2, static_cast<uint32_t>(value&0xFFFFFFFF));
}

} // namespace hw
