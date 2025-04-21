#pragma once

#include "Cache.h"
#include "Node.h"
#include "LinkedList.h"
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <vector>
#include <algorithm>

/**
 * @brief Least Recently Used (LRU) cache implementation.
 *
 * @tparam Key   The type of the cache key.
 * @tparam Value The type of the cache value.
 */
template<typename Key, typename Value>
class Lru : public Cache<Key, Value> {
public:
    using LruNode = Node<Key, Value>;
    using LruNodePtr = std::shared_ptr<LruNode>;
    using LruMap = std::unordered_map<Key, LruNodePtr>;

    /**
     * @brief Construct an LRU cache with a given capacity.
     * @param cap The maximum number of items the cache can hold.
     */
    Lru(int cap) : capacity(cap), size(0) {
        list = std::make_shared<LinkedList<Key, Value>>();
    }
    
    /**
     * @brief Insert or update a value in the cache.
     * @param key   The key to insert or update.
     * @param value The value to associate with the key.
     */
    virtual void put(const Key key, const Value value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cacheMap.find(key) != cacheMap.end()) {
            auto node = cacheMap[key];
            list->remove(node);
        } else {
            if (size >= capacity) {
                removelru();
            }
        }
        insertBack(key, value);
    }
    
    /**
     * @brief Retrieve a value from the cache.
     * @param key The key to look up.
     * @return The value associated with the key, or a default value if not found.
     */
    virtual Value get(const Key key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cacheMap.find(key) != cacheMap.end()) {
            auto node = cacheMap[key];
            Value res = node->getValue();
            list->remove(node);
            insertBack(node);
            return res;
        }
        return Value();
    }
    
    /**
     * @brief Remove a key from the cache.
     * @param key The key to remove.
     */
    void remove(const Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cacheMap.find(key) != cacheMap.end()) {
            auto node = cacheMap[key];
            list->remove(node);
        }
    }

    /**
     * @brief Check if a key exists in the cache.
     * @param key The key to check.
     * @return True if the key exists, false otherwise.
     */
    bool contains(const Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return cacheMap.find(key) != cacheMap.end();
    }

    /**
     * @brief Get the frequency of a key in the cache.
     * @param key The key to check.
     * @return The frequency of the key, or 0 if not found.
     */
    int getFrequency(const Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cacheMap.find(key) != cacheMap.end()) {
            return cacheMap[key]->getFrequency();
        }
        return 0;
    }

    /**
     * @brief Set the frequency of a key in the cache.
     * @param key The key to update.
     * @param freq The new frequency value.
     */
    void setFrequency(const Key key, int freq) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cacheMap.find(key) != cacheMap.end()) {
            cacheMap[key]->setFrequency(freq);
        }
    }
private:
    std::shared_ptr<LinkedList<Key, Value>> list; ///< The main cache list.
    int size; ///< The current number of items in the cache.
    int capacity; ///< The maximum capacity of the cache.
    LruMap cacheMap; ///< Key-node mapping for fast lookup.
    std::mutex mutex_; ///< Mutex for thread safety.
    
    /**
     * @brief Insert a new node at the back of the list and update the cache map.
     * @param key The key to insert.
     * @param value The value to insert.
     * @return The pointer to the new node.
     */
    LruNodePtr insertBack(const Key& key, const Value& value) {
        ++size;
        auto newNode = std::make_shared<LruNode>(key, value);
        list->insertToEnd(newNode);
        cacheMap[key] = newNode;
        return newNode;
    }
    
    /**
     * @brief Insert an existing node at the back of the list and update the cache map.
     * @param node The node to insert.
     */
    void insertBack(LruNodePtr node) {
        ++size;
        list->insertToEnd(node);
        cacheMap[node->getKey()] = node;
    }

    /**
     * @brief Remove the least recently used node from the cache.
     */
    void removelru() {
        auto node = list->removeFront();
        cacheMap.erase(node->getKey());
        --size;
    }
};

template<typename Key, typename Value>
class LruK : public Lru<Key, Value> {
public:
    /**
     * @brief Construct an LRU-K cache with a given capacity, cold cache size, and promotion threshold.
     * @param cap The maximum number of items the cache can hold.
     * @param coldCacheSize The size of the cold cache.
     * @param kVal The promotion threshold for moving items from the cold cache to the main cache.
     */
    LruK(int cap, int coldCacheSize, int kVal = 1) 
    : Lru<Key, Value>(cap), 
    promotionThresholds(kVal), 
    coldCache(std::make_unique<Lru<Key, Value>>(coldCacheSize)){} // store cold entries

    /**
     * @brief Insert or update a value in the LRU-K cache.
     * @param key   The key to insert or update.
     * @param value The value to associate with the key.
     */
    void put(const Key key, const Value value) override {
        if(Lru<Key, Value>::contains(key)){
            Lru<Key, Value>::put(key, value);
            return;
        }
        int KeyFreq = coldCache->getFrequency(key);
        if(KeyFreq >= promotionThresholds){
            coldCache->remove(key);
            Lru<Key,Value>::put(key, value);
        }
        else {
            coldCache->put(key, value);
            coldCache->setFrequency(key, KeyFreq + 1);
        }
    }
    
    /**
     * @brief Retrieve a value from the LRU-K cache.
     * @param key The key to look up.
     * @return The value associated with the key, or a default value if not found.
     */
    Value get(const Key key) override {
        if (Lru<Key, Value>::contains(key)) {
            return Lru<Key, Value>::get(key);
        } else if (coldCache->contains(key)) {
            int keyFreq = coldCache->getFrequency(key);
            Value val = coldCache->get(key);
            if (keyFreq >= promotionThresholds) {
                coldCache->remove(key);
                Lru<Key, Value>::put(key, val);
                return Lru<Key, Value>::get(key);
            } else {
                coldCache->setFrequency(key, keyFreq + 1);
                return val;
            }
        }
        return Value();
    }
    

private:
    int promotionThresholds; ///< The promotion threshold for moving items from the cold cache to the main cache.
    std::unique_ptr<Lru<Key, Value>> coldCache; ///< The cold cache for storing less frequently accessed items.
};

template<typename Key, typename Value>
class HashLruK {
public:
    /**
     * @brief Construct a Hash-based LRU-K cache with a given capacity, slice count, cold cache size, and promotion threshold.
     * @param cap The maximum number of items the cache can hold.
     * @param slice The number of slices to divide the cache into.
     * @param coldCacheSize The size of the cold cache.
     * @param promotionThreshold The promotion threshold for moving items from the cold cache to the main cache.
     */
    HashLruK(int cap, int slice, int coldCacheSize, int promotionThreshold)
      : sliceNum(slice), capacity(cap), promotionThreshold(promotionThreshold)
    {
        int sliceSize = capacity / sliceNum;
        lruKShards.reserve(sliceNum);
        for (int i = 0; i < sliceNum; ++i) {
            lruKShards.emplace_back(std::make_unique<LruK<Key, Value>>(sliceSize, coldCacheSize, promotionThreshold));
        }
    }
    
    /**
     * @brief Insert or update a value in the Hash-based LRU-K cache.
     * @param key   The key to insert or update.
     * @param value The value to associate with the key.
     */
    void put(const Key key, const Value value) {
        size_t idx = hash(key);
        lruKShards[idx]->put(key, value);
    }
    
    /**
     * @brief Retrieve a value from the Hash-based LRU-K cache.
     * @param key The key to look up.
     * @return The value associated with the key, or a default value if not found.
     */
    Value get(const Key key) {
        size_t idx = hash(key);
        return lruKShards[idx]->get(key);
    }
    
private:
    int capacity; ///< The maximum capacity of the cache.
    int sliceNum; ///< The number of slices in the cache.
    int promotionThreshold; ///< The promotion threshold for moving items from the cold cache to the main cache.
    std::vector<std::unique_ptr<LruK<Key, Value>>> lruKShards; ///< The shards of the LRU-K cache.
    
    /**
     * @brief Hash function to determine the shard index for a given key.
     * @param key The key to hash.
     * @return The index of the shard.
     */
    size_t hash(const Key& key) {
        return std::hash<Key>()(key) % sliceNum;
    }
};
