#pragma once
#include "Cache.h"
#include "LinkedList.h"

template<typename Key, typename Value>
class ArcLru : public Cache<Key, Value> {
private:
    int capacity;
    int promotionThreshold;
    std::shared_ptr<LinkedList<Key, Value>> list;
    std::unordered_map<Key, std::shared_ptr<Node<Key, Value>>> cacheMap;
    std::shared_ptr<Node<Key, Value>> ghostlist;
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
        return node->getFrequency() >= promotionThreshold;
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
        auto node = ghostlist->removeFront();
        ghostMap.erase(node->getKey());
    }
    


public:
    ArcLru(int cap, int threshold) : capacity(cap), promotionThreshold(threshold) {
        list = std::make_shared<LinkedList<Key, Value>>();
        ghostlist = std::make_shared<LinkedList<Key, Value>>();
    }

    bool checkGhost(const Key& key) {
        if(ghostMap.find(key) != ghostMap.end()) {
            return true;
        }
        return false;
    }

    void increaseCapacity(){
        std::Lock_guard<std::mutex> lock(mutex_);
        capacity++;
    }

    void decreaseCapacity(){
        std::Lock_guard<std::mutex> lock(mutex_);
        if(capacity > 0) {
            capacity--;
        }
    }

    bool put(const Key key, const Value value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if(cacheMap.find(key) != cacheMap.end()) {
            return updateNodeValue(cacheMap[key], value);
        }
        return insertNewMain(key, value);
    }

    bool get(const Key key, Value& value, bool& flag ) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if(cacheMap.find(key) != cacheMap.end()) {
            auto node = cacheMap[key];
            value = node->getValue();
            flag = updateNodeValue(node, value);
            return true;
        }
        return false;
    }

};