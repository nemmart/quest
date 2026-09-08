#pragma once
#include "FSObject.hpp"
#include <string>
#include <set>
#include <map>
#include <vector>
#include <stdexcept>


namespace os {
class FSDirectory : public FSObject {
public:
  std::string path;
  std::set<std::string> list;
  std::map<std::string, FSObject*> entries;

  FSDirectory() {}

  std::string get_path() override { return path; }
  void set_path(const std::string& p) override {
    if(!path.empty()) throw std::runtime_error("Attempt to change an FSObject path");
    path = p;
  }

  std::vector<std::string> contents();
  void insert(const std::string& name, FSObject* object) { entries[name] = object; }
  void remove(const std::string& name) { entries.erase(name); }
  int32_t file_count() { return static_cast<int32_t>(entries.size()); }
};

} // namespace os
