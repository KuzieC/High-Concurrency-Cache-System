#ifndef CACHE_GROUP_H
#define CACHE_GROUP_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "include/Lru.h"
#include "include/peer.h"
#include "include/singleflight.h"

enum class Sync {
    SET,
    DELETE
};

std::unordered_map<std::string,CacheGroup> cacheGroups;
std::mutex cacheGroupsMutex;

template<typename Key, typename Value>
class CacheGroup {
public:
    CacheGroup(std::string groupName, std::function<Value(const Key&)> cacheMissHandler, std::string etcdPrefix, std::string etcdKey, std::string etcdEndpoints)
        : groupName_(groupName),
          cacheMissHandler_(cacheMissHandler),
          isClosed_(false),
          etcdPrefix_(etcdPrefix),
          etcdKey_(etcdKey),
          etcdEndpoints_(etcdEndpoints) {
        cache_ = std::make_unique<Lru<Key, Value>>();
        peerPicker_ = std::make_unique<PeerPicker>(etcdPrefix, etcdKey, etcdEndpoints);
    }

    CacheGroup(const CacheGroup&& other) noexcept {
        groupName_ = std::move(other.groupName_);
        cacheMissHandler_ = std::move(other.cacheMissHandler_);
        isClosed_ = other.isClosed_;
        etcdPrefix_ = other.etcdPrefix_;
        etcdKey_ = other.etcdKey_;
        etcdEndpoints_ = other.etcdEndpoints_;
        cache_ = std::move(other.cache_);
        peerPicker_ = std::move(other.peerPicker_);
    }

    CacheGroup& operator=(const CacheGroup&& other) noexcept {
        if (this != &other) {
            groupName_ = std::move(other.groupName_);
            cacheMissHandler_ = std::move(other.cacheMissHandler_);
            isClosed_ = other.isClosed_;
            etcdPrefix_ = other.etcdPrefix_;
            etcdKey_ = other.etcdKey_;
            etcdEndpoints_ = other.etcdEndpoints_;
            cache_ = std::move(other.cache_);
            peerPicker_ = std::move(other.peerPicker_);
        }
        return *this;
    }

    static CacheGroup& CreateCacheGroup(const std::string& groupName, 
                                    std::function<Value(const Key&)> cacheMissHandler, 
                                    const std::string& etcdPrefix, 
                                    const std::string& etcdKey, 
                                    const std::string& etcdEndpoints) {
        std::lock_guard<std::mutex> lock(cacheGroupsMutex);
        auto it = cacheGroups.find(groupName);
        if (it != cacheGroups.end()) {
            return it->second;
        }

        auto [iter, success] = cacheGroups.emplace(groupName, 
                                                   CacheGroup(groupName, cacheMissHandler, etcdPrefix, etcdKey, etcdEndpoints));
        return iter->second;
    } 

    static CacheGroup* GetCacheGroup(const std::string& groupName) {
        std::shared_mutex lock(cacheGroupsMutex);
        auto it = cacheGroups.find(groupName);
        if (it != cacheGroups.end()) {
            return &it->second;
        }
        return nullptr;
    }

    template<typename K, typename V>
    std::optional<V> Get(const K& key){
        auto res = cache_->get(key);
        if(res) {
            return res;
        }

        return LoadFromPeer(key);
    }

    template<typename K, typename V>
    void Set(const K& key, const V& value, bool needBoardcast) {
        cache_->put(key, value);
        if (needBoardcast) {
            BoardCast(key, value, Sync::SET);
        }
    }

    template<typename K, typename V>
    void Del(const K& key, bool needBoardcast) {
        cache_->remove(key);
        if (needBoardcast) {
            BoardCast(key, V(), Sync::DELETE);
        }
    }

    template<typename K, typename V>
    void BoardCast(const K& key, const V& value, Sync sync) {
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

    template<typename K, typename V>
    std::optional<V> LoadFromPeer(const K& key) {
        auto res = singleFlight_.run(key, [&]) {
            auto peer = peerPicker_->PickPeer(key);
            if( peer) {
                auto value = peer->Get(key);
                if(!value){
                    spdlog::warn("Failed to load key {} from peer", key);
                    value = cacheMissHandler_(key);
                    if(!value) {
                        spdlog::error("Failed to load key {} from cacheMissHandler", key);
                        return std::nullopt;
                    }
                }
            }
            return value;
        };
        if(!res) {
            spdlog::error("Failed to load key {} from singleFlight", key);
            return std::nullopt;
        }
        return res;
    }

private:
    std::unique_ptr<Lru<Key, Value>> cache_;
    std::unique_ptr<PeerPicker> peerPicker_;
    std::string groupName_;
    std::atomic<bool> isClosed_;
    std::function<Value(const Key&)> cacheMissHandler_;
    SingleFlight<Value> singleFlight_;
    std::string etcdPrefix_;
    std::string etcdKey_;
    std::string etcdEndpoints_;
};
#endif // CACHE_GROUP_H