#include "FSChannel.hpp"
#include "FSFile.hpp"
#include "FSPagedFile.hpp"
#include "FSStreamedFile.hpp"
#include "FSTerminal.hpp"
#include "FSObject.hpp"
#include "OSError.hpp"
#include "FS.hpp"
#include "../hw/PageSet.hpp"




namespace os {
using namespace hw;

FSChannel::FSChannel(int32_t mode, FSPagedFile* pf)
  : mode(mode), paged_file(pf), streamed_file(nullptr), stream_io(nullptr) {}

FSChannel::FSChannel(int32_t mode, FSStreamedFile* sf)
  : mode(mode), paged_file(nullptr), streamed_file(sf), stream_io(nullptr) {}

FSChannel::FSChannel(int32_t mode, FSStreamIO* sio)
  : mode(mode), paged_file(nullptr), streamed_file(nullptr), stream_io(sio) {}

FSChannel::~FSChannel() {
  delete paged_file;
  delete streamed_file;
  // stream_io not owned
}

FSChannel* FSChannel::open_for_paged_io(const std::string& fs_path, bool read_only) {
  FSObject* object = FS::retrieve(fs_path);
  if(!object) throw OSError(OSError::FS_FILE_NOT_FOUND);
  FSFile* file = dynamic_cast<FSFile*>(object);
  if(!file) throw OSError(OSError::FS_WRONG_FILE_TYPE);

  int32_t m = PAGED_IO | READ_PERMISSION;
  if(!read_only) m |= WRITE_PERMISSION;
  return new FSChannel(m, new FSPagedFile(file));
}

FSChannel* FSChannel::open_for_streamed_io(const std::string& fs_path, bool read_only) {
  FSObject* object = FS::retrieve(fs_path);
  if(!object) throw OSError(OSError::FS_FILE_NOT_FOUND);
  FSFile* file = dynamic_cast<FSFile*>(object);
  if(!file) throw OSError(OSError::FS_WRONG_FILE_TYPE);

  int32_t m = STREAM_IO | READ_PERMISSION;
  if(!read_only) m |= WRITE_PERMISSION;
  return new FSChannel(m, new FSStreamedFile(file));
}

FSChannel* FSChannel::open_for_streamed_io(FSStreamIO* stream, int32_t permission) {
  return new FSChannel(permission | STREAM_IO, stream);
}

void FSChannel::close() {
  if(streamed_file) streamed_file->close();
}

std::vector<uint8_t> FSChannel::read_page(int32_t page_number) {
  if((mode & 0x01) != PAGED_IO)
    throw OSError(OSError::FS_PAGING_NOT_ALLOWED_ON_FILE);
  if(page_number < 0 || page_number >= paged_file->page_count())
    throw OSError(OSError::FS_INVALID_PAGE_NUMBER);
  return paged_file->load_page(page_number);
}

std::vector<std::vector<uint8_t>> FSChannel::read_all_pages() {
  if((mode & 0x01) != PAGED_IO)
    throw OSError(OSError::FS_PAGING_NOT_ALLOWED_ON_FILE);
  return paged_file->all_pages();
}

int32_t FSChannel::page_count() {
  if((mode & 0x01) != PAGED_IO)
    throw OSError(OSError::FS_PAGING_NOT_ALLOWED_ON_FILE);
  return paged_file->page_count();
}

PageSet* FSChannel::page_set() {
  if((mode & 0x01) != PAGED_IO)
    throw OSError(OSError::FS_PAGING_NOT_ALLOWED_ON_FILE);
  return paged_file->page_set();
}

void FSChannel::set_position(int32_t offset) {
  if(streamed_file)
    streamed_file->set_position(offset);
  else
    throw OSError(OSError::FS_SET_POSITION_NOT_ALLOWED_ON_FILE);
}

int32_t FSChannel::read(std::vector<uint8_t>& bytes, bool line_mode) {
  if((mode & 0x01) != STREAM_IO)
    throw OSError(OSError::FS_STREAMING_NOT_ALLOWED_ON_FILE);
  if(line_mode) {
    FSTerminal* terminal = dynamic_cast<FSTerminal*>(stream_io);
    if(terminal)
      return terminal->read(bytes, line_mode);
  }
  if(stream_io)
    return stream_io->read(bytes);
  if(streamed_file)
    return streamed_file->read(bytes);
  throw OSError(OSError::FS_STREAMING_NOT_ALLOWED_ON_FILE);
}

void FSChannel::write(const std::vector<uint8_t>& bytes) {
  if((mode & 0x01) != STREAM_IO)
    throw OSError(OSError::FS_STREAMING_NOT_ALLOWED_ON_FILE);
  if(stream_io)
    stream_io->write(bytes);
  else if(streamed_file)
    streamed_file->write(bytes);
  else
    throw OSError(OSError::FS_STREAMING_NOT_ALLOWED_ON_FILE);
}

FSFile* FSChannel::get_file() {
  if(paged_file)
    return paged_file->file;
  if(streamed_file)
    return streamed_file->file;
  return nullptr;
}

} // namespace os
