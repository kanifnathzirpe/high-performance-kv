#include "kv/shard.hpp"

#include <mutex>
#include <utility>

namespace kv {

Status Shard::set(const Key& key, const Value& value, std::optional<Ttl> ttl) {
    const SetRequest request{key, value, ttl};
    return set_batch(std::span{&request, 1});
}

Status Shard::set_batch(std::span<const SetRequest> requests) {
    for (const auto& request : requests) {
        if (request.key.empty() ||
            (request.ttl.has_value() && *request.ttl <= Ttl::zero())) {
            return Status::INVALID_ARGUMENT;
        }
    }

    std::unique_lock lock(mutex_);
    const auto now = std::chrono::steady_clock::now();

    for (const auto& request : requests) {
        const auto expires_at = request.ttl.has_value()
            ? std::optional{now + *request.ttl}
            : std::nullopt;

        data_[request.key] = {request.value, expires_at};
    }

    return Status::OK;
}

Result Shard::get(std::string_view key) const {
    if (key.empty()) {
        return {Status::INVALID_ARGUMENT, std::nullopt};
    }

    const Key lookup_key(key);
    const auto now = std::chrono::steady_clock::now();

    {
        std::shared_lock lock(mutex_);

        const auto it = data_.find(lookup_key);
        if (it == data_.end()) {
            return {Status::NOT_FOUND, std::nullopt};
        }

        if (!is_expired(it->second, now)) {
            return {Status::OK, it->second.value};
        }
    }

    std::unique_lock lock(mutex_);
    const auto it = data_.find(lookup_key);
    if (it == data_.end()) {
        return {Status::NOT_FOUND, std::nullopt};
    }

    if (is_expired(it->second, now)) {
        data_.erase(it);
        return {Status::NOT_FOUND, std::nullopt};
    }

    return {Status::OK, it->second.value};
}

Status Shard::remove(std::string_view key) {
    if (key.empty()) {
        return Status::INVALID_ARGUMENT;
    }

    std::unique_lock lock(mutex_);

    const auto it = data_.find(std::string(key));

    if (it == data_.end()) {
        return Status::NOT_FOUND;
    }

    if (is_expired(it->second, std::chrono::steady_clock::now())) {
        data_.erase(it);
        return Status::NOT_FOUND;
    }

    data_.erase(it);

    return Status::OK;
}

bool Shard::exists(std::string_view key) const {
    if (key.empty()) {
        return false;
    }

    const Key lookup_key(key);
    const auto now = std::chrono::steady_clock::now();

    {
        std::shared_lock lock(mutex_);

        const auto it = data_.find(lookup_key);
        if (it == data_.end()) {
            return false;
        }

        if (!is_expired(it->second, now)) {
            return true;
        }
    }

    std::unique_lock lock(mutex_);
    const auto it = data_.find(lookup_key);
    if (it == data_.end()) {
        return false;
    }

    if (is_expired(it->second, now)) {
        data_.erase(it);
        return false;
    }

    return true;
}

bool Shard::is_expired(
    const Entry& entry,
    std::chrono::steady_clock::time_point now) {
    return entry.expires_at.has_value() && now >= *entry.expires_at;
}

} // namespace kv
