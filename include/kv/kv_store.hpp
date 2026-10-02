#pragma once

#include "kv/shard.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace kv {

class KVStore {
public:
    static constexpr std::size_t kShardCount = 8;

    Status set(
        const Key& key,
        const Value& value,
        std::optional<Ttl> ttl = std::nullopt);

    Status set_batch(std::span<const SetRequest> requests);

    Result get(std::string_view key) const;

    std::vector<Result> get_batch(std::span<const Key> keys) const;

    Status remove(std::string_view key);

    bool exists(std::string_view key) const;

private:
    Shard& shard_for(std::string_view key);
    const Shard& shard_for(std::string_view key) const;

    std::array<Shard, kShardCount> shards_;
};

} // namespace kv
