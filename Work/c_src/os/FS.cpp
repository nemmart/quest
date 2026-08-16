#include "FS.hpp"
#include "FSObject.hpp"
#include "FSFile.hpp"
#include "FSDirectory.hpp"
#include "OSError.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <stdexcept>
#include <filesystem>




namespace os {
namespace fs = std::filesystem;

FSDirectory* FS::root = nullptr;
std::map<std::string, FSObject*> FS::files;
std::recursive_mutex FS::fs_mutex;

std::string FS::validate_path(const std::string& fs_path) {
  if(fs_path.empty() || fs_path[0] != ':')
    return "";
  for(size_t i = 0; i < fs_path.size(); i++) {
    char c = fs_path[i];
    if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '.' || c == ':' || c == '_'))
      return "";
  }
  for(size_t i = 1; i < fs_path.size(); i++)
    if(fs_path[i-1] == ':' && fs_path[i] == ':')
      return "";
  if(fs_path.size() > 1 && fs_path.back() == ':')
    return "";
  std::string upper = fs_path;
  std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
  return upper;
}

std::string FS::parent_path(const std::string& fs_path) {
  auto pos = fs_path.rfind(':');
  if(pos == 0) return ":";
  return fs_path.substr(0, pos);
}

std::string FS::file_name(const std::string& fs_path) {
  auto pos = fs_path.rfind(':');
  if(pos == 0 || pos == std::string::npos) return "";
  return fs_path.substr(pos + 1);
}

void FS::initialize_with_path(const std::string& load_path) {
  fs::path p(load_path);
  if(!fs::exists(p))
    throw std::runtime_error("FS initialization failed - the load path '" + load_path + "' does not exist");

  std::string name = p.filename().string();
  if(validate_path(":" + name).empty())
    throw std::runtime_error("FS initialization failed - invalid file name");

  std::lock_guard<std::recursive_mutex> lock(fs_mutex);
  if(!files.empty())
    throw std::runtime_error("FS has already been initialized");

  root = new FSDirectory();
  files[":"] = root;
  mkdir(":PER");
  mkdir(":SYSTEM");

  if(fs::is_directory(p))
    load(load_path, "");
  else
    load(load_path, ":" + name);

  for(auto& [k, v] : files)
    fprintf(stderr, "%s\n", k.c_str());
}

void FS::load(const std::string& load_path, const std::string& fs_path) {
  fs::path p(load_path);
  int32_t error = SUCCESS;

  if(fs::is_directory(p)) {
    if(!fs_path.empty())
      error = mkdir(fs_path);
    if(error != SUCCESS) {
      printf("FS load - failed to create FS directory '%s', error code %d\n", fs_path.c_str(), error);
      return;
    }
    for(auto& entry : fs::directory_iterator(p)) {
      std::string name = entry.path().filename().string();
      if(name == "." || name == "..") continue;
      std::string validated = validate_path(fs_path + ":" + name);
      if(validated.empty())
        printf("FS load - skipping '%s', invalid file name\n", name.c_str());
      else
        load(entry.path().string(), validated);
    }
  }
  else {
    FSFile* fs_file = new FSFile();
    error = fs_file->load_pages(load_path);
    if(error == SUCCESS)
      error = insert(fs_path, fs_file);
    if(error != SUCCESS) {
      printf("FS load - failed to load/insert FS file '%s', error code %d\n", fs_path.c_str(), error);
      delete fs_file;
    }
  }
}

FSObject* FS::retrieve(const std::string& fs_path) {
  std::lock_guard<std::recursive_mutex> lock(fs_mutex);
  auto it = files.find(fs_path);
  if(it == files.end()) return nullptr;
  return it->second;
}

int32_t FS::insert(const std::string& raw_path, FSObject* object) {
  std::string fs_path = validate_path(raw_path);
  if(fs_path.empty()) return OSError::FS_INVALID_PATH;

  if(files.empty()) return OSError::FS_NOT_INITIALIZED;
  if(files.count(fs_path)) return OSError::FS_ALREADY_EXISTS;

  FSObject* parent_obj = nullptr;
  auto it = files.find(parent_path(fs_path));
  if(it != files.end()) parent_obj = it->second;

  FSDirectory* parent = dynamic_cast<FSDirectory*>(parent_obj);
  if(!parent) return OSError::FS_PARENT_DIRECTORY_DOES_NOT_EXIST;

  object->set_path(fs_path);
  files[fs_path] = object;
  parent->insert(file_name(fs_path), object);
  return SUCCESS;
}

int32_t FS::mkdir(const std::string& raw_path) {
  std::string fs_path = validate_path(raw_path);
  if(fs_path.empty()) return OSError::FS_INVALID_PATH;

  auto it = files.find(fs_path);
  if(it != files.end()) {
    if(dynamic_cast<FSDirectory*>(it->second))
      return SUCCESS;
    return OSError::FS_ALREADY_EXISTS;
  }
  return insert(fs_path, new FSDirectory());
}

int32_t FS::remove(const std::string& raw_path) {
  std::string fs_path = validate_path(raw_path);
  if(fs_path.empty()) return OSError::FS_INVALID_PATH;
  if(fs_path == ":" || fs_path == ":PER" || fs_path == ":SYSTEM")
    return OSError::FS_PROTECTED_DIRECTORY;

  auto it = files.find(fs_path);
  if(it == files.end()) return OSError::FS_DOES_NOT_EXIST;

  FSDirectory* dir = dynamic_cast<FSDirectory*>(it->second);
  if(dir && dir->file_count() != 0)
    return OSError::FS_DIRECTORY_IS_NOT_EMPTY;

  files.erase(it);
  FSObject* parent_obj = nullptr;
  auto pit = files.find(parent_path(fs_path));
  if(pit != files.end()) parent_obj = pit->second;
  FSDirectory* parent = dynamic_cast<FSDirectory*>(parent_obj);
  if(parent) parent->remove(file_name(fs_path));
  return SUCCESS;
}

void FS::save_all() {
  std::lock_guard<std::recursive_mutex> lock(fs_mutex);
  for(auto& [path, obj] : files) {
    FSFile* file = dynamic_cast<FSFile*>(obj);
    if(!file) continue;
    if(file->is_memory_mapped()) {
      fprintf(stderr, "Flushing mapped file: %s\n", path.c_str());
      file->force();
      file->close_mapped();
    }
    else if(file->file_path.empty()) {
      continue;  // No backing file
    }
    else {
      if(file->modified) {
        fprintf(stderr, "Writing: %s\n", path.c_str());
        file->store_pages(nullptr);
      }
    }
  }
}

} // namespace os
