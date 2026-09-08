// src/types/OperatingSystem.hpp
#pragma once
#include <cstdint>
#include <string>

namespace types {

class String;
class ByteArray;
class WordArray;

class SharedRandomState;

} // namespace types

namespace quest { class SharedData; }

namespace types {

// All methods return 0 on success, non-zero error code on failure.
// Output values are returned via reference parameters.

class OperatingSystem {
public:
  virtual ~OperatingSystem() = default;

  // Shared memory initialization — opens and maps the three shared
  // files (SHARED_DATA_FILE, WORLD_DATA_FILE, CASTLE_DATA_FILE).
  // Returns a SharedData* that provides access to all three regions.
  // The OperatingSystem retains ownership of the returned object.
  // Called once during game startup (from quest::init_shared_data).
  virtual quest::SharedData* init_shared_data() = 0;

  // Returns the random state created by init_shared_data().
  // Must be called after init_shared_data().
  virtual SharedRandomState* get_random_state() = 0;

  // Process identity
  virtual int32_t current_pid(int32_t& pid) = 0;

  // IPC
  virtual int32_t connect(int32_t pid) = 0;
  virtual int32_t disconnect(int32_t pid) = 0;
  virtual int32_t serve(int32_t port) = 0;

  // Process termination (SYSCALL 0310 — never returns, throws)
  [[noreturn]] virtual void terminate_process(const std::string& message) = 0;
  virtual int32_t lookup_port(const String& service_name, int32_t& port) = 0;

  // ISEND (SYSCALL 025) — fire and forget
  // value: 32-bit payload used when send_data is empty
  virtual int32_t ipc_send(int32_t destination_port,
                           int32_t origin_port,
                           int32_t user_flags,
                           const WordArray& send_data,
                           int32_t value) = 0;

  // IREC (SYSCALL 026) — blocking receive
  // origin: packed (pid<<16)|port of sender
  // value: 32-bit payload returned when receive_data is empty
  virtual int32_t ipc_receive(int32_t& origin,
                              int32_t& destination_port,
                              int32_t& user_flags,
                              WordArray& receive_data,
                              int32_t& value) = 0;

  // ISR (SYSCALL 0142) — send then receive
  virtual int32_t ipc_send_receive(int32_t destination_port,
                                   int32_t origin_port,
                                   int32_t& user_flags,
                                   const WordArray& send_data,
                                   int32_t send_value,
                                   WordArray& receive_data,
                                   int32_t& receive_value) = 0;

  // Terminal I/O
  virtual int32_t write_screen(int32_t channel, const String& text) = 0;
  virtual int32_t write_screen(int32_t channel, const String& text,
                               int row, int col, int32_t options) = 0;
  virtual int32_t read_screen(int32_t channel, String& result,
                              int32_t max_length) = 0;

  // File I/O
  virtual int32_t open_file(const String& filename, int32_t options,
                            int32_t& channel) = 0;
  virtual int32_t close_file(int32_t channel) = 0;

  // options: AOS/VS ?ISTI flags. ?IBIN (0x1000) = binary/raw mode.
  virtual int32_t read_channel(int32_t channel, ByteArray& buffer,
                               int32_t options, bool& eof) = 0;
  // Positioned read: seek to record_number first, then read.
  virtual int32_t read_channel_at(int32_t channel, ByteArray& buffer,
                                  int32_t options, int32_t record_number,
                                  bool& eof) = 0;
  virtual int32_t write_channel_bytes(int32_t channel,
                                      const ByteArray& data) = 0;
  virtual int32_t write_channel_bytes_at(int32_t channel,
                                         const ByteArray& data,
                                         int32_t record_number) = 0;

  // Shared I/O
  virtual int32_t open_shared_io_file(const String& filename,
                                      int32_t read_only,
                                      int32_t& channel) = 0;
  virtual int32_t get_shared_page(int32_t channel, int32_t map_address,
                                  int32_t page_count,
                                  int32_t disk_page_number) = 0;

  // Task management
  virtual int32_t create_task(int32_t entry_point, int32_t ac2_value,
                              int32_t stack_size) = 0;

  // Console interrupt
  virtual int32_t await_console_interrupt() = 0;

  // Shared page management (SYSCALL 073/044)
  virtual int32_t gshpt(int32_t& shared_start, int32_t& page_count) = 0;
  virtual int32_t sshpt(int32_t shared_start, int32_t page_count) = 0;

  // Time
  virtual int32_t get_current_time(int32_t& seconds, int32_t& minutes,
                                   int32_t& hours) = 0;

  // File creation
  virtual int32_t create_ipc_file(const String& filename, int32_t local_port) = 0;
  virtual int32_t recreate_file(const String& filename) = 0;
};

} // namespace types
