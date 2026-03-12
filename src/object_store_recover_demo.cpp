#include <iostream>
#include <string>
#include <vector>

#include "kv/kv_store.h"
#include "kv/object_store.h"

namespace {

std::string to_string(const std::vector<std::uint8_t>& data) {
  return std::string(data.begin(), data.end());
}

}  // namespace

int main() {
  kv::KVStore store;
  if (!store.open("/tmp/kv_object_demo.wal")) {
    std::cerr << "Failed to open KVStore WAL\n";
    return 1;
  }

  kv::ObjectStore object_store(store, 8);

  auto get_res = object_store.GetObject("photos", "trip/recovery.txt");
  if (!get_res.found) {
    std::cerr << "Recovery failed: " << get_res.error << "\n";
    return 1;
  }

  std::cout << "Recovery phase successful\n";
  std::cout << "  bucket       = " << get_res.metadata.bucket << "\n";
  std::cout << "  key          = " << get_res.metadata.key << "\n";
  std::cout << "  object_id    = " << get_res.metadata.object_id << "\n";
  std::cout << "  size_bytes   = " << get_res.metadata.size_bytes << "\n";
  std::cout << "  chunk_count  = " << get_res.metadata.chunk_count << "\n";
  std::cout << "  content_type = " << get_res.metadata.content_type << "\n";
  std::cout << "  etag         = " << get_res.metadata.etag << "\n";
  std::cout << "  data         = " << to_string(get_res.data) << "\n";

  return 0;
}