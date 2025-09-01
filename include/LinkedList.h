#pragma once
#include "Node.h"
#include <memory>
#include <mutex>

/**
 * @brief Doubly linked list for managing cache nodes.
 *
 * @tparam Key   The type of the cache key.
 * @tparam Value The type of the cache value.
 */
template<typename Key, typename Value>
class LinkedList {
public:
    /**
     * @brief Construct an empty linked list with dummy head and tail nodes.
     */
    LinkedList() {
        head = std::make_shared<Node<Key, Value>>(-1, -1);
        tail = std::make_shared<Node<Key, Value>>(-1, -1);
        head->next = tail;
        tail->prev = head;
    }

    /**
     * @brief Insert a node at the end of the list.
     * @param node The node to insert.
     */
    void insertToEnd(const std::shared_ptr<Node<Key, Value>>& node) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto last = tail->prev.lock();
        last->next = node;
        node->prev = last;
        node->next = tail;
        tail->prev = node;
        size++;
    }

    /**
     * @brief Remove a node from the list.
     * @param node The node to remove.
     */
    void remove(std::shared_ptr<Node<Key, Value>>& node) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto prevNode = node->prev.lock();
        auto nextNode = node->next;
        prevNode->next = nextNode;
        nextNode->prev = prevNode;
        node->next = nullptr;
        size--;
    }

    /**
     * @brief Remove and return the node at the front of the list.
     * @return The removed node, or nullptr if the list is empty.
     */
    std::shared_ptr<Node<Key, Value>> removeFront() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto first = head->next;
        if (first == tail) return nullptr;
        head->next = first->next;
        first->next->prev = head;
        first->next = nullptr;
        size--;
        return first;
    }

    /**
     * @brief Check if the list is empty.
     * @return True if the list is empty, false otherwise.
     */
    bool isEmpty() const {
        return head->next == tail;
    }

    /**
     * @brief Get the number of nodes in the list (excluding dummy nodes).
     * @return The size of the list.
     */
    int getSize() {
        std::lock_guard<std::mutex> lock(mutex_);
        return size;
    }

private:
    int size = 0; ///< The number of nodes in the list (excluding dummy nodes).
    std::shared_ptr<Node<Key, Value>> head; ///< Dummy head node.
    std::shared_ptr<Node<Key, Value>> tail; ///< Dummy tail node.
    std::mutex mutex_; ///< Mutex for thread-safe operations.
};
