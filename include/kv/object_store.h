#pragma once

#include <string>

#include "kv/kv_store.h"
#include "kv/object_store_types.h"

namespace kv {

class ObjectStore {
 public:
  explicit ObjectStore(KVStore& kv, std::uint32_t chunk_size_bytes = 4096);

  bool CreateBucket(const BucketName& bucket);
  bool BucketExists(const BucketName& bucket) const;

  PutObjectResult PutObject(const PutObjectRequest& req);
  GetObjectResult GetObject(const BucketName& bucket, const ObjectKey& key);
  DeleteObjectResult DeleteObject(const BucketName& bucket, const ObjectKey& key);

  ListObjectsResult ListObjects(const BucketName& bucket,
                                const std::string& prefix = "");

  // v0.8
  std::size_t GarbageCollectChunks();                              

 private:
  KVStore& kv_;
  std::uint32_t chunk_size_bytes_{4096};
};

}  // namespace kv