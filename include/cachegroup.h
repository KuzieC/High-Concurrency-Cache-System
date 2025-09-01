#ifndef CACHE_GROUP_H
#define CACHE_GROUP_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

#include "include/Lru.h"
#include "include/peer.h"
#include "include/singleflight.h"

/**
 * @brief Synchronization operation types for cache broadcasting.
 */
enum class Sync {
    SET,    ///< Set operation - add or update a key-value pair.
    DELETE  ///< Delete operation - remove a key-value pair.
};

// Forward declaration
template<typename Value>
class CacheGroup;

std::unordered_map<std::string, CacheGroup<google::protobuf::Any>> cacheGroups;
std::mutex cacheGroupsMutex;

/**
 * @brief Distributed cache group with peer synchronization and service discovery.
 * 
 * CacheGroup manages a distributed cache that can synchronize with peer nodes
 * using etcd for service discovery. It provides automatic cache miss handling,
 * peer selection, and data synchronization across multiple cache instances.
 *
 * @tparam Value The type of the cache value.
 */
template<typename Value>
class CacheGroup {
public:
    /**
     * @brief Construct a CacheGroup with distributed cache capabilities.
     * 
     * @param groupName The name identifier for this cache group.
     * @param cacheMissHandler Function called when a key is not found locally or in peers.
     * @param etcdServiceName The prefix for service registration in etcd.
     * @param etcdKey The specific key for this cache instance in etcd.
     * @param etcdEndpoints Comma-separated list of etcd endpoints.
     */
    CacheGroup(std::string groupName, std::function<Value(const std::string&)> cacheMissHandler, std::string etcdServiceName, std::string etcdKey, std::string etcdEndpoints)
        : groupName_(groupName),
          cacheMissHandler_(cacheMissHandler),
          isClosed_(false),
          etcdServiceName_(etcdServiceName),
          etcdKey_(etcdKey),
          etcdEndpoints_(etcdEndpoints) {
        cache_ = std::make_unique<Lru<std::string, Value>>();
        peerPicker_ = std::make_unique<PeerPicker>(etcdServiceName, etcdKey, etcdEndpoints);
    }

    /**
     * @brief Move constructor for CacheGroup.
     * 
     * @param other The CacheGroup to move from.
     */
    CacheGroup(const CacheGroup&& other) noexcept {
        groupName_ = std::move(other.groupName_);
        cacheMissHandler_ = std::move(other.cacheMissHandler_);
        isClosed_ = other.isClosed_;
        etcdServiceName_ = other.etcdServiceName_;
        etcdKey_ = other.etcdKey_;
        etcdEndpoints_ = other.etcdEndpoints_;
        cache_ = std::move(other.cache_);
        peerPicker_ = std::move(other.peerPicker_);
    }

    /**
     * @brief Move assignment operator for CacheGroup.
     * 
     * @param other The CacheGroup to move from.
     * @return Reference to this CacheGroup.
     */
    CacheGroup& operator=(const CacheGroup&& other) noexcept {
        if (this != &other) {
            groupName_ = std::move(other.groupName_);
            cacheMissHandler_ = std::move(other.cacheMissHandler_);
            isClosed_ = other.isClosed_;
            etcdServiceName_ = other.etcdServiceName_;
            etcdKey_ = other.etcdKey_;
            etcdEndpoints_ = other.etcdEndpoints_;
            cache_ = std::move(other.cache_);
            peerPicker_ = std::move(other.peerPicker_);
        }
        return *this;
    }

    /**
     * @brief Create or retrieve a CacheGroup instance.
     * 
     * This static method implements a singleton pattern for CacheGroup instances,
     * ensuring only one CacheGroup exists per group name.
     * 
     * @param groupName The name identifier for the cache group.
     * @param cacheMissHandler Function to handle cache misses.
     * @param etcdServiceName The etcd service prefix.
     * @param etcdKey The etcd service key.
     * @param etcdEndpoints The etcd endpoints.
     * @return Reference to the CacheGroup instance.
     */
    static CacheGroup& CreateCacheGroup(const std::string& groupName, 
                                    std::function<Value(const std::string&)> cacheMissHandler, 
                                    const std::string& etcdServiceName, 
                                    const std::string& etcdKey, 
                                    const std::string& etcdEndpoints) {
        std::lock_guard<std::mutex> lock(cacheGroupsMutex);
        auto it = cacheGroups.find(groupName);
        if (it != cacheGroups.end()) {
            return it->second;
        }

        auto [iter, success] = cacheGroups.emplace(groupName, 
                                                   CacheGroup(groupName, cacheMissHandler, etcdServiceName, etcdKey, etcdEndpoints));
        return iter->second;
    } 

    /**
     * @brief Retrieve an existing CacheGroup by name.
     * 
     * @param groupName The name of the cache group to retrieve.
     * @return Pointer to the CacheGroup if found, nullptr otherwise.
     */
    static CacheGroup* GetCacheGroup(const std::string& groupName) {
        std::lock_guard<std::mutex> lock(cacheGroupsMutex);
        auto it = cacheGroups.find(groupName);
        if (it != cacheGroups.end()) {
            return &it->second;
        }
        return nullptr;
    }

    /**
     * @brief Retrieve a value from the cache or peers.
     * 
     * This method first checks the local cache, then attempts to load
     * from peers if not found locally.
     * 
     * @param key The string key to retrieve.
     * @return Optional containing the value if found, empty otherwise.
     */
    std::optional<Value> Get(const std::string& key) {
        auto res = cache_->get(key);
        if(res) {
            return res;
        }

        return LoadFromPeer(key);
    }

    /**
     * @brief Set a key-value pair in the cache with optional broadcasting.
     * 
     * @param key The string key to set.
     * @param value The value to associate with the key.
     * @param needBoardcast Whether to broadcast this update to peers.
     */
    void Set(const std::string& key, const Value& value, bool needBoardcast) {
        cache_->put(key, value);
        if (needBoardcast) {
            BoardCast(key, value, Sync::SET);
        }
    }

    /**
     * @brief Delete a key from the cache with optional broadcasting.
     * 
     * @param key The string key to delete.
     * @param needBoardcast Whether to broadcast this deletion to peers.
     */
    void Del(const std::string& key, bool needBoardcast) {
        cache_->remove(key);
        if (needBoardcast) {
            BoardCast(key, Value(), Sync::DELETE);
        }
    }

    /**
     * @brief Broadcast a cache operation to the appropriate peer.
     * 
     * @param key The string key being operated on.
     * @param value The value (ignored for DELETE operations).
     * @param sync The type of operation (SET or DELETE).
     */
    void BoardCast(const std::string& key, const Value& value, Sync sync) {
        auto peer = peerPicker_->PickPeer(key);
        if (peer) {
            switch (sync) {
                case Sync::SET:
                    peer->Set(key, value);
                    break;
                case Sync::DELETE:
                    peer->Del(key);
                    break;
                default:
                    break;
            }
        }
    }

    /**
     * @brief Load a value from peers using SingleFlight to prevent duplicate requests.
     * 
     * This method uses the SingleFlight pattern to ensure that concurrent requests
     * for the same key result in only one network call to peers.
     * 
     * @param key The string key to load from peers.
     * @return Optional containing the loaded value if successful.
     */
    std::optional<Value> LoadFromPeer(const std::string& key) {
        auto res = singleFlight_.run(key, [&](const std::string& k) -> std::optional<Value> {
            auto peer = peerPicker_->PickPeer(k);
            if (peer) {
                auto value = peer->Get(k);
                if (!value) {
                    spdlog::warn("Failed to load key {} from peer", k);
                    value = cacheMissHandler_(k);
                    if (!value) {
                        spdlog::error("Failed to load key {} from cacheMissHandler", k);
                        return std::nullopt;
                    }
                }
                return value;
            }
            return cacheMissHandler_(k);
        });
        
        if (!res) {
            spdlog::error("Failed to load key {} from singleFlight", key);
            return std::nullopt;
        }
        return res;
    }

private:
    std::unique_ptr<Lru<std::string, Value>> cache_; ///< Local cache instance.
    std::unique_ptr<PeerPicker> peerPicker_; ///< Peer selection and management.
    std::string groupName_; ///< Name of this cache group.
    std::atomic<bool> isClosed_; ///< Flag indicating if the cache group is closed.
    std::function<Value(const std::string&)> cacheMissHandler_; ///< Function to handle cache misses.
    SingleFlight<Value> singleFlight_; ///< SingleFlight instance to prevent duplicate requests.
    std::string etcdServiceName_; ///< etcd service prefix.
    std::string etcdKey_; ///< etcd service key.
    std::string etcdEndpoints_; ///< etcd endpoints configuration.
};
#endif // CACHE_GROUP_H