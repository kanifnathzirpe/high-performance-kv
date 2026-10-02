#include "kv/kv_store.hpp"
#include "kv/shard.hpp"

#include <atomic>
#include <barrier>
#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
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

void batch_and_ttl_test() {
    using namespace std::chrono_literals;

    kv::KVStore store;
    const std::vector<kv::SetRequest> requests{
        {"account:1", "cash", std::nullopt},
        {"account:2", "margin", 100ms},
        {"account:3", "restricted", std::nullopt},
    };

    assert(store.set_batch(requests) == kv::Status::OK);

    const std::vector<kv::Key> keys{"account:3", "missing", "account:1"};
    const auto results = store.get_batch(keys);
    assert(results.size() == keys.size());
    assert(results[0].value == "restricted");
    assert(results[1].status == kv::Status::NOT_FOUND);
    assert(results[2].value == "cash");

    assert(store.set("temporary", "value", 10ms) == kv::Status::OK);
    assert(store.get("temporary").value == "value");
    std::this_thread::sleep_for(25ms);
    assert(store.get("temporary").status == kv::Status::NOT_FOUND);
    assert(!store.exists("temporary"));

    const std::vector<kv::SetRequest> invalid_requests{
        {"valid-key", "value", std::nullopt},
        {"invalid-ttl", "value", 0ms},
    };
    assert(store.set_batch(invalid_requests) == kv::Status::INVALID_ARGUMENT);
    assert(!store.exists("valid-key"));

    std::cout << "Batch and TTL test passed!\n";
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

void kv_store_contention_stress_test() {
    using namespace std::chrono_literals;

    kv::KVStore store;

    constexpr int kThreadCount = 32;
    constexpr int kOperationsPerThread = 10'000;
    constexpr int kSharedKeyCount = 64;

    std::barrier start_line(kThreadCount);
    std::atomic<int> completed{0};

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);

    for (int thread_id = 0; thread_id < kThreadCount; ++thread_id) {
        threads.emplace_back([&store, &start_line, &completed, thread_id]() {
            std::minstd_rand generator(thread_id + 1);
            start_line.arrive_and_wait();

            for (int operation = 0; operation < kOperationsPerThread; ++operation) {
                const kv::Key key =
                    "shared:" + std::to_string(generator() % kSharedKeyCount);
                const kv::Value value =
                    "thread:" + std::to_string(thread_id) +
                    ":operation:" + std::to_string(operation);

                switch (generator() % 6) {
                    case 0:
                        assert(store.set(key, value) == kv::Status::OK);
                        break;
                    case 1: {
                        const auto result = store.get(key);
                        assert(result.status == kv::Status::OK ||
                               result.status == kv::Status::NOT_FOUND);
                        if (result.status == kv::Status::OK) {
                            assert(result.value.has_value());
                        }
                        break;
                    }
                    case 2: {
                        const auto status = store.remove(key);
                        assert(status == kv::Status::OK ||
                               status == kv::Status::NOT_FOUND);
                        break;
                    }
                    case 3:
                        static_cast<void>(store.exists(key));
                        break;
                    case 4:
                        assert(store.set(key, value, 5ms) == kv::Status::OK);
                        break;
                    case 5: {
                        const std::vector<kv::SetRequest> requests{
                            {key, value, std::nullopt},
                            {"shared:" +
                                 std::to_string(generator() % kSharedKeyCount),
                             value,
                             5ms},
                        };
                        assert(store.set_batch(requests) == kv::Status::OK);
                        break;
                    }
                }
            }

            ++completed;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    assert(completed == kThreadCount);

    std::cout << "KVStore contention stress test passed!\n";
}

int main() {
    kv_store_routing_test();
    batch_and_ttl_test();
    concurrent_test();
    kv_store_contention_stress_test();
    return 0;
}
