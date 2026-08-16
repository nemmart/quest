#pragma once
// Trace facility: -trace FILE -types TYPE,TYPE,...
//
// Types are short strings gating independent streams of trace lines, all
// written to one file with the type as the first token, e.g.:
//   scalls seq=000042 caller=QUEST tid=1 call=?READ ac0=...
//   shared seq=000043 caller=QUEST_SERVER page=SHARED_DATA_FILE:3 ...
// Current types: scalls (system call entry/exit), shared (writes to
// shared-mapped file pages). Unknown types warn and are ignored.

#include <cstdint>
#include <string>

namespace os {

class Trace {
public:
  // Called once from Launch after parsing flags. Returns false on failure
  // (e.g. file cannot be opened).
  static bool initialize(const std::string& file, const std::string& types_csv);

  // Fast check used to gate call sites.
  static bool enabled(const char* type);

  // Append one line: "<type> seq=NNNNNN <caller> <text>". Thread-safe.
  static void line(const char* type, const std::string& caller, const std::string& text);

  // Human name for an AOS/VS system call number (octal constants from
  // OSContext), e.g. 0302 -> "?READ". Unknown -> "?<octal>".
  static std::string call_name(int32_t call);

  static void shutdown();
};

} // namespace os
