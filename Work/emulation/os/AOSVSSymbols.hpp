#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>


namespace os {
class AOSVSSymbols {
public:
  static const std::unordered_map<std::string, uint32_t>& symbols();
};

} // namespace os
