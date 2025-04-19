#pragma once

#include "ArcLfu.h"
#include "ArcLru.h"
#include "Cache.h"
#include "LinkedList.h"

template<typename Key, typename Value>
class Arc : public Cache<Key, Value> {
private:
    int capacaity;
    int promotionThreshold;
    std::unique_ptr<ArcLru<Key, Value>> lruCache;
    std::unique_ptr<ArcLfu<Key, Value>> lfuCache;
    bool checkGhost(const Key& key) {
        bool ret = false;
        if(lruCache->checkGhost(key)){
            ret = true;
            if(lfuCache->decreaseCapacity()) lruCache->increaseCapacity();
        }
        else if(lfuCache->checkGhost(key)){
            ret = true;
            if(lruCache->decreaseCapacity()) lfuCache->increaseCapacity();
        }
        return ret;
    }
public:
    Arc(int capacity, int promotionThreshold = 2) : capacaity(capacity), promotionThreshold(promotionThreshold) {
        lruCache = std::make_unique<ArcLru<Key, Value>>(capacaity, promotionThreshold);
        lfuCache = std::make_unique<ArcLfu<Key, Value>>(capacaity, promotionThreshold);
    }

    void put(const Key key, const Value value) override {
        if(checkGhost(key)){
            lfuCache->put(key, value);
        }
        else{
            bool flag = false;
            lruCache->put(key, value, flag);
            if(flag){
                lfuCache->put(key, value);
            }
        }
    }

    Value get(const Key key) override {
        checkGhost(key);
        Value value{};
        bool flag = false;
        if(lruCache->get(key,value, flag)){
            if(flag){
                lfuCache->put(key, value);
            }   
            return value;
        }
        return lfuCache->get(key);
    }
};
