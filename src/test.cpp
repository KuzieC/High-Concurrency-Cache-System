#define CACHE_POLICY_1 Lru
#define CACHE_POLICY_2 Lfu
#define CACHE_POLICY_3 Arc
#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>
#include <algorithm>
#include <thread>
#include <atomic>
#include "../include/Lru.h"
#include "../include/Lfu.h"
#include "Arc.h"

// Global number of worker threads used in each test.
const int NUM_THREADS = 4;

// Global cache instances for each test scenario.
// You only need to adjust the construction here.
namespace caches {
    // Hot Data Access Test caches.
    const int HOT_CAPACITY = 50;
    CACHE_POLICY_1<int, int> hotLruCache(HOT_CAPACITY);
    CACHE_POLICY_2<int, int> hotLfuCache(HOT_CAPACITY);
    CACHE_POLICY_3<int, int> hotArcCache(HOT_CAPACITY);

    // Loop Pattern Test caches.
    const int LOOP_CAPACITY = 50;
    CACHE_POLICY_1<int, int> loopLruCache(LOOP_CAPACITY);
    CACHE_POLICY_2<int, int> loopLfuCache(LOOP_CAPACITY);
    CACHE_POLICY_3<int, int> loopArcCache(LOOP_CAPACITY);

    // Workload Shift Test caches.
    const int SHIFT_CAPACITY = 50;
    CACHE_POLICY_1<int, int> shiftLruCache(SHIFT_CAPACITY);
    CACHE_POLICY_2<int, int> shiftLfuCache(SHIFT_CAPACITY);
    CACHE_POLICY_3<int, int> shiftArcCache(SHIFT_CAPACITY);
}

class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}

    double elapsed() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

/**
 * @brief Print test results for a single cache algorithm.
 * 
 * @param testName The name of the test scenario.
 * @param capacity The cache capacity used in the test.
 * @param hits The number of cache hits.
 * @param misses The number of cache misses.
 */
void printResults(const std::string& testName, int capacity, int hits, int misses) {
    std::cout << "\n=== " << testName << " ===\n";
    std::cout << "Cache Capacity: " << capacity << "\n";
    std::cout << "Hit Rate: " << std::fixed << std::setprecision(2)
              << (100.0 * hits / (hits + misses)) << "%\n";
    std::cout << "Miss Rate: " << std::fixed << std::setprecision(2)
              << (100.0 * misses / (hits + misses)) << "%\n";
}

/**
 * @brief Print a comparative table of test results for multiple cache algorithms.
 * 
 * This function creates a formatted table showing hit rates for LRU, LFU, and ARC
 * cache algorithms across different test scenarios.
 * 
 * @param testNames Array of test scenario names.
 * @param capacities Array of cache capacities used in each test.
 * @param hits 2D array of hit counts [test][algorithm].
 * @param misses 2D array of miss counts [test][algorithm].
 */
void printResultsTable(const std::string testNames[3], int capacities[3], int hits[3][3], int misses[3][3]) {
    std::cout << std::left << std::setw(28) << "Test Case";
    std::cout << std::setw(18) << "LRU (Hit%)";
    std::cout << std::setw(18) << "LFU (Hit%)";
    std::cout << std::setw(18) << "ARC (Hit%)" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    for (int i = 0; i < 3; ++i) {
        std::cout << std::setw(28) << testNames[i];
        for (int j = 0; j < 3; ++j) {
            double hitRate = (hits[i][j] + misses[i][j]) ? (100.0 * hits[i][j] / (hits[i][j] + misses[i][j])) : 0.0;
            std::cout << std::setw(18) << (std::to_string(hitRate) + "%");
        }
        std::cout << std::endl;
    }
    std::cout << std::string(80, '-') << std::endl;
}

// -----------------------
// Hot Data Access Test
// -----------------------
/**
 * @brief Test cache performance with hot data access patterns.
 * 
 * This test simulates a workload where a small subset of keys (hot data)
 * are accessed frequently (30% probability) while a larger set of cold
 * keys are accessed less frequently. This pattern tests how well each
 * cache algorithm handles temporal locality.
 * 
 * @param lruHits Reference to store LRU cache hit count.
 * @param lruMisses Reference to store LRU cache miss count.
 * @param lfuHits Reference to store LFU cache hit count.
 * @param lfuMisses Reference to store LFU cache miss count.
 * @param arcHits Reference to store ARC cache hit count.
 * @param arcMisses Reference to store ARC cache miss count.
 */
void testHotDataAccess(int& lruHits, int& lruMisses, int& lfuHits, int& lfuMisses, int& arcHits, int& arcMisses) {
    const int OPERATIONS = 100000;
    const int HOT_KEYS = 20;
    const int COLD_KEYS = 5000;
    
    std::atomic<int> lruHitsAtomic{0}, lruMissesAtomic{0};
    std::atomic<int> lfuHitsAtomic{0}, lfuMissesAtomic{0};
    std::atomic<int> arcHitsAtomic{0}, arcMissesAtomic{0};

    // Worker function that processes a partition of operations.
    auto worker = [&](int startOp, int endOp) {
        std::random_device rd;
        std::mt19937 gen(rd());
        for (int op = startOp; op < endOp; ++op) {
            int key = (op % 100 < 30) ? (gen() % HOT_KEYS)
                                      : (HOT_KEYS + (gen() % COLD_KEYS));

            caches::hotLruCache.put(key, key * 10);
            caches::hotLfuCache.put(key, key * 10);
            caches::hotArcCache.put(key, key * 10);
        }
        for (int op = startOp; op < endOp; ++op) {
            int key = (op % 100 < 30) ? (gen() % HOT_KEYS)
                                      : (HOT_KEYS + (gen() % COLD_KEYS));
            if (caches::hotLruCache.get(key) != 0) {
                ++lruHitsAtomic;
            } else {
                ++lruMissesAtomic;
            }
            if (caches::hotLfuCache.get(key) != 0) {
                ++lfuHitsAtomic;
            } else {
                ++lfuMissesAtomic;
            }
            if (caches::hotArcCache.get(key) != 0) {
                ++arcHitsAtomic;
            } else {
                ++arcMissesAtomic;
            }
        }
    };

    // Launch worker threads and partition the operations.
    std::vector<std::thread> threads;
    int opsPerThread = OPERATIONS / NUM_THREADS;
    int remainder = OPERATIONS % NUM_THREADS;
    int currentOp = 0;
    for (int i = 0; i < NUM_THREADS; ++i) {
        int endOp = currentOp + opsPerThread + (i < remainder ? 1 : 0);
        threads.emplace_back(worker, currentOp, endOp);
        currentOp = endOp;
    }
    for (auto &t : threads)
        t.join();

    lruHits = lruHitsAtomic.load();
    lruMisses = lruMissesAtomic.load();
    lfuHits = lfuHitsAtomic.load();
    lfuMisses = lfuMissesAtomic.load();
    arcHits = arcHitsAtomic.load();
    arcMisses = arcMissesAtomic.load();
}

// -----------------------
// Loop Pattern Test
// -----------------------
/**
 * @brief Test cache performance with loop access patterns.
 * 
 * This test simulates a workload with sequential loop access patterns
 * where keys are accessed in cyclic order. This pattern can be particularly
 * challenging for cache algorithms and tests their ability to handle
 * scan resistance and working set management.
 * 
 * @param lruHits Reference to store LRU cache hit count.
 * @param lruMisses Reference to store LRU cache miss count.
 * @param lfuHits Reference to store LFU cache hit count.
 * @param lfuMisses Reference to store LFU cache miss count.
 * @param arcHits Reference to store ARC cache hit count.
 * @param arcMisses Reference to store ARC cache miss count.
 */
void testLoopPattern(int& lruHits, int& lruMisses, int& lfuHits, int& lfuMisses, int& arcHits, int& arcMisses) {
    const int LOOP_SIZE = 500;
    const int OPERATIONS = 200000;
    
    std::atomic<int> lruHitsAtomic{0}, lruMissesAtomic{0};
    std::atomic<int> lfuHitsAtomic{0}, lfuMissesAtomic{0};
    std::atomic<int> arcHitsAtomic{0}, arcMissesAtomic{0};

    // Pre-populate the global Loop Pattern caches.
    for (int key = 0; key < LOOP_SIZE; ++key) {
        caches::loopLruCache.put(key, key * 10);
        caches::loopLfuCache.put(key, key * 10);
        caches::loopArcCache.put(key, key * 10);
    }

    // Worker function for Loop Pattern Test.
    auto worker = [=, &lruHitsAtomic, &lruMissesAtomic, &lfuHitsAtomic, &lfuMissesAtomic, &arcHitsAtomic, &arcMissesAtomic](int startOp, int endOp) {
        std::random_device rd;
        std::mt19937 gen(rd());
        int currentPos = 0;

        for (int op = startOp; op < endOp; ++op) {
            int key;
            if (op % 100 < 70) {
                key = currentPos;
                currentPos = (currentPos + 1) % LOOP_SIZE;
            } else if (op % 100 < 85) {
                key = gen() % LOOP_SIZE;
            } else {
                key = LOOP_SIZE + (gen() % LOOP_SIZE);
            }

            if (caches::loopLruCache.get(key) != 0) {
                ++lruHitsAtomic;
            } else {
                ++lruMissesAtomic;
                caches::loopLruCache.put(key, key * 10);
            }
            if (caches::loopLfuCache.get(key) != 0) {
                ++lfuHitsAtomic;
            } else {
                ++lfuMissesAtomic;
                caches::loopLfuCache.put(key, key * 10);
            }
            if (caches::loopArcCache.get(key) != 0) {
                ++arcHitsAtomic;
            } else {
                ++arcMissesAtomic;
                caches::loopArcCache.put(key, key * 10);
            }
        }
    };

    // Launch worker threads.
    std::vector<std::thread> threads;
    int opsPerThread = OPERATIONS / NUM_THREADS;
    int remainder = OPERATIONS % NUM_THREADS;
    int currentOp = 0;
    for (int i = 0; i < NUM_THREADS; ++i) {
        int endOp = currentOp + opsPerThread + (i < remainder ? 1 : 0);
        threads.emplace_back(worker, currentOp, endOp);
        currentOp = endOp;
    }
    for (auto &t : threads)
        t.join();

    lruHits = lruHitsAtomic.load();
    lruMisses = lruMissesAtomic.load();
    lfuHits = lfuHitsAtomic.load();
    lfuMisses = lfuMissesAtomic.load();
    arcHits = arcHitsAtomic.load();
    arcMisses = arcMissesAtomic.load();
}

// -----------------------
// Workload Shift Test
// -----------------------
/**
 * @brief Test cache performance with shifting workload patterns.
 * 
 * This test simulates a complex workload that shifts between different
 * access patterns over time: hot data access, large range access,
 * loop access, locality access, and mixed access. This tests how well
 * cache algorithms adapt to changing workload characteristics.
 * 
 * @param lruHits Reference to store LRU cache hit count.
 * @param lruMisses Reference to store LRU cache miss count.
 * @param lfuHits Reference to store LFU cache hit count.
 * @param lfuMisses Reference to store LFU cache miss count.
 * @param arcHits Reference to store ARC cache hit count.
 * @param arcMisses Reference to store ARC cache miss count.
 */
void testWorkloadShift(int& lruHits, int& lruMisses, int& lfuHits, int& lfuMisses, int& arcHits, int& arcMisses) {
    const int OPERATIONS = 80000;
    const int PHASE_LENGTH = OPERATIONS / 5;
    
    std::atomic<int> lruHitsAtomic{0}, lruMissesAtomic{0};
    std::atomic<int> lfuHitsAtomic{0}, lfuMissesAtomic{0};
    std::atomic<int> arcHitsAtomic{0}, arcMissesAtomic{0};

    // Worker function for Workload Shift Test.
    auto worker = [=, &lruHitsAtomic, &lruMissesAtomic, &lfuHitsAtomic, &lfuMissesAtomic, &arcHitsAtomic, &arcMissesAtomic](int startOp, int endOp) {
        std::random_device rd;
        std::mt19937 gen(rd());
        for(int key = 0; key < 1000; ++key) {
            caches::shiftLruCache.put(key, key * 10);
            caches::shiftLfuCache.put(key, key * 10);
            caches::shiftArcCache.put(key, key * 10);
        }

        for (int op = startOp; op < endOp; ++op) {
            int key;
            if (op < PHASE_LENGTH) { //hot data access
                key = gen() % 5;
            } else if (op < PHASE_LENGTH * 2) { //large range access
                key = gen() % 1000;
            } else if (op < PHASE_LENGTH * 3) { //loop access
                key = (op - PHASE_LENGTH * 2) % 100;
            } else if (op < PHASE_LENGTH * 4) { //locality access
                int locality = (op / 1000) % 10;
                key = locality * 20 + (gen() % 20);
            } else { //mixed access
                int r = gen() % 100;
                if (r < 30) {
                    key = gen() % 5;
                } else if (r < 60) {
                    key = 5 + (gen() % 95);
                } else {
                    key = 100 + (gen() % 900);
                }
            }
            if (caches::shiftLruCache.get(key) != 0) {
                ++lruHitsAtomic;
            } else {
                ++lruMissesAtomic;
                caches::shiftLruCache.put(key, key * 10);
            }
            if (caches::shiftLfuCache.get(key) != 0) {
                ++lfuHitsAtomic;
            } else {
                ++lfuMissesAtomic;
                caches::shiftLfuCache.put(key, key * 10);
            }
            if (caches::shiftArcCache.get(key) != 0) {
                ++arcHitsAtomic;
            } else {
                ++arcMissesAtomic;
                caches::shiftArcCache.put(key, key * 10);
            }
        }
    };

    // Launch worker threads.
    std::vector<std::thread> threads;
    int opsPerThread = OPERATIONS / NUM_THREADS;
    int remainder = OPERATIONS % NUM_THREADS;
    int currentOp = 0;
    for (int i = 0; i < NUM_THREADS; ++i) {
        int endOp = currentOp + opsPerThread + (i < remainder ? 1 : 0);
        threads.emplace_back(worker, currentOp, endOp);
        currentOp = endOp;
    }
    for (auto &t : threads)
        t.join();

    lruHits = lruHitsAtomic.load();
    lruMisses = lruMissesAtomic.load();
    lfuHits = lfuHitsAtomic.load();
    lfuMisses = lfuMissesAtomic.load();
    arcHits = arcHitsAtomic.load();
    arcMisses = arcMissesAtomic.load();
}

/**
 * @brief Main function to run all cache performance tests.
 * 
 * This function coordinates the execution of all cache performance tests
 * (Hot Data Access, Loop Pattern, and Workload Shift) and prints a
 * comparative table of results showing hit rates for LRU, LFU, and ARC
 * cache algorithms.
 * 
 * @return 0 on successful completion.
 */
int testCaches() {
    // Arrays to store results for each test case.
    const std::string testNames[3] = {"Hot Data Access Test", "Loop Pattern Test", "Workload Shift Test"};
    int capacities[3] = {caches::HOT_CAPACITY, caches::LOOP_CAPACITY, caches::SHIFT_CAPACITY};
    int hits[3][3] = {0};
    int misses[3][3] = {0};

    // Run each test concurrently in its own thread.
    std::thread threadHot([&]() {
        testHotDataAccess(hits[0][0], misses[0][0], hits[0][1], misses[0][1], hits[0][2], misses[0][2]);
    });
    std::thread threadLoop([&]() {
        testLoopPattern(hits[1][0], misses[1][0], hits[1][1], misses[1][1], hits[1][2], misses[1][2]);
    });
    std::thread threadWork([&]() {
        testWorkloadShift(hits[2][0], misses[2][0], hits[2][1], misses[2][1], hits[2][2], misses[2][2]);
    });

    threadHot.join();
    threadLoop.join();
    threadWork.join();

    // Print results in a table format.
    printResultsTable(testNames, capacities, hits, misses);

    return 0;
}
