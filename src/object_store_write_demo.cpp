#include <iostream>
#include <string>
#include <vector>

#include "kv/kv_store.h"
#include "kv/object_store.h"

namespace {

std::vector<std::uint8_t> to_bytes(const std::string& s) {
  return std::vector<std::uint8_t>(s.begin(), s.end());
}

}  // namespace

int main() {
  kv::KVStore store;
  if (!store.open("/tmp/kv_object_demo.wal")) {
    std::cerr << "Failed to open KVStore WAL\n";
    return 1;
  }

  kv::ObjectStore object_store(store, 8);

  const std::string bucket = "photos";
  if (!object_store.CreateBucket(bucket)) {
    std::cerr << "CreateBucket failed\n";
    return 1;
  }

  kv::PutObjectRequest req;
  req.bucket = bucket;
  req.key = "trip/recovery.txt";
  req.data = to_bytes("Recovered object data after restart!");
  req.content_type = "text/plain";

  auto put_res = object_store.PutObject(req);
  if (!put_res.ok) {
    std::cerr << "PutObject failed: " << put_res.error << "\n";
    return 1;
  }

  std::cout << "Write phase complete\n";
  std::cout << "  object_id   = " << put_res.object_id << "\n";
  std::cout << "  size_bytes  = " << put_res.size_bytes << "\n";
  std::cout << "  chunk_count = " << put_res.chunk_count << "\n";
  std::cout << "  etag        = " << put_res.etag << "\n";
  std::cout << "Safe to terminate and restart recovery demo.\n";

  return 0;
}