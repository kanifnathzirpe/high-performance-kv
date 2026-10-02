#include "kv/shard.hpp"

#include <mutex>
#include <utility>

namespace kv {

Status Shard::set(const Key& key, const Value& value) {
    if (key.empty()) {
        return Status::INVALID_ARGUMENT;
    }

    std::unique_lock lock(mutex_);

    data_[key] = value;

    return Status::OK;
}

Result Shard::get(std::string_view key) const {
    if (key.empty()) {
        return {
            Status::INVALID_ARGUMENT,
            std::nullopt
        };
    }

    std::shared_lock lock(mutex_);

    auto it = data_.find(std::string(key));

    if (it == data_.end()) {
        return {
            Status::NOT_FOUND,
            std::nullopt
        };
    }

    return {
        Status::OK,
        it->second
    };
}

Status Shard::remove(std::string_view key) {
    if (key.empty()) {
        return Status::INVALID_ARGUMENT;
    }

    std::unique_lock lock(mutex_);

    auto it = data_.find(std::string(key));

    if (it == data_.end()) {
        return Status::NOT_FOUND;
    }

    data_.erase(it);

    return Status::OK;
}

bool Shard::exists(std::string_view key) const {
    if (key.empty()) {
        return false;
    }

    std::shared_lock lock(mutex_);

    return data_.find(std::string(key)) != data_.end();
}

} // namespace kv