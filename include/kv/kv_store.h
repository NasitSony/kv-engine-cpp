#pragma once
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <iostream>

#include "kv/wal.h"
#include "kv/raft_sm.h"

namespace kv {

class KVStore : public IRaftStateMachine{
public:
  // v0.2: must be called before PUT/DEL for WAL + recovery
  bool open(const std::string& wal_path);

  void put(std::string key, std::string value);
  std::optional<std::string> get(const std::string& key) const;
  bool del(const std::string& key);
  std::size_t size() const;

  bool save_to_file(const std::string& path) const;
  bool load_from_file(const std::string& path);

  bool save_snapshot(const std::string& path);  
  bool load_snapshot(const std::string& path);
  bool checkpoint(const std::string& snapshot_path,
                const std::string& wal_path);
  
  bool flush_wal();

  // v0.7 prefix scan API
  std::vector<std::string>
  list_keys_with_prefix(const std::string& prefix) const;


  void set_group_commit_every(int n) ;

  // Raft state machine apply (must NOT append to WAL)
  void ApplyPut(std::string key, std::string value) override;
  void ApplyDel(const std::string& key) override;

private:
  friend class Wal;
  bool load_from_file_unlocked(const std::string& path);
  bool save_to_file_unlocked(const std::string& path) const;
  int group_commit_every_ = 5;

  

  // Used ONLY during WAL replay to avoid re-logging.
  void apply_put_no_log(std::string key, std::string value) {
    map_[std::move(key)] = std::move(value);
  }
  void apply_del_no_log(const std::string& key) {
    map_.erase(key);
  }

  // Used ONLY during KVStore::open() while holding mu_.
  void apply_put_no_log_unlocked(std::string key, std::string value) {
    std::cerr << "[apply] PUT " << key << "=" << value << "\n";
    map_[std::move(key)] = std::move(value);
  }
  void apply_del_no_log_unlocked(const std::string& key) {
    map_.erase(key);
  }

  mutable std::shared_mutex mu_;
  std::unordered_map<std::string, std::string> map_;

  Wal wal_;
  uint64_t seq_{0};
  bool opened_{false};

};

} // namespace kv