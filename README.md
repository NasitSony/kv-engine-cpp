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


