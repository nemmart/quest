// src/rt/write.cpp
#include "write.hpp"
#include "../types/Context.hpp"
#include "../types/OperatingSystem.hpp"
#include "../types/ByteArray.hpp"
#include "../types/PLIError.hpp"

namespace rt {

void write_3(types::Context& ctx, int32_t channel,
             const types::ByteArray& buffer, int32_t& length) {
  int32_t err=ctx.os.write_channel_bytes(channel, buffer);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
  length=static_cast<int32_t>(buffer.size());
}

void write_6(types::Context& ctx, int32_t channel,
             const types::ByteArray& buffer, int32_t& length,
             int32_t options, int32_t extra, int32_t record_number) {
  static constexpr int32_t IPST=0x8000;
  int32_t err;
  if(options&IPST)
    err=ctx.os.write_channel_bytes_at(channel, buffer, record_number);
  else
    err=ctx.os.write_channel_bytes(channel, buffer);
  if(err) throw types::PLIError(static_cast<uint32_t>(err));
  length=static_cast<int32_t>(buffer.size());
}

} // namespace rt
