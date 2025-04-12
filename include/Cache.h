#pragma once

template<typename Key, typename Value>
class Cache {
public:
    Cache(){};
    virtual ~Cache(){};
    virtual void put(const Key key,const Value value) = 0;
    virtual Value get(const Key key) = 0;
};