## 🚀 kv-engine-cpp

A correctness-first replicated key–value storage engine in C++

This project builds a crash-consistent and replicated key–value storage engine from first principles.
It focuses on **durability, deterministic recovery**, and **consensus-based replication** — the core foundations behind modern distributed databases and large-scale infrastructure systems.

The system evolves step-by-step from a local storage engine to a replicated distributed service.
---

## ⚡ Key Results

- Ensures **crash-consistent recovery**, guaranteeing zero data loss up to the last durability boundary (fsync / explicit FLUSH)
- Implements **group commit batching**, improving write throughput by ~2.7× compared to flush-per-write durability
- Achieves **deterministic state reconstruction** through ordered WAL replay, ensuring identical recovery outcomes
- Detects and safely ignores **partial or corrupted WAL records** using end-to-end CRC validation
- Reduces recovery time via **snapshot-based checkpointing**, eliminating the need for full log replay after crashes
- Implements **Raft-based replicated state machine semantics**, enabling leader election, majority-based commit, and consistent log replication across nodes
- Demonstrates **fault-tolerant failover and recovery**, including leader crash, re-election, follower catch-up, and client redirection to the active leader


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


## 🚀 Current Version: v0.5

WAL • Crash Recovery • Snapshots • Group Commit • Raft-based Replication

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



## 🧱 Architecture Overview

**Single-Node Storage Engine**

Client 
  |
KVStore (in-memory map)
  |
WAL (Write-Ahead Log, append-only)
  |
Disk

  
**Replicated Distributed Layer**

Client
  |
Raft Leader
  |
Replicated Log
  |
State Machines (KVStore replicas)

---

## ✨ Features Implemented
**✅ v0.1 — In-Memory KV Store**

- Thread-safe key–value map
- PUT / GET / DEL operations
- Reader–writer locking with shared_mutex

**✅ v0.2 — Write-Ahead Log (WAL) + Crash Recovery**

- Append-only WAL
- fsync durability guarantees
- Crash consistency under process kill
- Deterministic state reconstruction via WAL replay
- Corruption detection (partial/torn writes ignored)

***Guarantee:***
If a write returns OK, it survives crashes.

**✅ v0.3 — Snapshots & Log Compaction**

- Periodic snapshot of in-memory state
- WAL truncation after snapshot
- Faster restarts
- Reduced disk growth

**✅ v0.4 — Group Commit & Performance**

- Batched WAL flushes (group commit)
- Reduced fsync frequency
- Lock contention reduction
- Maintains durability while improving throughput


**✅ v0.5 — Raft Consensus Replication**

- Leader election
- Heartbeat mechanism
- Log replication across nodes
- Majority quorum commit
- Commit index advancement
- Follower catch-up after crash
- Client redirection to leader

Guarantee:
Cluster remains consistent despite node failures.


**✅ v0.6 — Mini Object Storage Layer**

- Bucket creation
- Object PUT / GET / DELETE
- Chunked storage for larger objects
- Object metadata management
- Logical delete via metadata state
- Object reconstruction after restart using the underlying WAL-backed KV engine

Guarantee:  
Committed objects remain recoverable after restart, inheriting the durability and crash-recovery semantics of the storage engine.

### Object Write Commit Semantics
Object writes follow a correctness-first design:

1. Object data is split into chunks  
2. Each chunk is written as a KV entry  
3. Object metadata is written last and acts as the commit point  

An object is considered valid only if committed metadata exists. During recovery, objects without committed metadata are ignored.

### Storage Layout
- `bucket:<bucket-name>` → bucket metadata
- `objmeta:<bucket>:<object-key>` → object metadata
- `objchunk:<object-id>:<chunk-index>` → object chunk data
- `bucketidx:<bucket>:<object-key>` → object index entry

**✅ v0.7 — Prefix Scan + ListObjects**

- Prefix-based key scanning in the KV engine
- Bucket object listing via metadata index
- Prefix filtering support for hierarchical-style paths
- Deterministic lexicographic listing order
- Logical delete filtering during listing

Guarantee:
- Object listings reflect the committed metadata state of the system. Only objects with valid committed metadata entries are returned during listing.
- Object Listing Semantics
- Object listing works by scanning the bucket index namespace:
```bash
bucketidx:<bucket>:<object-key>
```

The KV engine performs a prefix scan over this namespace to retrieve
matching object keys. For each entry:

1. The object key is extracted from the index entry
2. Object metadata is loaded from:
   ```bash
   objmeta:<bucket>:<object-key>
   ```

3. Deleted objects are filtered out
4. Remaining objects are returned in lexicographic order

This enables operations equivalent to:
- ListObjects(bucket)
- ListObjects(bucket, prefix)

Storage Index Layout
```bash
- bucket:<bucket-name> → bucket metadata  
- objmeta:<bucket>:<object-key> → object metadata  
- objchunk:<object-id>:<chunk-index> → object data chunks  
- bucketidx:<bucket>:<object-key> → object index entry used for prefix scans
```




## 🧪 Failure Scenarios Tested

✔ Process crash (kill -9)
✔ Partial disk writes
✔ Leader node crash
✔ Follower crash & recovery
✔ Log divergence repair
✔ Replica catch-up via log backtracking


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
## 🖥️ CLI Usage

**Start KV CLI**
```bash
./build/kv_cli
```
**Commands**

```bash
PUT key value
GET key
DEL key
SIZE
SNAP
FLUSH
EXIT
```

## 🌐 Raft Replication Demo

This demo shows leader election, log replication, and fault-tolerant recovery using a 3-node Raft cluster.

**Features Demonstrated**

- Automatic leader election
- Strong consistency via majority commit
- Log replication across nodes
- Leader crash and re-election
- Follower log catch-up after restart
- Client redirection to active leader

**Run Raft Simulation**

```bash
./build/raft_demo
```
**Example Output**

```bash
[raft] node 3 became LEADER term=1
Leader is node 3
ProposePut(a=100) -> true
s1.get(a)=100
s2.get(a)=100
s3.get(a)=100

=== Simulating leader crash ===
[raft] node 2 became LEADER term=2
New leader is node 2
ProposePut(b=200) -> true
s1.get(b)=200
s2.get(b)=200
s3.get(b)=200
```
This demonstrates Raft’s guarantees of:
- Leader-based coordination
- Majority-based commit
- Safety under node failures
- Deterministic state convergence

## 💥 Crash Recovery Demo

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

### Object Storage Recovery Demo

Write an object:

```bash
./build/object_store_write_demo
```

Recover after restart:
```bash
./build/object_store_recover_demo
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

## 📁 Storage Files
```bash
/tmp/kv.wal        → Write-Ahead Log
/tmp/kv.snapshot   → Snapshot file
```
## 🛠️ Build Instructions

```bash
cmake -S . -B build
cmake --build build
```


### Object Write Commit Semantics

Object writes follow a correctness-first design aligned with the WAL-based storage engine.

Write path:

1. Object data is split into chunks.
2. Each chunk is written as a KV entry.
3. Object metadata is written last and acts as the commit point.

An object is considered valid only if committed metadata exists.  
During recovery, objects without committed metadata are ignored.

This ensures deterministic recovery consistent with the storage engine's durability guarantees.

## 🧠 What This Project Demonstrates

**Storage Engine Concepts**

- WAL durability
- Crash consistency
- Deterministic replay
- Snapshotting & compaction

**Distributed Systems Concepts**

- Consensus protocols
- Leader election
- Log replication
- Quorum commits
- Failure recovery

**Systems Engineering**

- Correctness-first design
- Failure-aware architecture
- Deterministic state machines
- Clean layering (storage → replication → clients)


## 🔮 Roadmap

**v0.6 — Replicated Metadata Service**

A control-plane metadata store for distributed systems:
- Shard ownership metadata
- Leader leases
- Rebalancing coordination

**v0.7 — LLM Serving Cache Coordinator**

- KV-cache placement metadata
- Node failure reassignment
- Session routing
- AI inference infrastructure support

**v0.8 — Formal Correctness & Testing**

- Failure injection tests
- Jepsen-style validation
- Deterministic replay testing


## 🎯 Project Goals

This project is not about building the fastest KV store.
It is about understanding:
- How data survives crashes
- How write durability actually works
- How distributed replicas stay consistent
- How consensus protocols coordinate state
- How storage engines underpin AI & cloud infrastructure




## 🎓 Educational Value

This project serves as a hands-on exploration of:
- Database internals
- Distributed consensus
- Systems reliability engineering
- AI infrastructure foundations


## 👤 Author

**Nasit Sarwar Sony**
Distributed Systems & AI Infrastructure Engineer

Built as part of a deep exploration into:
Distributed Systems • Storage Engines • Consensus Protocols • Fault Tolerance • AI Infrastructure

## 📜 License

MIT License
