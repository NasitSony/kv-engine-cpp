#pragma once

#include <cstdint>
#include <string>

#include "kv/object_store_types.h"

namespace kv {

struct ObjectStoreKeyCodec {
  static std::string BucketMetaKey(const BucketName& bucket) {
    return "bucket:" + bucket;
  }

  static std::string ObjectMetaKey(const BucketName& bucket, const ObjectKey& key) {
    return "objmeta:" + bucket + ":" + key;
  }

  static std::string BucketIndexKey(const BucketName& bucket, const ObjectKey& key) {
    return "bucketidx:" + bucket + ":" + key;
  }

  static std::string ChunkKey(const ObjectId& object_id, std::uint32_t index) {
    return "objchunk:" + object_id + ":" + std::to_string(index);
  }
};

}  // namespace kv