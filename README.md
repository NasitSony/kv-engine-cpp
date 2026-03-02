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
