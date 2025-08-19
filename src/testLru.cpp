// testLru.cpp

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <cstdlib>
#include "../include/Lru.h"  // Contains Lru, LruK, and HashLruK implementations

// Workload parameters
const int NUM_THREADS = 10;
const int OPS_PER_THREAD = 100000;
const int KEY_RANGE = 1000;      // Keys will be in [0, KEY_RANGE)
const int CACHE_CAPACITY = 1000;
const int COLD_CACHE_SIZE = 500;
const int PROMOTION_THRESHOLD = 2;
const int HASH_SLICES = 8;


/**
 * @brief Workload function that performs random put and get operations on a cache.
 * 
 * This function simulates a realistic cache workload by performing a mix of
 * put and get operations with random keys within a specified range.
 * 
 * @tparam CacheType The type of cache to test (LRU, LRU-K, HashLRU-K, etc.).
 * @param cache Reference to the cache instance.
 * @param threadId Unique identifier for this thread (used for seeding random generator).
 */
template<typename CacheType>
void workload(CacheType &cache, int threadId) {
    // Seed rand() differently per thread (for simplicity)
    std::srand(std::time(nullptr) + threadId);
    for (int i = 0; i < OPS_PER_THREAD; ++i) {
        int key = std::rand() % KEY_RANGE;
        // We'll simulate a simple workload where 50% of the time we do a put (with value = key*10)
        // and 50% a get.
        if ((std::rand() % 2) == 0) {
            cache.put(key, key * 10);
        } else {
            // For get: if there is a hit, nothing is done; on miss, the cache returns default value.
            volatile int v = cache.get(key);
            (void)v; // silence unused variable warning
        }
    }
}

/**
 * @brief Run a multithreaded performance test on a cache implementation.
 * 
 * This function creates multiple threads that concurrently access the cache,
 * measures the total execution time, and returns the elapsed time in milliseconds.
 * 
 * @tparam CacheType The type of cache to test.
 * @param cache Reference to the cache instance.
 * @return The elapsed time in milliseconds for all threads to complete.
 */
template<typename CacheType>
double runMultithreadedTest(CacheType &cache) {
    std::vector<std::thread> threads;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(workload<CacheType>, std::ref(cache), i);
    }
    for (auto &th : threads) {
        th.join();
    }
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    return elapsed.count();
}

/**
 * @brief Main test function comparing LRU-K and HashLRU-K performance.
 * 
 * This function benchmarks both LRU-K and HashLRU-K cache implementations
 * under the same multithreaded workload and reports performance metrics
 * including throughput and improvement percentages.
 * 
 * @return 0 on successful completion.
 */
int testLru() {
    std::cout << "=== Multithreaded Cache Performance Comparison ===\n";

    // Test LRU-K:
    LruK<int, int> lruK(CACHE_CAPACITY, COLD_CACHE_SIZE, PROMOTION_THRESHOLD);
    double lruKTime = runMultithreadedTest(lruK);
    std::cout << "LRU-K total time: " << lruKTime << " ms" << std::endl;

    // Test HashLRU-K:
    HashLruK<int, int> hashLruK(CACHE_CAPACITY, HASH_SLICES, COLD_CACHE_SIZE, PROMOTION_THRESHOLD);
    double hashLruKTime = runMultithreadedTest(hashLruK);
    std::cout << "HashLRU-K total time: " << hashLruKTime << " ms" << std::endl;

    // Compute throughput improvement (operations per ms)
    double opsTotal = static_cast<double>(NUM_THREADS * OPS_PER_THREAD);
    double lruKThroughput = opsTotal / lruKTime;
    double hashLruKThroughput = opsTotal / hashLruKTime;
    double improvementPercent = ((hashLruKThroughput - lruKThroughput) / lruKThroughput) * 100.0;

    std::cout << "\n--- Performance Metrics ---\n";
    std::cout << "LRU-K throughput: " << lruKThroughput << " ops/ms\n";
    std::cout << "HashLRU-K throughput: " << hashLruKThroughput << " ops/ms\n";
    std::cout << "HashLRU-K improvement: " << improvementPercent << " % over LRU-K\n\n";
    return 0;
}

