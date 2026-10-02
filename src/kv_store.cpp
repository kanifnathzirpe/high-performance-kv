#include "kv/kv_store.hpp"

#include <functional>

namespace kv {

Status KVStore::set(const Key& key, const Value& value) {
    return shard_for(key).set(key, value);
}

Result KVStore::get(std::string_view key) const {
    return shard_for(key).get(key);
}

Status KVStore::remove(std::string_view key) {
    return shard_for(key).remove(key);
}

bool KVStore::exists(std::string_view key) const {
    return shard_for(key).exists(key);
}

Shard& KVStore::shard_for(std::string_view key) {
    const auto shard_index = std::hash<std::string_view>{}(key) % kShardCount;
    return shards_[shard_index];
}

const Shard& KVStore::shard_for(std::string_view key) const {
    const auto shard_index = std::hash<std::string_view>{}(key) % kShardCount;
    return shards_[shard_index];
}

} // namespace kv
