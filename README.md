# kv-engine-cpp

# kv-engine-cpp (v0.1)

A minimal in-memory key-value store written in C++20.

This project is the foundation for building a production-style storage system,
focusing on correctness, durability, and distributed replication in later versions.

## Features (v0.1)
- In-memory key-value store
- PUT / GET / DEL operations
- Thread-safe (shared mutex)
- Simple CLI interface

## Example
kv> PUT user:1 nasit
OK
kv> GET user:1
nasit
kv> DEL user:1
1


## Build & Run
```bash
cmake -S . -B build
cmake --build build
./build/kv_cli


Roadmap
 - v0.2 — Write-Ahead Log (WAL) + crash recovery
 - v0.3 — Memtable + SSTable (LSM-tree basics)
 - v0.4 — Compaction + storage efficiency
 - v0.5 — Benchmarking + production-style testing
 - v0.6 — Distributed replication (Raft)

Motivation

Most systems fail not in the happy path, but under failures.

This project incrementally builds a storage system that remains correct
under crashes, concurrency, and eventually distributed faults.

Status

🚧 Actively building (starting with v0.1)

---

# 🚀 GitHub push commands

```bash
git init
git add .
git commit -m "v0.1: In-memory KV store (PUT/GET/DEL, thread-safe CLI)"
git branch -M main
git remote add origin <your-repo-url>
git push -u origin main
