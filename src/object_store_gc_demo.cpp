#include <iostream>
#include <string>
#include <vector>

#include "kv/kv_store.h"
#include "kv/object_store.h"

std::vector<std::uint8_t> to_bytes(const std::string& s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

std::string to_string(const std::vector<std::uint8_t>& v) {
    return std::string(v.begin(), v.end());
}

int main() {
    kv::KVStore store;

    if (!store.open("/tmp/kv_gc_demo.wal")) {
        std::cerr << "Failed to open KVStore\n";
        return 1;
    }

    kv::ObjectStore obj(store, 8);

    obj.CreateBucket("photos");

    // First object
    kv::PutObjectRequest r1;
    r1.bucket = "photos";
    r1.key = "trip/a.txt";
    r1.data = to_bytes("old-data");
    r1.content_type = "text/plain";

    obj.PutObject(r1);

    // Overwrite object
    kv::PutObjectRequest r2;
    r2.bucket = "photos";
    r2.key = "trip/a.txt";
    r2.data = to_bytes("new-data");
    r2.content_type = "text/plain";

    obj.PutObject(r2);

    // Verify current object
    auto get_res = obj.GetObject("photos", "trip/a.txt");

    if (get_res.found) {
        std::cout << "Current object data: "
                  << to_string(get_res.data)
                  << "\n";
    }

    // Run GC
    auto deleted = obj.GarbageCollectChunks();

    std::cout << "GC deleted chunks: "
              << deleted
              << "\n";

    return 0;
}