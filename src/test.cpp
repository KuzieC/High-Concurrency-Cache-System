#define CACHE_POLICY_1 Arc
#define CACHE_POLICY_2 Arc
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
const int NUM_THREADS = 16;

// Global cache instances for each test scenario.
// You only need to adjust the construction here.
namespace caches {
    // Hot Data Access Test caches.
    const int HOT_CAPACITY = 5;
    CACHE_POLICY_1<int, int> hotLruCache(HOT_CAPACITY);
    CACHE_POLICY_2<int, int> hotLfuCache(HOT_CAPACITY);
    CACHE_POLICY_3<int, int> hotArcCache(HOT_CAPACITY);

    // Loop Pattern Test caches.
    const int LOOP_CAPACITY = 50;
    CACHE_POLICY_1<int, int> loopLruCache(LOOP_CAPACITY);
    CACHE_POLICY_2<int, int> loopLfuCache(LOOP_CAPACITY);
    CACHE_POLICY_3<int, int> loopArcCache(LOOP_CAPACITY);

    // Workload Shift Test caches.
    const int SHIFT_CAPACITY = 4;
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

void printResults(const std::string& testName, int capacity, int hits, int misses) {
    std::cout << "\n=== " << testName << " ===\n";
    std::cout << "Cache Capacity: " << capacity << "\n";
    std::cout << "Hit Rate: " << std::fixed << std::setprecision(2)
              << (100.0 * hits / (hits + misses)) << "%\n";
    std::cout << "Miss Rate: " << std::fixed << std::setprecision(2)
              << (100.0 * misses / (hits + misses)) << "%\n";
}

// -----------------------
// Hot Data Access Test
// -----------------------
void testHotDataAccess() {
    const int OPERATIONS = 10000;
    const int HOT_KEYS = 2;
    const int COLD_KEYS = 5000;
    
    std::atomic<int> lruHits{0}, lruMisses{0};
    std::atomic<int> lfuHits{0}, lfuMisses{0};
    std::atomic<int> arcHits{0}, arcMisses{0};

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
                ++lruHits;
            } else {
                ++lruMisses;
            }
            if (caches::hotLfuCache.get(key) != 0) {
                ++lfuHits;
            } else {
                ++lfuMisses;
            }
            if (caches::hotArcCache.get(key) != 0) {
                ++arcHits;
            } else {
                ++arcMisses;
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

    printResults("Hot Data Access Test (LRU)", caches::HOT_CAPACITY, lruHits.load(), lruMisses.load());
    printResults("Hot Data Access Test (LFU)", caches::HOT_CAPACITY, lfuHits.load(), lfuMisses.load());
    printResults("Hot Data Access Test (ARC)", caches::HOT_CAPACITY, arcHits.load(), arcMisses.load());
}

// -----------------------
// Loop Pattern Test
// -----------------------
void testLoopPattern() {
    const int LOOP_SIZE = 500;
    const int OPERATIONS = 200000;
    
    std::atomic<int> lruHits{0}, lruMisses{0};
    std::atomic<int> lfuHits{0}, lfuMisses{0};
    std::atomic<int> arcHits{0}, arcMisses{0};

    // Pre-populate the global Loop Pattern caches.
    for (int key = 0; key < LOOP_SIZE; ++key) {
        caches::loopLruCache.put(key, key * 10);
        caches::loopLfuCache.put(key, key * 10);
        caches::loopArcCache.put(key, key * 10);
    }

    // Worker function for Loop Pattern Test.
    auto worker = [=, &lruHits, &lruMisses, &lfuHits, &lfuMisses, &arcHits, &arcMisses](int startOp, int endOp) {
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
                ++lruHits;
            } else {
                ++lruMisses;
                caches::loopLruCache.put(key, key * 10);
            }
            if (caches::loopLfuCache.get(key) != 0) {
                ++lfuHits;
            } else {
                ++lfuMisses;
                caches::loopLfuCache.put(key, key * 10);
            }
            if (caches::loopArcCache.get(key) != 0) {
                ++arcHits;
            } else {
                ++arcMisses;
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

    printResults("Loop Pattern Test (LRU)", caches::LOOP_CAPACITY, lruHits.load(), lruMisses.load());
    printResults("Loop Pattern Test (LFU)", caches::LOOP_CAPACITY, lfuHits.load(), lfuMisses.load());
    printResults("Loop Pattern Test (ARC)", caches::LOOP_CAPACITY, arcHits.load(), arcMisses.load());
}

// -----------------------
// Workload Shift Test
// -----------------------
void testWorkloadShift() {
    const int OPERATIONS = 80000;
    const int PHASE_LENGTH = OPERATIONS / 5;
    
    std::atomic<int> lruHits{0}, lruMisses{0};
    std::atomic<int> lfuHits{0}, lfuMisses{0};
    std::atomic<int> arcHits{0}, arcMisses{0};

    // Worker function for Workload Shift Test.
    auto worker = [=, &lruHits, &lruMisses, &lfuHits, &lfuMisses, &arcHits, &arcMisses](int startOp, int endOp) {
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
                ++lruHits;
            } else {
                ++lruMisses;
                caches::shiftLruCache.put(key, key * 10);
            }
            if (caches::shiftLfuCache.get(key) != 0) {
                ++lfuHits;
            } else {
                ++lfuMisses;
                caches::shiftLfuCache.put(key, key * 10);
            }
            if (caches::shiftArcCache.get(key) != 0) {
                ++arcHits;
            } else {
                ++arcMisses;
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

    printResults("Workload Shift Test (LRU)", caches::SHIFT_CAPACITY, lruHits.load(), lruMisses.load());
    printResults("Workload Shift Test (LFU)", caches::SHIFT_CAPACITY, lfuHits.load(), lfuMisses.load());
    printResults("Workload Shift Test (ARC)", caches::SHIFT_CAPACITY, arcHits.load(), arcMisses.load());
}

int testCaches() {
    // Run each test concurrently in its own thread.
    // (Each test internally spawns worker threads.)
    std::thread threadHot(testHotDataAccess);
    std::thread threadLoop(testLoopPattern);
    std::thread threadWork(testWorkloadShift);

    threadHot.join();
    threadLoop.join();
    threadWork.join();
    return 0;
}
