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
    Lfu(int capacity) : size(0), minFreq(1), cap(capacity) {
        freqList[minFreq] = std::make_unique<LinkedList<Key, Value>>();
    }

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
    virtual void GetHook(){};
    virtual void removeLFUHook(int fre){};
private:
    int size;
    int minFreq;
    int cap;
    std::mutex mutex_;
    std::unordered_map<Key, std::shared_ptr<Node<Key, Value>>> mp;
    std::unordered_map<int, std::unique_ptr<LinkedList<Key, Value>>> freqList;

    void updateNode(std::shared_ptr<Node<Key, Value>>& node) {
        removeNode(node);
        node->setFrequency(node->getFrequency() + 1);
        insertNode(node);
    }

    void removeLFU() {
        auto node = freqList[minFreq]->removeLFU();
        removeLFUHook(node->getFrequency());
        mp.erase(node->key);
    }

    void removeNode(std::shared_ptr<Node<Key, Value>>& node) {
        int freq = node->getFrequency();
        freqList[freq]->remove(node);
    }

    void insertNode(std::shared_ptr<Node<Key, Value>>& node) {
        if (freqList.find(node->getFrequency()) == freqList.end()) {
            freqList[node->getFrequency()] = std::make_unique<LinkedList<Key, Value>>();
        }
        freqList[node->getFrequency()]->insertToEnd(node);
    }

    void insertNewNode(std::shared_ptr<Node<Key, Value>>& node) {
        if (freqList.find(node->getFrequency()) == freqList.end()) {
            freqList[node->getFrequency()] = std::make_unique<LinkedList<Key, Value>>();
        }
        freqList[node->getFrequency()]->insertToEnd(node);
        minFreq = 1;
    }

    //minFreq might not be continuous, so we need to find the minimum frequency in freqList
    //and update it when we remove the LFU node.
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

template<typename Key, typename Value>
class AvgLfu : public Lfu<Key, Value> {
private: 
    int averageFreq;
    int totalFreq;
    int maximumFreq;
    void increaseTotalFreq() {
        totalFreq++;
        averageFreq = totalFreq / Lfu<Key, Value>::mp.size();
        if (averageFreq > maximumFreq) {
            handleFreq();
        }
    }

    void decreaseTotalFreq(int num) {
        totalFreq -= num;
        averageFreq = totalFreq / Lfu<Key, Value>::mp.size();
    }

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
    virtual void GetHook() override {
        increaseTotalFreq();
    }
    
    virtual void removeLFUHook(int fre) override {
        decreaseTotalFreq(fre);
    }


public:
    AvgLfu(int cap, int maxFreq = 10) : Lfu<Key, Value>(cap), maximumFreq(maxFreq) {
        averageFreq = 0;
        totalFreq = 0;
    }

};

template<typename Key, typename Value>
class HashAvgLfu {
private:
    int sliceNum;
    int sliceSize;
    int capacity;
    std::vector<std::unique_ptr<AvgLfu<Key, Value>>> avgLfuShards;

    size_t hash(const Key& key) {
        return std::hash<Key>()(key) % sliceNum;
    }
public:
    HashAvgLfu(int cap, int slice, int maximumAverageThreshold = 10) :sliceNum(slice), capacity(cap) {
        sliceSize = capacity / sliceNum;
        avgLfuShards.reserve(sliceNum);
        for (int i = 0; i < sliceNum; ++i) {
            avgLfuShards.emplace_back(std::make_unique<AvgLfu<Key, Value>>(sliceSize,maximumAverageThreshold));
        }
    }

    void put(const Key key, const Value value) {
        size_t idx = hash(key);
        avgLfuShards[idx]->put(key, value);
    }

    Value get(const Key key) {
        size_t idx = hash(key);
        return avgLfuShards[idx]->get(key);
    }
};