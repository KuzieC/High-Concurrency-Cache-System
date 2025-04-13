#pragma once

#include "Cache.h"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <vector>

template<typename Key, typename Value>
class Node {
private:
    Key key;
    Value value;
    int frequency; // frequency of access in coldLRU, if exceeded, promote to LRU
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev; // weak pointer to avoid circular reference
public:
    Node(const Key& key, const Value& value) : key(key), value(value), next(nullptr), frequency(0) {}
    ~Node() {}
    Key getKey() const { return key; }
    Value getValue() const { return value; }
    std::shared_ptr<Node> getNext() const { return next; }
    std::weak_ptr<Node> getPrev() const { return prev; }
    void setNext(std::shared_ptr<Node> nextNode) { next = nextNode; }
    void setPrev(std::weak_ptr<Node> prevNode) { prev = prevNode; }
    void setValue(const Value& value) { this->value = value; }
    void setFrequency(int freq) { frequency = freq; }
    int getFrequency() const { return frequency; }
    void incrementFrequency() { frequency++; } // increment frequency for coldLRU
};

template<typename Key, typename Value>
class Lru : public Cache<Key, Value> {
public:
    using LruNode = Node<Key, Value>;
    using LruNodePtr = std::shared_ptr<LruNode>;
    using LruMap = std::unordered_map<Key, LruNodePtr>;

    Lru(int cap) : capacity(cap), size(0) {
        head = std::make_shared<LruNode>(Key(), Value());
        tail = std::make_shared<LruNode>(Key(), Value());
        head->setNext(tail);
        tail->setPrev(head);
    }
    
    virtual ~Lru() {
        std::lock_guard<std::mutex> lock(mutex_); // lock the mutex to ensure thread safety
        while (head->getNext() != tail) {
            removeNode(head->getNext());
        }
        head.reset();
        tail.reset();
        cacheMap.clear();
    }
    
    virtual void put(const Key key, const Value value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cacheMap.find(key) != cacheMap.end()) {
            //if modifying frequency here, the nodes are removed and constructed again with empty frequency, 
            // which is not the expected behavior.
            auto node = cacheMap[key];
            removeNode(node);
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
            //same as above, if modifying frequency here, the nodes are removed and constructed again with empty frequency
            auto node = cacheMap[key];
            Value res = node->getValue();
            removeNode(node);
            insertBack(node);
            return res;
        }
        return Value();
    }
    
    void remove(const Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cacheMap.find(key) != cacheMap.end()) {
            auto node = cacheMap[key];
            removeNode(node);
        }
    }
    //adding a new function to check if the key is in the cache, instead of comparing with the result 
    // of get funciton which return Value() for not found key, which is not the expected behavior.
    bool contains(const Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return cacheMap.find(key) != cacheMap.end();
    }
    //thus adding these functions to get and modify the frequency of the key in the cache when needed 
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
    LruNodePtr head;
    LruNodePtr tail;
    int size;
    int capacity;
    LruMap cacheMap;
    std::mutex mutex_;
    
    LruNodePtr insertBack(const Key& key, const Value& value) {
        ++size;
        auto newNode = std::make_shared<LruNode>(key, value);
        newNode->setPrev(tail->getPrev());
        tail->getPrev().lock()->setNext(newNode);
        newNode->setNext(tail);
        tail->setPrev(newNode);
        cacheMap[key] = newNode;
        return newNode;
    }
    
    void insertBack(LruNodePtr node) {
        ++size;
        node->setPrev(tail->getPrev());
        tail->getPrev().lock()->setNext(node);
        node->setNext(tail);
        tail->setPrev(node);
        cacheMap[node->getKey()] = node;
    }

    void removeNode(LruNodePtr node) {
        --size;
        node->getPrev().lock()->setNext(node->getNext());
        node->getNext()->setPrev(node->getPrev());
        cacheMap.erase(node->getKey());
        node->setNext(nullptr);
    }

    void removelru() {
        removeNode(head->getNext());
    }
};

// frequency has been added to the node class, so no need for a separate class for cold cache entries
// template<typename Key, typename Value>
// class ColdEntry {
// public:
//     ColdEntry() : frequency(0), value(Value()) {}
//     size_t frequency; // frequency of access, if exceeded, promote to LRU
//     Value value;
// };

// once a key pair is promoted to the main cache, it is removed from the cold cache 
// and the frequency is no longer needed in the main cache.
// future improvements: demote cold entries to the main cache if they are evicted from the main cache
template<typename Key, typename Value>
class LruK : public Lru<Key, Value> {
public:
    // using ColdEntryT = ColdEntry<Key,Value>;
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
