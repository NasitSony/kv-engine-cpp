#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kv {

using ObjectId = std::string;
using BucketName = std::string;
using ObjectKey = std::string;

struct BucketMetadata {
  BucketName name;
  std::uint64_t created_at_epoch_ms{0};
};

struct ChunkDescriptor {
  std::uint32_t index{0};
  std::uint64_t offset{0};
  std::uint32_t size_bytes{0};
  std::string chunk_key;
};

enum class ObjectState : std::uint8_t {
  Pending = 0,
  Committed = 1,
  Deleted = 2,
};

struct ObjectMetadata {
  ObjectId object_id;
  BucketName bucket;
  ObjectKey key;

  std::uint64_t size_bytes{0};
  std::uint32_t chunk_count{0};
  std::uint32_t chunk_size_bytes{0};

  std::string content_type{"application/octet-stream"};
  std::string etag;
  std::uint64_t created_at_epoch_ms{0};
  std::uint64_t updated_at_epoch_ms{0};

  ObjectState state{ObjectState::Committed};

  std::vector<ChunkDescriptor> chunks;
};

struct PutObjectRequest {
  BucketName bucket;
  ObjectKey key;
  std::vector<std::uint8_t> data;
  std::string content_type{"application/octet-stream"};
};

struct PutObjectResult {
  bool ok{false};
  std::string error;

  ObjectId object_id;
  std::uint64_t size_bytes{0};
  std::uint32_t chunk_count{0};
  std::string etag;
};

struct GetObjectResult {
  bool found{false};
  std::string error;

  ObjectMetadata metadata;
  std::vector<std::uint8_t> data;
};

struct ObjectListEntry {
  ObjectKey key;
  std::uint64_t size_bytes{0};
  std::string content_type;
  std::string etag;
  std::uint64_t updated_at_epoch_ms{0};
};

struct ListObjectsResult {
  bool ok{false};
  std::string error;
  std::vector<ObjectListEntry> objects;
};

struct DeleteObjectResult {
  bool ok{false};
  std::string error;
};

}  // namespace kv