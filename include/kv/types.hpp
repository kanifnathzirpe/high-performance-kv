#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace kv {

using Key = std::string;
using Value = std::string;

using SequenceNumber = std::uint64_t;

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

} // namespace kv