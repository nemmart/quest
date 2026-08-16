#include "PageSet.hpp"




namespace hw {
PageSet::PageSet() {
  std::memset(blocks, 0, sizeof(blocks));
}

void PageSet::map_page(Page* page, uint32_t page_number) {
  uint32_t block = (page_number>>9)&0xFFF;
  if (blocks[block] == nullptr)
    blocks[block] = new Block();
  blocks[block]->pages[page_number&0x1FF] = page;
}

void PageSet::unmap_page(uint32_t page_number) {
  uint32_t block = (page_number>>9)&0xFFF;
  blocks[block]->pages[page_number&0x1FF] = nullptr;
}

Page* PageSet::find_page(uint32_t page_number) {
  uint32_t block = (page_number>>9)&0xFFF;
  return blocks[block]->pages[page_number&0x1FF];
}

uint32_t PageSet::read_byte(uint32_t address) {
  uint32_t block = (address>>20)&0xFFF;
  uint32_t page = (address>>11)&0x1FF;
  uint32_t offset = address&0x7FF;

  if (blocks[block] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + " not loaded");
  if (blocks[block]->pages[page] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + ", page " + std::to_string(page) + " not loaded");

  return blocks[block]->pages[page]->read_byte(offset);
}

uint32_t PageSet::read_word(uint32_t address) {
  uint32_t block = (address>>19)&0xFFF;
  uint32_t page = (address>>10)&0x1FF;
  uint32_t offset = address&0x3FF;

  if (blocks[block] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + " not loaded");
  if (blocks[block]->pages[page] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + ", page " + std::to_string(page) + " not loaded");

  return blocks[block]->pages[page]->read_word(offset);
}

uint32_t PageSet::read_wide(uint32_t address) {
  uint32_t block = (address>>19)&0xFFF;
  uint32_t page = (address>>10)&0x1FF;
  uint32_t offset = address&0x3FF;

  if (offset == 1023) {
    // read might cross a page boundary
    uint32_t high = read_word(address);
    uint32_t low = read_word(address+1);
    return (high<<16) | (low&0xFFFF);
  }
  if (blocks[block] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + " not loaded");
  if (blocks[block]->pages[page] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + ", page " + std::to_string(page) + " not loaded");

  return blocks[block]->pages[page]->read_wide(offset);
}

uint64_t PageSet::read_quad(uint32_t address) {
  uint64_t high = read_wide(address), low = read_wide(address+2);
  return (high<<32) + (low&0xFFFFFFFF);
}

void PageSet::write_byte(uint32_t address, uint32_t value) {
  uint32_t block = (address>>20)&0xFFF;
  uint32_t page = (address>>11)&0x1FF;
  uint32_t offset = address&0x7FF;

  if (blocks[block] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + " not loaded");
  if (blocks[block]->pages[page] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + ", page " + std::to_string(page) + " not loaded");

  blocks[block]->pages[page]->write_byte(offset, value);
}

void PageSet::write_word(uint32_t address, uint32_t value) {
  uint32_t block = (address>>19)&0xFFF;
  uint32_t page = (address>>10)&0x1FF;
  uint32_t offset = address&0x3FF;

  if (blocks[block] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + " not loaded");
  if (blocks[block]->pages[page] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + ", page " + std::to_string(page) + " not loaded");

  blocks[block]->pages[page]->write_word(offset, value);
}

void PageSet::write_wide(uint32_t address, uint32_t value) {
  uint32_t block = (address>>19)&0xFFF;
  uint32_t page = (address>>10)&0x1FF;
  uint32_t offset = address&0x3FF;

  if (offset == 1023) {
    // write crosses a page boundary
    write_word(address, value>>16);
    write_word(address+1, value&0xFFFF);
    return;
  }
  if (blocks[block] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + " not loaded");
  if (blocks[block]->pages[page] == nullptr)
    throw std::runtime_error("Segment fault - block " + std::to_string(block) + ", page " + std::to_string(page) + " not loaded");

  blocks[block]->pages[page]->write_wide(offset, value);
}

void PageSet::write_quad(uint32_t address, uint64_t value) {
  write_wide(address, static_cast<uint32_t>(value>>32));
  write_wide(address+2, static_cast<uint32_t>(value&0xFFFFFFFF));
}

} // namespace hw
