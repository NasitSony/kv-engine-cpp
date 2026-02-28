#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace kv {

class Wal {
 public:
  enum class Type : uint8_t { Put = 1, Del = 2 };

  Wal() = default;
  ~Wal();

  Wal(const Wal&) = delete;
  Wal& operator=(const Wal&) = delete;

  bool open(const std::string& path);

  // Returns false only on hard I/O error. Corrupt/truncated tail is treated as normal boundary.
  bool replay_into(class KVStore& store, uint64_t& max_seq);

  bool append_put(uint64_t seq, std::string_view key, std::string_view value);
  bool append_del(uint64_t seq, std::string_view key);

  bool truncate_to_last_good();

 private:
  bool write_record(Type t, uint64_t seq, std::string_view key, std::string_view value);

  int fd_ = -1;
  std::string path_;
  uint64_t last_good_offset_ = 0;
};

}  // namespace kv