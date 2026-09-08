#include "ProbeSuppressions.hpp"
#include "OSTask.hpp"
#include "OSProcess.hpp"
#include "Trace.hpp"
#include "../hw/Machine.hpp"
#include "../hw/Memory.hpp"
#include "../debug/CallStack.hpp"
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <set>

namespace os {

// ---------------------------------------------------------------------------
// THE TABLE. One entry per catalogued specimen; dated comment names it.
// ---------------------------------------------------------------------------
static const ProbeSuppression table[] = {
  // Specimen #1 (2026-08-14, Project 10 REPORT §7 F2): ?WRITE_SCREEN
  // loads never-written frame locals [4..5] into ac0 for SYSCALL 0303
  // (RTBridge.hpp dead-stack residue class). master ac0=0x0000FFFF
  // (255-byte buffer, channel 0377 per METHOD §13 ?ERMSG convention),
  // clone ac0=0 under zeroing. Fires on every login.
  { 0303, 0x7017E2F4, 0x1 /*ac0*/, "2026-08-14 #1 ?WRITE_SCREEN dead locals", true /*SILENT 2026-08-14: 445 hits/72 values/expedition 1, zero consequence*/ },
  // Specimen #2 (2026-08-14, this expedition, run clone_login1): game
  // code at 70175F2E (QUEST+0x27E chain) issues ?ISR (0142) with a
  // never-initialized pointer in ac0 (master=7000021C residue, clone=0);
  // packet words +12..+15 carry the same residue pointers, UNCONSUMED
  // by the handler this run (IREC length=0) — content tier stays armed
  // for the day they are read. Fires once, early login (instr ~2042).
  { 0142, 0x70175F2E, 0x1 /*ac0*/, "2026-08-14 #2 game ?ISR dead ptr ac0", false /*NOTE: one login hit/session, still observing*/ },
};
static const int table_size = sizeof(table) / sizeof(table[0]);

// Provisional numbering for new specimens found this run; the catalogue
// assigns final ids. Starts after the seeded table.
static int next_provisional = table_size + 1;

// Specimens log: host cwd, survives scratch deletion between runs and is
// swept into every checkpoint. ("Next to the data files" read as: next to
// the durable run artifacts — the scratch QUEST copy is deleted per run.)
static FILE* specimens_file() {
  static FILE* f = nullptr;
  if(!f) {
    f = fopen("probe_specimens.log", "a");
    if(f) setvbuf(f, nullptr, _IONBF, 0);   // survive a crash
  }
  return f;
}

// Write one line/blob to stderr AND the specimens file.
static void emit(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  FILE* f = specimens_file();
  if(f) {
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
  }
}

void ProbeSuppressions::packet_content_flag(uint32_t byte_lo, uint32_t byte_hi,
                                            uint32_t width,
                                            uint64_t master_value,
                                            uint64_t clone_value,
                                            hw::Machine* master) {
  emit("\n***** PROBE PACKET-CONTENT FLAG (HIGH SIGNAL — reviewer tier) *****\n"
       "mediated handler read differs: width %u byte range %06X..%06X\n"
       "  master=%08llX  clone=%08llX  (continuing, master authoritative)\n",
       width, byte_lo, byte_hi,
       (unsigned long long)master_value, (unsigned long long)clone_value);
  if(master)
    emit("  at master pc=%08X instr=%llu\n", (uint32_t)master->pc,
         (unsigned long long)master->instruction_count);
  emit("*******************************************************************\n\n");
}

// Per-site input file (user ruling, Aug 14): probe_suppressions.txt in
// the launch cwd, loaded once at first probe-mode lookup. Line format:
//   <call-octal> <site-hex> <regs: any of a0 a1 a2> <SILENT|NOTE> # note
// Blank lines and #-comments skipped. If the file is absent, the
// built-in table above is the fallback seed (and the loader says so).
static std::vector<ProbeSuppression> file_entries;
static std::vector<std::string> file_notes;
static bool file_loaded=false;
static void load_file() {
  file_loaded=true;
  std::ifstream f("probe_suppressions.txt");
  if(!f) { fprintf(stderr,"PROBE: no probe_suppressions.txt — using built-in seed table\n"); return; }
  std::string line;
  while(std::getline(f,line)) {
    size_t h=line.find('#'); std::string note = h==std::string::npos?"":line.substr(h+1);
    std::string body = h==std::string::npos?line:line.substr(0,h);
    std::istringstream is(body);
    std::string calls, sites, tier; uint32_t mask=0; std::string tok;
    if(!(is>>calls>>sites)) continue;
    std::vector<std::string> rest;
    while(is>>tok) rest.push_back(tok);
    if(rest.empty()) continue;
    tier=rest.back(); rest.pop_back();
    for(auto&r:rest){ if(r=="a0")mask|=1; else if(r=="a1")mask|=2; else if(r=="a2")mask|=4; }
    file_notes.push_back(note.empty()?body:note);
    ProbeSuppression e;
    e.call=(int32_t)strtol(calls.c_str(),nullptr,8);
    e.site_pc=(uint32_t)strtoul(sites.c_str(),nullptr,16);
    e.reg_mask=mask; e.note=nullptr; e.silent=(tier=="SILENT");
    file_entries.push_back(e);
  }
  for(size_t i=0;i<file_entries.size();i++) file_entries[i].note=file_notes[i].c_str();
  fprintf(stderr,"PROBE: loaded %zu suppression entries from probe_suppressions.txt\n",file_entries.size());
}

const ProbeSuppression* ProbeSuppressions::find(int32_t call, uint32_t site_pc,
                                                uint32_t mismatch_mask) {
  if(!file_loaded) load_file();
  for(const auto& e : file_entries)
    if(e.call==call && e.site_pc==site_pc && (mismatch_mask & ~e.reg_mask)==0)
      return &e;
  if(!file_entries.empty()) return nullptr;  // file is authoritative when present
  for(int i = 0; i < table_size; i++) {
    const ProbeSuppression& e = table[i];
    if(e.call == call && e.site_pc == site_pc &&
       (mismatch_mask & ~e.reg_mask) == 0)
      return &e;
  }
  return nullptr;
}


static std::map<const ProbeSuppression*, unsigned long> silent_hits;
static std::map<const ProbeSuppression*, unsigned long> note_hits;
static std::map<const ProbeSuppression*, std::set<uint32_t>> note_values;
static std::vector<std::string> new_paste_lines;  // curator's report

static std::map<const ProbeSuppression*, std::set<uint32_t>> silent_values;

void ProbeSuppressions::summarize() {
  for(auto& [e, n] : silent_hits)
    fprintf(stderr, "PROBE silent-specimen summary [%s]: %lu hits, "
            "%zu distinct master ac0 values\n",
            e->note, n, silent_values[e].size());
  for(auto& [e, n] : note_hits)
    fprintf(stderr, "PROBE note-specimen summary [%s]: %lu hits, %zu "
            "distinct values%s\n",
            e->note, n, note_values[e].size(),
            n ? " — promotion candidate if this stays boring" : "");
  if(!new_paste_lines.empty()) {
    fprintf(stderr, "\nPROBE CATALOGUE ACTION ITEMS — %zu new specimen(s) this "
            "session; paste into probe_suppressions.txt after review:\n",
            new_paste_lines.size());
    for(auto& l : new_paste_lines) fprintf(stderr, "  %s\n", l.c_str());
    fprintf(stderr, "  (full forensic records in probe_specimens.log)\n");
  } else if(!silent_hits.empty() || !note_hits.empty()) {
    fprintf(stderr, "PROBE: no new specimens this session — catalogue complete "
            "for played paths.\n");
  }
}

void ProbeSuppressions::log_known(const ProbeSuppression* entry, int32_t call,
                                  uint32_t site_pc,
                                  const int32_t* master_ac,
                                  const int32_t* clone_ac) {
  if(entry->silent) {
    silent_hits[entry]++;
    silent_values[entry].insert((uint32_t)master_ac[0]);
    return;
  }
  note_hits[entry]++;
  note_values[entry].insert((uint32_t)master_ac[0]);
  emit("PROBE known specimen [%s]: call=%04o site=%08X "
       "m=%08X/%08X/%08X c=%08X/%08X/%08X (master authoritative)\n",
       entry->note, call, site_pc,
       (uint32_t)master_ac[0], (uint32_t)master_ac[1], (uint32_t)master_ac[2],
       (uint32_t)clone_ac[0], (uint32_t)clone_ac[1], (uint32_t)clone_ac[2]);
}

// Dump 16 words of the syscall packet neighborhood from one engine's
// memory, returning the words for diff marking. ac2 is the conventional
// packet pointer; an unmapped read must not kill the run, so probe the
// page first.
static bool dump_packet(hw::Machine* m, uint32_t addr, uint16_t* out) {
  if(!m || !m->memory) return false;
  for(int i = 0; i < 16; i++) {
    uint32_t a = addr + i;
    if(!m->memory->find_page((a >> 10) & 0x1FFFFF))
      return false;
    out[i] = (uint16_t)m->memory->read_word(a);
  }
  return true;
}

void ProbeSuppressions::forensic_record(int32_t call, uint32_t master_site,
                                        OSTask* master_task, OSTask* clone_task,
                                        const int32_t* master_ac,
                                        const int32_t* clone_ac,
                                        uint32_t mismatch_mask, bool local_call,
                                        const uint32_t* recent_sites,
                                        int recent_count) {
  hw::Machine* mm = master_task ? master_task->machine : nullptr;
  hw::Machine* cm = clone_task ? clone_task->machine : nullptr;
  int id = next_provisional++;
  { char buf[160];
    snprintf(buf, sizeof(buf), "%04o %08X %s%s%s NOTE  # 2026-08-XX #%d %s — <fill provenance>",
             call, master_site,
             (mismatch_mask&1)?"a0 ":"", (mismatch_mask&2)?"a1 ":"", (mismatch_mask&4)?"a2 ":"",
             id, Trace::call_name(call).c_str());
    new_paste_lines.push_back(buf); }

  emit("\n=========== PROBE NEW SPECIMEN (provisional #%d) ===========\n", id);
  emit("syscall %04o (%s)  site pc %08X  [suppression key: (%04o, 0x%08X, {",
       call, Trace::call_name(call).c_str(), master_site, call, master_site);
  for(int i = 0; i < 3; i++)
    if(mismatch_mask & (1u << i)) emit("ac%d,", i);
  emit("})]\n");
  if(local_call)
    emit("*** LOCAL-class call: master-authority does NOT apply — both\n"
         "*** engines executed their OWN call with differing arguments.\n"
         "*** FLAGGED for reviewer classification (docs/Project11).\n");
  if(mm && cm)
    emit("ordinal=%d  master tid instr=%llu  clone instr=%llu  (skew %lld)\n",
         mm->lockstep_ordinal,
         (unsigned long long)mm->instruction_count,
         (unsigned long long)cm->instruction_count,
         (long long)(mm->instruction_count - cm->instruction_count));

  // Register files, mismatching mediation regs marked.
  for(int side = 0; side < 2; side++) {
    hw::Machine* m = side ? cm : mm;
    const int32_t* med = side ? clone_ac : master_ac;
    emit("%s: pc=%08X", side ? "clone " : "master", m ? (uint32_t)m->pc : 0);
    for(int i = 0; i < 3; i++)
      emit("  ac%d=%08X%s", i, (uint32_t)med[i],
           (mismatch_mask & (1u << i)) ? "*" : " ");
    if(m)
      emit("  ac3=%08X  wsp=%08X wfp=%08X psr=%04X c=%d\n",
           (uint32_t)m->ac[3], (uint32_t)m->wsp, (uint32_t)m->wfp,
           (uint32_t)m->get_psr() & 0xFFFF, m->c);
    else
      emit("  <machine unavailable>\n");
  }

  // Packet neighborhood (16 words at ac2, both engines, diffs marked).
  uint16_t mp[16], cp[16];
  bool mok = dump_packet(mm, (uint32_t)master_ac[2], mp);
  bool cok = dump_packet(cm, (uint32_t)clone_ac[2], cp);
  if(mok && cok) {
    emit("packet[16w] @m:%08X @c:%08X (word: master/clone, * = diff)\n",
         (uint32_t)master_ac[2], (uint32_t)clone_ac[2]);
    for(int i = 0; i < 16; i++)
      emit("  +%02d %04X/%04X%s%s", i, mp[i], cp[i],
           mp[i] != cp[i] ? "*" : " ", (i % 4 == 3) ? "\n" : "");
    if(master_ac[2] != clone_ac[2])
      emit("  NOTE: packet POINTERS differ — content diff is against"
           " different addresses\n");
  } else
    emit("packet dump skipped (ac2 not a mapped address on %s side)\n",
         mok ? "clone" : "master");

  // Recent verified rendezvous site pcs (locates the game phase).
  emit("recent rendezvous sites:");
  for(int i = 0; i < recent_count; i++)
    if(recent_sites[i]) emit(" %08X", recent_sites[i]);
  emit("\n");

  // Master-side provenance trail — the caller chain a human cures from.
  // Replicates CallStack::backtrace's frame walk into the record itself
  // (backtrace() prints to stdout only; the record must survive alone).
  emit("master call-stack backtrace:\n");
  if(mm && mm->call_stack) {
    debug::CallStack* cs = mm->call_stack;
    auto& v = cs->call_stack;
    for(int index = (int)v.size() - 1; index >= 0; index--) {
      std::string location;
      if(index == (int)v.size() - 1)
        location = cs->location_description(mm->pc, v[index].entry_address);
      else {
        int32_t cia = v[index + 1].call_instruction_address;
        if(cia != -1)
          location = cs->location_description(cia, v[index].entry_address);
        else {
          char buf[16];
          snprintf(buf, sizeof(buf), "%08X", v[index + 1].return_address);
          location = buf;
        }
      }
      emit("  frame %2d -- %s\n", index, location.c_str());
    }
  } else
    emit("  <unavailable>\n");
  emit("================= END SPECIMEN #%d (continuing) =================\n\n",
       id);
}

} // namespace os
