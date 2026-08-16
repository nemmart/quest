#pragma once
#include "../hw/Page.hpp"


namespace os {

// Step 3 of the lockstep harness (docs/LockstepHarness.md): the server's
// mapping of a shared data page is wrapped in a MirrorPage so that every
// server write lands both on the real page (which the master and the
// server share) and on the clone's private copy. Reads come from the real
// page. The write pair is held together under Lockstep::WriteGate so a
// client pair can never observe one page updated and not the other; the
// gate is reentrant, so the underlying ArrayPage gates nest harmlessly.
//
// Compare-on-read is ENABLED: every server read reads both pages, USES
// the real value, and COMPARES the clone's copy — divergence is reported
// at the moment the server would consume the data. The formerly blocking
// window (a master handler write racing its replay into the copy) is
// closed at the source: the OSContext write funnel dual-writes mediated
// handler writes to mirrored shared pages (real + copy under one gate,
// replay skips them as delivered). Off-worker reads take the WriteGate so
// the two-sided read cannot interleave a client pair.

class MirrorPage : public hw::Page {
public:
  hw::Page* real;
  hw::Page* mirror;

  MirrorPage(hw::Page* real, hw::Page* mirror) : real(real), mirror(mirror) {}

  uint32_t read(uint32_t offset) override;
  void write(uint32_t offset, uint32_t value) override;

  uint32_t read_byte(uint32_t offset) override;
  uint32_t read_word(uint32_t offset) override;
  uint32_t read_wide(uint32_t offset) override;

  void write_byte(uint32_t offset, uint32_t value) override;
  void write_word(uint32_t offset, uint32_t value) override;
  void write_wide(uint32_t offset, uint32_t value) override;
};

} // namespace os
