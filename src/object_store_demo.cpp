#include <iostream>
#include <string>
#include <vector>

#include "kv/kv_store.h"
#include "kv/object_store.h"

namespace {

std::vector<std::uint8_t> to_bytes(const std::string& s) {
  return std::vector<std::uint8_t>(s.begin(), s.end());
}

std::string to_string(const std::vector<std::uint8_t>& data) {
  return std::string(data.begin(), data.end());
}

}  // namespace

int main() {
  try {
    kv::KVStore store;
    if (!store.open("/tmp/kv.wal")) {
       std::cerr << "Failed to open KVStore WAL\n";
       return 1;
    }
    kv::ObjectStore object_store(store, 8);

    std::cout << "=== Mini Object Store Demo ===\n";

    const std::string bucket = "photos";
    if (!object_store.CreateBucket(bucket)) {
      std::cerr << "CreateBucket failed\n";
      return 1;
    }
    std::cout << "Created bucket: " << bucket << "\n";

    kv::PutObjectRequest put_req;
    put_req.bucket = bucket;
    put_req.key = "trip/hello.txt";
    put_req.data = to_bytes("Hello from MiniS3 built on KVStore!");
    put_req.content_type = "text/plain";

    auto put_res = object_store.PutObject(put_req);
    if (!put_res.ok) {
      std::cerr << "PutObject failed: " << put_res.error << "\n";
      return 1;
    }

    std::cout << "PutObject OK\n";
    std::cout << "  object_id   = " << put_res.object_id << "\n";
    std::cout << "  size_bytes  = " << put_res.size_bytes << "\n";
    std::cout << "  chunk_count = " << put_res.chunk_count << "\n";
    std::cout << "  etag        = " << put_res.etag << "\n";

    auto get_res = object_store.GetObject(bucket, "trip/hello.txt");
    if (!get_res.found) {
      std::cerr << "GetObject failed: " << get_res.error << "\n";
      return 1;
    }

    std::cout << "\nGetObject OK\n";
    std::cout << "  bucket       = " << get_res.metadata.bucket << "\n";
    std::cout << "  key          = " << get_res.metadata.key << "\n";
    std::cout << "  object_id    = " << get_res.metadata.object_id << "\n";
    std::cout << "  size_bytes   = " << get_res.metadata.size_bytes << "\n";
    std::cout << "  chunk_count  = " << get_res.metadata.chunk_count << "\n";
    std::cout << "  content_type = " << get_res.metadata.content_type << "\n";
    std::cout << "  etag         = " << get_res.metadata.etag << "\n";
    std::cout << "  data         = " << to_string(get_res.data) << "\n";

    auto del_res = object_store.DeleteObject(bucket, "trip/hello.txt");
    if (!del_res.ok) {
      std::cerr << "DeleteObject failed: " << del_res.error << "\n";
      return 1;
    }

    std::cout << "\nDeleteObject OK\n";

    auto get_after_delete = object_store.GetObject(bucket, "trip/hello.txt");
    if (get_after_delete.found) {
      std::cerr << "Expected object to be deleted, but it was still found\n";
      return 1;
    }

    std::cout << "Get after delete correctly failed: "
              << get_after_delete.error << "\n";

    std::cout << "\n=== Demo completed successfully ===\n";
    return 0;

  } catch (const std::exception& ex) {
    std::cerr << "Unhandled exception: " << ex.what() << "\n";
    return 1;
  } catch (...) {
    std::cerr << "Unhandled unknown exception\n";
    return 1;
  }
}