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
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev; // weak pointer to avoid circular reference
public:
    Node(const Key& key, const Value& value) : key(key), value(value), next(nullptr) {}
    ~Node() {}
    Key getKey() const { return key; }
    Value getValue() const { return value; }
    std::shared_ptr<Node> getNext() const { return next; }
    std::weak_ptr<Node> getPrev() const { return prev; }
    void setNext(std::shared_ptr<Node> nextNode) { next = nextNode; }
    void setPrev(std::weak_ptr<Node> prevNode) { prev = prevNode; }
    void setValue(const Value& value) { this->value = value; }
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

    bool contains(const Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return cacheMap.find(key) != cacheMap.end();
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

template<typename Key, typename Value>
class ColdEntry {
public:
    ColdEntry() : frequency(0), value(Value()) {}
    size_t frequency; // frequency of access, if exceeded, promote to LRU
    Value value;
};

template<typename Key, typename Value>
class LruK : public Lru<Key, Value> {
public:
    using ColdEntryT = ColdEntry<Key,Value>;
    LruK(int cap, int coldCacheSize, int kVal = 1) 
    : Lru<Key, Value>(cap), 
    promotionThresholds(kVal), 
    coldCache(std::make_unique<Lru<Key, ColdEntryT>>(coldCacheSize)){} // store cold entries

    void put(const Key key, const Value value) override {
        if(Lru<Key, Value>::contains(key)){
            Lru<Key, Value>::put(key, value);
            return;
        }
        ColdEntryT entry = coldCache->get(key);
        if(entry.frequency > promotionThresholds){
            coldCache->remove(key);
            Lru<Key,Value>::put(key, value);
        }
        else {
            entry.frequency++;
            entry.value = value;
            coldCache->put(key, entry);
        }
    }

    Value get(const Key key) override {
        ColdEntryT entry = coldCache->get(key);
        if(entry.frequency > promotionThresholds){
            coldCache->remove(key);
            Lru<Key, Value>::put(key, entry.value);
        }
        else {
            entry.frequency++;
            coldCache->put(key, entry);
        }
        return entry.value;
    }

private:
    int promotionThresholds;
    std::unique_ptr<Lru<Key, ColdEntryT>> coldCache;
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
