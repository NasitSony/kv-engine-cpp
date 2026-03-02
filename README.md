# kv-engine-cpp

A correctness-first key–value storage engine built in C++, focusing on durability, crash recovery, and failure-aware system design.

This project explores how real storage systems behave under failure, starting from first principles and evolving toward distributed and fault-tolerant systems.

---

## 🚀 Current Version: v0.4  
**WAL + Crash Recovery + Snapshots + Group Commit**

---

## ✅ Guarantees

### Crash Consistency
After a crash, the system recovers to the last valid state using snapshot + WAL replay.

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

🧠 Why This Matters

Modern systems depend on:
- Reliable persistence
- Correct recovery after crashes
- Deterministic state reconstruction
- This project builds those foundations step by step.

🔧 Future Directions

- Failure injection testing
- Deterministic replay framework
- Integration with distributed consensus
- Formal verification (long-term)

👤 Author

Built as part of a deeper exploration into distributed systems, fault tolerance, and correctness-driven system design.
