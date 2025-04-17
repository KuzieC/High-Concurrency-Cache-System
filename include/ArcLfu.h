#pragma once
#include "Cache.h"
#include "LinkedList.h"

template<typename Key, typename Value>
class ArcLfu : public Cache<Key, Value> {
private:
    int capacity;
    int promotionThreshold;
    std::unordered_map<Key, std::shared_ptr<Node<Key, Value>>> cacheMap;
    std::unordered_map<Key, std::shared_ptr<Node<Key, Value>>> ghostMap;
    std::shared_ptr<LinkedList<Key, Value>> ghostlist;
    std::unordered_map<int, std::unique_ptr<LinkedList<Key, Value>>> freqList;
    std::mutex mutex_;
    int minFreq;

    void updateMinFreq() {
        minFreq = INT_MAX;
        for(auto it = freqList.begin(); it != freqList.end(); ++it) {
            if(it->second->isEmpty()) {
                continue;
            }
            minFreq = std::min(minFreq, it->first);
        }
    }

    void updateNode(std::shared_ptr<Node<Key, Value>>& node) {
        freqList[node->getFrequency()]->remove(node);
        node->setFrequency(node->getFrequency() + 1);
        freqList[node->getFrequency()]->insertToEnd(node);
    }

    void insertNewMain(const Key& key, const Value& value) {
        if(cacheMap.size() >= capacity) {
            evictMain();
        }
        auto node = std::make_shared<Node<Key, Value>>(key, value);
        freqList[1]->insertToEnd(node);
        cacheMap[node->getKey()] = node;
        minFreq = 1;
    }

    void evictMain() {
        auto node = freqList[minFreq]->removeFront();
        if(node == nullptr) return; // No node to evict
        cacheMap.erase(node->getKey());
        if(ghostlist.size() > capacity) {
            removeOldestGhost();
        }
        insertGhost(node);
    }

    void insertGhost(std::shared_ptr<Node<Key, Value>> node) {
        ghostlist->insertToEnd(node);
        ghostMap[node->getKey()] = node;
        node->setFrequency(1);
    }

    void removeOldestGhost() {
        auto node = ghostlist->removeFront();
        ghostMap.erase(node->getKey());
    }
public:
    Value get(const Key& key){
        Value value{};
        if(get(key, value)) {
            return value;
        }
        return Value(); // Return default value if not found
    }
    bool checkGhost(const Key& key){
        if(ghostMap.find(key) != ghostMap.end()) {
            auto node = ghostMap[key];
            removeGhost(node);
            return true;
        }
    }
    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(cacheMap.find(key) != cacheMap.end()) {
            auto node = cacheMap[key];
            value = node->getValue();
            updateNode(node);
            if(node->getFrequency() - 1 == minFreq && freqList[minFreq]->isEmpty()) {
                updateMinFreq
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
    
    void put(const Key& key, const Value& value) {
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