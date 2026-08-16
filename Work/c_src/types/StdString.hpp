// src/types/StdString.hpp
#pragma once
#include "String.hpp"

namespace types {

// Lightweight adapter wrapping std::string as types::String.
// Used by quest:: and emu_quest:: functions to pass string literals
// and computed strings to rt:: functions that expect const String&.

class StdString : public String {
public:
  explicit StdString(const std::string& s) : data(s) {}
  explicit StdString(std::string&& s) : data(std::move(s)) {}

  size_t size() const override { return data.size(); }
  char operator[](size_t pos) const override { return data[pos]; }

  std::string str() const override { return data; }

  void set_char(size_t pos, char c) override { data[pos]=c; }
  void set_size(size_t len) override { data.resize(len); }

  void assign(const std::string& s) override { data=s; }

private:
  std::string data;
};

} // namespace types
