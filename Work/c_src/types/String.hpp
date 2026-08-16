// src/types/String.hpp
#pragma once
#include <cstddef>
#include <string>
#include <stdexcept>

namespace types {

class String {
public:
  virtual ~String() = default;

  static constexpr size_t npos=static_cast<size_t>(-1);

  virtual size_t size() const = 0;
  size_t length() const { return size(); }
  bool empty() const { return size()==0; }

  virtual char operator[](size_t pos) const = 0;
  virtual char at(size_t pos) const {
    if(pos>=size()) throw std::out_of_range("String::at");
    return (*this)[pos];
  }

  virtual size_t find(char c, size_t pos=0) const {
    for(size_t i=pos; i<size(); i++)
      if((*this)[i]==c) return i;
    return npos;
  }

  virtual std::string str() const {
    std::string result;
    result.reserve(size());
    for(size_t i=0; i<size(); i++)
      result+=(*this)[i];
    return result;
  }

  std::string substr(size_t pos=0, size_t len=npos) const {
    if(pos>size()) throw std::out_of_range("String::substr");
    size_t actual=(len==npos || pos+len>size()) ? size()-pos : len;
    std::string result;
    result.reserve(actual);
    for(size_t i=0; i<actual; i++)
      result+=(*this)[pos+i];
    return result;
  }

  virtual void set_char(size_t pos, char c) = 0;
  virtual void set_size(size_t len) = 0;

  virtual void assign(const std::string& s) {
    set_size(s.size());
    for(size_t i=0; i<s.size(); i++)
      set_char(i, s[i]);
  }

  virtual void assign(const String& s) {
    set_size(s.size());
    for(size_t i=0; i<s.size(); i++)
      set_char(i, s[i]);
  }
};

} // namespace types
