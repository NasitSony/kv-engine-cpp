#include "kv/kv_store.h"
#include <fstream>

namespace kv {

void KVStore::put(std::string key, std::string value) {
  std::unique_lock lock(mu_);
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
  return map_.erase(key) > 0;
}

std::size_t KVStore::size() const {
  std::shared_lock lock(mu_);
  return map_.size();
}

bool KVStore::save_to_file(const std::string& path) const {
  std::shared_lock lock(mu_);
  std::ofstream out(path);
  if (!out) return false;
  for (auto& [k, v] : map_) {
    out << k << '\t' << v << '\n';
  }
  return true;
}

bool KVStore::load_from_file(const std::string& path) {
  std::ifstream in(path);
  if (!in) return false;
  std::string k, v;
  while (in >> k >> v) {
    map_[k] = v;
  }
  return true;
}

}
