#pragma once
#include "LinkedList.h"
#include <unordered_map>
#include <mutex>

template<typename Key, typename Value>
class ArcLru {
private:
    int capacity;
    int promotionThreshold;
    std::shared_ptr<LinkedList<Key, Value>> list;
    std::unordered_map<Key, std::shared_ptr<Node<Key, Value>>> cacheMap;
    std::shared_ptr<LinkedList<Key, Value>> ghostlist;
    std::unordered_map<Key, std::shared_ptr<Node<Key, Value>>> ghostMap;
    std::mutex mutex_;

    bool updateNodeValue(std::shared_ptr<Node<Key, Value>>& node, const Value& value) {
        node->setValue(value);
        updateNode(node);
        return node->getFrequency() >= promotionThreshold;
    }

    void updateNode(std::shared_ptr<Node<Key, Value>>& node) {
        list->remove(node);
        node->setFrequency(node->getFrequency() + 1);
        list->insertToEnd(node);
    }

    bool insertNewMain(const Key& key, const Value& value) {
        if(list->getSize() >= capacity) {
            evictMain();
        }
        auto node = std::make_shared<Node<Key, Value>>(key, value);
        list->insertToEnd(node);
        cacheMap[node->getKey()] = node;
        return false;
    }

    void removeMain(std::shared_ptr<Node<Key, Value>>& node) {
        list->remove(node);
        cacheMap.erase(node->getKey());
    }

    void evictMain() {
        auto node = list->removeFront();

        if(node == nullptr) return; // No node to evict
        cacheMap.erase(node->getKey());
        if(ghostlist->getSize() >= capacity) {
            removeOldestGhost();
        }
        insertGhost(node);
    }

    void insertGhost(std::shared_ptr<Node<Key, Value>> node) {
        ghostlist->insertToEnd(node);
        ghostMap[node->getKey()] = node;
        node->setFrequency(1);
    }

    void removeGhost(std::shared_ptr<Node<Key, Value>>& node) {
        ghostlist->remove(node);
        ghostMap.erase(node->getKey());
    }

    void removeOldestGhost() {
        if(ghostlist->isEmpty()) return; // No ghost node to remove
        auto node = ghostlist->removeFront();
        ghostMap.erase(node->getKey());
    }
    


public:
    ArcLru(int cap, int threshold) : capacity(cap), promotionThreshold(threshold) {
        list = std::make_shared<LinkedList<Key, Value>>();
        ghostlist = std::make_shared<LinkedList<Key, Value>>();
    }

    bool checkGhost(const Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(ghostMap.find(key) != ghostMap.end()) {
            auto node = ghostMap[key];
            removeGhost(node);
            return true;
        }
        return false;
    }

    void increaseCapacity(){
        std::lock_guard<std::mutex> lock(mutex_);
        capacity++;
    }

    bool decreaseCapacity(){
        std::lock_guard<std::mutex> lock(mutex_);
        if(capacity > 0) {
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