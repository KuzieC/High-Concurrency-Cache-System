#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>
#include <algorithm>
#include "../include/Lru.h"
#include "../include/Lfu.h"

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

void testHotDataAccess() {
    const int CAPACITY = 50;
    const int OPERATIONS = 500000;
    const int HOT_KEYS = 20;
    const int COLD_KEYS = 5000;

    Lru<int, int> lruCache(CAPACITY);
    Lfu<int, int> Lfu(CAPACITY);

    std::random_device rd;
    std::mt19937 gen(rd());

    int lruHits = 0, lruMisses = 0;
    int lfuHits = 0, lfuMisses = 0;

    for (int op = 0; op < OPERATIONS; ++op) {
        int key = (op % 100 < 70) ? gen() % HOT_KEYS : HOT_KEYS + (gen() % COLD_KEYS);

        // LRU Cache
        if (lruCache.get(key) != 0) {
            ++lruHits;
        } else {
            ++lruMisses;
            lruCache.put(key, key * 10);
        }

        // LFU Cache
        if (Lfu.get(key) != 0) {
            ++lfuHits;
        } else {
            ++lfuMisses;
            Lfu.put(key, key * 10);
        }
    }

    printResults("Hot Data Access Test (LRU)", CAPACITY, lruHits, lruMisses);
    printResults("Hot Data Access Test (LFU)", CAPACITY, lfuHits, lfuMisses);
}

void testLoopPattern() {
    const int CAPACITY = 50;
    const int LOOP_SIZE = 500;
    const int OPERATIONS = 200000;

    Lru<int, int> lruCache(CAPACITY);
    Lfu<int, int> Lfu(CAPACITY);

    int lruHits = 0, lruMisses = 0;
    int lfuHits = 0, lfuMisses = 0;

    for (int key = 0; key < LOOP_SIZE; ++key) {
        lruCache.put(key, key * 10);
        Lfu.put(key, key * 10);
    }

    int currentPos = 0;
    std::random_device rd;
    std::mt19937 gen(rd());

    for (int op = 0; op < OPERATIONS; ++op) {
        int key;
        if (op % 100 < 60) {
            key = currentPos;
            currentPos = (currentPos + 1) % LOOP_SIZE;
        } else if (op % 100 < 90) {
            key = gen() % LOOP_SIZE;
        } else {
            key = LOOP_SIZE + (gen() % LOOP_SIZE);
        }

        // LRU Cache
        if (lruCache.get(key) != 0) {
            ++lruHits;
        } else {
            ++lruMisses;
            lruCache.put(key, key * 10);
        }

        // LFU Cache
        if (Lfu.get(key) != 0) {
            ++lfuHits;
        } else {
            ++lfuMisses;
            Lfu.put(key, key * 10);
        }
    }

    printResults("Loop Pattern Test (LRU)", CAPACITY, lruHits, lruMisses);
    printResults("Loop Pattern Test (LFU)", CAPACITY, lfuHits, lfuMisses);
}

void testWorkloadShift() {
    const int CAPACITY = 4;
    const int OPERATIONS = 80000;
    const int PHASE_LENGTH = OPERATIONS / 5;

    Lru<int, int> lruCache(CAPACITY);
    Lfu<int, int> Lfu(CAPACITY);

    int lruHits = 0, lruMisses = 0;
    int lfuHits = 0, lfuMisses = 0;

    std::random_device rd;
    std::mt19937 gen(rd());

    for (int op = 0; op < OPERATIONS; ++op) {
        int key;
        if (op < PHASE_LENGTH) {
            key = gen() % 5;
        } else if (op < PHASE_LENGTH * 2) {
            key = gen() % 1000;
        } else if (op < PHASE_LENGTH * 3) {
            key = (op - PHASE_LENGTH * 2) % 100;
        } else if (op < PHASE_LENGTH * 4) {
            int locality = (op / 1000) % 10;
            key = locality * 20 + (gen() % 20);
        } else {
            int r = gen() % 100;
            if (r < 30) {
                key = gen() % 5;
            } else if (r < 60) {
                key = 5 + (gen() % 95);
            } else {
                key = 100 + (gen() % 900);
            }
        }

        // LRU Cache
        if (lruCache.get(key) != 0) {
            ++lruHits;
        } else {
            ++lruMisses;
            lruCache.put(key, key * 10);
        }

        // LFU Cache
        if (Lfu.get(key) != 0) {
            ++lfuHits;
        } else {
            ++lfuMisses;
            Lfu.put(key, key * 10);
        }
    }

    printResults("Workload Shift Test (LRU)", CAPACITY, lruHits, lruMisses);
    printResults("Workload Shift Test (LFU)", CAPACITY, lfuHits, lfuMisses);
}

int testCaches() {
    testHotDataAccess();
    testLoopPattern();
    testWorkloadShift();
    return 0;
}