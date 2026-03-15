#include "kv/object_store.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <set>
#include <unordered_set>
#include <string_view>

#include "kv/object_store_key_codec.h"

namespace kv {
namespace {

std::uint64_t now_epoch_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(
             system_clock::now().time_since_epoch())
      .count();
}

// Simple stable hash for v0.1 etag.
// Not cryptographic; good enough for first milestone/demo.
std::string compute_etag(const std::vector<std::uint8_t>& data) {
  const std::string_view sv(reinterpret_cast<const char*>(data.data()), data.size());
  std::size_t h = std::hash<std::string_view>{}(sv);

  std::ostringstream out;
  out << std::hex << h;
  return out.str();
}

ObjectId make_object_id(const BucketName& bucket, const ObjectKey& key) {
  std::ostringstream out;
  out << bucket << ":" << key << ":" << now_epoch_ms();
  return out.str();
}

// Metadata serialization format (single string):
// object_id \t bucket \t key \t size_bytes \t chunk_count \t chunk_size_bytes \t
// content_type \t etag \t created_at \t updated_at \t state
//
// This is intentionally simple for v0.1.
// Assumption: bucket/key/content_type do not contain tabs/newlines.
std::string serialize_metadata(const ObjectMetadata& m) {
  std::ostringstream out;
  out << m.object_id << '\t'
      << m.bucket << '\t'
      << m.key << '\t'
      << m.size_bytes << '\t'
      << m.chunk_count << '\t'
      << m.chunk_size_bytes << '\t'
      << m.content_type << '\t'
      << m.etag << '\t'
      << m.created_at_epoch_ms << '\t'
      << m.updated_at_epoch_ms << '\t'
      << static_cast<int>(m.state);
  return out.str();
}

std::optional<ObjectMetadata> deserialize_metadata(const std::string& s) {
  std::istringstream in(s);
  ObjectMetadata m;
  int state_int = 0;

  if (!std::getline(in, m.object_id, '\t')) return std::nullopt;
  if (!std::getline(in, m.bucket, '\t')) return std::nullopt;
  if (!std::getline(in, m.key, '\t')) return std::nullopt;

  std::string size_bytes;
  std::string chunk_count;
  std::string chunk_size_bytes;
  std::string created_at;
  std::string updated_at;
  std::string state_str;

  if (!std::getline(in, size_bytes, '\t')) return std::nullopt;
  if (!std::getline(in, chunk_count, '\t')) return std::nullopt;
  if (!std::getline(in, chunk_size_bytes, '\t')) return std::nullopt;
  if (!std::getline(in, m.content_type, '\t')) return std::nullopt;
  if (!std::getline(in, m.etag, '\t')) return std::nullopt;
  if (!std::getline(in, created_at, '\t')) return std::nullopt;
  if (!std::getline(in, updated_at, '\t')) return std::nullopt;
  if (!std::getline(in, state_str, '\t')) return std::nullopt;

  try {
    m.size_bytes = std::stoull(size_bytes);
    m.chunk_count = static_cast<std::uint32_t>(std::stoul(chunk_count));
    m.chunk_size_bytes = static_cast<std::uint32_t>(std::stoul(chunk_size_bytes));
    m.created_at_epoch_ms = std::stoull(created_at);
    m.updated_at_epoch_ms = std::stoull(updated_at);
    state_int = std::stoi(state_str);
  } catch (...) {
    return std::nullopt;
  }

  m.state = static_cast<ObjectState>(state_int);

  return m;
}

std::string serialize_bucket_meta(const BucketMetadata& b) {
  std::ostringstream out;
  out << b.name << '\t' << b.created_at_epoch_ms;
  return out.str();
}

std::optional<BucketMetadata> deserialize_bucket_meta(const std::string& s) {
  std::istringstream in(s);
  BucketMetadata b;
  std::string created_at;

  if (!std::getline(in, b.name, '\t')) return std::nullopt;
  if (!std::getline(in, created_at, '\t')) return std::nullopt;

  try {
    b.created_at_epoch_ms = std::stoull(created_at);
  } catch (...) {
    return std::nullopt;
  }

  return b;
}

}  // namespace

ObjectStore::ObjectStore(KVStore& kv, std::uint32_t chunk_size_bytes)
    : kv_(kv),
      chunk_size_bytes_(chunk_size_bytes == 0 ? 4096 : chunk_size_bytes) {}

bool ObjectStore::CreateBucket(const BucketName& bucket) {
  if (bucket.empty()) return false;

  const std::string bucket_key = ObjectStoreKeyCodec::BucketMetaKey(bucket);
  if (kv_.get(bucket_key).has_value()) return true;

  BucketMetadata meta;
  meta.name = bucket;
  meta.created_at_epoch_ms = now_epoch_ms();

  kv_.put(bucket_key, serialize_bucket_meta(meta));
  return true;
}

bool ObjectStore::BucketExists(const BucketName& bucket) const {
  if (bucket.empty()) return false;
  return kv_.get(ObjectStoreKeyCodec::BucketMetaKey(bucket)).has_value();
}

PutObjectResult ObjectStore::PutObject(const PutObjectRequest& req) {
  PutObjectResult result;

  if (req.bucket.empty()) {
    result.error = "bucket is empty";
    return result;
  }
  if (req.key.empty()) {
    result.error = "object key is empty";
    return result;
  }
  if (!BucketExists(req.bucket)) {
    result.error = "bucket does not exist";
    return result;
  }

  // Detect overwrite of existing object
  const auto existing_meta_raw =
    kv_.get(ObjectStoreKeyCodec::ObjectMetaKey(req.bucket, req.key));

  if (existing_meta_raw.has_value()) {
    auto old_meta = deserialize_metadata(*existing_meta_raw);

    if (old_meta.has_value() &&
        old_meta->state != ObjectState::Deleted) {

        std::cerr << "[overwrite] replacing object_id="
                  << old_meta->object_id
                  << " bucket=" << req.bucket
                  << " key=" << req.key
                  << "\n";

        // Future improvement (v0.8 GC):
        // old_meta->object_id will be used to reclaim old chunks
    }
}

  const ObjectId object_id = make_object_id(req.bucket, req.key);
  const std::uint64_t now_ms = now_epoch_ms();
  const std::string etag = compute_etag(req.data);

  const std::size_t total_size = req.data.size();
  const std::uint32_t chunk_count =
      static_cast<std::uint32_t>((total_size + chunk_size_bytes_ - 1) / chunk_size_bytes_);

  // 1) Write chunks first
  for (std::uint32_t i = 0; i < chunk_count; ++i) {
    const std::size_t start = static_cast<std::size_t>(i) * chunk_size_bytes_;
    const std::size_t end = std::min(start + static_cast<std::size_t>(chunk_size_bytes_), total_size);
    const std::size_t len = end - start;

    std::string chunk_value;
    chunk_value.assign(reinterpret_cast<const char*>(req.data.data() + start), len);

    kv_.put(ObjectStoreKeyCodec::ChunkKey(object_id, i), std::move(chunk_value));
  }

  // 2) Metadata commit point
  ObjectMetadata meta;
  meta.object_id = object_id;
  meta.bucket = req.bucket;
  meta.key = req.key;
  meta.size_bytes = total_size;
  meta.chunk_count = chunk_count;
  meta.chunk_size_bytes = chunk_size_bytes_;
  meta.content_type = req.content_type;
  meta.etag = etag;
  meta.created_at_epoch_ms = now_ms;
  meta.updated_at_epoch_ms = now_ms;
  meta.state = ObjectState::Committed;

  kv_.put(ObjectStoreKeyCodec::ObjectMetaKey(req.bucket, req.key),
          serialize_metadata(meta));

  // 3) Optional index entry (useful for future list/prefix scan)
  kv_.put(ObjectStoreKeyCodec::BucketIndexKey(req.bucket, req.key), object_id);

  // 4) Durability boundary
  if (!kv_.flush_wal()) {
    result.error = "failed to flush WAL";
    return result;
  }

  result.ok = true;
  result.object_id = object_id;
  result.size_bytes = total_size;
  result.chunk_count = chunk_count;
  result.etag = etag;
  return result;
}

GetObjectResult ObjectStore::GetObject(const BucketName& bucket, const ObjectKey& key) {
  GetObjectResult result;

  const auto raw_meta = kv_.get(ObjectStoreKeyCodec::ObjectMetaKey(bucket, key));
  if (!raw_meta.has_value()) {
    result.error = "object metadata not found";
    return result;
  }

  auto meta = deserialize_metadata(*raw_meta);
  if (!meta.has_value()) {
    result.error = "failed to deserialize object metadata";
    return result;
  }

  if (meta->state == ObjectState::Deleted) {
    result.error = "object is deleted";
    return result;
  }

  result.metadata = *meta;
  result.data.reserve(static_cast<std::size_t>(meta->size_bytes));

  for (std::uint32_t i = 0; i < meta->chunk_count; ++i) {
    const auto chunk =
        kv_.get(ObjectStoreKeyCodec::ChunkKey(meta->object_id, i));
    if (!chunk.has_value()) {
      result.error = "missing object chunk at index " + std::to_string(i);
      result.data.clear();
      return result;
    }

    result.data.insert(result.data.end(),
                       chunk->begin(),
                       chunk->end());
  }

  result.found = true;
  return result;
}

DeleteObjectResult ObjectStore::DeleteObject(const BucketName& bucket, const ObjectKey& key) {
  DeleteObjectResult result;

  const auto raw_meta = kv_.get(ObjectStoreKeyCodec::ObjectMetaKey(bucket, key));
  if (!raw_meta.has_value()) {
    result.error = "object metadata not found";
    return result;
  }

  auto meta = deserialize_metadata(*raw_meta);
  if (!meta.has_value()) {
    result.error = "failed to deserialize object metadata";
    return result;
  }

  meta->state = ObjectState::Deleted;
  meta->updated_at_epoch_ms = now_epoch_ms();

  kv_.put(ObjectStoreKeyCodec::ObjectMetaKey(bucket, key),
          serialize_metadata(*meta));

  // Logical delete for v0.1.
  // Keep chunks for now; later you can add background GC.
  kv_.del(ObjectStoreKeyCodec::BucketIndexKey(bucket, key));

  if (!kv_.flush_wal()) {
    result.error = "failed to flush WAL";
    return result;
  }

  result.ok = true;
  return result;
}

ListObjectsResult
ObjectStore::ListObjects(const BucketName& bucket,
                         const std::string& prefix) {
  ListObjectsResult result;
  result.ok = true;

  if (bucket.empty()) {
    result.ok = false;
    result.error = "bucket is empty";
    return result;
  }

  if (!BucketExists(bucket)) {
    result.ok = false;
    result.error = "bucket does not exist";
    return result;
  }

  const std::string scan_prefix =
      ObjectStoreKeyCodec::BucketIndexKey(bucket, prefix);

  const auto index_keys = kv_.list_keys_with_prefix(scan_prefix);

  const std::string base_prefix = "bucketidx:" + bucket + ":";

  for (const auto& index_key : index_keys) {
    if (index_key.size() < base_prefix.size()) {
      continue;
    }

    const std::string object_key =
        index_key.substr(base_prefix.size());

    const auto raw_meta =
        kv_.get(ObjectStoreKeyCodec::ObjectMetaKey(bucket, object_key));

    if (!raw_meta.has_value()) {
      continue;
    }

    auto meta = deserialize_metadata(*raw_meta);

    if (!meta.has_value()) {
      continue;
    }

    if (meta->state == ObjectState::Deleted) {
      continue;
    }

    ObjectListEntry entry;
    entry.key = meta->key;
    entry.size_bytes = meta->size_bytes;
    entry.content_type = meta->content_type;
    entry.etag = meta->etag;
    entry.updated_at_epoch_ms = meta->updated_at_epoch_ms;

    result.objects.push_back(std::move(entry));
  }

  return result;
}

std::size_t ObjectStore::GarbageCollectChunks() {
  std::size_t deleted_chunks = 0;

  // Step 1: collect reachable object_ids from committed metadata
  std::unordered_set<std::string> reachable_object_ids;

  const auto meta_keys = kv_.list_keys_with_prefix("objmeta:");

  for (const auto& meta_key : meta_keys) {
    const auto raw_meta = kv_.get(meta_key);

    if (!raw_meta.has_value()) {
      continue;
    }

    auto meta = deserialize_metadata(*raw_meta);

    if (!meta.has_value()) {
      continue;
    }

    if (meta->state == ObjectState::Committed) {
      reachable_object_ids.insert(meta->object_id);
    }
  }

  // Step 2: scan all chunk keys
  const std::string prefix = "objchunk:";
  const auto chunk_keys = kv_.list_keys_with_prefix(prefix);

  for (const auto& chunk_key : chunk_keys) {
    const std::size_t last_colon = chunk_key.rfind(':');

    if (last_colon == std::string::npos ||
        last_colon <= prefix.size()) {
      continue;
    }

    std::string_view object_id_view(
    chunk_key.data() + prefix.size(),
    last_colon - prefix.size());

    if (reachable_object_ids.find(std::string(object_id_view)) ==
      reachable_object_ids.end()) {

        if (kv_.del(chunk_key)) {
          ++deleted_chunks;
       }
    }
  }

  kv_.flush_wal();

  return deleted_chunks;
}

}  // namespace kv