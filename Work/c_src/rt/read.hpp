// src/rt/read.hpp
#pragma once
#include <cstdint>

namespace types {
class Context;
class ByteArray;
}

namespace rt {

// ?READ with 4 args — sequential text-mode read
// Reads into buffer (capacity = requested length).
// On return, buffer.size() = actual bytes read.
// eof set to true on end-of-file.
// Throws PLIError on other errors.
void read_4(types::Context& ctx, int32_t channel, types::ByteArray& buffer, bool& eof);

// ?READ with 6 args — read with options (e.g. binary/single-char mode)
// options: AOS/VS ?ISTI flags. ?IBIN (0x1000) = binary/raw mode.
// extra: additional option bits (arg6), merged with options internally.
void read_6(types::Context& ctx, int32_t channel, types::ByteArray& buffer, bool& eof,
            int32_t options, int32_t extra);

// ?READ with 7 args — positioned read with options
// record_number: file position to seek to before reading.
void read_7(types::Context& ctx, int32_t channel, types::ByteArray& buffer, bool& eof,
            int32_t options, int32_t extra, int32_t record_number);

} // namespace rt
