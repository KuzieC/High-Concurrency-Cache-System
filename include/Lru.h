#pragma once

#include "Cache.h"
#include <memory>

template<typename Key, typename Value>
class Node{
private:
    Key key;
    Value value;
    Node* next;
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev;
public:
    Node(const Key& key, const Value& value) : key(key), value(value), next(nullptr) {}
    ~Node(){};
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
    Lru(int cap):capacity(cap){};
    virtual ~Lru(){};
    virtual void put(const Key key, const Value value) override {
        // Implementation of put method
    }
    virtual Value get(const Key key) override {
        // Implementation of get method
        return Value(); // Placeholder return value
    }
private:
    std::shared_ptr<Node<Key, Value>> head;
    std::shared_ptr<Node<Key, Value>> tail;
    int size;
    int capacity;
    std::unordered_map<Key, std::shared_ptr<Node<Key, Value>>> cacheMap;
};