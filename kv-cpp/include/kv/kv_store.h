#pragma once
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace kv {
class KVStore {
public:
  void put(std::string key, std::string value);
  std::optional<std::string> get(const std::string& key) const;
  bool del(const std::string& key);
  std::size_t size() const;
  bool save_to_file(const std::string& path) const;
  bool load_from_file(const std::string& path);

private:
  mutable std::shared_mutex mu_;
  std::unordered_map<std::string, std::string> map_;
};
}
