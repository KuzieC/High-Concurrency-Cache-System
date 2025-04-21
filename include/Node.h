#pragma once
#include <memory>

template<typename Key, typename Value>
class LinkedList; // Forward declaration

template<typename Key, typename Value>
class Node {
    friend class LinkedList<Key, Value>;
private:
    Key key;                ///< The key stored in the node.
    Value val;              ///< The value stored in the node.
    int freq;               ///< Frequency counter for LFU/ARC policies.
    std::shared_ptr<Node> next; ///< Pointer to the next node.
    std::weak_ptr<Node> prev;   ///< Pointer to the previous node.

public:
    /**
     * @brief Construct a node with a key and value.
     * @param k The key.
     * @param v The value.
     */
    Node(Key k, Value v) : key(k), val(v), freq(1), next(nullptr) {}
    /**
     * @brief Default constructor.
     */
    Node() : key(), val(), freq(1), next(nullptr) {}
    /**
     * @brief Get the key stored in the node.
     * @return The key.
     */
    Key getKey() const { return key; }
    /**
     * @brief Get the value stored in the node.
     * @return The value.
     */
    Value getValue() const { return val; }
    /**
     * @brief Get the frequency counter.
     * @return The frequency.
     */
    int getFrequency() const { return freq; }
    /**
     * @brief Set the value stored in the node.
     * @param v The new value.
     */
    void setValue(const Value& v) { val = v; }
    /**
     * @brief Set the frequency counter.
     * @param f The new frequency.
     */
    void setFrequency(int f) { freq = f; }
};
