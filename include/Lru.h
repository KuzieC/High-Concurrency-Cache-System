#pragma once

#include "Cache.h"
#include "Node.h"
#include "LinkedList.h"
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <vector>
#include <algorithm>

template<typename Key, typename Value>
class Lru : public Cache<Key, Value> {
public:
    using LruNode = Node<Key, Value>;
    using LruNodePtr = std::shared_ptr<LruNode>;
    using LruMap = std::unordered_map<Key, LruNodePtr>;

    Lru(int cap) : capacity(cap), size(0) {
        list = std::make_shared<LinkedList<Key, Value>>();
    }
    
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
    
    void remove(const Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cacheMap.find(key) != cacheMap.end()) {
            auto node = cacheMap[key];
            list->remove(node);
        }
    }

    bool contains(const Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return cacheMap.find(key) != cacheMap.end();
    }

    int getFrequency(const Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cacheMap.find(key) != cacheMap.end()) {
            return cacheMap[key]->getFrequency();
        }
        return 0;
    }

    void setFrequency(const Key key, int freq) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cacheMap.find(key) != cacheMap.end()) {
            cacheMap[key]->setFrequency(freq);
        }
    }
private:
    std::shared_ptr<LinkedList<Key, Value>> list;
    int size;
    int capacity;
    LruMap cacheMap;
    std::mutex mutex_;
    
    LruNodePtr insertBack(const Key& key, const Value& value) {
        ++size;
        auto newNode = std::make_shared<LruNode>(key, value);
        list->insertToEnd(newNode);
        cacheMap[key] = newNode;
        return newNode;
    }
    
    void insertBack(LruNodePtr node) {
        ++size;
        list->insertToEnd(node);
        cacheMap[node->getKey()] = node;
    }

    void removelru() {
        auto node = list->removeFront();
        cacheMap.erase(node->getKey());
        --size;
    }
};

template<typename Key, typename Value>
class LruK : public Lru<Key, Value> {
public:
    LruK(int cap, int coldCacheSize, int kVal = 1) 
    : Lru<Key, Value>(cap), 
    promotionThresholds(kVal), 
    coldCache(std::make_unique<Lru<Key, Value>>(coldCacheSize)){} // store cold entries

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
    int promotionThresholds;
    std::unique_ptr<Lru<Key, Value>> coldCache;
};

template<typename Key, typename Value>
class HashLruK {
public:
    HashLruK(int cap, int slice, int coldCacheSize, int promotionThreshold)
      : sliceNum(slice), capacity(cap), promotionThreshold(promotionThreshold)
    {
        int sliceSize = capacity / sliceNum;
        lruKShards.reserve(sliceNum);
        for (int i = 0; i < sliceNum; ++i) {
            lruKShards.emplace_back(std::make_unique<LruK<Key, Value>>(sliceSize, coldCacheSize, promotionThreshold));
        }
    }
    
    void put(const Key key, const Value value) {
        size_t idx = hash(key);
        lruKShards[idx]->put(key, value);
    }
    
    Value get(const Key key) {
        size_t idx = hash(key);
        return lruKShards[idx]->get(key);
    }
    
private:
    int capacity;
    int sliceNum;
    int promotionThreshold;
    std::vector<std::unique_ptr<LruK<Key, Value>>> lruKShards;
    
    size_t hash(const Key& key) {
        return std::hash<Key>()(key) % sliceNum;
    }
};
