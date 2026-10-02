#include "kv/kv_store.hpp"
#include "kv/shard.hpp"

#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

void kv_store_routing_test() {
    kv::KVStore store;

    const std::vector<std::pair<kv::Key, kv::Value>> entries{
        {"user:1", "Ada"},
        {"user:2", "Grace"},
        {"price:AAPL", "227.48"},
        {"price:MSFT", "415.26"},
        {"session:abc", "active"},
        {"session:def", "expired"},
        {"order:1001", "open"},
        {"order:1002", "filled"},
    };

    for (const auto& [key, value] : entries) {
        assert(store.set(key, value) == kv::Status::OK);
    }

    for (const auto& [key, value] : entries) {
        const auto result = store.get(key);
        assert(result.status == kv::Status::OK);
        assert(result.value == value);
        assert(store.exists(key));
    }

    assert(store.remove("price:AAPL") == kv::Status::OK);
    assert(!store.exists("price:AAPL"));
    assert(store.get("price:AAPL").status == kv::Status::NOT_FOUND);
    assert(store.remove("price:AAPL") == kv::Status::NOT_FOUND);

    assert(store.set("", "invalid") == kv::Status::INVALID_ARGUMENT);
    assert(store.get("").status == kv::Status::INVALID_ARGUMENT);
    assert(store.remove("") == kv::Status::INVALID_ARGUMENT);
    assert(!store.exists(""));

    std::cout << "KVStore routing test passed!\n";
}

void concurrent_test() {
    kv::Shard shard;

    constexpr int kThreadCount = 32;
    constexpr int kOperationsPerThread = 10'000;

    std::atomic<int> completed{0};

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);

    for (int thread_id = 0; thread_id < kThreadCount; ++thread_id) {
        threads.emplace_back([&shard, &completed, thread_id]() {
            for (int i = 0; i < kOperationsPerThread; ++i) {
                const std::string key =
                    "key:" + std::to_string(thread_id);

                const std::string value =
                    "value:" + std::to_string(i);

                // Write
                auto status = shard.set(key, value);
                assert(status == kv::Status::OK);

                // Read
                auto result = shard.get(key);

                assert(result.status == kv::Status::OK);
                assert(result.value.has_value());

                // Existence check
                assert(shard.exists(key));
            }

            ++completed;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    assert(completed == kThreadCount);

    std::cout << "Concurrent test passed!\n";
}
int main() {
    kv_store_routing_test();
    concurrent_test();
    return 0;
}
