# High-Performance Distributed Key-Value Database

A high-performance key-value database built from scratch in C++20.

The project focuses on:

- Concurrent data access
- Low-latency operations
- Memory efficiency
- Persistent storage
- LSM-tree architecture
- Distributed partitioning
- Replication
- Fault tolerance
- Performance benchmarking

## Status

🚧 Day 1 — Core KV Engine (Steps 1–10 complete)

The current engine is in-memory and supports thread-safe CRUD operations,
eight-way shard routing, batch writes, TTL expiry, automated tests, and
benchmarks.

See the [Day 1 architecture](docs/architecture.md) for the API, concurrency
guarantees, build instructions, and baseline benchmark setup.
