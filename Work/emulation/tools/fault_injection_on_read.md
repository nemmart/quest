# Fault injection: read-triggered clone-copy flip

Validated technique for tripping MirrorPage compare-on-read at the exact
moment of server consumption (August 2026 session; see
EmulationVerification.md §11). Wall-clock-timed flips are unreliable —
the server reads shared pages only in bursts during request processing
(e.g. ~45k reads of :SHARED_DATA_FILE:4 word 0x323 during character
creation) and reads *nothing* while the game idles at the command
prompt — so the flip is triggered from inside the read path itself: on
the Nth MirrorPage read of a matching page, flip a copy byte; the same
read's compare then trips.

Temporary snippet for `os/MirrorPage.cpp` (call `maybe_perturb_on_read`
at the top of each read method, before the real/copy fetch; label from
the real page's trace_label, so run with `-trace FILE -types shared`):

```cpp
// TEMPORARY: QUEST_PERTURB_ON_READ="N:label_substr:byte_offset"
static void maybe_perturb_on_read(hw::Page* real, hw::Page* mirror,
                                  const char* label) {
  static int remaining = -2;
  static std::string want;
  static uint32_t byte_off = 0;
  if(remaining == -2) {
    const char* v = getenv("QUEST_PERTURB_ON_READ");
    if(!v) { remaining = -1; return; }
    std::string s(v);
    size_t p1 = s.find(':'), p2 = s.rfind(':');
    remaining = atoi(s.substr(0, p1).c_str());
    want = s.substr(p1 + 1, p2 - p1 - 1);
    byte_off = atoi(s.substr(p2 + 1).c_str()) & 0x7FF;
  }
  if(remaining <= 0 || !strstr(label, want.c_str()))
    return;
  if(--remaining == 0)
    if(auto* cap = dynamic_cast<ArrayPage*>(mirror)) {
      cap->bytes[byte_off] ^= 1;
      fprintf(stderr, "PERTURB-ON-READ: flipped %s~clone off=0x%03X\n",
              label, byte_off);
    }
}
```

Demonstrated result (QUEST_PERTURB_ON_READ="20000:SHARED_DATA_FILE:4:1607"):

```
MirrorPage read mismatch (server consuming shared data):
  page=:SHARED_DATA_FILE:4 op=word off=0x323 real=00003ECF clone_copy=00003ECE
reader: QUEST_SERVER (instruction)
  FIND_OBJECT+0xA7 <- IPC_TASK+0xB25 <- QUEST_SERVER+0x7D2
```

Companion results from the same fault class (byte 0x647 of
:SHARED_DATA_FILE:4, wall-clock flips):
- flip while client pairs are flowing → **pair-boundary page audit**
  trips within PAGE_AUDIT_INTERVAL pairs, exact byte reported;
- with the audit disabled, the **batch-pair checker** trips when the
  clone consumes the byte (ac1 off by exactly the flipped bit, in
  DISPLAY_SCREEN <- START_TURN).

A useful target-finding aid: temporarily log MirrorPage reads
(page label, op, offset, timestamp) to see what the server actually
consumes and when.
