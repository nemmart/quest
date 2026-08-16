#include "Trace.hpp"
#include "OSContext.hpp"
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <set>
#include <sstream>

namespace os {

static FILE* trace_file = nullptr;
static std::set<std::string> trace_types;
static std::mutex trace_mutex;
static uint64_t trace_seq = 0;

static const char* known_types[] = { "scalls", "shared", "lockstep", "rtcalls", "pagemap", "redirect", "gcalls" };

bool Trace::initialize(const std::string& file, const std::string& types_csv) {
  trace_file = fopen(file.c_str(), "w");
  if(trace_file == nullptr) {
    fprintf(stderr, "Trace: cannot open '%s'\n", file.c_str());
    return false;
  }

  std::stringstream ss(types_csv);
  std::string type;
  while(std::getline(ss, type, ',')) {
    if(type.empty())
      continue;
    bool known = false;
    for(const char* k : known_types)
      if(type == k)
        known = true;
    if(!known)
      fprintf(stderr, "Trace: unknown type '%s' (ignored)\n", type.c_str());
    else
      trace_types.insert(type);
  }
  if(trace_types.empty()) {
    fprintf(stderr, "Trace: no valid types enabled\n");
    return false;
  }
  return true;
}

bool Trace::enabled(const char* type) {
  return trace_file != nullptr && trace_types.count(type) != 0;
}

void Trace::line(const char* type, const std::string& caller, const std::string& text) {
  if(trace_file == nullptr)
    return;
  std::lock_guard<std::mutex> lock(trace_mutex);
  fprintf(trace_file, "%s seq=%06llu caller=%s %s\n",
          type, static_cast<unsigned long long>(trace_seq++),
          caller.c_str(), text.c_str());
  fflush(trace_file);
}

std::string Trace::call_name(int32_t call) {
  switch(call) {
    case OSContext::CREATE:   return "?CREATE";
    case OSContext::ISEND:    return "?ISEND";
    case OSContext::IREC:     return "?IREC";
    case OSContext::ISR:      return "?ISR";
    case OSContext::ILKUP:    return "?ILKUP";
    case OSContext::CON:      return "?CON";
    case OSContext::DCON:     return "?DCON";
    case OSContext::TASK:     return "?TASK";
    case OSContext::REC:      return "?REC";
    case OSContext::KILAD:    return "?KILAD";
    case OSContext::MEM:      return "?MEM";
    case OSContext::MEMI:     return "?MEMI";
    case OSContext::SSHPT:    return "?SSHPT";
    case OSContext::SOPEN:    return "?SOPEN";
    case OSContext::SPAGE:    return "?SPAGE";
    case OSContext::GSHPT:    return "?GSHPT";
    case OSContext::GTOD:     return "?GTOD";
    case OSContext::RECREATE: return "?RECREATE";
    case OSContext::OPEN:     return "?OPEN";
    case OSContext::CLOSE:    return "?CLOSE";
    case OSContext::READ:     return "?READ";
    case OSContext::WRITE:    return "?WRITE";
    case OSContext::UPDATE:   return "?UPDATE";
    case OSContext::UIDSTAT:  return "?UIDSTAT";
    case OSContext::PNAME:    return "?PNAME";
    case OSContext::SERVE:    return "?SERVE";
    case OSContext::RETURN:   return "?RETURN";
    case OSContext::IXIT:     return "?IXIT";
    case OSContext::INTWT:    return "?INTWT";
    case OSContext::DADID:    return "?DADID";
    case OSContext::WDELAY:   return "?WDELAY";
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "?%04o", call);
  return std::string(buf);
}

void Trace::shutdown() {
  if(trace_file != nullptr) {
    fclose(trace_file);
    trace_file = nullptr;
  }
}

} // namespace os
