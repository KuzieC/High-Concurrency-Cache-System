#pragma once
#include <memory>

template<typename Key, typename Value>
class LinkedList; // Forward declaration

template<typename Key, typename Value>
class Node {
    friend class LinkedList<Key, Value>;
private:
    Key key;
    Value val;
    int freq;
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev;

    Node(Key k, Value v) : key(k), val(v), freq(1), next(nullptr) {}
    Node() : key(), val(), freq(1), next(nullptr) {}
public:
    Key getKey() const { return key; }
    Value getValue() const { return val; }
    int getFrequency() const { return freq; }
    void setValue(const Value& v) { val = v; }
    void setFrequency(int f) { freq = f; }
};
