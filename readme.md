# Cache Algorithms Project

This project implements and compares several advanced cache replacement algorithms in C++. The codebase is modular and extensible, supporting:
- LRU (Least Recently Used)
- LRU-K
- Sharded LRU-K
- LFU (Least Frequently Used)
- AvgLFU (LFU with average frequency control)
- Sharded AvgLFU
- ARC (Adaptive Replacement Cache)

Each algorithm is implemented as a generic C++ template class, and the project includes multithreaded test harnesses for benchmarking and comparison.

## LRU Family

### LRU (Least Recently Used)
**Problem:**
- LRU evicts the least recently accessed item. It is simple and effective for recency-based workloads.
- **Limitation:** LRU performs poorly when frequently accessed items are spaced far apart (e.g., loop or scan patterns), leading to cache pollution and low hit rates.

### LRU-K
**Improvement:**
- LRU-K tracks the K-th most recent access for each item, promoting items to the main cache only after K hits in a cold cache.
- **Solves:** Reduces cache pollution from one-time or infrequent accesses, improving hit rates for items with true temporal locality.
- **Limitation:** Still suffers from scalability issues in highly concurrent environments.

### Sharded LRU-K
**Improvement:**
- The cache is divided into multiple shards (sub-caches), each operating independently.
- **Solves:** Reduces lock contention and improves scalability in multithreaded scenarios.
- **Limitation:** Sharding can lead to uneven cache utilization if the hash function is not well-chosen.

## LFU Family

### LFU (Least Frequently Used)
**Problem:**
- LFU evicts the least frequently accessed item, favoring items with high long-term access frequency.
- **Limitation:** LFU can suffer from cache pollution by items that were hot in the distant past but are no longer relevant (cache "inertia").

### AvgLFU (LFU with Average Frequency Control)
**Improvement:**
- AvgLFU periodically reduces the frequency counts of all items when the average frequency exceeds a threshold.
- **Solves:** Prevents cache inertia by allowing old, once-hot items to be evicted, keeping the cache responsive to changing access patterns.

### Sharded AvgLFU
**Improvement:**
- Like Sharded LRU-K, divides the AvgLFU cache into multiple shards for better concurrency.
- **Solves:** Reduces lock contention and improves throughput in multithreaded environments.

## ARC Family

### ARC (Adaptive Replacement Cache)
**Improvement:**
- ARC dynamically balances between recency (LRU) and frequency (LFU) by maintaining four lists: recent, frequent, and their respective ghost lists.
- **Solves:**
  - Adapts to both recency- and frequency-dominated workloads.
  - Avoids cache pollution from scans (like LRU-K) and cache inertia (like LFU).
  - Automatically tunes itself to the workload, providing robust performance across diverse patterns.

## How ARC Solves LRU and LFU Problems
- ARC uses both recency and frequency information, adapting its internal balance based on observed workload.
- The ghost lists allow ARC to detect when the workload shifts (e.g., from recency to frequency dominated) and adjust accordingly.
- In this codebase, ARC is implemented by combining an LRU and an LFU component, with dynamic capacity adjustment and ghost lists for adaptation.

## Usage
- All cache classes are generic and thread-safe.
- The `test.cpp` file benchmarks all cache types under various access patterns (hot data, loop, workload shift) and prints a comparative table of hit rates.

## Example Result Table

```
Test Case                   LRU (Hit%)        LFU (Hit%)        ARC (Hit%)
--------------------------------------------------------------------------------
Hot Data Access Test        0.291000%         28.933000%        26.298000%
Loop Pattern Test           13.906000%        8.514500%         31.064500%
Workload Shift Test         34.088750%        30.962500%        55.203750%
--------------------------------------------------------------------------------
```

---
For more details, see the Doxygen comments in each header file.
