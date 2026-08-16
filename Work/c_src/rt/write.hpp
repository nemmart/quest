// src/rt/write.hpp
#pragma once
#include <cstdint>

namespace types {
class Context;
class ByteArray;
}

namespace rt {

// ?WRITE with 3 args — sequential write
// length updated to actual bytes written on return.
void write_3(types::Context& ctx, int32_t channel,
             const types::ByteArray& buffer, int32_t& length);

// ?WRITE with 6 args — positioned write with options
void write_6(types::Context& ctx, int32_t channel,
             const types::ByteArray& buffer, int32_t& length,
             int32_t options, int32_t extra, int32_t record_number);

} // namespace rt
