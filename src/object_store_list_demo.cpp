#include <iostream>
#include <vector>

#include "kv/kv_store.h"
#include "kv/object_store.h"

std::vector<std::uint8_t> to_bytes(const std::string& s) {
  return std::vector<std::uint8_t>(s.begin(), s.end());
}

int main() {
  kv::KVStore store;

  if (!store.open("/tmp/kv_list_demo.wal")) {
    std::cerr << "Failed to open store\n";
    return 1;
  }

  kv::ObjectStore obj(store, 8);

  obj.CreateBucket("photos");

  kv::PutObjectRequest a;
  a.bucket = "photos";
  a.key = "trip/a.txt";
  a.data = to_bytes("aaa");
  a.content_type = "text/plain";
  obj.PutObject(a);

  kv::PutObjectRequest b;
  b.bucket = "photos";
  b.key = "trip/b.txt";
  b.data = to_bytes("bbb");
  b.content_type = "text/plain";
  obj.PutObject(b);

  kv::PutObjectRequest c;
  c.bucket = "photos";
  c.key = "notes/c.txt";
  c.data = to_bytes("ccc");
  c.content_type = "text/plain";
  obj.PutObject(c);

  auto res = obj.ListObjects("photos", "trip/");

  for (auto& o : res.objects) {
    std::cout << o.key << " size=" << o.size_bytes << "\n";
  }

  return 0;
}