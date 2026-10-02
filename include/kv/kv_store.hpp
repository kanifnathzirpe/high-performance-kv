#pragma once

#include "kv/shard.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace kv {

class KVStore {
public:
    static constexpr std::size_t kShardCount = 8;

    Status set(const Key& key, const Value& value);

    Result get(std::string_view key) const;

    Status remove(std::string_view key);

    bool exists(std::string_view key) const;

private:
    Shard& shard_for(std::string_view key);
    const Shard& shard_for(std::string_view key) const;

    std::array<Shard, kShardCount> shards_;
};

} // namespace kv
