// src/emu_types/OperatingSystem.hpp
#pragma once
#include "../types/OperatingSystem.hpp"

namespace hw { class Machine; }

namespace emu_types {

class OperatingSystem : public types::OperatingSystem {
public:
  hw::Machine& machine;

  explicit OperatingSystem(hw::Machine& m);
  ~OperatingSystem() override;

  quest::SharedData* init_shared_data() override;
  types::SharedRandomState* get_random_state() override;

  int32_t current_pid(int32_t& pid) override;
  int32_t connect(int32_t pid) override;
  int32_t disconnect(int32_t pid) override;
  int32_t serve(int32_t port) override;
  [[noreturn]] void terminate_process(const std::string& message) override;
  int32_t lookup_port(const types::String& service_name, int32_t& port) override;
  int32_t ipc_send(int32_t destination_port,
                   int32_t origin_port,
                   int32_t user_flags,
                   const types::WordArray& send_data,
                   int32_t value) override;
  int32_t ipc_receive(int32_t& origin,
                      int32_t& destination_port,
                      int32_t& user_flags,
                      types::WordArray& receive_data,
                      int32_t& value) override;
  int32_t ipc_send_receive(int32_t destination_port,
                           int32_t origin_port,
                           int32_t& user_flags,
                           const types::WordArray& send_data,
                           int32_t send_value,
                           types::WordArray& receive_data,
                           int32_t& receive_value) override;
  int32_t write_screen(int32_t channel, const types::String& text) override;
  int32_t write_screen(int32_t channel, const types::String& text,
                       int row, int col, int32_t options) override;
  int32_t read_screen(int32_t channel, types::String& result,
                      int32_t max_length) override;
  int32_t read_channel(int32_t channel, types::ByteArray& buffer,
                       int32_t options, bool& eof) override;
  int32_t read_channel_at(int32_t channel, types::ByteArray& buffer,
                          int32_t options, int32_t record_number,
                          bool& eof) override;
  int32_t open_file(const types::String& filename, int32_t options,
                    int32_t& channel) override;
  int32_t close_file(int32_t channel) override;
  int32_t write_channel_bytes(int32_t channel,
                              const types::ByteArray& data) override;
  int32_t write_channel_bytes_at(int32_t channel,
                                 const types::ByteArray& data,
                                 int32_t record_number) override;
  int32_t open_shared_io_file(const types::String& filename,
                              int32_t read_only,
                              int32_t& channel) override;
  int32_t get_shared_page(int32_t channel, int32_t map_address,
                          int32_t page_count,
                          int32_t disk_page_number) override;
  int32_t create_task(int32_t entry_point, int32_t ac2_value,
                      int32_t stack_size) override;
  int32_t await_console_interrupt() override;
  int32_t gshpt(int32_t& shared_start, int32_t& page_count) override;
  int32_t sshpt(int32_t shared_start, int32_t page_count) override;
  int32_t get_current_time(int32_t& seconds, int32_t& minutes,
                           int32_t& hours) override;
  int32_t create_ipc_file(const types::String& filename,
                          int32_t local_port) override;
  int32_t recreate_file(const types::String& filename) override;

private:
  quest::SharedData* owned_shared_data;
  types::SharedRandomState* owned_random_state;
};

} // namespace emu_types
