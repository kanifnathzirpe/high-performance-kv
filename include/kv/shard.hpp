#pragma once

#include "kv/types.hpp"

#include <shared_mutex>
#include <string_view>
#include <unordered_map>

namespace kv {

class Shard {
public:
    Shard() = default;

    Shard(const Shard&) = delete;
    Shard& operator=(const Shard&) = delete;

    Status set(const Key& key, const Value& value);

    Result get(std::string_view key) const;

    Status remove(std::string_view key);

    bool exists(std::string_view key) const;

private:
    std::unordered_map<Key, Value> data_;
    mutable std::shared_mutex mutex_;
};

} // namespace kv