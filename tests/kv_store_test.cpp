#include "kv/kv_store.hpp"
#include "kv/shard.hpp"

#include <atomic>
#include <barrier>
#include <chrono>
#include <random>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

TEST(KVStoreTest, RoutesKeysAndProvidesCrudOperations) {
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
        EXPECT_EQ(store.set(key, value), kv::Status::OK);
    }

    for (const auto& [key, value] : entries) {
        const auto result = store.get(key);
        ASSERT_EQ(result.status, kv::Status::OK);
        ASSERT_TRUE(result.value.has_value());
        EXPECT_EQ(*result.value, value);
        EXPECT_TRUE(store.exists(key));
    }

    EXPECT_EQ(store.remove("price:AAPL"), kv::Status::OK);
    EXPECT_FALSE(store.exists("price:AAPL"));
    EXPECT_EQ(store.get("price:AAPL").status, kv::Status::NOT_FOUND);
    EXPECT_EQ(store.remove("price:AAPL"), kv::Status::NOT_FOUND);

    EXPECT_EQ(store.set("", "invalid"), kv::Status::INVALID_ARGUMENT);
    EXPECT_EQ(store.get("").status, kv::Status::INVALID_ARGUMENT);
    EXPECT_EQ(store.remove(""), kv::Status::INVALID_ARGUMENT);
    EXPECT_FALSE(store.exists(""));
}

TEST(KVStoreTest, SupportsBatchOperationsAndTtlExpiry) {
    using namespace std::chrono_literals;

    kv::KVStore store;
    const std::vector<kv::SetRequest> requests{
        {"account:1", "cash", std::nullopt},
        {"account:2", "margin", 100ms},
        {"account:3", "restricted", std::nullopt},
    };

    ASSERT_EQ(store.set_batch(requests), kv::Status::OK);

    const std::vector<kv::Key> keys{"account:3", "missing", "account:1"};
    const auto results = store.get_batch(keys);
    ASSERT_EQ(results.size(), keys.size());
    ASSERT_TRUE(results[0].value.has_value());
    EXPECT_EQ(*results[0].value, "restricted");
    EXPECT_EQ(results[1].status, kv::Status::NOT_FOUND);
    ASSERT_TRUE(results[2].value.has_value());
    EXPECT_EQ(*results[2].value, "cash");

    ASSERT_EQ(store.set("temporary", "value", 10ms), kv::Status::OK);
    ASSERT_TRUE(store.get("temporary").value.has_value());
    std::this_thread::sleep_for(25ms);
    EXPECT_EQ(store.get("temporary").status, kv::Status::NOT_FOUND);
    EXPECT_FALSE(store.exists("temporary"));

    const std::vector<kv::SetRequest> invalid_requests{
        {"valid-key", "value", std::nullopt},
        {"invalid-ttl", "value", 0ms},
    };
    EXPECT_EQ(store.set_batch(invalid_requests), kv::Status::INVALID_ARGUMENT);
    EXPECT_FALSE(store.exists("valid-key"));
}

TEST(ShardTest, SupportsConcurrentAccess) {
    kv::Shard shard;

    constexpr int kThreadCount = 32;
    constexpr int kOperationsPerThread = 10'000;

    std::atomic<int> completed{0};
    std::atomic<bool> operations_succeeded{true};

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);

    for (int thread_id = 0; thread_id < kThreadCount; ++thread_id) {
        threads.emplace_back([&shard, &completed, &operations_succeeded, thread_id]() {
            for (int operation = 0; operation < kOperationsPerThread; ++operation) {
                const std::string key = "key:" + std::to_string(thread_id);
                const std::string value = "value:" + std::to_string(operation);

                if (shard.set(key, value) != kv::Status::OK) {
                    operations_succeeded = false;
                }

                const auto result = shard.get(key);
                if (result.status != kv::Status::OK || !result.value.has_value()) {
                    operations_succeeded = false;
                }

                if (!shard.exists(key)) {
                    operations_succeeded = false;
                }
            }

            ++completed;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_TRUE(operations_succeeded);
    EXPECT_EQ(completed, kThreadCount);
}

TEST(KVStoreTest, WithstandsContentionAcrossOperations) {
    using namespace std::chrono_literals;

    kv::KVStore store;

    constexpr int kThreadCount = 32;
    constexpr int kOperationsPerThread = 10'000;
    constexpr int kSharedKeyCount = 64;

    std::barrier start_line(kThreadCount);
    std::atomic<int> completed{0};
    std::atomic<bool> operations_succeeded{true};

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);

    for (int thread_id = 0; thread_id < kThreadCount; ++thread_id) {
        threads.emplace_back(
            [&store, &start_line, &completed, &operations_succeeded, thread_id]() {
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
                            if (store.set(key, value) != kv::Status::OK) {
                                operations_succeeded = false;
                            }
                            break;
                        case 1: {
                            const auto result = store.get(key);
                            if ((result.status != kv::Status::OK &&
                                 result.status != kv::Status::NOT_FOUND) ||
                                (result.status == kv::Status::OK &&
                                 !result.value.has_value())) {
                                operations_succeeded = false;
                            }
                            break;
                        }
                        case 2: {
                            const auto status = store.remove(key);
                            if (status != kv::Status::OK &&
                                status != kv::Status::NOT_FOUND) {
                                operations_succeeded = false;
                            }
                            break;
                        }
                        case 3:
                            static_cast<void>(store.exists(key));
                            break;
                        case 4:
                            if (store.set(key, value, 5ms) != kv::Status::OK) {
                                operations_succeeded = false;
                            }
                            break;
                        case 5: {
                            const std::vector<kv::SetRequest> requests{
                                {key, value, std::nullopt},
                                {"shared:" +
                                     std::to_string(generator() % kSharedKeyCount),
                                 value,
                                 5ms},
                            };
                            if (store.set_batch(requests) != kv::Status::OK) {
                                operations_succeeded = false;
                            }
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

    EXPECT_TRUE(operations_succeeded);
    EXPECT_EQ(completed, kThreadCount);
}
