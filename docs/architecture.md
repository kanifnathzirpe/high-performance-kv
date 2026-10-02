# Day 1 Architecture

## Scope

Day 1 provides an in-memory C++20 key-value engine with thread-safe sharding,
batch writes, time-to-live (TTL) support, automated tests, and a benchmark
baseline. Persistence, replication, networking, LSM trees, and distributed
membership are intentionally out of scope for this milestone.

## Component layout

```text
Application
    |
    v
KVStore
    |  hash(key) % 8
    +----------------------------+
    |        |        |          |
    v        v        v          v
Shard 0   Shard 1   ...       Shard 7
    |                              |
    +-- unordered_map<Key, Entry> -+
        shared_mutex per shard
```

`KVStore` owns eight `Shard` instances. It routes every key with
`std::hash<std::string_view>{}(key) % 8`; callers never select a shard
themselves. This spreads unrelated keys across independent locks while keeping
the routing calculation constant-time.

## Data model and API

Keys and values are strings. A stored entry contains the value plus an optional
`steady_clock` expiry time. `Status` distinguishes successful operations,
missing keys, and invalid arguments.

| Operation | Behavior |
|---|---|
| `set(key, value, ttl)` | Inserts or replaces a key. A TTL is optional and, if supplied, must be positive. |
| `get(key)` | Returns `OK` and a value, or `NOT_FOUND`. |
| `remove(key)` | Removes a live key, or returns `NOT_FOUND`. |
| `exists(key)` | Checks whether a live key is present. |
| `set_batch(requests)` | Validates then writes a group of `SetRequest` values. |
| `get_batch(keys)` | Returns one `Result` per input key, in input order. |

Empty keys and non-positive TTL values are invalid. `exists` returns `false`
for an empty key because its return type has no status channel.

## Concurrency model

Each shard protects its `unordered_map` with `std::shared_mutex`:

- Reads start with a shared lock, so independent readers of one shard can run
  concurrently.
- Writes, deletes, batch writes, and expired-entry cleanup hold that shard's
  exclusive lock.
- Different shards have different locks, so operations on keys routed to
  different shards proceed independently.

An expired key found during a read or existence check first releases the shared
lock, then rechecks it under the exclusive lock before erasing it. The recheck
preserves a concurrent renewed value rather than deleting it.

## TTL behavior

TTL uses `std::chrono::steady_clock`, avoiding wall-clock adjustments. Expiry
is lazy: `get`, `exists`, and `remove` treat expired entries as absent and
remove them when encountered. There is no background sweeper yet, so an expired
key that is never accessed can retain memory until a later cleanup mechanism is
introduced.

## Batch semantics

`set_batch` validates every request before it writes any entry. It then groups
requests by shard and applies each nonempty group while holding one exclusive
lock for that shard. This avoids invalid-request partial writes and reduces
lock acquisition relative to repeatedly calling single-key `set` on the same
shard.

Batch writes are not a global transaction: readers can observe completed shard
groups independently while a multi-shard batch is running. Similarly,
`get_batch` preserves input order but does not provide a point-in-time snapshot
across shards.

## Validation and benchmarking

The CMake test target uses GoogleTest and CTest. It covers CRUD and routing,
batch and TTL behavior, concurrent single-shard access, and a 32-thread mixed
KVStore contention workload. The concurrency tests have also passed under
ThreadSanitizer.

Google Benchmark is available through the opt-in `KV_BUILD_BENCHMARKS` option.
It measures steady-state single-key `set`, single-key `get`, and batch writes
of 1, 8, and 64 entries. An initial three-repetition Release run on the Day 1
development machine measured approximately 81 ns for `set`, 64 ns for `get`,
and 11.3–14.6 million batch items per second. CPU frequency scaling was
enabled, so these values are a comparison baseline, not portable guarantees.

## Build and run

Run the test suite:

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Build and run the Release benchmarks:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DKV_BUILD_BENCHMARKS=ON
cmake --build build-release --parallel
./build-release/kv_benchmark --benchmark_min_time=0.05s --benchmark_repetitions=3
```

GoogleTest and Google Benchmark are fetched by CMake when their respective
targets are enabled.
