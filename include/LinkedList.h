#pragma once
#include "Node.h"
#include <memory>

// Doubly linked list for cache nodes
// Supports insert, remove, removeFront, isEmpty

template<typename Key, typename Value>
class LinkedList {
public:
    LinkedList() {
        head = std::make_shared<Node<Key, Value>>();
        tail = std::make_shared<Node<Key, Value>>();
        head->next = tail;
        tail->prev = head;
    }

    void insertToEnd(const std::shared_ptr<Node<Key, Value>>& node) {
        auto last = tail->prev.lock();
        last->next = node;
        node->prev = last;
        node->next = tail;
        tail->prev = node;
    }

    void remove(std::shared_ptr<Node<Key, Value>>& node) {
        auto prevNode = node->prev.lock();
        auto nextNode = node->next;
        prevNode->next = nextNode;
        nextNode->prev = prevNode;
        node->next = nullptr;
    }

    std::shared_ptr<Node<Key, Value>> removeFront() {
        auto first = head->next;
        if (first == tail) return nullptr;
        head->next = first->next;
        first->next->prev = head;
        first->next = nullptr;
        return first;
    }

    bool isEmpty() const {
        return head->next == tail;
    }

private:
    std::shared_ptr<Node<Key, Value>> head;
    std::shared_ptr<Node<Key, Value>> tail;
};
