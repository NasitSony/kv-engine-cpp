# kv-engine-cpp

A crash-consistent key–value storage engine in C++ implementing write-ahead logging (WAL), group commit, and deterministic recovery. Designed with a correctness-first approach, the system focuses on durability guarantees, failure handling, and reliable state reconstruction under crash scenarios.

Inspired by real-world storage systems such as RocksDB and etcd, this project explores how core storage primitives evolve toward distributed and fault-tolerant systems.
---


## ⚡ Key Results

- Ensures **crash-consistent recovery** with zero data loss up to the last durability boundary (`fsync` / `FLUSH`)
- Implements **group commit batching**, improving write throughput by ~2.7× compared to flush-per-write
- Achieves **deterministic recovery**, guaranteeing identical state reconstruction via WAL replay
- Detects and safely ignores **partial or corrupted WAL records** using CRC validation
- Reduces recovery time via **snapshot-based checkpointing**, avoiding full log replay

## 📊 Performance Snapshot (Group Commit vs Immediate Flush)

**Workload:** 10,000 sequential `PUT`s + final `FLUSH` (local disk)

|      Mode       |    Setting   | total_ms |   ops/sec      |
|-----------------|-------------:|---------:|---------------:|
| Immediate flush | `SETBATCH 1` |  255 ms  |  39,216 ops/s  |
| Group commit    | `SETBATCH 5` |  96 ms   | 104,167 ops/s  |

**Result:** Group commit improves write throughput by ~**2.66×** by batching WAL flushes (reducing flush frequency compared to flushing every write).


## 🏭 Why This Matters (Industry Context)

Modern storage and distributed systems rely on strong durability and recovery guarantees. Databases, stream processors, and consensus systems depend on write-ahead logging (WAL), crash consistency, and deterministic state reconstruction to maintain correctness under failures.

This project implements these core primitives from first principles, mirroring the foundations of production systems such as RocksDB, etcd, and distributed databases. WAL, batching (group commit), snapshotting, and recovery form the backbone of reliable data infrastructure.

By focusing on real failure scenarios (crashes, partial writes, corruption), this system demonstrates how correctness and durability are enforced in production backends, forming the basis for replication, consensus, and fault-tolerant distributed systems.


## 🚀 Current Version: v0.4

WAL • Crash Recovery • Snapshots • Group Commit

---

## ✅ Guarantees

### Crash Consistency
After a crash, the system recovers to the last valid durable state using snapshot + WAL replay.

### Deterministic Recovery
Replaying the same WAL produces the same state.

### Corruption Handling
Incomplete or corrupted WAL records are detected and ignored.

### Durability Boundary (v0.4)
Writes are **batched** and become durable on:
- periodic flush (group commit), or  
- manual `FLUSH` command  

---

## 🧠 Design Overview

### Architecture

Client ->  KVStore (in-memory map) -> WAL (append-only log, buffered) -> Disk


---

### Write Path (v0.4)

PUT / DEL
- append to WAL buffer
- apply to in-memory map
- fsync on batch boundary / FLUSH


---

### Recovery Path

On startup:
- load snapshot
- replay WAL
- validate records (CRC)
- apply valid entries
- stop at first corrupted entry


---

## 🧪 Failure Model

Handled:
- Process crashes (`kill -9`)
- Partial/torn writes
- Interrupted disk writes

Not yet handled:
- Distributed replication
- Byzantine faults
- Performance tuning beyond batching

---

## ⚙️ How to Run

```bash
cmake -S . -B build
cmake --build build
./build/kv_cli
```
💻 CLI Commands

```bash
PUT key value
GET key
DEL key
SIZE
SNAP
FLUSH
EXIT
```
💥 Crash Recovery Demo
```bash
./build/kv_cli
```

```bash
PUT x 100
PUT y 200
FLUSH
```
Crash:

```bash
pkill -9 kv_cli
```

Restart:

```bash
./build/kv_cli
GET x   # 100
GET y   # 200
```

## 📊 Performance Benchmark Demo

Run benchmark with different durability settings:

### Immediate flush (no batching)
```bash
./build/kv_cli
SETBATCH 1
BENCH 10000
```

### Group commit (batched flush)
```bash
./build/kv_cli
SETBATCH 5
BENCH 10000
```

📁 Storage Files
- WAL: /tmp/kv.wal
- Snapshot: /tmp/kv.snapshot

🛣️ Roadmap

v0.1 — In-Memory KV Store ✅
- Thread-safe map
- Basic operations: PUT, GET, DEL

v0.2 — WAL + Crash Recovery ✅
- Append-only WAL
- fsync-based durability
- Recovery via replay
- Corruption handling

v0.3 — Snapshots + Compaction ✅
- Snapshot (checkpoint)
- WAL truncation
- Faster recovery

v0.4 — Group Commit (Current) ✅
- Buffered WAL writes
- Batched fsync (group commit)
- Manual FLUSH
- Explicit durability boundary

v0.5 — Replication (next)
- Leader-based replication
- Log replication
- Basic consensus

v0.6 — Fault-Tolerant / BFT Extensions
- Quorum replication
- Byzantine fault tolerance
- Integration with consensus protocols

🎯 Project Goals

This project is not about building a fast KV store.
It is about understanding:
- Failure-aware system design
- Durability guarantees
- Deterministic recovery
- Storage → distributed systems connection



🔧 Future Directions

- Failure injection testing
- Deterministic replay framework
- Integration with distributed consensus
- Formal verification (long-term)


👤 Author

Built as part of a deeper exploration into distributed systems, fault tolerance, and correctness-driven system design.
