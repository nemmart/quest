// src/types/WordArray.hpp
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace types {

class WordArray {
public:
  virtual ~WordArray() = default;

  virtual size_t size() const = 0;       // current content length (words)
  virtual size_t capacity() const = 0;   // maximum buffer length (words)
  bool empty() const { return size()==0; }

  virtual int32_t operator[](size_t pos) const = 0;
  virtual void set_word(size_t pos, int32_t val) = 0;
  virtual void set_size(size_t len) = 0;

  virtual std::vector<int32_t> to_vector() const {
    std::vector<int32_t> result(size());
    for(size_t i=0; i<size(); i++)
      result[i]=(*this)[i];
    return result;
  }

  virtual void assign(const std::vector<int32_t>& data) {
    set_size(data.size());
    for(size_t i=0; i<data.size(); i++)
      set_word(i, data[i]);
  }
};

} // namespace types
