#include "kv/kv_store.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
namespace kv {

/*bool KVStore::open(const std::string& wal_path) {
  std::unique_lock lock(mu_);
  if (opened_) return true;

  if (!wal_.open(wal_path)) return false;

  // Replay WAL into map_ (no locks inside replay; we already hold mu_)
  // But replay_into calls apply_* which touches map_. We are holding mu_ here -> safe.
  uint64_t max_seq = 0;
  bool ok = wal_.replay_into(*this, max_seq);
  if (!ok) return false;

  seq_ = max_seq;
  opened_ = true;

  // Optional cleanup: remove corrupted tail
  // wal_.truncate_to_last_good();

  return true;
}*/
bool KVStore::open(const std::string& wal_path) {
  std::unique_lock lock(mu_);
  std::cerr << "[open] start\n";

  if (opened_) return true;

  std::cerr << "[open] load snapshot\n";
  (void)load_from_file_unlocked("/tmp/kv.snapshot"); // <-- NO DEADLOCK

  std::cerr << "[open] wal open: " << wal_path << "\n";
  if (!wal_.open(wal_path)) return false;

  std::cerr << "[open] wal replay\n";
  uint64_t max_seq = 0;
  if (!wal_.replay_into(*this, max_seq)) return false;

  seq_ = max_seq;
  opened_ = true;
  std::cerr << "[open] done (seq=" << seq_ << ")\n";
  return true;
}
void KVStore::put(std::string key, std::string value) {
  std::unique_lock lock(mu_);
  if (!opened_) return; // or throw

  uint64_t s = ++seq_;
  if (!wal_.append_put(s, key, value)) {
    // do not apply if durability failed
    return;
  }
  map_[std::move(key)] = std::move(value);
}

std::optional<std::string> KVStore::get(const std::string& key) const {
  std::shared_lock lock(mu_);
  auto it = map_.find(key);
  if (it == map_.end()) return std::nullopt;
  return it->second;
}

bool KVStore::del(const std::string& key) {
  std::unique_lock lock(mu_);
  if (!opened_) return false; // or throw

  uint64_t s = ++seq_;
  if (!wal_.append_del(s, key)) {
    return false;
  }
  return map_.erase(key) > 0;
}

std::size_t KVStore::size() const {
  std::shared_lock lock(mu_);
  return map_.size();
}

// Keep snapshot utilities if you want, but note:
// v0.2 correctness is via WAL; snapshot is optional.
bool KVStore::load_from_file(const std::string& path) {
  std::unique_lock lock(mu_);
  return load_from_file_unlocked(path);
}

bool KVStore::save_to_file(const std::string& path) const {
  std::shared_lock lock(mu_);
  return save_to_file_unlocked(path);
}

bool KVStore::save_snapshot(const std::string& path) {
  std::shared_lock lock(mu_);

  std::ofstream out(path + ".tmp");
  if (!out) return false;

  for (auto& [k, v] : map_) {
    out << k << '\t' << v << '\n';
  }

  out.close();

  // atomic replace
  std::rename((path + ".tmp").c_str(), path.c_str());

  return true;
}

bool KVStore::checkpoint(const std::string& snapshot_path,
                         const std::string& wal_path) {
  std::unique_lock lock(mu_);
  if (!opened_) return false;

  // 1. Save snapshot
  if (!save_to_file_unlocked(snapshot_path)) return false;

  // 2. Rotate WAL
  wal_.close();
  std::remove(wal_path.c_str());

  if (!wal_.open(wal_path)) return false;

  return true;
}

bool KVStore::load_from_file_unlocked(const std::string& path) {
  std::ifstream in(path);
  if (!in) return false;
  std::string k, v;
  while (in >> k >> v) {
    map_[k] = v;
  }
  return true;
}

bool KVStore::save_to_file_unlocked(const std::string& path) const {
  std::ofstream out(path);
  if (!out) return false;
  for (const auto& [k, v] : map_) {
    out << k << '\t' << v << '\n';
  }
  return true;
}

} // namespace kv