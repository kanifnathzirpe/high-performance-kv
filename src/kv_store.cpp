#include "kv/kv_store.hpp"

#include <functional>

namespace kv {

Status KVStore::set(const Key& key, const Value& value, std::optional<Ttl> ttl) {
    return shard_for(key).set(key, value, ttl);
}

Status KVStore::set_batch(std::span<const SetRequest> requests) {
    for (const auto& request : requests) {
        if (request.key.empty() ||
            (request.ttl.has_value() && *request.ttl <= Ttl::zero())) {
            return Status::INVALID_ARGUMENT;
        }
    }

    std::array<std::vector<SetRequest>, kShardCount> requests_by_shard;
    for (const auto& request : requests) {
        const auto shard_index = std::hash<std::string_view>{}(request.key) % kShardCount;
        requests_by_shard[shard_index].push_back(request);
    }

    for (std::size_t shard_index = 0; shard_index < kShardCount; ++shard_index) {
        auto& shard_requests = requests_by_shard[shard_index];
        if (!shard_requests.empty()) {
            const auto status = shards_[shard_index].set_batch(shard_requests);
            if (status != Status::OK) {
                return status;
            }
        }
    }

    return Status::OK;
}

Result KVStore::get(std::string_view key) const {
    return shard_for(key).get(key);
}

std::vector<Result> KVStore::get_batch(std::span<const Key> keys) const {
    std::vector<Result> results;
    results.reserve(keys.size());

    for (const auto& key : keys) {
        results.push_back(get(key));
    }

    return results;
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
