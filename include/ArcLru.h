#pragma once
#include "LinkedList.h"
#include <unordered_map>
#include <mutex>

/**
 * @brief LRU component for ARC cache, with ghost list support.
 *
 * @tparam Key   The type of the cache key.
 * @tparam Value The type of the cache value.
 */
template<typename Key, typename Value>
class ArcLru {
private:
    int capacity; ///< The maximum number of items the cache can hold.
    int promotionThreshold; ///< The frequency threshold for promotion.
    std::shared_ptr<LinkedList<Key, Value>> list; ///< The main cache list.
    std::unordered_map<Key, std::shared_ptr<Node<Key, Value>>> cacheMap; ///< Map for quick access to main cache nodes.
    std::shared_ptr<LinkedList<Key, Value>> ghostlist; ///< The ghost list for tracking evicted items.
    std::unordered_map<Key, std::shared_ptr<Node<Key, Value>>> ghostMap; ///< Map for quick access to ghost list nodes.
    std::mutex mutex_; ///< Mutex for thread safety.

    /**
     * @brief Update a node's value and frequency, and check promotion.
     * @param node The node to update.
     * @param value The new value.
     * @return True if the node's frequency meets or exceeds the promotion threshold.
     */
    bool updateNodeValue(std::shared_ptr<Node<Key, Value>>& node, const Value& value) {
        node->setValue(value);
        updateNode(node);
        return node->getFrequency() >= promotionThreshold;
    }

    /**
     * @brief Update a node's frequency and move it to the end of the list.
     * @param node The node to update.
     */
    void updateNode(std::shared_ptr<Node<Key, Value>>& node) {
        list->remove(node);
        node->setFrequency(node->getFrequency() + 1);
        list->insertToEnd(node);
    }

    /**
     * @brief Insert a new node into the main cache.
     * @param key The key to insert.
     * @param value The value to insert.
     * @return Always false (for compatibility with ARC logic).
     */
    bool insertNewMain(const Key& key, const Value& value) {
        if(list->getSize() >= capacity) {
            evictMain();
        }
        auto node = std::make_shared<Node<Key, Value>>(key, value);
        list->insertToEnd(node);
        cacheMap[node->getKey()] = node;
        return false;
    }

    /**
     * @brief Remove a node from the main cache.
     * @param node The node to remove.
     */
    void removeMain(std::shared_ptr<Node<Key, Value>>& node) {
        list->remove(node);
        cacheMap.erase(node->getKey());
    }

    /**
     * @brief Evict the least recently used node from the main cache.
     */
    void evictMain() {
        auto node = list->removeFront();

        if(node == nullptr) return; // No node to evict
        cacheMap.erase(node->getKey());
        if(ghostlist->getSize() >= capacity) {
            removeOldestGhost();
        }
        insertGhost(node);
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
     * @brief Remove a node from the ghost list.
     * @param node The node to remove.
     */
    void removeGhost(std::shared_ptr<Node<Key, Value>>& node) {
        ghostlist->remove(node);
        ghostMap.erase(node->getKey());
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
     * @brief Construct an ArcLru cache with a given capacity and promotion threshold.
     * @param cap The maximum number of items the cache can hold.
     * @param threshold The frequency threshold for promotion.
     */
    ArcLru(int cap, int threshold) : capacity(cap), promotionThreshold(threshold) {
        list = std::make_shared<LinkedList<Key, Value>>();
        ghostlist = std::make_shared<LinkedList<Key, Value>>();
    }

    /**
     * @brief Check if a key exists in the ghost list and remove it if found.
     * @param key The key to check.
     * @return True if the key was found and removed, false otherwise.
     */
    bool checkGhost(const Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(ghostMap.find(key) != ghostMap.end()) {
            auto node = ghostMap[key];
            removeGhost(node);
            return true;
        }
        return false;
    }

    /**
     * @brief Increase the cache capacity by one.
     */
    void increaseCapacity(){
        std::lock_guard<std::mutex> lock(mutex_);
        capacity++;
    }

    /**
     * @brief Decrease the cache capacity by one, evicting if necessary.
     * @return True if the capacity was decreased, false otherwise.
     */
    bool decreaseCapacity(){
        std::lock_guard<std::mutex> lock(mutex_);
        if(capacity > 1) {
            capacity--;
            if(list->getSize() > capacity) {
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
     * @brief Insert or update a value in the cache, with promotion flag.
     * @param key   The key to insert or update.
     * @param value The value to associate with the key.
     * @param flag  Output flag indicating if the node was promoted.
     */
    void put(const Key key, const Value value, bool& flag)  {
        std::lock_guard<std::mutex> lock(mutex_);
        if(cacheMap.find(key) != cacheMap.end()) {
            flag = updateNodeValue(cacheMap[key], value);
        }
        else if(ghostMap.find(key) != ghostMap.end()) {
            auto node = ghostMap[key];
            removeGhost(node);
        }
        flag = insertNewMain(key, value);
    }

    /**
     * @brief Retrieve a value from the cache, with promotion flag.
     * @param key   The key to look up.
     * @param value Output parameter for the value.
     * @param flag  Output flag indicating if the node was promoted.
     * @return True if the key was found, false otherwise.
     */
    bool get(const Key key, Value& value, bool& flag ) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(cacheMap.find(key) != cacheMap.end()) {
            auto node = cacheMap[key];
            value = node->getValue();
            flag = updateNodeValue(node, value);
            return true;
        }
        else if(ghostMap.find(key) != ghostMap.end()) {
            auto node = ghostMap[key];
            value = node->getValue();
            removeGhost(node);
            insertNewMain(key, value);
        }
        return false;
    }

};