# gRPC Communication Architecture

This document explains which components in the High-Concurrency Cache System use gRPC to communicate with each other.

## Quick Answer

The following components use gRPC for communication:

1. **CacheServer** (gRPC Server) - Provides cache service endpoints
2. **Peer** (gRPC Client) - Communicates with remote cache servers  
3. **HttpGateway** (gRPC Client) - Routes HTTP requests to cache servers
4. **CacheGroup** (gRPC Client Orchestrator) - Manages distributed cache operations
5. **PeerPicker** (gRPC Client Manager) - Manages peer selection and connections

## Overview

The distributed cache system uses gRPC as the primary communication protocol between different components. The gRPC service is defined in `src/cache.proto` and provides three main operations: Get, Set, and Delete.

## gRPC Service Definition

```protobuf
service Cache {
    rpc Get(Request) returns (GetResponse);
    rpc Set(Request) returns (SetResponse);
    rpc Delete(Request) returns (DeleteResponse);
}
```

## Components Using gRPC Communication

### 1. CacheServer (gRPC Server)

**Location**: `include/cacheserver.h`, `src/cacheserver.cpp`

**Role**: Acts as a gRPC server that provides cache services

**Key Features**:
- Inherits from `cache::Cache::Service` 
- Implements the gRPC service methods: `Get()`, `Set()`, `Delete()`
- Listens on a network address for incoming gRPC requests
- Registers itself with etcd for service discovery
- Uses `grpc::ServerBuilder` to create and start the server

**Communication Pattern**: 
- **Receives** gRPC calls from other cache nodes, HTTP gateway, and client applications

### 2. Peer (gRPC Client)

**Location**: `include/peer.h`

**Role**: Represents a remote cache node and provides gRPC client interface

**Key Features**:
- Contains `std::unique_ptr<cache::Cache::Stub> stub_` for gRPC communication
- Provides templated `get()` and `set()` methods that make gRPC calls
- Handles connection management, timeouts, and error handling
- Creates gRPC channels using `grpc::CreateCustomChannel()`

**Communication Pattern**:
- **Sends** gRPC calls to remote CacheServer instances
- Used by PeerPicker to communicate with other cache nodes in the cluster

### 3. HttpGateway (gRPC Client)

**Location**: `include/httpgateway.h`, `src/httpgateway.cpp`

**Role**: HTTP-to-gRPC gateway that routes HTTP requests to cache nodes

**Key Features**:
- Provides RESTful HTTP interface for cache operations
- Uses consistent hashing to route requests to appropriate cache nodes
- Creates gRPC channels and stubs to communicate with cache servers
- Method `GetCacheClient()` creates gRPC channels using `grpc::CreateChannel()`

**Communication Pattern**:
- **Receives** HTTP requests from clients
- **Sends** gRPC calls to CacheServer instances based on consistent hashing
- Acts as a protocol bridge between HTTP and gRPC

### 4. PeerPicker (gRPC Client Manager)

**Location**: `include/peerpicker.h`, `src/peerpicker.cpp`

**Role**: Manages a collection of Peer objects for distributed cache operations

**Key Features**:
- Maintains a map of `std::shared_ptr<peer>` instances
- Uses consistent hashing (`ConsistentHash`) to select appropriate peers
- Integrates with etcd for service discovery
- Method `PickPeer()` returns the appropriate Peer for a given key

**Communication Pattern**:
- **Manages** Peer objects that send gRPC calls to remote cache nodes
- Provides load balancing and peer selection for distributed operations

### 5. CacheGroup (High-Level Cache Orchestrator)

**Location**: `include/cachegroup.h`

**Role**: High-level cache management with distributed synchronization

**Key Features**:
- Uses PeerPicker to manage and select peers for cache operations
- Implements cache miss handling with peer fallback
- Provides broadcasting functionality for cache synchronization (Set/Delete operations)
- Integrates SingleFlight pattern to prevent duplicate requests
- Method `LoadFromPeer()` uses gRPC calls through Peer objects

**Communication Pattern**:
- **Uses** PeerPicker and Peer objects to send gRPC calls to remote cache nodes
- **Coordinates** distributed cache operations with automatic peer synchronization
- **Manages** cache miss handling by trying peers first, then fallback handlers

## Communication Flow

### 1. Inter-Node Communication (Cache Node to Cache Node)
```
CacheGroup → PeerPicker → Peer → gRPC → CacheServer
```
- Cache nodes communicate with each other using the Peer class
- Used for data synchronization and distributed cache operations
- Managed by PeerPicker for load balancing
- CacheGroup orchestrates the process with broadcasting for Set/Delete operations

### 2. HTTP Gateway to Cache Nodes
```
HTTP Client → HttpGateway → gRPC → CacheServer
```
- HTTP requests are converted to gRPC calls
- Consistent hashing determines which cache node handles the request
- Provides RESTful API access to the distributed cache

### 3. Client Application to Cache Nodes (Direct)
```
Client App → gRPC → CacheServer
```
- Applications can directly communicate with cache servers using gRPC
- Bypass the HTTP gateway for better performance

### 4. Cache Miss Handling
```
CacheGroup → PeerPicker → Peer → gRPC → CacheServer (peer)
           ↘ (if peer fails) → Cache Miss Handler (fallback)
```
- When a key is not found locally, CacheGroup attempts to load from peers
- Uses SingleFlight pattern to prevent duplicate requests
- Falls back to configured cache miss handler if peers don't have the data

## Service Discovery Integration

All gRPC communication is enhanced by etcd-based service discovery:

1. **CacheServer** registers itself with etcd when starting
2. **HttpGateway** discovers available cache nodes through etcd
3. **PeerPicker** monitors etcd for peer changes and updates the hash ring
4. **Peer** objects are created/destroyed based on service discovery events

## Key Benefits of gRPC Architecture

1. **Type Safety**: Protobuf provides strong typing for all messages
2. **Performance**: Binary serialization and HTTP/2 support
3. **Code Generation**: Automatic client/server code generation
4. **Streaming**: Support for bidirectional streaming (though not used in current implementation)
5. **Language Agnostic**: Can be extended to support clients in different languages

## Network Topology

```
┌─────────────────┐    HTTP     ┌─────────────────┐    gRPC    ┌─────────────────┐
│   HTTP Client   │────────────→│   HttpGateway   │───────────→│   CacheServer   │
└─────────────────┘             └─────────────────┘            └─────────────────┘
                                                                          │
┌─────────────────┐                                                    gRPC
│   CacheGroup    │                                                       │
│     (Node A)    │                                                       ▼
└─────────┬───────┘                                               ┌─────────────────┐
          │                                                       │   CacheServer   │
          ▼                                                       │     (Node B)    │
┌─────────────────┐    gRPC     ┌─────────────────┐              └─────────────────┘
│   PeerPicker    │───────────→│      Peer       │─────────┐
└─────────────────┘             └─────────────────┘         │
          ▲                              │                  │    gRPC
          │                            gRPC                 └───────────→┌─────────────────┐
          │                              │                               │   CacheServer   │
┌─────────────────┐                      ▼                               │     (Node C)    │
│      etcd       │              ┌─────────────────┐                     └─────────────────┘
│ Service Discovery│              │   CacheServer   │
└─────────────────┘              │     (Node B)    │
                                 └─────────────────┘
```

## Complete gRPC Communication Summary

The High-Concurrency Cache System uses gRPC extensively for inter-component communication. Here are **all the components that use gRPC**:

### gRPC Servers:
1. **CacheServer** - Implements the cache gRPC service

### gRPC Clients:
1. **Peer** - Client interface to communicate with remote CacheServer instances
2. **HttpGateway** - Routes HTTP requests to CacheServer instances via gRPC
3. **CacheGroup** - Uses PeerPicker and Peer for distributed cache operations
4. **PeerPicker** - Manages Peer instances that make gRPC calls

### Communication Patterns:
- **Node-to-Node**: CacheGroup → PeerPicker → Peer → gRPC → CacheServer
- **HTTP Gateway**: HttpGateway → gRPC → CacheServer  
- **Direct Client**: Client Application → gRPC → CacheServer
- **Cache Synchronization**: CacheGroup broadcasts Set/Delete operations via gRPC
- **Cache Miss Recovery**: CacheGroup attempts peer recovery via gRPC before fallback

All components integrate with **etcd for service discovery**, enabling dynamic peer management and automatic failover in the distributed cache system.