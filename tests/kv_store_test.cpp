#include "kv/shard.hpp"

#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

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
    concurrent_test();
    return 0;
}