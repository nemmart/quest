#pragma once
#include <string>
#include <map>
#include <mutex>
#include <cstdint>


namespace os {
class FSObject;
class FSDirectory;

class FS {
public:
  static constexpr int32_t SUCCESS = 0;

  static FSDirectory* root;
  static std::map<std::string, FSObject*> files;
  static std::recursive_mutex fs_mutex;

  static std::string validate_path(const std::string& fs_path);
  static std::string parent_path(const std::string& fs_path);
  static std::string file_name(const std::string& fs_path);

  static void initialize_with_path(const std::string& load_path);
  static void load(const std::string& load_path, const std::string& fs_path);

  static FSObject* retrieve(const std::string& fs_path);
  static int32_t insert(const std::string& fs_path, FSObject* object);
  static int32_t mkdir(const std::string& fs_path);
  static int32_t remove(const std::string& fs_path);

  static void save_all();
};

} // namespace os
