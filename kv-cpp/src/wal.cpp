#include "kv/wal.h"
#include "kv/kv_store.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <unistd.h>
#include <iostream>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace kv {

// -------- CRC32 (IEEE) --------
static uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t n) {
  static uint32_t table[256];
  static bool inited = false;
  if (!inited) {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      table[i] = c;
    }
    inited = true;
  }

  crc = ~crc;
  for (size_t i = 0; i < n; ++i) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
  return ~crc;
}

static bool write_all(int fd, const void* buf, size_t n) {
  const uint8_t* p = static_cast<const uint8_t*>(buf);
  while (n > 0) {
    ssize_t w = ::write(fd, p, n);
    if (w < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    p += static_cast<size_t>(w);
    n -= static_cast<size_t>(w);
  }
  return true;
}

static bool read_exact(int fd, void* buf, size_t n) {
  uint8_t* p = static_cast<uint8_t*>(buf);
  size_t got = 0;
  while (got < n) {
    ssize_t r = ::read(fd, p + got, n - got);
    if (r < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (r == 0) return false; // EOF mid-read
    got += static_cast<size_t>(r);
  }
  return true;
}

#pragma pack(push, 1)
struct WalHeader {
  uint32_t magic;     // 'KVLG'
  uint16_t version;   // 1
  uint8_t  type;      // 1=PUT,2=DEL
  uint32_t key_len;
  uint32_t val_len;   // 0 for DEL
  uint64_t seq;
};
#pragma pack(pop)

static constexpr uint32_t kMagic = 0x474C564Bu;   // 'K''V''L''G'
static constexpr uint16_t kVersion = 1;

Wal::~Wal() {
  if (fd_ >= 0) ::close(fd_);
}

bool Wal::open(const std::string& path) {
  path_ = path;
  fd_ = ::open(path.c_str(), O_CREAT | O_RDWR, 0644);
  if (fd_ < 0) return false;
  if (::lseek(fd_, 0, SEEK_END) < 0) return false;
  last_good_offset_ = 0;
  return true;
}

bool Wal::append_put(uint64_t seq, std::string_view key, std::string_view value) {
  return write_record(Type::Put, seq, key, value);
}

bool Wal::append_del(uint64_t seq, std::string_view key) {
  return write_record(Type::Del, seq, key, {});
}

bool Wal::write_record(Type t, uint64_t seq, std::string_view key, std::string_view value) {
  if (fd_ < 0) return false;

  WalHeader h{};
  h.magic = kMagic;
  h.version = kVersion;
  h.type = static_cast<uint8_t>(t);
  h.key_len = static_cast<uint32_t>(key.size());
  h.val_len = static_cast<uint32_t>(value.size());
  h.seq = seq;

  uint32_t crc = 0;
  crc = crc32_update(crc, reinterpret_cast<const uint8_t*>(&h), sizeof(h));
  crc = crc32_update(crc, reinterpret_cast<const uint8_t*>(key.data()), key.size());
  if (t == Type::Put) {
    crc = crc32_update(crc, reinterpret_cast<const uint8_t*>(value.data()), value.size());
  }

  // Serialize into a contiguous buffer
  const size_t total =
      sizeof(WalHeader) +
      key.size() +
      (t == Type::Put ? value.size() : 0) +
      sizeof(uint32_t);

  std::string rec;
  rec.resize(total);

  char* p = rec.data();
  std::memcpy(p, &h, sizeof(h));
  p += sizeof(h);

  if (!key.empty()) {
    std::memcpy(p, key.data(), key.size());
    p += key.size();
  }
  if (t == Type::Put && !value.empty()) {
    std::memcpy(p, value.data(), value.size());
    p += value.size();
  }

  std::memcpy(p, &crc, sizeof(crc));

  // v0.4: buffer record, do not fsync here
  buffer_.push_back(std::move(rec));
  return true;
}

bool Wal::replay_into(KVStore& store, uint64_t& max_seq) {
  if (fd_ < 0) return false;

  if (::lseek(fd_, 0, SEEK_SET) < 0) return false;

  uint64_t last_good = 0;
  max_seq = 0;
  int applied = 0;

  

  for (;;) {
    WalHeader h{};

    

    ssize_t r = ::read(fd_, &h, sizeof(h));
    if (r == 0) break;                         // clean EOF
    if (r < 0) { if (errno == EINTR) continue; break; }
    if (static_cast<size_t>(r) != sizeof(h)) break; // truncated header

    if (h.magic != kMagic || h.version != kVersion) break;
    if (h.type != static_cast<uint8_t>(Type::Put) && h.type != static_cast<uint8_t>(Type::Del)) break;

    std::string key(h.key_len, '\0');
    std::string val(h.val_len, '\0');

    if (h.key_len && !read_exact(fd_, key.data(), key.size())) break;
    if (h.type == static_cast<uint8_t>(Type::Put) && h.val_len && !read_exact(fd_, val.data(), val.size())) break;

    uint32_t stored_crc = 0;
    if (!read_exact(fd_, &stored_crc, sizeof(stored_crc))) break;

    uint32_t crc = 0;
    crc = crc32_update(crc, reinterpret_cast<const uint8_t*>(&h), sizeof(h));
    crc = crc32_update(crc, reinterpret_cast<const uint8_t*>(key.data()), key.size());
    if (h.type == static_cast<uint8_t>(Type::Put)) {
      crc = crc32_update(crc, reinterpret_cast<const uint8_t*>(val.data()), val.size());
    }

   


    if (crc != stored_crc) break;

    // Apply to store WITHOUT re-logging.
    /*if (h.type == static_cast<uint8_t>(Type::Put)) {
       store.apply_put_no_log_unlocked(std::move(key), std::move(val));
    } else {
      store.apply_del_no_log_unlocked(key);
    }*/
  
   if (h.type == static_cast<uint8_t>(Type::Put)) {
     store.map_[std::move(key)] = std::move(val);
     applied++;
    } else {
      store.map_.erase(key);
     \
      applied++;
   }

    max_seq = std::max(max_seq, h.seq);

    off_t off = ::lseek(fd_, 0, SEEK_CUR);
    if (off < 0) break;
    last_good = static_cast<uint64_t>(off);
  }

  last_good_offset_ = last_good;

  // ready for appends
  ::lseek(fd_, 0, SEEK_END);
  //std::cerr << "[replay] applied_count=" << applied << "\n";
  return true;
}

bool Wal::truncate_to_last_good() {
  if (fd_ < 0) return false;
  return ::ftruncate(fd_, static_cast<off_t>(last_good_offset_)) == 0;
}

void Wal::close() {
  if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

bool Wal::flush() {
  if (fd_ < 0) return false;
  if (buffer_.empty()) return true;

  for (const auto& rec : buffer_) {
    const char* p = rec.data();
    size_t n = rec.size();
    while (n > 0) {
      ssize_t w = ::write(fd_, p, n);
      if (w < 0) return false;
      p += static_cast<size_t>(w);
      n -= static_cast<size_t>(w);
    }
  }

  // macOS: use fsync (fdatasync is not available)
  if (::fsync(fd_) != 0) return false;

  buffer_.clear();
  return true;
}

} // namespace kv