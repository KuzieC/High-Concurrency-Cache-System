#pragma once
#include "LinkedList.h"
#include <unordered_map>
#include <mutex>

/**
 * @brief LFU component for ARC cache, with ghost list support.
 *
 * @tparam Key   The type of the cache key.
 * @tparam Value The type of the cache value.
 */
template<typename Key, typename Value>
class ArcLfu {
private:
    int capacity;
    int promotionThreshold;
    std::unordered_map<Key, std::shared_ptr<Node<Key, Value>>> cacheMap;
    std::unordered_map<Key, std::shared_ptr<Node<Key, Value>>> ghostMap;
    std::shared_ptr<LinkedList<Key, Value>> ghostlist;
    std::unordered_map<int, std::unique_ptr<LinkedList<Key, Value>>> freqList;
    std::mutex mutex_;
    int minFreq;

    /**
     * @brief Update the minimum frequency in the frequency list.
     */
    void updateMinFreq() {
        minFreq = INT_MAX;
        for(auto it = freqList.begin(); it != freqList.end(); ++it) {
            if(it->second->isEmpty()) {
                continue;
            }
            minFreq = std::min(minFreq, it->first);
        }
    }

    /**
     * @brief Update a node's frequency and move it to the correct list.
     * @param node The node to update.
     */
    void updateNode(std::shared_ptr<Node<Key, Value>>& node) {
        freqList[node->getFrequency()]->remove(node);
        node->setFrequency(node->getFrequency() + 1);
        if(freqList.find(node->getFrequency()) == freqList.end()) {
            freqList[node->getFrequency()] = std::make_unique<LinkedList<Key, Value>>();
        }
        freqList[node->getFrequency()]->insertToEnd(node);
    }

    /**
     * @brief Insert a new node into the main cache.
     * @param key The key to insert.
     * @param value The value to insert.
     */
    void insertNewMain(const Key& key, const Value& value) {
        if(cacheMap.size() >= capacity) {
            evictMain();
        }
        auto node = std::make_shared<Node<Key, Value>>(key, value);
        freqList[1]->insertToEnd(node);
        cacheMap[node->getKey()] = node;
        minFreq = 1;
    }

    /**
     * @brief Evict the least frequently used node from the main cache.
     */
    void evictMain() {
        auto node = freqList[minFreq]->removeFront();
        if(node == nullptr) return; // No node to evict
        cacheMap.erase(node->getKey());
        if(ghostlist->getSize() > capacity) {
            removeOldestGhost();
        }
        insertGhost(node);
    }

    /**
     * @brief Remove a node from the ghost list.
     * @param node The node to remove.
     */
    void removeGhost(std::shared_ptr<Node<Key, Value>>& node) {
        ghostlist->remove(node);
        ghostMap.erase(node->getKey());
    }

    /**
     * @brief Insert a node into the ghost list.
     * @param node The node to insert.
     */
    void insertGhost(std::shared_ptr<Node<Key, Value>> node) {
        ghostlist->insertToEnd(node);
        ghostMap[node->getKey()] = node;
        node->setFrequency(1);
    }

    /**
     * @brief Remove the oldest node from the ghost list.
     */
    void removeOldestGhost() {
        if(ghostlist->isEmpty()) return; // No ghost node to remove
        auto node = ghostlist->removeFront();
        ghostMap.erase(node->getKey());
    }
public:
    /**
     * @brief Construct an ArcLfu cache with a given capacity and promotion threshold.
     * @param cap The maximum number of items the cache can hold.
     * @param threshold The frequency threshold for promotion.
     */
    ArcLfu(int cap, int threshold) : capacity(cap), promotionThreshold(threshold) {
        ghostlist = std::make_shared<LinkedList<Key, Value>>();
        minFreq = 1;
        freqList[minFreq] = std::make_unique<LinkedList<Key, Value>>();
    }

    /**
     * @brief Increase the cache capacity by one.
     */
    void increaseCapacity() {
        std::lock_guard<std::mutex> lock(mutex_);
        capacity++;
    }

    /**
     * @brief Decrease the cache capacity by one, evicting if necessary.
     * @return True if the capacity was decreased, false otherwise.
     */
    bool decreaseCapacity() {
        std::lock_guard<std::mutex> lock(mutex_);
        // if capacity reach 0, we can't decrease it anymore
        if(capacity > 1) {
            capacity--;
            if(cacheMap.size() > capacity) {
                evictMain();
            }
            if(ghostlist->getSize() > capacity) {
                removeOldestGhost();
            }
            return true;
        }
        return false;
    }

    /**
     * @brief Retrieve a value from the cache.
     * @param key The key to look up.
     * @return The value associated with the key, or a default value if not found.
     */
    Value get(const Key& key){
        Value value{};
        if(get(key, value)) {
            return value;
        }
        return Value(); // Return default value if not found
    }

    /**
     * @brief Check if a key exists in the ghost list and remove it if found.
     * @param key The key to check.
     * @return True if the key was found and removed, false otherwise.
     */
    bool checkGhost(const Key& key){
        std::lock_guard<std::mutex> lock(mutex_);
        if(ghostMap.find(key) != ghostMap.end()) {
            auto node = ghostMap[key];
            removeGhost(node);
            return true;
        }
        return false;
    }

    /**
     * @brief Retrieve a value from the cache, with output parameter.
     * @param key The key to look up.
     * @param value Output parameter for the value.
     * @return True if the key was found, false otherwise.
     */
    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(cacheMap.find(key) != cacheMap.end()) {
            auto node = cacheMap[key];
            value = node->getValue();
            updateNode(node);
            if(node->getFrequency() - 1 == minFreq && freqList[minFreq]->isEmpty()) {
                updateMinFreq();
            }
            return true;
        } else if(ghostMap.find(key) != ghostMap.end()) {
            auto node = ghostMap[key];
            value = node->getValue();
            removeGhost(node);
            insertNewMain(key, value);
        }
        return false;
    }
    
    /**
     * @brief Insert or update a value in the cache.
     * @param key   The key to insert or update.
     * @param value The value to associate with the key.
     */
    void put(const Key key, const Value value)  {
        std::lock_guard<std::mutex> lock(mutex_);
        if(cacheMap.find(key) != cacheMap.end()) {
            auto node = cacheMap[key];
            node->setValue(value);
            updateNode(node);
            if(node->getFrequency() - 1 == minFreq && freqList[minFreq]->isEmpty()) {
                updateMinFreq();
            }
            return;
        } else if(ghostMap.find(key) != ghostMap.end()) {
            auto node = ghostMap[key];
            removeGhost(node);
            insertNewMain(key, value);
            return;
        }
        insertNewMain(key, value);
    }
};