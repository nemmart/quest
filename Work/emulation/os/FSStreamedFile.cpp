#include "FSStreamedFile.hpp"
#include "FSFile.hpp"
#include "OSError.hpp"




namespace os {
FSStreamedFile::FSStreamedFile(FSFile* file)
  : file(file), position(0), updated(false) {}

int32_t FSStreamedFile::available() {
  return file->get_length() - position;
}

void FSStreamedFile::set_position(int32_t pos) {
  if(pos > file->get_length())
    pos = file->get_length();
  position = pos;
}

int32_t FSStreamedFile::read(std::vector<uint8_t>& bytes) {
  int32_t amount = static_cast<int32_t>(bytes.size());
  if(amount > available())
    amount = available();
  // Reading from a copy is fine
  int32_t cur_page = -1;
  std::vector<uint8_t> content;
  for(int32_t i = 0; i < amount; i++) {
    if((position >> 11) != cur_page) {
      cur_page = position >> 11;
      content = file->load_page(cur_page);
    }
    bytes[i] = content[position & 0x7FF];
    position++;
  }
  return amount;
}

void FSStreamedFile::write(const std::vector<uint8_t>& bytes) {
  file->modified = true;
  if(position + static_cast<int32_t>(bytes.size()) > file->get_length())
    file->set_length(position + static_cast<int32_t>(bytes.size()));

  // Write through the PageSet so changes hit the actual Page objects.
  // Java's loadPage() returned a reference to the internal byte[],
  // but our load_page() returns a copy, so we must use write_byte().
  PageSet* ps = file->get_page_set();
  for(int32_t i = 0; i < static_cast<int32_t>(bytes.size()); i++) {
    ps->write_byte(position, bytes[i]);
    position++;
  }
  updated = true;
}

void FSStreamedFile::close() {
  if(updated) {
    int32_t error = file->store_pages(nullptr);
    if(error != 0)
      throw OSError(error);
  }
}

} // namespace os
