#include "FSDirectory.hpp"




namespace os {
std::vector<std::string> FSDirectory::contents() {
  std::vector<std::string> result;
  for(auto& name : list)
    result.push_back(path + ":" + name);
  return result;
}

} // namespace os
