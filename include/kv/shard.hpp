#pragma once

#include "kv/types.hpp"

#include <chrono>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string_view>
#include <unordered_map>

namespace kv {

class Shard {
public:
    Shard() = default;

    Shard(const Shard&) = delete;
    Shard& operator=(const Shard&) = delete;

    Status set(
        const Key& key,
        const Value& value,
        std::optional<Ttl> ttl = std::nullopt);

    Status set_batch(std::span<const SetRequest> requests);

    Result get(std::string_view key) const;

    Status remove(std::string_view key);

    bool exists(std::string_view key) const;

private:
    struct Entry {
        Value value;
        std::optional<std::chrono::steady_clock::time_point> expires_at;
    };

    static bool is_expired(
        const Entry& entry,
        std::chrono::steady_clock::time_point now);

    mutable std::unordered_map<Key, Entry> data_;
    mutable std::shared_mutex mutex_;
};

} // namespace kv
