#pragma once

#include "Cache.h"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <vector>
#include <algorithm> // for std::min

template<typename Key, typename Value>
class AvgLfu; // Forward declaration

template<typename Key, typename Value>
class LfuNode {
public:
    Key key;
    Value val;
    int freq;
    std::shared_ptr<LfuNode> next;
    std::weak_ptr<LfuNode> prev;

    LfuNode(Key k, Value v) : key(k), val(v), freq(1), next(nullptr) {}
};

template<typename Key, typename Value>
class LinkList {
public:
    LinkList() {
        head = std::make_shared<LfuNode<Key, Value>>(Key(), Value());
        tail = std::make_shared<LfuNode<Key, Value>>(Key(), Value());
        head->next = tail;
        tail->prev = head;
    }

    void insertToEnd(Key key, Value val) {
        auto newLfuNode = std::make_shared<LfuNode<Key, Value>>(key, val);
        auto lastLfuNode = tail->prev.lock();
        lastLfuNode->next = newLfuNode;
        newLfuNode->prev = lastLfuNode;
        newLfuNode->next = tail;
        tail->prev = newLfuNode;
    }

    void insertToEnd(const std::shared_ptr<LfuNode<Key, Value>>& LfuNode) {
        auto lastLfuNode = tail->prev.lock();
        lastLfuNode->next = LfuNode;
        LfuNode->prev = lastLfuNode;
        LfuNode->next = tail;
        tail->prev = LfuNode;
    }

    void remove(std::shared_ptr<LfuNode<Key, Value>>& LfuNode) {
        auto prevLfuNode = LfuNode->prev.lock();
        auto nextLfuNode = LfuNode->next;
        prevLfuNode->next = nextLfuNode;
        nextLfuNode->prev = prevLfuNode;
        LfuNode->next = nullptr;
    }

    std::shared_ptr<LfuNode<Key, Value>> removeLFU() {
        auto firstLfuNode = head->next;
        head->next = firstLfuNode->next;
        firstLfuNode->next->prev = head;
        firstLfuNode->next = nullptr;
        return firstLfuNode;
    }

    bool isEmpty() {
        return head->next == tail;
    }

private:
    std::shared_ptr<LfuNode<Key, Value>> head;
    std::shared_ptr<LfuNode<Key, Value>> tail;
};

template<typename Key, typename Value>
class Lfu : public Cache<Key, Value> {
public:
    Lfu(int capacity) : size(0), minFreq(1), cap(capacity) {
        freqList[minFreq] = std::make_unique<LinkList<Key, Value>>();
    }

    void put(const Key key, const Value value) override {
        if (cap <= 0) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (mp.find(key) != mp.end()) {
            updateNode(mp[key]);
            mp[key]->val = value;
            updateMinFreq();
            return;
        }
        if (size == cap) {
            removeLFU();
            size--;
        }
        auto newLfuNode = std::make_shared<LfuNode<Key, Value>>(key, value);
        insertNewLfuNode(newLfuNode);
        mp[key] = newLfuNode;
        size++;
    }

    Value get(const Key key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (mp.find(key) == mp.end()) return Value();
        auto LfuNode = mp[key];
        updateNode(LfuNode);
        if (LfuNode->freq - 1 == minFreq && freqList[minFreq]->isEmpty()) {
            minFreq++;
        }
        //override the GetHook function in HashAvgLfu class
        GetHook();
        return LfuNode->val;
    }
protected:
    virtual void GetHook(){};
    virtual void removeLFUHook(int fre){};
private:
    int size;
    int minFreq;
    int cap;
    std::mutex mutex_;
    std::unordered_map<Key, std::shared_ptr<LfuNode<Key, Value>>> mp;
    std::unordered_map<int, std::unique_ptr<LinkList<Key, Value>>> freqList;

    void updateNode(std::shared_ptr<LfuNode<Key, Value>>& LfuNode) {
        removeLfuNode(LfuNode);
        LfuNode->freq++;
        insertLfuNode(LfuNode);
    }

    void removeLFU() {
        auto LfuNode = freqList[minFreq]->removeLFU();
        removeLFUHook(LfuNode->freq);
        mp.erase(LfuNode->key);
    }

    void removeLfuNode(std::shared_ptr<LfuNode<Key, Value>>& LfuNode) {
        int freq = LfuNode->freq;
        freqList[freq]->remove(LfuNode);
    }

    void insertLfuNode(std::shared_ptr<LfuNode<Key, Value>>& LfuNode) {
        if (freqList.find(LfuNode->freq) == freqList.end()) {
            freqList[LfuNode->freq] = std::make_unique<LinkList<Key, Value>>();
        }
        freqList[LfuNode->freq]->insertToEnd(LfuNode);
    }

    void insertNewLfuNode(std::shared_ptr<LfuNode<Key, Value>>& LfuNode) {
        if (freqList.find(LfuNode->freq) == freqList.end()) {
            freqList[LfuNode->freq] = std::make_unique<LinkList<Key, Value>>();
        }
        freqList[LfuNode->freq]->insertToEnd(LfuNode);
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
            auto LfuNode = it->second;
            Lfu<Key, Value>::removeLfuNode(LfuNode);
            LfuNode->freq = std::max(1, LfuNode->freq - maximumFreq);
            totalFreq += LfuNode->freq;
            Lfu<Key, Value>::insertLfuNode(LfuNode);
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