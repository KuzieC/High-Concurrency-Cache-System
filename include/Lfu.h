#pragma once

#include "Cache.h"
#include "Node.h"
#include "LinkedList.h"
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <vector>
#include <algorithm> // for std::min

template<typename Key, typename Value>
class AvgLfu; // Forward declaration

template<typename Key, typename Value>
class Lfu : public Cache<Key, Value> {
public:
    /**
     * @brief Construct an LFU cache with a given capacity.
     * @param capacity The maximum number of items the cache can hold.
     */
    Lfu(int capacity) : size(0), minFreq(1), cap(capacity) {
        freqList[minFreq] = std::make_unique<LinkedList<Key, Value>>();
    }

    /**
     * @brief Insert or update a value in the cache.
     * @param key   The key to insert or update.
     * @param value The value to associate with the key.
     */
    void put(const Key key, const Value value) override {
        if (cap <= 0) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (mp.find(key) != mp.end()) {
            updateNode(mp[key]);
            mp[key]->setValue(value);
            updateMinFreq();
            return;
        }
        if (size == cap) {
            removeLFU();
            size--;
        }
        auto newNode = std::make_shared<Node<Key, Value>>(key, value);
        insertNewNode(newNode);
        mp[key] = newNode;
        size++;
    }

    /**
     * @brief Retrieve a value from the cache.
     * @param key The key to look up.
     * @return The value associated with the key, or a default value if not found.
     */
    Value get(const Key key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (mp.find(key) == mp.end()) return Value();
        auto node = mp[key];
        updateNode(node);
        if (node->getFrequency() - 1 == minFreq && freqList[minFreq]->isEmpty()) {
            minFreq++;
        }
        //override the GetHook function in HashAvgLfu class
        GetHook();
        return node->getValue();
    }
protected:
    /**
     * @brief Hook for custom logic on get (for derived classes).
     */
    virtual void GetHook(){};
    /**
     * @brief Hook for custom logic on LFU removal (for derived classes).
     * @param fre The frequency of the removed node.
     */
    virtual void removeLFUHook(int fre){};
private:
    int size; ///< The current number of items in the cache.
    int minFreq; ///< The current minimum frequency in the cache.
    int cap; ///< The maximum capacity of the cache.
    std::mutex mutex_; ///< Mutex for thread safety.
    std::unordered_map<Key, std::shared_ptr<Node<Key, Value>>> mp; ///< Key-node mapping for fast lookup.
    std::unordered_map<int, std::unique_ptr<LinkedList<Key, Value>>> freqList; ///< Frequency-list mapping for LFU.

    /**
     * @brief Update a node's frequency and move it to the correct list.
     * @param node The node to update.
     */
    void updateNode(std::shared_ptr<Node<Key, Value>>& node) {
        removeNode(node);
        node->setFrequency(node->getFrequency() + 1);
        insertNode(node);
    }

    /**
     * @brief Remove the least frequently used node from the cache.
     */
    void removeLFU() {
        auto node = freqList[minFreq]->removeFront();
        removeLFUHook(node->getFrequency());
        mp.erase(node->getKey());
    }

    /**
     * @brief Remove a node from its frequency list.
     * @param node The node to remove.
     */
    void removeNode(std::shared_ptr<Node<Key, Value>>& node) {
        int freq = node->getFrequency();
        freqList[freq]->remove(node);
    }

    /**
     * @brief Insert a node into its frequency list.
     * @param node The node to insert.
     */
    void insertNode(std::shared_ptr<Node<Key, Value>>& node) {
        if (freqList.find(node->getFrequency()) == freqList.end()) {
            freqList[node->getFrequency()] = std::make_unique<LinkedList<Key, Value>>();
        }
        freqList[node->getFrequency()]->insertToEnd(node);
    }

    /**
     * @brief Insert a new node into the cache and frequency list.
     * @param node The node to insert.
     */
    void insertNewNode(std::shared_ptr<Node<Key, Value>>& node) {
        if (freqList.find(node->getFrequency()) == freqList.end()) {
            freqList[node->getFrequency()] = std::make_unique<LinkedList<Key, Value>>();
        }
        freqList[node->getFrequency()]->insertToEnd(node);
        minFreq = 1;
    }

    /**
     * @brief Update the minimum frequency after node removal or modification.
     * 
     * The minimum frequency might not be continuous, so we need to find 
     * the actual minimum frequency in the frequency list and update it 
     * when we remove the LFU node.
     */
    void updateMinFreq() {
        minFreq = INT_MAX;
        for (auto it = freqList.begin(); it != freqList.end(); ++it) {
            if (it->second->isEmpty()) {
                continue;
            }
            minFreq = std::min(minFreq, it->first);
        }
    }

    friend class AvgLfu<Key, Value>;
};

/**
 * @brief LFU cache with average frequency control for adaptive eviction.
 *
 * @tparam Key   The type of the cache key.
 * @tparam Value The type of the cache value.
 */
template<typename Key, typename Value>
class AvgLfu : public Lfu<Key, Value> {
private: 
    int averageFreq;
    int totalFreq;
    int maximumFreq;

    /**
     * @brief Increase the total frequency and update the average frequency.
     */
    void increaseTotalFreq() {
        totalFreq++;
        averageFreq = totalFreq / Lfu<Key, Value>::mp.size();
        if (averageFreq > maximumFreq) {
            handleFreq();
        }
    }

    /**
     * @brief Decrease the total frequency and update the average frequency.
     * @param num The frequency to subtract.
     */
    void decreaseTotalFreq(int num) {
        totalFreq -= num;
        averageFreq = totalFreq / Lfu<Key, Value>::mp.size();
    }

    /**
     * @brief Handle frequency adjustment when average frequency exceeds the maximum.
     */
    void handleFreq() {
        totalFreq = 0;
        for (auto it = Lfu<Key, Value>::mp.begin(); it != Lfu<Key, Value>::mp.end(); ++it) {
            auto node = it->second;
            Lfu<Key, Value>::removeNode(node);
            node->setFrequency(std::max(1, node->getFrequency() - maximumFreq));
            totalFreq += node->getFrequency();
            Lfu<Key, Value>::insertNode(node);
        }
        averageFreq = totalFreq / Lfu<Key, Value>::mp.size();
        Lfu<Key, Value>::updateMinFreq();
    }
        
protected:
    /**
     * @brief Hook for custom logic on get (for derived classes).
     */
    virtual void GetHook() override {
        increaseTotalFreq();
    }
    
    /**
     * @brief Hook for custom logic on LFU removal (for derived classes).
     * @param fre The frequency of the removed node.
     */
    virtual void removeLFUHook(int fre) override {
        decreaseTotalFreq(fre);
    }

public:
    /**
     * @brief Construct an AvgLfu cache with a given capacity and maximum frequency.
     * @param cap The maximum number of items the cache can hold.
     * @param maxFreq The maximum average frequency threshold.
     */
    AvgLfu(int cap, int maxFreq = 10) : Lfu<Key, Value>(cap), maximumFreq(maxFreq) {
        averageFreq = 0;
        totalFreq = 0;
    }

};

/**
 * @brief Sharded LFU cache with average frequency control.
 *
 * @tparam Key   The type of the cache key.
 * @tparam Value The type of the cache value.
 */
template<typename Key, typename Value>
class HashAvgLfu {
private:
    int sliceNum;
    int sliceSize;
    int capacity;
    std::vector<std::unique_ptr<AvgLfu<Key, Value>>> avgLfuShards;

    /**
     * @brief Hash function to determine the shard index for a given key.
     * @param key The key to hash.
     * @return The index of the shard.
     */
    size_t hash(const Key& key) {
        return std::hash<Key>()(key) % sliceNum;
    }
public:
    /**
     * @brief Construct a HashAvgLfu cache with a given capacity and number of slices.
     * @param cap The maximum number of items the cache can hold.
     * @param slice The number of slices (shards) in the cache.
     * @param maximumAverageThreshold The maximum average frequency threshold for each shard.
     */
    HashAvgLfu(int cap, int slice, int maximumAverageThreshold = 10) :sliceNum(slice), capacity(cap) {
        sliceSize = capacity / sliceNum;
        avgLfuShards.reserve(sliceNum);
        for (int i = 0; i < sliceNum; ++i) {
            avgLfuShards.emplace_back(std::make_unique<AvgLfu<Key, Value>>(sliceSize,maximumAverageThreshold));
        }
    }

    /**
     * @brief Insert or update a value in the cache.
     * @param key   The key to insert or update.
     * @param value The value to associate with the key.
     */
    void put(const Key key, const Value value) {
        size_t idx = hash(key);
        avgLfuShards[idx]->put(key, value);
    }

    /**
     * @brief Retrieve a value from the cache.
     * @param key The key to look up.
     * @return The value associated with the key, or a default value if not found.
     */
    Value get(const Key key) {
        size_t idx = hash(key);
        return avgLfuShards[idx]->get(key);
    }
};