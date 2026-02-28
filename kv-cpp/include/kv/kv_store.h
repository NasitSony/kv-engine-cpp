#pragma once
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <cstdint>

#include "kv/wal.h"

namespace kv {

class KVStore {
public:
  // v0.2: must be called before PUT/DEL for WAL + recovery
  bool open(const std::string& wal_path);

  void put(std::string key, std::string value);
  std::optional<std::string> get(const std::string& key) const;
  bool del(const std::string& key);
  std::size_t size() const;

  bool save_to_file(const std::string& path) const;
  bool load_from_file(const std::string& path);

private:
  friend class Wal;

  // Used ONLY during WAL replay to avoid re-logging.
  void apply_put_no_log(std::string key, std::string value) {
    map_[std::move(key)] = std::move(value);
  }
  void apply_del_no_log(const std::string& key) {
    map_.erase(key);
  }

  mutable std::shared_mutex mu_;
  std::unordered_map<std::string, std::string> map_;

  Wal wal_;
  uint64_t seq_{0};
  bool opened_{false};
};

} // namespace kv