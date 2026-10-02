#include "kv/kv_store.hpp"

#include <string>
#include <vector>

#include <benchmark/benchmark.h>

namespace {

constexpr std::size_t kKeyCount = 1'024;
constexpr char kValue[] = "market-data-payload";

std::vector<kv::Key> make_keys(std::size_t count) {
    std::vector<kv::Key> keys;
    keys.reserve(count);

    for (std::size_t index = 0; index < count; ++index) {
        keys.push_back("benchmark:key:" + std::to_string(index));
    }

    return keys;
}

void BM_Set(benchmark::State& state) {
    kv::KVStore store;
    const auto keys = make_keys(kKeyCount);
    std::size_t key_index = 0;

    for (auto _ : state) {
        auto status = store.set(keys[key_index], kValue);
        benchmark::DoNotOptimize(status);
        key_index = (key_index + 1) % keys.size();
    }
}

void BM_Get(benchmark::State& state) {
    kv::KVStore store;
    const auto keys = make_keys(kKeyCount);

    for (const auto& key : keys) {
        store.set(key, kValue);
    }

    std::size_t key_index = 0;
    for (auto _ : state) {
        auto result = store.get(keys[key_index]);
        benchmark::DoNotOptimize(result);
        key_index = (key_index + 1) % keys.size();
    }
}

void BM_SetBatch(benchmark::State& state) {
    kv::KVStore store;
    const auto batch_size = static_cast<std::size_t>(state.range(0));
    const auto keys = make_keys(batch_size);

    std::vector<kv::SetRequest> requests;
    requests.reserve(batch_size);
    for (const auto& key : keys) {
        requests.push_back({key, kValue, std::nullopt});
    }

    for (auto _ : state) {
        auto status = store.set_batch(requests);
        benchmark::DoNotOptimize(status);
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(batch_size));
}

BENCHMARK(BM_Set);
BENCHMARK(BM_Get);
BENCHMARK(BM_SetBatch)->Arg(1)->Arg(8)->Arg(64);

} // namespace

BENCHMARK_MAIN();
