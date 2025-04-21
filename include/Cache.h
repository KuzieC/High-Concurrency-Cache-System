#pragma once

/**
 * @brief Abstract base class for cache policies.
 *
 * @tparam Key   The type of the cache key.
 * @tparam Value The type of the cache value.
 */
template<typename Key, typename Value>
class Cache {
public:
    /**
     * @brief Default constructor.
     */
    Cache(){};
    /**
     * @brief Virtual destructor.
     */
    virtual ~Cache(){};
    /**
     * @brief Insert or update a value in the cache.
     *
     * @param key   The key to insert or update.
     * @param value The value to associate with the key.
     */
    virtual void put(const Key key,const Value value) = 0;
    /**
     * @brief Retrieve a value from the cache.
     *
     * @param key The key to look up.
     * @return The value associated with the key, or a default value if not found.
     */
    virtual Value get(const Key key) = 0;
};