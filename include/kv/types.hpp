#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace kv {

using Key = std::string;
using Value = std::string;

using SequenceNumber = std::uint64_t;
using Ttl = std::chrono::milliseconds;

enum class Status {
    OK,
    NOT_FOUND,
    INVALID_ARGUMENT,
    ALREADY_EXISTS
};

struct Result {
    Status status;
    std::optional<Value> value;
};

struct SetRequest {
    Key key;
    Value value;
    std::optional<Ttl> ttl;
};

} // namespace kv
