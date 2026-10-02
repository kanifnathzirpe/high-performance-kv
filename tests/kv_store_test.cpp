#include "kv/shard.hpp"

#include <cassert>
#include <iostream>

int main() {
    kv::Shard shard;

    // SET
    auto set_status = shard.set("name", "Kanifnath");
    assert(set_status == kv::Status::OK);

    // GET
    auto result = shard.get("name");

    assert(result.status == kv::Status::OK);
    assert(result.value.has_value());
    assert(result.value.value() == "Kanifnath");

    // EXISTS
    assert(shard.exists("name"));

    // DELETE
    auto delete_status = shard.remove("name");
    assert(delete_status == kv::Status::OK);

    // Verify deletion
    assert(!shard.exists("name"));

    // Missing key
    auto missing = shard.get("unknown");

    assert(missing.status == kv::Status::NOT_FOUND);
    assert(!missing.value.has_value());

    std::cout << "All shard tests passed!\n";

    return 0;
}