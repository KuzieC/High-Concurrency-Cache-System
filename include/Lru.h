#pragma once

#include "Cache.h"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <iostream>

template<typename Key, typename Value>
class Node {
private:
    Key key;
    Value value;
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev;
public:
    Node(const Key& key, const Value& value) : key(key), value(value), next(nullptr) {}
    ~Node() {}

    Key getKey() const {
        return key;
    }

    Value getValue() const {
        return value;
    }

    std::shared_ptr<Node> getNext() const {
        return next;
    }

    std::weak_ptr<Node> getPrev() const {
        return prev;
    }

    void setNext(std::shared_ptr<Node> nextNode) {
        next = nextNode;
    }

    void setPrev(std::weak_ptr<Node> prevNode) {
        prev = prevNode;
    }

    void setValue(const Value& value) {
        this->value = value;
    }
};

template<typename Key, typename Value>
class Lru : public Cache<Key, Value> {
public:
    Lru(int cap);
    virtual ~Lru();
    virtual void put(const Key key, const Value value) override;
    virtual Value get(const Key key) override;
private:
    std::shared_ptr<Node<Key, Value>> head;
    std::shared_ptr<Node<Key, Value>> tail;
    int size;
    int capacity;
    std::unordered_map<Key, std::shared_ptr<Node<Key, Value>>> cacheMap;
    std::mutex mutex_;
    std::shared_ptr<Node<Key, Value>> insertBack(const Key& key, const Value& value);
    void insertBack(std::shared_ptr<Node<Key, Value>> node);
    void removeNode(std::shared_ptr<Node<Key, Value>> node);
    void removelru();
};

template<typename Key, typename Value>
Lru<Key, Value>::Lru(int cap) : capacity(cap), size(0) {
    head = std::make_shared<Node<Key, Value>>(Key(), Value());
    tail = std::make_shared<Node<Key, Value>>(Key(), Value());
    head->setNext(tail);
    tail->setPrev(head);
}

template<typename Key, typename Value>
void 
Lru<Key, Value>::put(Key key, Value value) {
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

template<typename Key, typename Value>
Lru<Key, Value>::~Lru() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (head->getNext() != tail) {
        removeNode(head->getNext());
    }
    head.reset();
    tail.reset();
    cacheMap.clear();
}

template<typename Key, typename Value>
Value 
Lru<Key, Value>::get(Key key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cacheMap.find(key) != cacheMap.end()) {
        auto node = cacheMap[key];
        Value res = node->getValue();
        removeNode(node);
        insertBack(node);
        return res;
    }
    else return Value(); 
}

template<typename Key, typename Value>
std::shared_ptr<Node<Key, Value>> 
Lru<Key, Value>::insertBack(const Key& key, const Value& value) {
    size++;
    auto newNode = std::make_shared<Node<Key, Value>>(key, value);
    newNode->setPrev(tail->getPrev());
    tail->getPrev().lock()->setNext(newNode);
    newNode->setNext(tail);
    tail->setPrev(newNode);
    cacheMap[key] = newNode;
    return newNode;
}

template<typename Key, typename Value>
void 
Lru<Key, Value>::insertBack(std::shared_ptr<Node<Key, Value>> node) {
    size++;
    node->setPrev(tail->getPrev());
    tail->getPrev().lock()->setNext(node);
    node->setNext(tail);
    tail->setPrev(node);
}

template<typename Key, typename Value>
void 
Lru<Key, Value>::removeNode(std::shared_ptr<Node<Key, Value>> node) {
    size--;
    node->getPrev().lock()->setNext(node->getNext());
    node->getNext()->setPrev(node->getPrev());
    cacheMap.erase(node->getKey());
    node->setNext(nullptr);
}

template<typename Key, typename Value>
void 
Lru<Key, Value>::removelru() {
    removeNode(head->getNext());
}