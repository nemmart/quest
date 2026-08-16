// src/rt/read.cpp
#include "read.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/ByteArray.hpp"
#include "../types/PLIError.hpp"

namespace rt {

void read_4(types::Context& ctx, int32_t channel, types::ByteArray& buffer, bool& eof) {
  int32_t err=ctx.os.read_channel(channel, buffer, 0, eof);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

void read_6(types::Context& ctx, int32_t channel, types::ByteArray& buffer, bool& eof,
            int32_t options, int32_t extra) {
  int32_t merged=(options&~7)|((extra>>13)&7);
  int32_t err=ctx.os.read_channel(channel, buffer, merged, eof);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

void read_7(types::Context& ctx, int32_t channel, types::ByteArray& buffer, bool& eof,
            int32_t options, int32_t extra, int32_t record_number) {
  int32_t merged=(options&~7)|((extra>>13)&7);
  int32_t err=ctx.os.read_channel_at(channel, buffer, merged, record_number, eof);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
}

} // namespace rt
