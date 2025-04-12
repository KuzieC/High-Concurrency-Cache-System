#include <stdio.h>
#include "../include/Lru.h"

int main() {
    // Create a HashLruK cache.
    // Parameters:
    //   capacity: overall capacity (e.g. 10 keys)
    //   slice: number of slices (shards) (e.g. 2)
    //   coldCacheSize: capacity for the cold cache in each shard (e.g. 5)
    //   promotionThreshold: frequency threshold to promote an entry to the main cache (e.g. 2)
    HashLruK<int, std::string> cache(10, 2, 5, 2);

    // Test basic put and get in the main LRU part.
    std::cout << "Inserting keys 1,2,3 into cache..." << std::endl;
    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    std::cout << "Retrieving keys:" << std::endl;
    std::cout << "Key 1: " << cache.get(1) << std::endl;
    std::cout << "Key 2: " << cache.get(2) << std::endl;
    std::cout << "Key 3: " << cache.get(3) << std::endl;

    // Test the promotion logic in LruK.
    // Assume that for a key not yet in the main cache, repeated puts will store an increasing frequency in the cold cache.
    std::cout << "\nTesting promotion for key 4:" << std::endl;
    for (int i = 0; i < 3; ++i) {  // call more times than the threshold (2)
        cache.put(4, "four");
    }
    // Once promotion threshold is exceeded, key 4 should be promoted to the main cache.
    std::cout << "Key 4 after promotion: " << cache.get(4) << std::endl;
    
    // Testing behavior on a missing key.
    std::cout << "\nTesting retrieval of a missing key (key 5):" << std::endl;
    std::string val = cache.get(5);
    if (val.empty()) {
        std::cout << "Key 5 not found (empty string returned)." << std::endl;
    } else {
        std::cout << "Key 5: " << val << std::endl;
    }

    // Optionally, update an existing key to see if update works correctly.
    std::cout << "\nUpdating key 2 to \"TWO_UPDATED\"." << std::endl;
    cache.put(2, "TWO_UPDATED");
    std::cout << "Key 2 after update: " << cache.get(2) << std::endl;

    return 0;
}