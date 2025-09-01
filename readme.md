# High-Concurrency Distributed Cache System

A high-performance distributed cache system implemented in C++17, using consistent hashing algorithm for data sharding and load balancing. The system supports multi-node cluster deployment and dynamic scaling to achieve efficient data sharing and access in distributed environments.

## System Architecture

### System Flow: HTTP Gateway to Database

The system follows this data flow pattern:

1. **HTTP Client** → Sends REST request (GET/SET/DELETE) to HttpGateway
2. **HttpGateway** → Uses consistent hashing to select target cache node → Converts HTTP to gRPC call
3. **CacheServer** → Routes the call to specific CacheGroup based on group name (multiple CacheGroups can exist in one physical node with different group names)
4. **CacheGroup** → Checks local cache → If cache hit, returns data immediately
5. **Cache Miss Handling**: CacheGroup → Uses PeerPicker to select peers → Queries other cache nodes via gRPC using Peer objects
6. **Database Fallback**: If not found in any cache node → Calls cacheMissHandler → Fetches from database
7. **Response Path**: Database → CacheGroup → CacheServer → HttpGateway → HTTP Client

### gRPC Communication Components

The system uses gRPC for all inter-component communication:

- **CacheServer** (gRPC Server): Implements cache service with Get/Set/Delete operations, routes to specific CacheGroup by group name
- **Peer** (gRPC Client): Communicates with remote cache servers
- **HttpGateway** (gRPC Client): Routes HTTP requests to cache servers via gRPC
- **CacheGroup**: Orchestrates distributed operations using PeerPicker and Peer objects (multiple groups per node supported)
- **PeerPicker**: Manages peer selection and load balancing using consistent hashing

## Key Features

1. **Adaptive Consistent Hashing**
   - Adaptive consistent hashing algorithm with virtual node support
   - Real-time monitoring of node load distribution and dynamic peer selection

2. **High Concurrency Support**
   - Shared_mutex for concurrent access (multiple readers, single writer)
   - Atomic operations ensuring thread safety of statistical data
   - SingleFlight pattern preventing cache breakdown and duplicate requests

3. **Service Discovery & Communication**
   - Etcd-based service registration and discovery with lease mechanism
   - gRPC protocol for high-performance inter-node communication
   - Automatic cleanup of failed nodes and dynamic peer management

4. **Data Consistency & Synchronization**
   - Automatic synchronization of Set/Delete operations across all relevant nodes
   - Cache miss recovery through peer communication before database fallback
   - Distributed cache coherency with eventual consistency guarantees

5. **HTTP Gateway & RESTful API**
   - HTTP-to-gRPC gateway providing RESTful interface
   - Consistent hashing for request routing to appropriate cache nodes

## Cache Algorithms Implementation

The system includes advanced cache replacement algorithms implemented as generic C++ template classes:

### Supported Algorithms
- **LRU (Least Recently Used)**: Simple recency-based eviction
- **LRU-K**: Multi-hit promotion with K-distance tracking  
- **Sharded LRU-K**: Parallel sharded version for high concurrency
- **LFU (Least Frequently Used)**: Frequency-based eviction
- **AvgLFU**: LFU with adaptive frequency decay
- **Sharded AvgLFU**: Parallel sharded frequency-based caching
- **ARC (Adaptive Replacement Cache)**: Dynamic LRU/LFU balance

### Key Features
- **Thread-safe**: All implementations support concurrent access
- **Generic**: Template-based design for any key-value types
- **Adaptive**: ARC automatically adapts to workload patterns
- **Scalable**: Sharded versions reduce lock contention

### Performance Example
```
Test Case                   LRU (Hit%)    LFU (Hit%)    ARC (Hit%)
----------------------------------------------------------------
Hot Data Access            0.29%         28.93%        26.30%
Loop Pattern               13.91%        8.51%         31.06%
Workload Shift             34.09%        30.96%        55.20%
```

*ARC demonstrates superior adaptability across varying workload patterns.*

---

## Building and Usage

The project uses standard C++17 with gRPC, etcd, and protobuf dependencies. See source files for detailed implementation and Doxygen documentation.
