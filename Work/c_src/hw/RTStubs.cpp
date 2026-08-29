// src/hw/RTStubs.cpp
#include "RTStubs.hpp"
#include "Machine.hpp"
#include "../debug/SymbolTable.hpp"
#include "../os/OSProcess.hpp"
#include "../debug/CallStack.hpp"
#include "../os/Trace.hpp"
#include "../runtime/fill_words.hpp"
#include "../runtime/udiv32.hpp"
#include "../runtime/unsigned_to_char.hpp"
#include "../runtime/o_on.hpp"
#include "../runtime/i_lock.hpp"
#include "../runtime/i_alloc.hpp"
#include "../runtime/t_area.hpp"
#include "../runtime/frames.hpp"
#include "../runtime/o_signal.hpp"
#include "../runtime/lib_error.hpp"
#include "../runtime/o_area.hpp"
#include "../runtime/p_defon.hpp"
#include "../runtime/r_signal.hpp"
#include "../runtime/def_on.hpp"
#include <cstdio>
#include <map>
#include <mutex>
#include <vector>
#include <cstring>
#include <algorithm>
#include <utility>

namespace hw {
using namespace debug;

bool RTStubs::active=false;
uint32_t RTStubs::start=0;
uint32_t RTStubs::stop=0;
uint8_t* RTStubs::entry_bits=nullptr;
uint8_t* RTStubs::translated_bits=nullptr;
uint8_t* RTStubs::terminal_bits=nullptr;
uint8_t* RTStubs::l2_bits=nullptr;

// The L2 stratum (docs/Layering.md census, ratified Aug 2026): every
// entry of the condition-system machinery. The crossings-only checker
// pairs at L1→L2 entries and L2→L1 exits and treats everything between
// as invisible interior — so this list is what "entering the handler
// machinery" MEANS to the harness. Terminal entries (?FATAL, I.STOP,
// DERR.TRP...) are the L3 door and stay in terminal_table; entries not
// listed in either are L0/L1 fabric.
//
// Two sub-populations, one rule each:
//  - translated L2 (all 20 registered entries): the clone runs C++, the
//    master absorbs the emulated subtree; the checker adds an ENTRY
//    rendezvous via deferred dispatch (defer_dispatch) on top of the
//    existing exit rendezvous.
//  - untranslated L2 (frozen/dead: scan 5 lists them; none has ever run
//    live): both engines emulate; the checker pairs once at the entry,
//    then arms rt_pending_return on BOTH roles so the whole emulated
//    subtree is one absorbed span ending at the L2→L1 exit — interior
//    entries stop being pairing events even in the all-emulated world.
static const char* l2_table[]={
  // registered / translated
  "I.PROLOG", "I.EPILOG", "I.GOTO", "O.ON", "O.REVERT", "T?AREA",
  "O?AREA", "O.SET", "O?SIGNAL", "O.SCONVE", "O.SSUBSC", "O.SFIXED",
  "O.SZEROD", "O.SOVERF", "O.SUNDER", "O.SERROR",
  "?DEFAULT_ERROR_HANDLER", "DEF?ON", "P?DEFON", "R?SIGNAL",
  // untranslated (frozen or dead — cold by every observation to date)
  "I.WPROLO", "I.DISPLA", "I.SFALT", "I.SFCON", "R.GOTO", "I.FFALT",
  "O.SEARCH", "O.SIGNAL", "R.SIGREC", "R.SIGNAL",
};
static constexpr int L2_COUNT=sizeof(l2_table)/sizeof(l2_table[0]);

// L1→L2 RETURN crossings (L2Contract.md §5): the two pcs where L1 code
// re-enters the signal machinery by RETURNING into it — a dispatched
// handler's WRTN to the O?SIGNAL tail, and (inside ?LIB_ERROR's body)
// the handler-return to the O?SIGNAL call's continuation. Arrival at
// either with no pending span is a crossing rendezvous. Both are cold
// on live paths today (live handlers unwind via I.GOTO; E3EF at depth 0
// is unreachable while ?LIB_ERROR is native) — dormant-but-correct, the
// same insurance class as the untranslated-L2 rule above.
static const uint32_t return_crossings[]={ 0x7017EE40u, 0x7017E3EFu };
uint32_t RTStubs::terminal_test_pc=0;
uint8_t  RTStubs::terminal_test_kind=1;
uint32_t RTStubs::turn_loop_pc=0;   // detached-master tripwire; see RTStubs.hpp

// Game-range terminal sites: pcs OUTSIDE the RT range where execution
// is about to leave the world by a never-returning syscall. Both
// engines pair one final time AT the site (before the syscall
// executes), the clone detaches, and the master performs the exit
// alone — closing the historical "Forced exit" hang (the pair used to
// ride into process teardown still coupled).
struct GameTerminal { uint32_t pc; uint8_t kind; };
static const GameTerminal game_terminals[]={
  // DELIBERATELY EMPTY: terminal syscalls are intercepted at DISPATCH,
  // keyed on the call NUMBER (OSTask::dispatch_system_call → 
  // Lockstep::retire_ordinal for ?RETURN 0310) — site-independent, so
  // every 0310 in game code is covered including any hiding in
  // disassembly holes. This table remains for future pc-anchored
  // terminal sites if one is ever needed.
};
static constexpr int GAME_TERMINAL_COUNT=sizeof(game_terminals)/sizeof(game_terminals[0]);

uint8_t RTStubs::terminal_kind(uint32_t pc) {
  if(terminal_test_pc!=0 && pc==terminal_test_pc)
    return terminal_test_kind;
  for(int i=0;i<GAME_TERMINAL_COUNT;i++)
    if(game_terminals[i].pc==pc)
      return game_terminals[i].kind;
  if(pc>=start && pc<stop && terminal_bits)
    return terminal_bits[pc-start];
  return 0;
}

bool RTStubs::is_terminal_pc(uint32_t pc) {
  return terminal_kind(pc)!=0;
}

bool RTStubs::is_l2_entry(uint32_t pc) {
  return l2_bits && pc>=start && pc<stop && l2_bits[pc-start];
}

bool RTStubs::defer_dispatch(uint32_t pc) {
  return l2_bits && translated_bits && pc>=start && pc<stop &&
         l2_bits[pc-start] && translated_bits[pc-start];
}

bool RTStubs::is_return_crossing(uint32_t pc) {
  if(!active)
    return false;
  for(uint32_t rc : return_crossings)
    if(pc==rc)
      return true;
  return false;
}
uint32_t RTStubs::inject_site=0;
int32_t  RTStubs::inject_type=0;
int32_t  RTStubs::inject_code=0;
bool     RTStubs::inject_resume=false;
bool     RTStubs::bad_token_armed=false;

// Terminal entries: reached only when the game is dying (normal exit or
// unhandled-condition death). Under -lockstep, arrival is the LAST verified
// pair — both engines are compared at the terminal entry, then the clone
// detaches and the master runs the terminal path to completion unverified
// (fidelity is owed to what the game can observe, and a terminal path
// observes nothing afterward). See Lockstep::detach.
// kind: 1 = DETACH (authentic death — clone stops, master finishes it);
// 2 = ABORT (proven corruption — one final verified pair, then the world
// hard-stops via Lockstep::abort_world, NO data write-back). DERR.TRP is
// ABORT by ruling (Layering.md ruling 7): a DERR means a subscript
// overran an array — "recovery" would proceed into a corrupted world.
struct TerminalEntry { const char* name; uint8_t kind; };
static const TerminalEntry terminal_table[]={
  { "I.STOP", 1 }, { "I.STOPM", 1 }, { "?FATAL", 1 },
  { "DERR.TRP", 2 },
  // "DEF?ON" removed by the Project 5 lift (Aug 2026): DEF?ON is now
  // ordinary verified L2 (translated, registered); the detach frontier
  // is ?FATAL. See docs/Project5/REPORT.md.
};
static constexpr int TERMINAL_COUNT=sizeof(terminal_table)/sizeof(terminal_table[0]);

// One X(id, "SYMBOL") row per runtime entry, generated from QUEST.ST over
// [?CHAR_TO_UNSIGNED, ?NTOP), deduplicated by address (aliases collapse to
// the primary name), plus SQR31?3. A few rows are data symbols (e.g.
// C.ERRNO, ?URTB); registering them is harmless — they only log if
// something actually calls them, which would itself be worth knowing.
#define RT_STUB_LIST \
  X(sqr31_3, "SQR31?3") \
  X(q_char_to_unsigned, "?CHAR_TO_UNSIGNED") \
  X(q_unsigned_to_char, "?UNSIGNED_TO_CHAR") \
  X(q_umul32, "?UMUL32") \
  X(q_udiv32, "?UDIV32") \
  X(q_await_console_interrupt, "?AWAIT_CONSOLE_INTERRUPT") \
  X(q_close_file, "?CLOSE_FILE") \
  X(q_connect, "?CONNECT") \
  X(q_create_task, "?CREATE_TASK") \
  X(mtq_lock, "MT?LOCK") \
  X(mtq_unlock, "MT?UNLOCK") \
  X(q_delay, "?DELAY") \
  X(q_get_shared_page, "?GET_SHARED_PAGE") \
  X(q_lookup_port, "?LOOKUP_PORT") \
  X(q_open_file, "?OPEN_FILE") \
  X(q_open_shared_io_file, "?OPEN_SHARED_IO_FILE") \
  X(q_lib_error_code, "?LIB_ERROR_CODE") \
  X(q_random_number, "?RANDOM_NUMBER") \
  X(q_read, "?READ") \
  X(q_read_screen, "?READ_SCREEN") \
  X(q_receive_task_message, "?RECEIVE_TASK_MESSAGE") \
  X(q_send_task_message, "?SEND_TASK_MESSAGE") \
  X(q_current_pid, "?CURRENT_PID") \
  X(mtq_sus, "MT?SUS") \
  X(mtq_idsus, "MT?IDSUS") \
  X(mtq_idkil, "MT?IDKIL") \
  X(mtq_pri, "MT?PRI") \
  X(mtq_idpri, "MT?IDPRI") \
  X(mtq_idrdy, "MT?IDRDY") \
  X(mtq_ersch, "MT?ERSCH") \
  X(mtq_drsch, "MT?DRSCH") \
  X(mtq_xmt, "MT?XMT") \
  X(mtq_xmtw, "MT?XMTW") \
  X(mtq_rec, "MT?REC") \
  X(mtq_recnw, "MT?RECNW") \
  X(mtq_idgo2, "MT?IDGO2") \
  X(mtq_task, "MT?TASK") \
  X(q_write, "?WRITE") \
  X(q_write_screen, "?WRITE_SCREEN") \
  X(q_fill_words, "?FILL_WORDS") \
  X(q_lib_error, "?LIB_ERROR") \
  X(q_default_error_handler, "?DEFAULT_ERROR_HANDLER") \
  X(swat_nin, "SWAT.NIN") \
  X(swat_rex, "SWAT.REX") \
  X(b_move, "B.MOVE") \
  X(c_index, "C.INDEX") \
  X(c_trans, "C.TRANS") \
  X(c_collat, "C.COLLAT") \
  X(x_cb, "X.CB") \
  X(d_mod, "D.MOD") \
  X(i_prolog, "I.PROLOG") \
  X(i_wprolo, "I.WPROLO") \
  X(i_displa, "I.DISPLA") \
  X(i_epilog, "I.EPILOG") \
  X(tq_initn, "T?INITN") \
  X(t_init, "T.INIT") \
  X(tq_kill, "T?KILL") \
  X(i_lock, "I.LOCK") \
  X(i_unlock, "I.UNLOCK") \
  X(iq_hpowner, "I?HPOWNER") \
  X(iq_asize, "I?ASIZE") \
  X(iq_inhpb, "I?INHPB") \
  X(iq_inhpw, "I?INHPW") \
  X(iq_inhpbs, "I?INHPBS") \
  X(iq_inhpws, "I?INHPWS") \
  X(iq_salloc, "I?SALLOC") \
  X(i_alloc, "I.ALLOC") \
  X(i_freeb, "I.FREEB") \
  X(i_freew, "I.FREEW") \
  X(i_free, "I.FREE") \
  X(i_tofree, "I.TOFREE") \
  X(i_init, "I.INIT") \
  X(q_ukil, "?UKIL") \
  X(q_stack_overhead, "?STACK_OVERHEAD") \
  X(i_ginit, "I.GINIT") \
  X(i_notice, "I.NOTICE") \
  X(i_fpu, "I.FPU") \
  X(i_sfalt, "I.SFALT") \
  X(i_sfcon, "I.SFCON") \
  X(r_goto, "R.GOTO") \
  X(i_goto, "I.GOTO") \
  X(i_ffalt, "I.FFALT") \
  X(derr_trp, "DERR.TRP") \
  X(tq_area, "T?AREA") \
  X(o_on, "O.ON") \
  X(o_revert, "O.REVERT") \
  X(o_search, "O.SEARCH") \
  X(o_signal, "O.SIGNAL") \
  X(oq_signal, "O?SIGNAL") \
  X(r_sigrec, "R.SIGREC") \
  X(o_sunder, "O.SUNDER") \
  X(o_soverf, "O.SOVERF") \
  X(o_szerod, "O.SZEROD") \
  X(o_sfixed, "O.SFIXED") \
  X(o_ssubsc, "O.SSUBSC") \
  X(o_sconve, "O.SCONVE") \
  X(o_serror, "O.SERROR") \
  X(o_set, "O.SET") \
  X(defq_on, "DEF?ON") \
  X(r_signal, "R.SIGNAL") \
  X(rq_signal, "R?SIGNAL") \
  X(q_snap, "?SNAP") \
  X(q_fatal, "?FATAL") \
  X(iq_pcs, "I?PCS") \
  X(iq_lineid, "I?LINEID") \
  X(q_find_scope, "?FIND_SCOPE") \
  X(q_find_lineid_index, "?FIND_LINEID_INDEX") \
  X(q_get_lineid_entry, "?GET_LINEID_ENTRY") \
  X(pq_snap, "P?SNAP") \
  X(cq_trim, "C?TRIM") \
  X(iq_line, "I?LINE") \
  X(q_write_error_channel, "?WRITE_ERROR_CHANNEL") \
  X(oq_area, "O?AREA") \
  X(pq_ipkt, "P?IPKT") \
  X(x_aic, "X.AIC") \
  X(x_ic, "X.IC") \
  X(f_stop, "F.STOP") \
  X(f_stopn, "F.STOPN") \
  X(i_stopm, "I.STOPM") \
  X(i_stop, "I.STOP") \
  X(langq_stop, "LANG?STOP") \
  X(langq_init, "LANG?INIT") \
  X(langq_flsh, "LANG?FLSH") \
  X(q_scope_init, "?SCOPE_INIT") \
  X(pq_defon, "P?DEFON") \
  X(i_start, "I.START") \
  X(cq_init, "C?INIT") \
  X(r_inerr, "R.INERR") \
  X(c_errno, "C.ERRNO") \
  X(derr_usr, "DERR.USR") \
  X(q_urtb, "?URTB") \
  X(_bomb, ".BOMB") \
  X(_kill, ".KILL") \
  X(_utsk, ".UTSK") \
  X(_ukil, ".UKIL") \
  X(q_bomb, "?BOMB") \
  X(q_utsk, "?UTSK")

struct RTStubDef {
  const char* name;
  uint32_t*   addr;
  NativeFunc  fn;
};

// Stub bodies and per-stub entry addresses (resolved at initialize).
#define X(id, sym) \
  static uint32_t id##_addr=0xFFFFFFFF; \
  static uint32_t id##_stub(Machine& machine) { \
    return RTStubs::log_and_continue(machine, sym, id##_addr); \
  }
RT_STUB_LIST
#undef X

static RTStubDef stub_table[]={
#define X(id, sym) { sym, &id##_addr, id##_stub },
RT_STUB_LIST
#undef X
};
static constexpr int STUB_COUNT=sizeof(stub_table)/sizeof(stub_table[0]);

static std::vector<uint8_t> entry_bit_store;
static std::vector<uint8_t> translated_bit_store;
static std::vector<uint8_t> terminal_bit_store;
static std::vector<uint8_t> l2_bit_store;

// Real translations: registered over their stubs in the clone; entries here
// are marked in translated_bits (both roles) so the master runs their
// emulated bodies to the return point instead of pairing at the entry.
// LEAF ROUTINES ONLY until nested-call pairing is designed.
struct RTTranslation { const char* name; NativeFunc fn; };
static RTTranslation translation_table[]={
  { "?FILL_WORDS", emu_rt::fill_words },
  { "?UDIV32", emu_rt::udiv32 },
  { "?UNSIGNED_TO_CHAR", emu_rt::unsigned_to_char },
  { "O.ON", emu_rt::o_on },
  { "O.REVERT", emu_rt::o_revert },
  { "I.LOCK", emu_rt::i_lock },
  { "I.UNLOCK", emu_rt::i_unlock },
  { "I.ALLOC", emu_rt::i_alloc },
  { "I.FREEB", emu_rt::i_freeb },
  { "I.FREEW", emu_rt::i_freew },
  { "I.FREE", emu_rt::i_free },
  { "T?AREA", emu_rt::t_area },
  { "I.PROLOG", emu_rt::i_prolog },
  { "I.EPILOG", emu_rt::i_epilog },
  { "I.GOTO", emu_rt::i_goto },
  { "O?SIGNAL", emu_rt::o_qsignal },
  { "O.SET", emu_rt::o_set },
  { "O.SERROR", emu_rt::o_serror },
  { "O.SCONVE", emu_rt::o_sconve },
  { "O.SSUBSC", emu_rt::o_ssubsc },
  { "O.SFIXED", emu_rt::o_sfixed },
  { "O.SZEROD", emu_rt::o_szerod },
  { "O.SOVERF", emu_rt::o_soverf },
  { "O.SUNDER", emu_rt::o_sunder },
  { "?LIB_ERROR", emu_rt::lib_error },
  { "?LIB_ERROR_CODE", emu_rt::lib_error_code },
  { "?DEFAULT_ERROR_HANDLER", emu_rt::default_error_handler },
  { "O?AREA", emu_rt::oq_area },
  { "P?DEFON", emu_rt::pq_defon },
  { "R?SIGNAL", emu_rt::rq_signal },
  { "DEF?ON", emu_rt::defq_on },   // Project 5 lift: registered together with
                                   // its terminal_table removal above.

};
static constexpr int TRANSLATION_COUNT=sizeof(translation_table)/sizeof(translation_table[0]);
static std::vector<std::pair<uint32_t, const char*>> sorted_entries;  // for dump attribution
static std::map<std::string, std::vector<uint8_t>> coverage;
static std::mutex coverage_mutex;

void RTStubs::initialize(SymbolTable& symbols, const std::string& program) {
  if(active)
    return;
  std::string upper=program;
  for(char& ch : upper) ch=toupper(ch);
  if(upper!="QUEST")
    return;

  uint32_t range_start=symbols.address_for_name("?CHAR_TO_UNSIGNED");
  uint32_t range_stop=symbols.address_for_name("?NTOP");
  if(range_start==0xFFFFFFFF || range_stop==0xFFFFFFFF || range_stop<=range_start) {
    fprintf(stderr, "RTStubs: cannot resolve RT range for %s\n", program.c_str());
    return;
  }

  for(int i=0; i<STUB_COUNT; i++)
    *stub_table[i].addr=symbols.address_for_name(stub_table[i].name);

  entry_bit_store.assign(range_stop-range_start, 0);
  for(int i=0; i<STUB_COUNT; i++) {
    uint32_t addr=*stub_table[i].addr;
    if(addr!=0xFFFFFFFF && addr>=range_start && addr<range_stop)
      entry_bit_store[addr-range_start]=1;
  }

  for(int i=0; i<STUB_COUNT; i++) {
    uint32_t addr=*stub_table[i].addr;
    if(addr!=0xFFFFFFFF)
      sorted_entries.push_back(std::make_pair(addr, stub_table[i].name));
  }
  std::sort(sorted_entries.begin(), sorted_entries.end());

  translated_bit_store.assign(range_stop-range_start, 0);
  for(int i=0; i<TRANSLATION_COUNT; i++) {
    uint32_t addr=symbols.address_for_name(translation_table[i].name);
    if(addr!=0xFFFFFFFF && addr>=range_start && addr<range_stop)
      translated_bit_store[addr-range_start]=1;
    else
      fprintf(stderr, "RTStubs: translation %s not in RT range\n", translation_table[i].name);
  }

  l2_bit_store.assign(range_stop-range_start, 0);
  for(int i=0; i<L2_COUNT; i++) {
    uint32_t addr=symbols.address_for_name(l2_table[i]);
    if(addr!=0xFFFFFFFF && addr>=range_start && addr<range_stop)
      l2_bit_store[addr-range_start]=1;
    else
      fprintf(stderr, "RTStubs: L2 entry %s not in RT range\n", l2_table[i]);
  }
  l2_bits=l2_bit_store.data();

  terminal_bit_store.assign(range_stop-range_start, 0);
  for(int i=0; i<TERMINAL_COUNT; i++) {
    uint32_t addr=symbols.address_for_name(terminal_table[i].name);
    if(addr!=0xFFFFFFFF && addr>=range_start && addr<range_stop)
      terminal_bit_store[addr-range_start]=terminal_table[i].kind;
    else
      fprintf(stderr, "RTStubs: terminal %s not in RT range\n", terminal_table[i].name);
  }
  if(const char* env=getenv("QUEST_INJECT")) {
    // <site>:<type>:<code>[:RESUME]; site = symbol name or hex pc.
    std::string spec(env);
    std::vector<std::string> f;
    size_t p=0;
    while(p<=spec.size()) {
      size_t q=spec.find(':', p);
      if(q==std::string::npos) { f.push_back(spec.substr(p)); break; }
      f.push_back(spec.substr(p, q-p));
      p=q+1;
    }
    if(f.size()>=3) {
      inject_site=symbols.address_for_name(f[0]);
      if(inject_site==0xFFFFFFFF)
        inject_site=static_cast<uint32_t>(strtoul(f[0].c_str(), nullptr, 16));
      inject_type=static_cast<int32_t>(strtol(f[1].c_str(), nullptr, 0));
      inject_code=static_cast<int32_t>(strtol(f[2].c_str(), nullptr, 0));
      inject_resume=(f.size()>3 && f[3]=="RESUME");
      fprintf(stderr, "RTStubs: INJECT armed at %08X type=%d code=%X%s (QUEST_INJECT)\n",
              inject_site, inject_type, inject_code, inject_resume?" RESUME":"");
    } else {
      // Project 14 near-miss: a malformed spec used to no-op with a
      // stderr note, letting an injection battery run go green while
      // testing nothing (caught only because the expected endpoint —
      // ?FATAL at 7017F036 — did not appear). An armed-but-unparseable
      // knob is a broken test, not a runnable one: fail at launch.
      fprintf(stderr, "RTStubs: QUEST_INJECT malformed (want site:type:code[:RESUME]) — refusing to launch\n");
      exit(2);
    }
  }
  if(getenv("QUEST_BAD_TOKEN")) {
    bad_token_armed=true;
    fprintf(stderr, "RTStubs: BAD-TOKEN one-shot armed for the first non-local I.GOTO (QUEST_BAD_TOKEN)\n");
  }
  if(const char* env=getenv("QUEST_TERMINAL")) {
    terminal_test_pc=static_cast<uint32_t>(strtoul(env, nullptr, 16));
    terminal_test_kind = strstr(env, ":ABORT") ? 2 : 1;
    fprintf(stderr, "RTStubs: TEST terminal point at %08X kind=%s (QUEST_TERMINAL)\n",
            terminal_test_pc, terminal_test_kind==2?"ABORT":"DETACH");
  }

  // Detached-master tripwire anchor (see RTStubs.hpp). Symbol-derived,
  // never hardcoded; unresolvable = disarmed + warned (fail loud enough
  // to notice, but a missing game symbol shouldn't kill non-game runs).
  turn_loop_pc=symbols.address_for_name("START_TURN");
  if(turn_loop_pc==0xFFFFFFFF) {
    turn_loop_pc=0;
    fprintf(stderr, "RTStubs: START_TURN unresolved - "
                    "detached-master tripwire DISARMED\n");
  }

  start=range_start;
  stop=range_stop;
  entry_bits=entry_bit_store.data();
  translated_bits=translated_bit_store.data();
  terminal_bits=terminal_bit_store.data();
  active=true;
  fprintf(stderr, "RTStubs: RT range %08X..%08X, %d entries\n", start, stop, STUB_COUNT);
}

// Synthesize the O?SIGNAL raise at the injection site — the SAME code
// runs on both roles; behavior differs only through the (clone-only)
// native registry, exactly as at a real LCALL site. Derivation of the
// staging: docs/Project5/DERIVATION.md (template = DEF?ON's resignal
// staging ef37-ef3d + EagleStack LCALL).
//   1. Push the argument VALUE cells (the injected "locals"): type,
//      key2(=0), code, and — for RESUME — a fourth cell holding -1.
//      RESUME is a FOUR-ARG raise with a negative flag: DEF?ON's
//      resume test reads bit15 of narrow [area+0x16], the HIGH word
//      of the wide flag store at [wsb-0x2A] that O?SIGNAL itself
//      overwrites on EVERY call (argc<=3 stores 0), so pre-poking the
//      bit can never survive the injected call — only the flag
//      argument can arm it. (Corrects the PROMPT's ":RESUME = set
//      bit15 at injection time" mechanism; see REPORT sec. 4.)
//   2. Push the arg pointers in reverse (argN first), each the
//      address of its cell.
//   3. LCALL replica: push (psr<<16)|argc, ac3 = the injection pc
//      (a handled signal or a native resume returns the game exactly
//      where it was), ovr = 0, shadow call_stack->call.
//   4. Dispatch tail: native registry lookup — non-null only on the
//      clone — with the central nested-span guard; else pc = entry.
uint32_t RTStubs::inject_fire(Machine& machine) {
  constexpr uint32_t O_SIGNAL_ENTRY = 0x7017EDEDu;
  int32_t argc = inject_resume ? 4 : 3;

  fprintf(stderr, "RTStubs: INJECT firing at %08X: O?SIGNAL(type=%d, key2=0, code=%X%s)\n",
          machine.pc, inject_type, inject_code, inject_resume ? ", flag=-1" : "");

  machine.wide_push(inject_type);                       // value cells
  int32_t cell_type = machine.wsp;
  machine.wide_push(0);
  int32_t cell_key2 = machine.wsp;
  machine.wide_push(inject_code);
  int32_t cell_code = machine.wsp;
  int32_t cell_flag = 0;
  if(inject_resume) {
    machine.wide_push(-1);
    cell_flag = machine.wsp;
  }
  if(inject_resume)                                     // arg pointers, argN first
    machine.wide_push(cell_flag);
  machine.wide_push(cell_code);
  machine.wide_push(cell_key2);
  machine.wide_push(cell_type);
  machine.wide_push((machine.get_psr()<<16) | argc);    // LCALL frame wide
  machine.ac[3]=static_cast<int32_t>(machine.pc);       // return = the injection pc
  machine.ovr=0;
  machine.call_stack->call(O_SIGNAL_ENTRY, machine.ac[3], machine.pc, argc);

  NativeFunc native=machine.process->native_registry.lookup(O_SIGNAL_ENTRY);
  if(native && machine.rt_pending_return==0 && defer_dispatch(O_SIGNAL_ENTRY)) {
    // Crossings-only checker: the injected raise pairs at the O?SIGNAL
    // entry exactly like a real call site — dispatch deferred, batch
    // breaks at the entry, native runs on resume.
    machine.pending_native=native;
    return O_SIGNAL_ENTRY;
  }
  if(native && machine.rt_pending_return==0)
    return native(machine);
  return O_SIGNAL_ENTRY;
}

void RTStubs::register_stubs(NativeRegistry& registry, SymbolTable& symbols) {
  int registered=0;
  for(int i=0; i<STUB_COUNT; i++) {
    uint32_t addr=*stub_table[i].addr;
    if(addr==0xFFFFFFFF)
      addr=symbols.address_for_name(stub_table[i].name);
    if(addr!=0xFFFFFFFF) {
      registry.register_by_address(addr, stub_table[i].fn);
      registered++;
    }
  }
  for(int i=0; i<TRANSLATION_COUNT; i++)
    registry.register_by_name(symbols, translation_table[i].name, translation_table[i].fn);
  fprintf(stderr, "RTStubs: %d stubs registered, %d translations\n", registered, TRANSLATION_COUNT);
}

uint32_t RTStubs::entry_address(const char* name) {
  for(int i=0; i<STUB_COUNT; i++)
    if(strcmp(stub_table[i].name, name)==0)
      return *stub_table[i].addr;
  return 0xFFFFFFFF;
}

void RTStubs::log_call(Machine& machine, const char* name, const char* tag) {
  if(os::Trace::enabled("rtcalls")) {
    char buf[96];
    snprintf(buf, sizeof(buf), "entry=%s%s ret=%08X", name, tag, machine.ac[3]);
    std::string label=machine.process ? machine.process->instance_label : "";
    if(label.empty() && machine.process)
      label=machine.process->program;
    os::Trace::line("rtcalls", label, buf);
  }
}

uint8_t* RTStubs::coverage_for(os::OSProcess* process) {
  if(!active || process==nullptr)
    return nullptr;
  std::string upper=process->program;
  for(char& ch : upper) ch=toupper(ch);
  if(upper!="QUEST")
    return nullptr;
  std::string label=process->instance_label.empty() ? process->program : process->instance_label;
  std::lock_guard<std::mutex> lock(coverage_mutex);
  std::vector<uint8_t>& bits=coverage[label];
  if(bits.empty())
    bits.assign(stop-start, 0);
  return bits.data();
}

// Attribute an address to the nearest preceding runtime entry.
static const char* attribute(uint32_t addr) {
  const char* best="?";
  for(size_t i=0; i<sorted_entries.size(); i++) {
    if(sorted_entries[i].first>addr)
      break;
    best=sorted_entries[i].second;
  }
  return best;
}

void RTStubs::dump_coverage() {
  if(!active)
    return;
  std::lock_guard<std::mutex> lock(coverage_mutex);
  for(std::map<std::string, std::vector<uint8_t>>::iterator it=coverage.begin(); it!=coverage.end(); ++it) {
    std::string filename="rtcov-"+it->first+".txt";
    FILE* f=fopen(filename.c_str(), "w");
    if(f==nullptr) {
      fprintf(stderr, "RTStubs: cannot write %s\n", filename.c_str());
      continue;
    }
    std::vector<uint8_t>& bits=it->second;
    uint32_t covered=0;
    for(uint32_t off=0; off<bits.size(); off++) {
      if(!bits[off])
        continue;
      covered++;
      uint32_t addr=start+off;
      fprintf(f, "%08X %s\n", addr, attribute(addr));
    }
    fclose(f);
    fprintf(stderr, "RTStubs: %s — %u of %u RT words covered\n",
            filename.c_str(), covered, static_cast<uint32_t>(bits.size()));
  }
}

uint32_t RTStubs::log_and_continue(Machine& machine, const char* name, uint32_t entry) {
  log_call(machine, name, "");
  return entry;
}

} // namespace hw
