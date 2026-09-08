#pragma once
#include <cstdint>
#include <string>


namespace hw {
struct Definition {
  std::string match;
  std::string name;
  std::string instruction_class;
  std::string instruction_format;
  int32_t oper;  // "operator" is a C++ keyword

  Definition(const std::string& match, const std::string& name,
             const std::string& instruction_class, const std::string& instruction_format,
             int32_t oper)
    : match(match), name(name), instruction_class(instruction_class),
      instruction_format(instruction_format), oper(oper) {}
};

} // namespace hw
