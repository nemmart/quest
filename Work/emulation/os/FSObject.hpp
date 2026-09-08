#pragma once
#include <string>
#include <stdexcept>


namespace os {
class FSObject {
public:
  virtual ~FSObject() = default;

  virtual std::string get_path() = 0;
  virtual void set_path(const std::string& path) = 0;
};

} // namespace os
