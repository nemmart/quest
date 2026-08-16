#pragma once
#include <cstdint>
#include <string>
#include <vector>


namespace hw { class Machine; class Memory; }

namespace os {
using namespace hw;

class OSProcess;
class OSTask;

// One captured caller-memory write during a mediated system call
// (docs/LockstepHarness.md): recorded on the master, replayed into the
// clone. width is 1 (byte), 2 (word), or 4 (wide).
struct MediatedWrite {
  uint8_t width;
  uint32_t address;
  uint32_t value;
  // True when the write was already applied to the clone at capture time
  // (funnel dual-write to a mirrored shared-region page); replay skips it.
  bool delivered = false;
};

class OSContext {
public:
  static constexpr int32_t HIGH = 0;
  static constexpr int32_t LOW = 1;
  static constexpr int32_t SUCCESS = 0;

  // System call numbers (octal literals)
  static constexpr int32_t CREATE  = 0000;
  static constexpr int32_t ISEND   = 0025;
  static constexpr int32_t IREC    = 0026;
  static constexpr int32_t ISR     = 0142;
  static constexpr int32_t ILKUP   = 0027;
  static constexpr int32_t CON     = 0167;
  static constexpr int32_t DCON    = 0170;

  static constexpr int32_t TASK    = 0500;
  static constexpr int32_t REC     = 0525;
  static constexpr int32_t KILAD   = 0505;
  static constexpr int32_t DFRSCH  = 0550;   // defer rescheduling (SWAT.REX)

  static constexpr int32_t MEM     = 0003;
  static constexpr int32_t MEMI    = 0014;
  static constexpr int32_t SSHPT   = 0044;
  static constexpr int32_t SOPEN   = 0063;
  static constexpr int32_t SCLOSE  = 0161;   // close a file opened for shared access
  static constexpr int32_t SPAGE   = 0060;
  static constexpr int32_t GSHPT   = 0073;
  static constexpr int32_t GTOD    = 0036;
  static constexpr int32_t RECREATE= 0336;
  static constexpr int32_t OPEN    = 0300;
  static constexpr int32_t CLOSE   = 0301;
  static constexpr int32_t READ    = 0302;
  static constexpr int32_t WRITE   = 0303;
  static constexpr int32_t UPDATE  = 0232;
  static constexpr int32_t UIDSTAT = 0333;

  static constexpr int32_t PNAME   = 0116;
  static constexpr int32_t RNGPR   = 0251;   // returns the .PR filename for a ring
  static constexpr int32_t SERVE   = 0171;
  static constexpr int32_t RETURN  = 0310;
  static constexpr int32_t ERMSG   = 0311;   // get the text of an error code
  static constexpr int32_t IXIT    = 0542;
  static constexpr int32_t INTWT   = 0016;
  static constexpr int32_t DADID   = 0127;
  static constexpr int32_t WDELAY  = 0263;

  OSProcess* process;
  OSTask* task;
  Memory* memory;
  Machine* machine;
  // When set (mediated call on the master), every caller-memory write is
  // logged here in addition to being performed.
  std::vector<MediatedWrite>* write_log = nullptr;
  // When set (mediated call on the master), every caller-memory read is
  // also performed against the clone's memory and compared, verifying the
  // call's inputs byte-for-byte (?WRITE payloads, ?ISR request content,
  // filenames, packet fields). Reads of addresses this call has already
  // written are skipped (the clone receives those via replay), as are
  // addresses in the mirrored shared region (server writes race there).
  Memory* read_verify = nullptr;
  OSTask* read_verify_task = nullptr;
  uint32_t clone_location(uint32_t address) const;   // M4a: master→clone dereference (Mapper.md §1.3), form-aware

  int32_t ac0, ac1, ac2, ac3;

  static OSContext* context_for_call(int32_t call, OSProcess* process, OSTask* task, Memory* memory, Machine* machine);

  OSContext(OSProcess* process, OSTask* task, Memory* memory, Machine* machine);
  virtual ~OSContext() = default;

  std::string full_path(const std::string& name);

  uint32_t aos_error(const std::string& name);
  uint32_t aos_symbol(const std::string& name);

  std::vector<uint8_t> read_byte_array(int32_t address, int32_t length);
  std::string read_string(int32_t address, int32_t length);
  std::string read_string(int32_t address);

  // Caller-memory write funnel: performs the write and, when write_log is
  // set, records it for lockstep replay. All syscall result writes must go
  // through these (directly or via the packet/array helpers below).
  uint32_t mem_read_byte(uint32_t address);
  uint32_t mem_read_word(uint32_t address);
  uint32_t mem_read_wide(uint32_t address);
  uint32_t last_clone_read_addr = 0;   // diagnostic (Stage 0b)
  void verify_read(uint32_t byte_lo, uint32_t byte_hi, uint64_t mine,
                   uint64_t other, uint32_t width);
  bool read_verifiable(uint32_t byte_lo, uint32_t byte_hi, uint32_t word_page);

  void mem_write_byte(uint32_t address, uint32_t value);
  void mem_write_word(uint32_t address, uint32_t value);
  void mem_write_wide(uint32_t address, uint32_t value);

  void write_byte_array(const std::vector<uint8_t>& bytes, int32_t address, int32_t length);
  void write_byte_array(const std::vector<uint8_t>& bytes, int32_t address);
  void write_string(const std::string& str, int32_t address);

  bool read_packet_flag(const std::string& symbol, const std::string& flag, int32_t location);
  int32_t read_packet_byte(const std::string& symbol, int32_t high_low, int32_t location);
  int32_t read_packet_word(const std::string& symbol, int32_t location);
  int32_t read_packet_wide(const std::string& symbol, int32_t location);

  bool read_packet_flag(const std::string& symbol, const std::string& flag);
  int32_t read_packet_byte(const std::string& symbol, int32_t high_low);
  int32_t read_packet_word(const std::string& symbol);
  int32_t read_packet_wide(const std::string& symbol);

  void write_packet_byte(const std::string& symbol, int32_t high_low, int32_t value, int32_t location);
  void write_packet_word(const std::string& symbol, int32_t value, int32_t location);
  void write_packet_wide(const std::string& symbol, int32_t value, int32_t location);

  void write_packet_byte(const std::string& symbol, int32_t high_low, int32_t value);
  void write_packet_word(const std::string& symbol, int32_t value);
  void write_packet_wide(const std::string& symbol, int32_t value);

  virtual int32_t dispatch_system_call(int32_t call);
};

} // namespace os
