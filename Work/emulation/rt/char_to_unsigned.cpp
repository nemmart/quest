// src/rt/char_to_unsigned.cpp
#include "char_to_unsigned.hpp"
#include "../types/Context.hpp"
#include "../types/String.hpp"
#include "../types/PLIError.hpp"

namespace rt {

static constexpr uint32_t ERR_CONVERSION = 0x00011611;  // invalid character
static constexpr uint32_t ERR_SIZE       = 0x00011606;  // overflow

static int digit_value(char c, int base) {
  int val;

  if(c>='0' && c<='9')
    val=c-'0';
  else if(c>='A' && c<='F')
    val=c-'A'+10;
  else if(c>='a' && c<='f')
    val=c-'a'+10;
  else
    return -1;

  if(val>=base)
    return -1;

  return val;
}

uint32_t char_to_unsigned_2(types::Context& ctx, const types::String& str, int base) {
  size_t len=str.size();
  size_t i=0;
  uint32_t result=0;
  int d;
  uint64_t next;

  if(base<2 || base>16)
    throw types::PLIError(ERR_CONVERSION);

  // Skip leading spaces
  while(i<len && str[i]==' ')
    i++;

  // Parse digits
  while(i<len && str[i]!=' ') {
    d=digit_value(str[i], base);
    if(d<0)
      throw types::PLIError(ERR_CONVERSION);

    next=static_cast<uint64_t>(result)*base+d;
    if(next>0xFFFFFFFF)
      throw types::PLIError(ERR_SIZE);

    result=static_cast<uint32_t>(next);
    i++;
  }

  // Verify trailing characters are all spaces
  while(i<len) {
    if(str[i]!=' ')
      throw types::PLIError(ERR_CONVERSION);
    i++;
  }

  return result;
}

uint32_t char_to_unsigned_1(types::Context& ctx, const types::String& str) {
  return char_to_unsigned_2(ctx, str, 10);
}

} // namespace rt
