#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "os/LockstepMediator.hpp"
#include "os/FS.hpp"
#include "os/Trace.hpp"
#include "os/FSConsole.hpp"
#include "os/FSTerminal.hpp"
#include "os/OSProcess.hpp"
#include "os/Launchable.hpp"
#include "os/FSStreamIO.hpp"
#include "hw/Decoder.hpp"
#include "hw/Machine.hpp"
#include "hw/AddressBook.hpp"
#include "hw/BlockSync.hpp"
#include "hw/IRExec.hpp"
#include "hw/MachineThread.hpp"
#include "hw/Lockstep.hpp"
#include "os/ProbeSuppressions.hpp"
#include "hw/RTStubs.hpp"
#include "runtime/error_handler.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <thread>
#include <chrono>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>

#include "os/OS.hpp"


using namespace os;
using namespace hw;
using namespace debug;

static int acceptor_fd = -1;
static volatile sig_atomic_t sigint_count = 0;

static void sigint_handler(int) {
  if(++sigint_count >= 2) {
    const char msg[] = "\nForced exit.\n";
    (void)!write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(1);
  }
  const char msg[] = "\nInterrupt - shutting down...\n";
  (void)!write(STDERR_FILENO, msg, sizeof(msg) - 1);
}

// -silent: the lockstep clone's terminal. Never genuinely read (clone
// ?READs are mediated; input is replayed from the master) and written
// only by the spectator echo, which this discards. Verification of clone
// output happens at mediation (checker 3), before any terminal.
class FSNullStream : public FSStreamIO {
public:
  int32_t available() override { return 0; }
  int32_t read(std::vector<uint8_t>&) override { return 0; }
  void write(const std::vector<uint8_t>&) override {}
  void close() override {}
};

// QUEST_PORT (Project 14 Phase B ruling): behavior-neutral env knob for
// parallel battery runs; default 8781. Fail-loud on an unparseable value
// (the QUEST_INJECT lesson: a malformed knob must never degrade silently).
static int terminal_port() {
  static int port = -1;
  if(port > 0) return port;
  port = 8781;
  if(const char* p = getenv("QUEST_PORT")) {
    char* end = nullptr;
    long v = strtol(p, &end, 10);
    if(end == p || *end != '\0' || v < 1 || v > 65535) {
      fprintf(stderr, "QUEST_PORT: unparseable value '%s' (want a decimal port 1-65535)\n", p);
      exit(2);
    }
    port = static_cast<int>(v);
  }
  return port;
}

static FSTerminal* wait_for_client() {
  if(acceptor_fd < 0) {
    acceptor_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if(acceptor_fd < 0)
      throw std::runtime_error("Unable to create server socket");
    int opt = 1;
    ::setsockopt(acceptor_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    int port = terminal_port();
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if(::bind(acceptor_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
      throw std::runtime_error("Unable to bind server socket on port " + std::to_string(port));
    if(::listen(acceptor_fd, 5) < 0)
      throw std::runtime_error("Unable to listen on server socket");
  }
  int client_fd = ::accept(acceptor_fd, nullptr, nullptr);
  if(client_fd < 0)
    throw std::runtime_error("Wait for terminal connection failed");
  return new FSTerminal(client_fd);
}

static std::string to_upper(const std::string& s) {
  std::string result = s;
  std::transform(result.begin(), result.end(), result.begin(), ::toupper);
  return result;
}

int main(int argc, char* argv[]) {
  // Unbuffered diagnostic - if this doesn't print, crash is before main()
  // (startup diagnostic removed)

  terminal_port();  // eager QUEST_PORT validation: an unparseable value refuses to launch

  try {
  // Optional: -trace FILE -types TYPE,TYPE,...   (e.g. -types scalls,shared)
  //           -lockstep  (master/clone paired execution; see
  //                       docs/LockstepHarness.md — requires exactly one
  //                       program launched exactly twice)
  //           -silent    (with -lockstep: the clone gets a discarding
  //                       null terminal instead of the second telnet
  //                       connection; single-terminal play. Verification
  //                       is unaffected — clone output is compared at
  //                       mediation before any terminal is involved.)
  std::string trace_file, trace_types;
  bool silent_clone = false;
  while(argc >= 2 && argv[1][0] == '-') {
    std::string flag = argv[1];
    if(flag == "-lockstep") {
      Lockstep::enabled = true;
      argv += 1;
      argc -= 1;
      continue;
    }
    if(flag == "-silent") {
      silent_clone = true;
      argv += 1;
      argc -= 1;
      continue;
    }
    if(flag.rfind("-handler=", 0) == 0) {
      // Project 8: L2 handler-state implementation selection
      // (docs/Project8/PROMPT.md). Stage A builds mv only.
      std::string value = flag.substr(9);
      if(value == "mv")
        rt::handler_mode = rt::HandlerMode::MV;
      else if(value == "native")
        rt::handler_mode = rt::HandlerMode::NATIVE;
      else if(value == "check")
        rt::handler_mode = rt::HandlerMode::CHECK;
      else {
        fprintf(stderr, "Unknown -handler= value '%s' (mv|native|check)\n", value.c_str());
        return 1;
      }
      argv += 1;
      argc -= 1;
      continue;
    }
    if(flag.rfind("-zero=", 0) == 0) {
      // Project 10: stack-claim zeroing configuration (docs/Layering.md
      // ruling 8). ONE switch — the asymmetric configuration IS the
      // experiment (user ruling: no separate -probe flag).
      //   both  (default) — every machine zeroes claims; full checker.
      //   none  — the pre-ruling bit-faithful attic; full checker.
      //   clone — the GARBAGE PROBE: master unzeroed, clone zeroed;
      //           register-value compare relaxed, everything else armed.
      std::string value = flag.substr(6);
      if(value == "both")
        hw::Machine::zero_mode = hw::Machine::ZERO_BOTH;
      else if(value == "none")
        hw::Machine::zero_mode = hw::Machine::ZERO_NONE;
      else if(value == "clone")
        hw::Machine::zero_mode = hw::Machine::ZERO_CLONE;
      else {
        fprintf(stderr, "Unknown -zero= value '%s' (both|none|clone)\n", value.c_str());
        return 1;
      }
      argv += 1;
      argc -= 1;
      continue;
    }
    if(argc < 3) {
      fprintf(stderr, "Flag %s requires a value\n", flag.c_str());
      return 1;
    }
    if(flag == "-trace")      trace_file = argv[2];
    else if(flag == "-types") trace_types = argv[2];
    else {
      fprintf(stderr, "Unknown flag %s\n", flag.c_str());
      return 1;
    }
    argv += 2;
    argc -= 2;
  }
  if(!trace_file.empty() != !trace_types.empty()) {
    fprintf(stderr, "-trace and -types must be used together\n");
    return 1;
  }
  if(silent_clone && !Lockstep::enabled) {
    fprintf(stderr, "-silent has no effect without -lockstep; ignoring\n");
    silent_clone = false;
  }
  if(hw::Machine::zero_mode == hw::Machine::ZERO_CLONE) {
    if(!Lockstep::enabled) {
      fprintf(stderr, "-zero=clone is the asymmetric garbage probe and requires -lockstep\n");
      return 1;
    }
    // clone mode IMPLIES the probe checker (one switch, user ruling).
    Lockstep::probe_relax_regs = true;
    fprintf(stderr,
      "=============================================================\n"
      "   GARBAGE PROBE (-zero=clone) — NOT a normal verification\n"
      "   master runs UNZEROED (1988 stack garbage preserved)\n"
      "   clone  runs ZEROED   (ruling 8 semantics)\n"
      "   register-VALUE comparison RELAXED\n"
      "   ARMED: pc, instruction counts, terminal/span structure,\n"
      "          trap sites, exceptions, syscall mediation\n"
      "   A hit = a located 1988 read-of-uninitialized bug\n"
      "=============================================================\n");
  }
  if(!trace_file.empty() && !os::Trace::initialize(trace_file, trace_types))
    return 1;
  // Gen-6 block-sync list (docs/Project22/BlockSyncDesign.md): the sync
  // identity is the (block entry address, per-client block ordinal) pair.
  // QUEST_BLOCKS (ground-truth CFG) + QUEST_SYNC_LIST (the translation
  // artifact) are REQUIRED under -lockstep; the loader validates the list
  // (every entry a quest.blocks start, gate addresses mandatory) and
  // refuses to run on any violation.
  if(hw::Lockstep::enabled && !hw::BlockSync::load_from_env())
    return 1;
  // P23 quest.ir (QUEST_IR=<file>): IR blocks the clone executes in
  // place of emulation. Loader refuses on provenance mismatch or any
  // validation failure; absent env = fully emulated clone.
  hw::IRExec::instance = hw::IRExec::load_from_env();
  // M4a address book (QUEST_ADDRESS_BOOK=<file>): which game routines run
  // their WSAVS frame in a fixed area (clone only). Absent = stock.
  if(!hw::AddressBook::load_from_env())
    return 1;
  // M4b push map (QUEST_PUSH_MAP=<file>): which call sites write their
  // args into the callee's area instead of pushing (clone only, gated
  // through the Mapper like the book). Absent = every site pushes (M4a).
  if(!hw::AddressBook::load_push_map_from_env())
    return 1;

  if(argc < 2) {
    fprintf(stderr, "Usage: %s [-trace FILE -types TYPE,...] <PR file>\n", argv[0]);
    fprintf(stderr, "-or-   %s [-trace FILE -types TYPE,...] <dir> <PR file 1> ... <PR file n>\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "If a PR file name starts with an @, then the launcher waits for a\n");
    fprintf(stderr, "terminal to connect to port %d.\n", terminal_port());
    return 1;
  }

  // Initialize instruction decoder tables and the shared machine thread
  // (Java does these at class-load time via static initializers)
  (void)!write(STDERR_FILENO, "Decoder::initialize()...\n", 24);
  Decoder::initialize();
  (void)!write(STDERR_FILENO, "MachineThread()...\n", 19);
  Machine::machine_thread = new MachineThread();
  (void)!write(STDERR_FILENO, "Init complete.\n", 15);

  int first_arg;
  if(argc == 2) {
    FS::initialize_with_path(argv[1]);
    first_arg = 1;
  }
  else {
    FS::initialize_with_path(argv[1]);
    first_arg = 2;
  }

  for(int i = first_arg; i < argc; i++) {
    std::string arg(argv[i]);
    FSStreamIO* stream;

    if(arg[0] == '@') {
      arg = arg.substr(1);
      // Under -lockstep -silent the CLONE (second instance of the
      // duplicated program) gets a null terminal instead of waiting for
      // a second telnet connection.
      bool is_clone = false;
      if(Lockstep::enabled && silent_clone) {
        std::string self = to_upper(arg);
        if(self.size() > 3 && self.substr(self.size() - 3) == ".PR")
          self = self.substr(0, self.size() - 3);
        int seen = 0;
        for(int j = first_arg; j <= i; j++) {
          std::string other = to_upper(argv[j][0] == '@' ? argv[j] + 1 : argv[j]);
          if(other.size() > 3 && other.substr(other.size() - 3) == ".PR")
            other = other.substr(0, other.size() - 3);
          if(other == self)
            seen++;
        }
        is_clone = seen == 2;
      }
      if(is_clone) {
        fprintf(stderr, "Silent clone terminal for @%s\n", arg.c_str());
        stream = new FSNullStream();
      }
      else {
        fprintf(stderr, "Waiting for terminal client for @%s\n", arg.c_str());
        stream = wait_for_client();
      }
    }
    else {
      stream = new FSConsole();
    }

    int type = 0;
    std::string upper = to_upper(arg);
    if(upper.size() > 3 && upper.substr(upper.size() - 3) == ".PR") {
      type = 1;
      arg = arg.substr(0, arg.size() - 3);
    }
    else if(FS::retrieve(":" + to_upper(arg) + ".PR") != nullptr) {
      type = 1;
    }

    if(type == 0) {
      printf("Launch target %s not found\n", arg.c_str());
    }
    else {
      OSProcess* os_process = new OSProcess(":", to_upper(arg));
      // Dual-emulation labeling: single instance of a program is "QUEST";
      // multiple instances become "QUEST1", "QUEST2", ...
      // Under -lockstep the duplicated program is the master/clone pair
      // (first = master, second = clone); everything else is server-side.
      {
        int total = 0, ordinal = 0;
        for(int j = first_arg; j < argc; j++) {
          std::string other = to_upper(argv[j][0] == '@' ? argv[j] + 1 : argv[j]);
          if(other.size() > 3 && other.substr(other.size() - 3) == ".PR")
            other = other.substr(0, other.size() - 3);
          if(other == to_upper(arg)) {
            total++;
            if(j <= i)
              ordinal++;
          }
        }
        if(total > 1)
          os_process->instance_label = to_upper(arg) + std::to_string(ordinal);
        if(Lockstep::enabled) {
          if(total == 1)
            os_process->lockstep_role = Lockstep::SERVER;
          else if(total == 2)
            os_process->lockstep_role =
              ordinal == 1 ? Lockstep::MASTER : Lockstep::CLONE;
          else {
            fprintf(stderr, "-lockstep requires the paired program to be "
                            "launched exactly twice\n");
            return 1;
          }
          fprintf(stderr, "Lockstep role for %s: %s\n",
                  os_process->instance_label.empty()
                    ? to_upper(arg).c_str()
                    : os_process->instance_label.c_str(),
                  os_process->lockstep_role == Lockstep::SERVER ? "SERVER"
                  : os_process->lockstep_role == Lockstep::MASTER ? "MASTER"
                  : "CLONE");
        }
      }
      Launchable* process = os_process;
      process->launch(stream);
    }
  }

  signal(SIGINT, sigint_handler);

  // Wait until all processes have terminated
  bool shutdown_sent = false;
  while(OS::global.has_processes()) {
    if(sigint_count > 0 && !shutdown_sent) {
      OS::global.shutdown_all();
      os::LockstepMediator::release_all();  // wake mediator-parked tasks (the ctrl-C wedge)
      shutdown_sent = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  fprintf(stderr, "All processes terminated.\n");
  hw::Memory::dump_ever_mapped(stderr);
  if(hw::Lockstep::suppress_save.load())
    fprintf(stderr, "FS::save_all SUPPRESSED (world abort, state presumed corrupt).\n");
  else
    FS::save_all();
  hw::RTStubs::dump_coverage();
  os::ProbeSuppressions::summarize();  // curator report — THE LAST OUTPUT before exit (user, Aug 14)
  }
  catch(std::exception& e) {
    fprintf(stderr, "Fatal error: %s\n", e.what());
    return 1;
  }
  return 0;
}
