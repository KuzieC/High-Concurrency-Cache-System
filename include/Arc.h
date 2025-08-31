#pragma once

#include "ArcLfu.h"
#include "ArcLru.h"
#include "Cache.h"
#include "LinkedList.h"

/**
 * @brief Adaptive Replacement Cache (ARC) implementation combining LRU and LFU.
 *
 * @tparam Key   The type of the cache key.
 * @tparam Value The type of the cache value.
 */
template<typename Key, typename Value>
class Arc : public Cache<Key, Value> {
private:
    int capacity; ///< The maximum number of items the cache can hold.
    int promotionThreshold; ///< The frequency threshold for promotion.
    std::unique_ptr<ArcLru<Key, Value>> lruCache; ///< LRU component of ARC.
    std::unique_ptr<ArcLfu<Key, Value>> lfuCache; ///< LFU component of ARC.

    /**
     * @brief Check if a key exists in the ghost lists and adjust capacities.
     * @param key The key to check.
     * @return True if the key was found in a ghost list, false otherwise.
     */
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
    /**
     * @brief Construct an ARC cache with a given capacity and promotion threshold.
     * @param capacity The maximum number of items the cache can hold.
     * @param promotionThreshold The frequency threshold for promotion.
     */
    Arc(int capacity, int promotionThreshold = 2) : capacity(capacity), promotionThreshold(promotionThreshold) {
        lruCache = std::make_unique<ArcLru<Key, Value>>(capacity, promotionThreshold);
        lfuCache = std::make_unique<ArcLfu<Key, Value>>(capacity, promotionThreshold);
    }

    /**
     * @brief Insert or update a value in the cache.
     * @param key   The key to insert or update.
     * @param value The value to associate with the key.
     */
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

    /**
     * @brief Retrieve a value from the cache.
     * @param key The key to look up.
     * @return The value associated with the key, or a default value if not found.
     */
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
